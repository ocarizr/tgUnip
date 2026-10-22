#include "Screen.h"
#include <Arduino.h>

namespace ui {
namespace {
const char *stateLabel(const ViewModel &model) {
    switch (model.state) {
    case imu::Calibrating:
        return "CALIBRANDO";
    case imu::Tracking:
        return "FUSAO";
    case imu::GyroOnly:
        return model.nominal ? "NOMINAL SO GIRO" : "SO GIRO";
    case imu::TimingFault:
        return "FALHA TEMPO";
    case imu::SensorFault:
        return "FALHA SENSOR";
    case imu::Saturation:
        return "SATURACAO";
    case imu::InvalidCalibration:
        return "SEM CALIBRACAO";
    case imu::NominalCalibration:
        return "NOMINAL";
    default:
        return "INICIALIZAR C";
    }
}

} // namespace

void Screen::begin() {
    if (enabled_) {
        hardware_.begin();
    }
}

void Screen::drawRow(const ViewModel &model) {
    // One text row per call. Actual worst call duration is reported; support rate needs
    // measurement.
    hardware_.lcd.setColor(255, 255, 255);
    hardware_.lcd.setBackColor(0, 0, 0);
    const imu::Angles &a = model.angles;
    const int y = 40 + renderLine_ * 30;
    char blank[] = "                    ";
    hardware_.lcd.print(blank, 0, y);
    const bool current = model.current;
    switch (renderLine_) {
    case 0:
        hardware_.lcd.print(const_cast<char *>(stateLabel(model)), 0, y);
        break;
    case 1:
        hardware_.lcd.print(const_cast<char *>("X deg"), 0, y);
        if (current) {
            hardware_.lcd.printNumF(a.x, 2, 120, y);
        } else {
            hardware_.lcd.print(const_cast<char *>("---"), 120, y);
        }
        break;
    case 2:
        hardware_.lcd.print(const_cast<char *>("Y deg"), 0, y);
        if (current && a.yDefined) {
            hardware_.lcd.printNumF(a.y, 2, 120, y);
        } else {
            hardware_.lcd.print(const_cast<char *>("---"), 120, y);
        }
        break;
    case 3:
        hardware_.lcd.print(const_cast<char *>("Tilt abs deg"), 0, y);
        if (current) {
            hardware_.lcd.printNumF(a.tilt, 2, 208, y);
        } else {
            hardware_.lcd.print(const_cast<char *>("---"), 208, y);
        }
        break;
    case 4:
        hardware_.lcd.print(
            current ? (model.relative ? "RELATIVO" : "ABSOLUTO") : "SEM DADO ATUAL", 0, y);
        break;
    case 5:
        hardware_.lcd.print(const_cast<char *>("Tara    Calibrar"), 0, y);
        break;
    }
}

Action Screen::readTouch() {
    // O toque compartilha o pino 2 da pinagem histórica; não há nova interrupção.
    if (!hardware_.touch.dataAvailable()) {
        touched_ = false;
        return Action::None;
    }
    hardware_.touch.read();
    int x = hardware_.touch.getX(), y = hardware_.touch.getY();
    Action action = Action::None;
    if (!touched_ && y >= 180 && y < 220) {
        if (x >= 0 && x < 140) {
            action = Action::Tare;
        }
        if (x >= 160 && x < 320) {
            action = Action::Calibrate;
        }
    }
    touched_ = true;
    return action;
}

bool Screen::due(uint32_t now) const {
    return enabled_ && static_cast<uint32_t>(now - lastRender_) >= 50000u;
}

Action Screen::service(const ViewModel &model, uint32_t now) {
    if (!due(now)) {
        return Action::None;
    }
    lastRender_ = now;
    if (displayedState_ != model.state) {
        renderLine_ = 0;
        displayedState_ = model.state;
    }
    // now também precede a preparação do ViewModel pela aplicação. Contar esse
    // trabalho mantém o diagnóstico do custo de atualização, além do desenho.
    const uint32_t start = now;
    drawRow(model);
    renderLine_ = static_cast<uint8_t>((renderLine_ + 1) % 6);
    const uint32_t elapsed = micros() - start;
    if (elapsed > maxRenderUs_) {
        maxRenderUs_ = elapsed;
    }
    return readTouch();
}
} // namespace ui
