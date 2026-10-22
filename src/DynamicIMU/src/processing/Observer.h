#ifndef DYNAMIC_IMU_PROCESSING_OBSERVER_H
#define DYNAMIC_IMU_PROCESSING_OBSERVER_H

#include "Attitude.h"
#include "Statistics.h"

namespace imu {
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
