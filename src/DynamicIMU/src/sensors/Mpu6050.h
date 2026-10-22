#ifndef DYNAMIC_IMU_SENSORS_MPU6050_H
#define DYNAMIC_IMU_SENSORS_MPU6050_H

#include "RawSample.h"

namespace sensors {
struct Sample {
    imu::Raw raw;
    uint32_t timestamp;
    uint16_t acquisitionUs;
};

enum class PollResult : uint8_t { Idle, SampleReady, Fault };

// Aquisição bruta: configuração, DATA_RDY, burst coerente e diagnóstico I²C.
// Não conhece o estimador, a serial ou a interface gráfica.
class Mpu6050 {
  public:
    bool configure();
    PollResult poll(uint32_t now, Sample &sample);
    bool checkConfiguration(uint32_t now);

    uint32_t sequence() const {
        return sequence_;
    }

    uint32_t estimatedMisses() const {
        return estimatedMisses_;
    }

  private:
    static constexpr uint8_t address = 0x68;
    bool connected_ = false;
    uint32_t lastPoll_ = 0, lastConfig_ = 0, lastSample_ = 0;
    uint32_t sequence_ = 0, estimatedMisses_ = 0;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count);
    bool configurationMatches();
};
} // namespace sensors

#endif
