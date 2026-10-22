#ifndef DYNAMIC_IMU_SENSORS_MPU6050_SPECS_H
#define DYNAMIC_IMU_SENSORS_MPU6050_SPECS_H

namespace mpu6050 {
// InvenSense PS-MPU-6000A-00 rev. 3.4, seções 6.1–6.2, 25 °C.
// São dados de catálogo, não coeficientes medidos deste exemplar.
constexpr float accelCountsPerG = 16384.0f; // AFS_SEL=0, ±2 g.
constexpr float gyroCountsPerDps = 131.0f;  // FS_SEL=0, ±250 °/s.
constexpr float sensitivityTolerance = 0.03f;
constexpr float gyroInitialBiasDps = 20.0f;
constexpr float gyroTemperatureVariationDps = 20.0f;

// Políticas de partida: margem sobre os valores de catálogo, não prova de repouso.
// A norma admite três eixos e rotação da montagem; 1 °/s é margem de engenharia.
constexpr float startupGyroNormDps =
    1.7320508f * (gyroInitialBiasDps + gyroTemperatureVariationDps) + 1.0f;
// ±3% de escala e offsets típicos de 50/50/80 mg motivam esta margem.
// Os offsets são típicos após MSL3, não limites máximos garantidos.
constexpr float startupAccelNormTolerance = 0.15f;
} // namespace mpu6050

#endif
