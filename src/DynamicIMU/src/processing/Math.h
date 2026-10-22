#ifndef DYNAMIC_IMU_PROCESSING_MATH_H
#define DYNAMIC_IMU_PROCESSING_MATH_H

#include <math.h>

namespace imu {
constexpr float pi = 3.14159265358979323846f;

// Vetor de três componentes; as unidades são definidas em cada interface.
struct Vec {
    float x, y, z;

    Vec(float a = 0, float b = 0, float c = 0) : x(a), y(b), z(c) {}

    Vec operator+(Vec b) const {
        return Vec(x + b.x, y + b.y, z + b.z);
    }

    Vec operator-(Vec b) const {
        return Vec(x - b.x, y - b.y, z - b.z);
    }

    Vec operator*(float s) const {
        return Vec(x * s, y * s, z * s);
    }

    float dot(Vec b) const {
        return x * b.x + y * b.y + z * b.z;
    }

    Vec cross(Vec b) const {
        return Vec(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);
    }

    float norm() const {
        return sqrtf(dot(*this));
    }

    bool finite() const {
        return isfinite(x) && isfinite(y) && isfinite(z);
    }
};

inline float clamp(float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
}

// Hamilton, escalar primeiro: rotação do referencial do corpo para navegação.
struct Quat {
    float w, x, y, z;

    Quat(float a = 1, float b = 0, float c = 0, float d = 0) : w(a), x(b), y(c), z(d) {}

    Quat operator*(Quat b) const {
        return Quat(w * b.w - x * b.x - y * b.y - z * b.z,
                    w * b.x + x * b.w + y * b.z - z * b.y,
                    w * b.y - x * b.z + y * b.w + z * b.x,
                    w * b.z + x * b.y - y * b.x + z * b.w);
    }

    Quat inverse() const {
        return Quat(w, -x, -y, -z);
    } // unit quaternion contract

    bool normalize() {
        float n = sqrtf(w * w + x * x + y * y + z * z);
        if (!isfinite(n) || n < 1e-9f) {
            return false;
        }
        w /= n;
        x /= n;
        y /= n;
        z /= n;
        return true;
    }

    Vec rotate(Vec v) const {
        Quat p = (*this) * Quat(0, v.x, v.y, v.z) * inverse();
        return Vec(p.x, p.y, p.z);
    }

    static Quat increment(Vec rotation) {
        float a = rotation.norm();
        float k = a < 1e-4f ? 0.5f - a * a / 48.0f : sinf(a * 0.5f) / a;
        return Quat(cosf(a * 0.5f), rotation.x * k, rotation.y * k, rotation.z * k);
    }

    static Quat fromUp(Vec u) {
        u = u * (1.0f / u.norm()); // caller verifies norm
        Vec axis(u.y, -u.x, 0);
        float s = axis.norm();
        if (s < 1e-7f) {
            return u.z >= 0 ? Quat() : Quat(0, 1, 0, 0);
        }
        return increment(axis * (atan2f(s, u.z) / s));
    }
};
} // namespace imu

#endif
