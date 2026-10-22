#include "Mpu6050.h"
#include <Arduino.h>
#include <Wire.h>

namespace sensors {
bool Mpu6050::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
}

bool Mpu6050::readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(address, count, static_cast<uint8_t>(true)) != count) {
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

bool Mpu6050::configurationMatches() {
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

bool Mpu6050::configure() {
    const uint8_t regs[] = {0x6B, 0x6C, 0x23, 0x6A, 0x19, 0x1A, 0x1B, 0x1C, 0x37, 0x38};
    const uint8_t values[] = {1, 0, 0, 0, 19, 3, 0, 0, 0, 1};
    // 1 kHz/(19+1)=50 Hz, DLPF_CFG=3, +/-2 g and +/-250 deg/s; FIFO disabled.
    for (uint8_t i = 0; i < 10; ++i) {
        if (!writeRegister(regs[i], values[i])) {
            connected_ = false;
            return false;
        }
    }
    connected_ = configurationMatches();
    return connected_;
}

PollResult Mpu6050::poll(uint32_t now, Sample &sample) {
    if (!connected_ || static_cast<uint32_t>(now - lastPoll_) < 1000u) {
        return PollResult::Idle;
    }
    lastPoll_ = now;
    uint8_t status = 0;
    if (!readRegisters(0x3A, &status, 1)) {
        connected_ = false;
        return PollResult::Fault;
    }
    if (!(status & 1)) {
        return PollResult::Idle;
    }

    uint8_t bytes[14];
    uint32_t start = micros();
    if (!readRegisters(0x3B, bytes, 14)) {
        connected_ = false;
        return PollResult::Fault;
    }
    sample.timestamp = micros();
    sample.acquisitionUs = static_cast<uint16_t>(sample.timestamp - start);
    sample.raw = imu::decode(bytes);
    ++sequence_;
    if (lastSample_) {
        uint32_t gap = sample.timestamp - lastSample_;
        if (gap > 30000u) {
            estimatedMisses_ += (gap + 10000u) / 20000u - 1;
        }
    }
    lastSample_ = sample.timestamp;
    return PollResult::SampleReady;
}

bool Mpu6050::checkConfiguration(uint32_t now) {
    if (static_cast<uint32_t>(now - lastConfig_) < 1000000u) {
        return true;
    }
    lastConfig_ = now;
    if (connected_ && !configurationMatches()) {
        connected_ = false;
        return false;
    }
    // A recuperação é explícita: configure() só é chamado na partida ou no comando C.
    return true;
}
} // namespace sensors
