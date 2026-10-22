#include "Instrument.h"

namespace processing {
Instrument::Instrument(const imu::Calibration &calibration, bool allowNominal)
    : calibration_(calibration), allowNominal_(allowNominal) {}

void Instrument::initialize(bool sensorAvailable) {
    if (!sensorAvailable) {
        sensorFault();
    } else if (!calibration_.valid()) {
        fault(imu::InvalidCalibration);
    } else {
        observer_.beginCalibration();
    }
}

void Instrument::requestCalibration(bool sensorAvailable) {
    if (sensorAvailable) {
        observer_.beginCalibration();
    } else {
        sensorFault();
    }
}

void Instrument::accept(const imu::Raw &raw, uint32_t timestamp) {
    if (!calibration_.valid()) {
        fault(imu::InvalidCalibration);
    } else if (imu::saturated(raw)) {
        fault(imu::Saturation);
    } else {
        observer_.sample(calibration_.accel(raw),
                         calibration_.gyro(raw),
                         timestamp,
                         calibration_.measured,
                         allowNominal_);
    }
}

void Instrument::fault(imu::State state) {
    observer_.fault(state);
}

void Instrument::sensorFault() {
    fault(imu::SensorFault);
}

void Instrument::checkFreshness(uint32_t now) {
    if (observer_.ready && observer_.stale(now)) {
        fault(imu::TimingFault);
    }
}

void Instrument::tare(uint32_t now) {
    if (!observer_.stale(now)) {
        observer_.tare();
    }
}

void Instrument::clearTare() {
    observer_.hasTare = false;
}

void Instrument::reset() {
    observer_.fault(imu::Uninitialized); // Exige C para inicializar novamente.
}
} // namespace processing
