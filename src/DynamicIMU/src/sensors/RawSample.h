#ifndef DYNAMIC_IMU_SENSORS_RAWSAMPLE_H
#define DYNAMIC_IMU_SENSORS_RAWSAMPLE_H

#include <stdint.h>

namespace imu {
inline int16_t signedWord(uint8_t hi, uint8_t lo) {
    const uint16_t u = static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    const int32_t s = u >= 32768u ? static_cast<int32_t>(u) - static_cast<int32_t>(65536)
                                  : static_cast<int32_t>(u);
    return static_cast<int16_t>(s);
}

struct Raw {
    int16_t a[3], temp, g[3];
};

inline Raw decode(const uint8_t p[14]) {
    Raw r{};
    for (uint8_t i = 0; i < 3; ++i) {
        r.a[i] = signedWord(p[2 * i], p[2 * i + 1]);
        r.g[i] = signedWord(p[8 + 2 * i], p[9 + 2 * i]);
    }
    r.temp = signedWord(p[6], p[7]);
    return r;
}

inline bool saturated(const Raw &r) {
    for (uint8_t i = 0; i < 3; ++i) {
        if (r.a[i] > 32700 || r.a[i] < -32700 || r.g[i] > 32700 || r.g[i] < -32700) {
            return true;
        }
    }
    return false;
}
} // namespace imu

#endif
