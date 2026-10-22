#ifndef DYNAMIC_IMU_PROCESSING_CALIBRATION_H
#define DYNAMIC_IMU_PROCESSING_CALIBRATION_H

#include "Math.h"
#include "../sensors/RawSample.h"
#include "../sensors/Mpu6050Specs.h"

namespace imu {
// Acelerômetro em g; girômetro em rad/s; montagem transforma sensor em corpo.
struct Calibration {
    uint16_t version = 1, id = 0;
    Vec offset,
        countsPerG =
            Vec(mpu6050::accelCountsPerG, mpu6050::accelCountsPerG, mpu6050::accelCountsPerG);
    Quat sensorToBody; // proper rotation; identity is a mounting assumption
    bool measured = false;

    bool valid() const {
        float n = sensorToBody.w * sensorToBody.w + sensorToBody.x * sensorToBody.x +
                  sensorToBody.y * sensorToBody.y + sensorToBody.z * sensorToBody.z;
        return version == 1 && offset.finite() && offset.norm() < 16000 && countsPerG.finite() &&
               countsPerG.x > 8000 && countsPerG.x < 24000 && countsPerG.y > 8000 &&
               countsPerG.y < 24000 && countsPerG.z > 8000 && countsPerG.z < 24000 && isfinite(n) &&
               fabsf(n - 1) < 0.001f;
    }

    Vec accel(const Raw &r) const {
        return sensorToBody.rotate(Vec((r.a[0] - offset.x) / countsPerG.x,
                                       (r.a[1] - offset.y) / countsPerG.y,
                                       (r.a[2] - offset.z) / countsPerG.z));
    }

    Vec gyro(const Raw &r) const {
        return sensorToBody.rotate(Vec(r.g[0], r.g[1], r.g[2]) *
                                   (pi / (180.0f * mpu6050::gyroCountsPerDps)));
    }
};
} // namespace imu

#endif
