#ifndef DYNAMIC_IMU_PROCESSING_ATTITUDE_H
#define DYNAMIC_IMU_PROCESSING_ATTITUDE_H

#include <stdint.h>
#include "Math.h"

namespace imu {
enum State : uint8_t {
    Uninitialized,
    Calibrating,
    Tracking,
    GyroOnly,
    TimingFault,
    SensorFault,
    Saturation,
    InvalidCalibration,
    LabUncalibrated
};

struct Angles {
    float x, y, tilt;
    bool yDefined;
};

inline Angles inclination(Vec up) {
    float n = up.norm();
    if (!up.finite() || n < 1e-6f) {
        return Angles{NAN, NAN, NAN, false};
    }
    up = up * (1 / n);
    float h = sqrtf(up.x * up.x + up.z * up.z);
    float y = h > 1e-6f ? atan2f(up.z, up.x) * 180 / pi : NAN;
    if (y <= -180) {
        y = 180; // canonical cut, including negative zero input
    }
    return Angles{atan2f(-up.y, h) * 180 / pi, y, acosf(clamp(up.x, -1, 1)) * 180 / pi, h > 1e-6f};
}

inline float wrapDegrees(float d) {
    return d - 360.0f * floorf((d + 180.0f) / 360.0f);
}
} // namespace imu

#endif
