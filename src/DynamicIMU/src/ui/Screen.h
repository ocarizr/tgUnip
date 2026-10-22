#ifndef DYNAMIC_IMU_UI_SCREEN_H
#define DYNAMIC_IMU_UI_SCREEN_H

#include "../hardware/DisplayHardware.h"
#include "../processing/Attitude.h"

namespace ui {
struct ViewModel {
    imu::State state;
    imu::Angles angles;
    bool nominal, current, relative;
};

enum class Action : uint8_t { None, Tare, Calibrate };

// Desenha valores recebidos e devolve intenções do usuário. Não lê o MPU,
// não calibra o estimador e não escreve na serial.
class Screen {
  public:
    explicit Screen(bool enabled) : enabled_(enabled) {}

    void begin();
    bool due(uint32_t now) const;
    Action service(const ViewModel &model, uint32_t now);

    uint32_t maxRenderUs() const {
        return maxRenderUs_;
    }

  private:
    hardware::DisplayHardware hardware_;
    bool enabled_, touched_ = false;
    uint32_t lastRender_ = 0, maxRenderUs_ = 0;
    uint8_t renderLine_ = 0;
    imu::State displayedState_ = imu::Uninitialized;

    void drawRow(const ViewModel &model);
    Action readTouch();
};
} // namespace ui

#endif
