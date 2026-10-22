// Revisão de 2026. Hardware e pinagem preservados da implementação de 2018.
#include <Wire.h>
#include <UTFT.h>
#include <URTouch.h>
#include "Config.h"

extern uint8_t BigFont[];
UTFT lcd(SSD1289, 38, 39, 40, 41);
URTouch touch(6, 5, 4, 3, 2);
imu::Calibration calibration;
imu::Observer observer;
constexpr uint8_t MPU = 0x68;
uint32_t sequence = 0, faults = 0, estimatedMisses = 0, telemetryDrops = 0;
uint32_t lastPoll = 0, lastConfig = 0, lastSample = 0, lastRender = 0, maxRenderUs = 0;
uint8_t tx[80], txPosition = 80, renderLine = 0;
bool device = false, touched = false;
uint16_t acquisitionUs = 0;
imu::State displayedState = imu::Uninitialized;

bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
}

bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
    Wire.beginTransmission(MPU);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(MPU, count, static_cast<uint8_t>(true)) != count) {
        while (Wire.available()) {
            Wire.read();
        }
        return false;
    }
    for (uint8_t i = 0; i < count; ++i) {
        data[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}

bool configurationMatches() {
    const uint8_t regs[] = {0x6B, 0x19, 0x1A, 0x1B, 0x1C, 0x38};
    const uint8_t values[] = {1, 19, 3, 0, 0, 1};
    for (uint8_t i = 0; i < 6; ++i) {
        uint8_t x = 0;
        if (!readRegisters(regs[i], &x, 1) || x != values[i]) {
            return false;
        }
    }
    uint8_t id = 0;
    return readRegisters(0x75, &id, 1) && (id & 0x7E) == 0x68;
}

bool configure() {
    const uint8_t regs[] = {0x6B, 0x6C, 0x23, 0x6A, 0x19, 0x1A, 0x1B, 0x1C, 0x37, 0x38};
    const uint8_t values[] = {1, 0, 0, 0, 19, 3, 0, 0, 0, 1};
    // 1 kHz/(19+1)=50 Hz, DLPF_CFG=3, +/-2 g and +/-250 deg/s; FIFO disabled.
    for (uint8_t i = 0; i < 10; ++i) {
        if (!writeRegister(regs[i], values[i])) {
            return false;
        }
    }
    return configurationMatches();
}

void fault(imu::State s) {
    ++faults;
    observer.fault(s);
}

uint16_t crc16(const uint8_t *p, uint8_t count) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < count; ++i) {
        crc ^= static_cast<uint16_t>(p[i]) << 8;
        for (uint8_t j = 0; j < 8; ++j) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

void put32(uint8_t pos, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) {
        tx[pos + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

void put16(uint8_t pos, uint16_t value) {
    tx[pos] = static_cast<uint8_t>(value);
    tx[pos + 1] = static_cast<uint8_t>(value >> 8);
}

void telemetry(const imu::Raw &raw, uint32_t stamp) {
    if (txPosition < 80) {
        ++telemetryDrops;
        return;
    }
    memset(tx, 0, sizeof tx);
    tx[0] = 0xA5;
    tx[1] = 0x5A;
    tx[2] = 1;
    tx[3] = observer.state;
    put32(4, sequence);
    put32(8, stamp);
    for (uint8_t i = 0; i < 3; ++i) {
        put16(12 + 2 * i, static_cast<uint16_t>(raw.a[i]));
        put16(20 + 2 * i, static_cast<uint16_t>(raw.g[i]));
    }
    put16(18, static_cast<uint16_t>(raw.temp));
    const float values[] = {observer.q.w,
                            observer.q.x,
                            observer.q.y,
                            observer.q.z,
                            observer.bias.x,
                            observer.bias.y,
                            observer.bias.z};
    static_assert(sizeof(float) == 4, "Telemetry requires IEEE binary32");
    memcpy(tx + 26, values, sizeof values);
    put16(54, calibration.id);
    put32(56, faults);
    put32(60, estimatedMisses);
    put32(64, telemetryDrops);
    put32(68, maxRenderUs);
    put16(72,
          (observer.ready ? 1 : 0) | (calibration.measured ? 2 : 0) |
              (ALLOW_UNCALIBRATED_LAB ? 4 : 0) | (observer.hasTare ? 8 : 0));
    put16(74, 1);
    put16(76, acquisitionUs);
    put16(78, crc16(tx, 78));
    txPosition = 0;
}

void serviceSerial() {
    int space = Serial.availableForWrite();
    if (txPosition < 80 && space > 0) {
        uint8_t n = static_cast<uint8_t>(space);
        if (n > 80 - txPosition) {
            n = 80 - txPosition;
        }
        Serial.write(tx + txPosition, n);
        txPosition += n;
    }
    if (Serial.available()) {
        char command = static_cast<char>(Serial.read());
        if (command == 'c') {
            device = configure();
            if (device) {
                observer.beginCalibration();
            } else {
                fault(imu::SensorFault);
            }
        }
        if (command == 't' && !observer.stale(micros())) {
            observer.tare();
        }
        if (command == 'u') {
            observer.hasTare = false;
        }
        if (command == 'r') {
            observer.fault(imu::Uninitialized); // explicit C needed to initialize again
        }
    }
}

const char *stateLabel() {
    switch (observer.state) {
    case imu::Calibrating:
        return "CALIBRANDO";
    case imu::Tracking:
        return "FUSAO";
    case imu::GyroOnly:
        return observer.lab ? "LAB SO GIRO" : "SO GIRO";
    case imu::TimingFault:
        return "FALHA TEMPO";
    case imu::SensorFault:
        return "FALHA SENSOR";
    case imu::Saturation:
        return "SATURACAO";
    case imu::InvalidCalibration:
        return "SEM CALIBRACAO";
    case imu::LabUncalibrated:
        return "LAB NAO CALIBRADO";
    default:
        return "INICIALIZAR C";
    }
}

void serviceDisplay(uint32_t now) {
    if (!ENABLE_TFT || static_cast<uint32_t>(now - lastRender) < 50000u) {
        return;
    }
    lastRender = now;
    if (displayedState != observer.state) {
        renderLine = 0;
        displayedState = observer.state;
    }
    uint32_t start = micros();
    // One text row per call. Actual worst call duration is reported; support rate needs
    // measurement.
    lcd.setColor(255, 255, 255);
    lcd.setBackColor(0, 0, 0);
    imu::Angles a = observer.angles();
    const int y = 40 + renderLine * 30;
    char blank[] = "                    ";
    lcd.print(blank, 0, y);
    bool current = !observer.stale(now);
    switch (renderLine) {
    case 0:
        lcd.print(const_cast<char *>(stateLabel()), 0, y);
        break;
    case 1:
        lcd.print(const_cast<char *>("X deg"), 0, y);
        if (current) {
            lcd.printNumF(a.x, 2, 120, y);
        } else {
            lcd.print(const_cast<char *>("---"), 120, y);
        }
        break;
    case 2:
        lcd.print(const_cast<char *>("Y deg"), 0, y);
        if (current && a.yDefined) {
            lcd.printNumF(a.y, 2, 120, y);
        } else {
            lcd.print(const_cast<char *>("---"), 120, y);
        }
        break;
    case 3:
        lcd.print(const_cast<char *>("Tilt abs deg"), 0, y);
        if (current) {
            lcd.printNumF(a.tilt, 2, 208, y);
        } else {
            lcd.print(const_cast<char *>("---"), 208, y);
        }
        break;
    case 4:
        lcd.print(current ? (observer.hasTare ? "RELATIVO" : "ABSOLUTO") : "SEM DADO ATUAL", 0, y);
        break;
    case 5:
        lcd.print(const_cast<char *>("Tara    Calibrar"), 0, y);
        break;
    }
    renderLine = static_cast<uint8_t>((renderLine + 1) % 6);
    const uint32_t elapsed = micros() - start;
    if (elapsed > maxRenderUs) {
        maxRenderUs = elapsed;
    }
    // No additional interrupt: URTouch already uses pin 2 in the historical wiring.
    if (touch.dataAvailable()) {
        touch.read();
        int x = touch.getX(), ty = touch.getY();
        if (!touched && ty >= 180 && ty < 220) {
            if (x >= 0 && x < 140 && !observer.stale(micros())) {
                observer.tare();
            }
            if (x >= 160 && x < 320) {
                device = configure();
                if (device) {
                    observer.beginCalibration();
                } else {
                    fault(imu::SensorFault);
                }
            }
        }
        touched = true;
    } else {
        touched = false;
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000);
    Wire.setWireTimeout(3000, true); // Arduino AVR core 1.8.6
    calibration = instrumentCalibration();
    if (ENABLE_TFT) {
        lcd.InitLCD(LANDSCAPE);
        lcd.setFont(BigFont);
        lcd.clrScr();
        touch.InitTouch(LANDSCAPE);
        touch.setPrecision(PREC_HI);
    }
    delay(100);
    device = configure();
    if (!device) {
        fault(imu::SensorFault);
    } else if (!calibration.valid()) {
        fault(imu::InvalidCalibration);
    } else {
        observer.beginCalibration();
    }
}

void loop() {
    const uint32_t now = micros();
    if (device && static_cast<uint32_t>(now - lastPoll) >= 1000u) {
        lastPoll = now;
        uint8_t status = 0;
        if (!readRegisters(0x3A, &status, 1)) {
            device = false;
            fault(imu::SensorFault);
        } else if (status & 1) {
            uint8_t bytes[14];
            uint32_t start = micros();
            if (!readRegisters(0x3B, bytes, 14)) {
                device = false;
                fault(imu::SensorFault);
            } else {
                uint32_t stamp = micros();
                acquisitionUs = static_cast<uint16_t>(stamp - start);
                ++sequence;
                if (lastSample) {
                    uint32_t gap = stamp - lastSample;
                    if (gap > 30000u) {
                        estimatedMisses += (gap + 10000u) / 20000u - 1;
                    }
                }
                lastSample = stamp;
                imu::Raw raw = imu::decode(bytes);
                if (!calibration.valid()) {
                    fault(imu::InvalidCalibration);
                } else if (imu::saturated(raw)) {
                    fault(imu::Saturation);
                } else {
                    observer.sample(calibration.accel(raw),
                                    calibration.gyro(raw),
                                    stamp,
                                    calibration.measured,
                                    ALLOW_UNCALIBRATED_LAB);
                }
                telemetry(raw, stamp); // Raw packets remain available before calibration.
            }
        }
    }
    if (static_cast<uint32_t>(now - lastConfig) >= 1000000u) {
        lastConfig = now;
        if (device && !configurationMatches()) {
            device = false;
            fault(imu::SensorFault);
        }
        // Recovery is explicit via C. A reset cannot silently retain a valid old attitude.
    }
    if (observer.ready && observer.stale(micros())) {
        fault(imu::TimingFault);
    }
    serviceSerial();
    serviceDisplay(micros());
}
