#ifndef DYNAMIC_IMU_CALIBRATION_DATA_H
#define DYNAMIC_IMU_CALIBRATION_DATA_H
// InvenSense PS-MPU-6000A-00 rev. 3.4: ±2 g, sensibilidade nominal, ID reservado zero.
// Os CSVs históricos não foram recuperados; estes não são coeficientes individuais medidos.
// A identidade da montagem exige conferência física; o viés do giro é estimado em repouso.
constexpr unsigned int CALIBRATION_ID = 0;
constexpr bool CALIBRATION_MEASURED = false;
constexpr float ACCEL_OFFSET[3] = {0, 0, 0};
constexpr float ACCEL_COUNTS_PER_G[3] = {16384, 16384, 16384};
constexpr float MOUNT_QUAT[4] = {1, 0, 0, 0};
#endif
