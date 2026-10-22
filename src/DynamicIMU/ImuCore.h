#ifndef DYNAMIC_IMU_CORE_H
#define DYNAMIC_IMU_CORE_H
#include <stdint.h>
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

inline int16_t signedWord(uint8_t hi, uint8_t lo) {
    const uint16_t u = static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    const int32_t s = u >= 32768u ? static_cast<int32_t>(u) - 65536L : static_cast<int32_t>(u);
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

// Acelerômetro em g; girômetro em rad/s; montagem transforma sensor em corpo.
struct Calibration {
    uint16_t version = 1, id = 0;
    Vec offset, countsPerG = Vec(16384, 16384, 16384);
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
        return sensorToBody.rotate(Vec(r.g[0], r.g[1], r.g[2]) * (pi / (180.0f * 131.0f)));
    }
};

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

// O repouso inicial estima o viés; aceleração linear pode invalidar a referência vertical.
class Observer {
  public:
    Quat q;
    Vec bias;
    State state = Uninitialized;
    float kp = 2.0f;
    bool ready = false, lab = false, hasTare = false;
    uint32_t last = 0;
    float tareX = 0, tareY = 0;
    Moments am, gm;
    uint32_t calibrationStart = 0;

    void beginCalibration() {
        ready = false;
        hasTare = false;
        am = Moments();
        gm = Moments();
        state = Calibrating;
        last = 0;
    }

    void fault(State s) {
        ready = false;
        hasTare = false;
        state = s;
    }

    Vec up() const {
        return q.inverse().rotate(Vec(0, 0, 1));
    }

    bool stale(uint32_t now) const {
        return !ready || static_cast<uint32_t>(now - last) > 30000u;
    }

    bool tare() {
        if (!ready) {
            return false;
        }
        Angles a = inclination(up());
        if (!a.yDefined) {
            return false;
        }
        tareX = a.x;
        tareY = a.y;
        hasTare = true;
        return true;
    }

    Angles angles() const {
        Angles a = inclination(up());
        if (hasTare) {
            a.x = wrapDegrees(a.x - tareX);
            if (a.yDefined) {
                a.y = wrapDegrees(a.y - tareY);
            }
        }
        return a;
    }

    void sample(Vec a, Vec g, uint32_t t, bool calibrated, bool allowLab = false) {
        if (!a.finite() || !g.finite()) {
            fault(SensorFault);
            return;
        }
        lab = !calibrated;
        if (lab && !allowLab) {
            fault(InvalidCalibration);
            return;
        }
        float dt = static_cast<float>(static_cast<uint32_t>(t - last)) * 1e-6f;
        if (state == Calibrating) {
            if (am.n && (dt < 0.005f || dt > 0.03f)) {
                am = Moments();
                gm = Moments();
            }
            last = t;
            if (fabsf(a.norm() - 1) > 0.08f || g.norm() > 0.08f) {
                am = Moments();
                gm = Moments();
                return;
            }
            if (am.n == 0) {
                calibrationStart = t;
            }
            am.add(a);
            gm.add(g);
            if (am.maxVariance() > 0.0004f || gm.maxVariance() > 0.000025f) {
                if (am.n >= 20) {
                    am = Moments();
                    gm = Moments();
                }
                return;
            }
            if (am.n >= 150 && static_cast<uint32_t>(t - calibrationStart) >= 3000000u) {
                q = Quat::fromUp(am.mean);
                bias = gm.mean;
                ready = true;
                state = lab ? LabUncalibrated : Tracking;
            }
            return;
        }
        if (!ready) {
            return; // explicit C required after a fault
        }
        if (dt < 0.005f || dt > 0.03f) {
            fault(TimingFault);
            return;
        }
        last = t;
        // Innovation evaluated at the propagated attitude (accelerometer at current timestamp).
        q = q * Quat::increment((g - bias) * dt);
        if (!q.normalize()) {
            fault(SensorFault);
            return;
        }
        float n = a.norm();
        bool accepted = false;
        if (n > 0.85f && n < 1.15f) {
            Vec u = a * (1 / n), v = up();
            // A direction gate, not an acceleration classifier. Recovery requires C for large
            // errors.
            if (u.dot(v) > 0.8660254f) {
                q = q * Quat::increment(u.cross(v) * (kp * dt));
                q.normalize();
                accepted = true;
            }
        }
        state = accepted ? (lab ? LabUncalibrated : Tracking) : GyroOnly;
    }
};
} // namespace imu
#endif
