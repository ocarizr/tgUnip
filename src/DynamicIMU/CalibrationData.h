#ifndef DYNAMIC_IMU_CALIBRATION_DATA_H
#define DYNAMIC_IMU_CALIBRATION_DATA_H
// Nominal factory sensitivity, NOT measured calibration. Mounting identity requires verification.
constexpr unsigned int CALIBRATION_ID = 0;
constexpr bool CALIBRATION_MEASURED = false;
constexpr float ACCEL_OFFSET[3] = {0, 0, 0};
constexpr float ACCEL_COUNTS_PER_G[3] = {16384, 16384, 16384};
constexpr float MOUNT_QUAT[4] = {1, 0, 0, 0};
#endif
