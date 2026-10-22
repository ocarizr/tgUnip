#ifndef DYNAMIC_IMU_PROCESSING_STATISTICS_H
#define DYNAMIC_IMU_PROCESSING_STATISTICS_H

#include <stdint.h>
#include "Math.h"

namespace imu {
// Média e variância incrementais de Welford, sem armazenar a janela de amostras.
struct Moments {
    uint16_t n = 0;
    Vec mean, m2;

    void add(Vec v) {
        ++n;
        Vec d = v - mean;
        mean = mean + d * (1.0f / n);
        Vec e = v - mean;
        m2 = m2 + Vec(d.x * e.x, d.y * e.y, d.z * e.z);
    }

    float maxVariance() const {
        if (n < 2) {
            return 1e9f;
        }
        return fmaxf(m2.x, fmaxf(m2.y, m2.z)) / (n - 1);
    }
};
} // namespace imu

#endif
