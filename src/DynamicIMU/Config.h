#ifndef DYNAMIC_IMU_CONFIG_H
#define DYNAMIC_IMU_CONFIG_H
#include "ImuCore.h"
// No historical constants are treated as new calibration measurements.
// Replace CalibrationData.h with output of scripts/calibrate.py after six-position acquisition.
#include "CalibrationData.h"
constexpr bool ALLOW_UNCALIBRATED_LAB = false;
constexpr bool ENABLE_TFT = true;

inline imu::Calibration instrumentCalibration() {
    imu::Calibration c;
    c.id = CALIBRATION_ID;
    c.measured = CALIBRATION_MEASURED;
    c.offset = imu::Vec(ACCEL_OFFSET[0], ACCEL_OFFSET[1], ACCEL_OFFSET[2]);
    c.countsPerG = imu::Vec(ACCEL_COUNTS_PER_G[0], ACCEL_COUNTS_PER_G[1], ACCEL_COUNTS_PER_G[2]);
    c.sensorToBody = imu::Quat(MOUNT_QUAT[0], MOUNT_QUAT[1], MOUNT_QUAT[2], MOUNT_QUAT[3]);
    return c;
}
#endif
