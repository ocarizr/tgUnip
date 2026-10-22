#ifndef DYNAMIC_IMU_PROCESSING_INSTRUMENT_H
#define DYNAMIC_IMU_PROCESSING_INSTRUMENT_H

#include "Calibration.h"
#include "Observer.h"

namespace processing {
// Aplica a calibração, verifica as amostras e controla o estado do estimador.
// Recebe timestamps como dados: nenhuma dependência de Arduino ou de I²C.
class Instrument {
  public:
    Instrument(const imu::Calibration &calibration, bool allowNominal);
    void initialize(bool sensorAvailable);
    void requestCalibration(bool sensorAvailable);
    void accept(const imu::Raw &raw, uint32_t timestamp);
    void sensorFault();
    void checkFreshness(uint32_t now);
    void tare(uint32_t now);
    void clearTare();
    void reset();

    const imu::Observer &observer() const {
        return observer_;
    }

    const imu::Calibration &calibration() const {
        return calibration_;
    }

    uint32_t faults() const {
        return observer_.faultCount;
    }

  private:
    imu::Calibration calibration_;
    imu::Observer observer_;
    bool allowNominal_;

    void fault(imu::State state);
};
} // namespace processing

#endif
