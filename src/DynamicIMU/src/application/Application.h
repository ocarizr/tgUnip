#ifndef DYNAMIC_IMU_APPLICATION_H
#define DYNAMIC_IMU_APPLICATION_H

#include "../sensors/Mpu6050.h"
#include "../processing/Instrument.h"
#include "../telemetry/SerialChannel.h"
#include "../ui/Screen.h"

namespace application {
// Liga os módulos e mantém a ordem das tarefas cooperativas do firmware.
class Application {
  public:
    Application(const imu::Calibration &calibration, bool allowNominal, bool enableScreen);
    void begin();
    void update();

  private:
    sensors::Mpu6050 sensor_;
    processing::Instrument instrument_;
    telemetry::SerialChannel serial_;
    ui::Screen screen_;
    bool allowNominal_;

    void handleCommand(char command);
    void publish(const sensors::Sample &sample);
    ui::ViewModel viewModel(uint32_t now) const;
};
} // namespace application

#endif
