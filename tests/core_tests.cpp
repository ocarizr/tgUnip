#include "../src/DynamicIMU/ImuCore.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
using namespace imu;
static int checks = 0;

static void expect(bool ok, const char *name) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", name);
        std::exit(1);
    }
}

static bool near(float a, float b, float eps = 1e-4f) {
    return fabsf(a - b) < eps;
}

static void vectorNear(Vec a, Vec b, float eps, const char *name) {
    expect((a - b).norm() < eps, name);
}

static float error(Quat a, Quat b) {
    Quat d = a.inverse() * b;
    return 2 * atan2f(sqrtf(d.x * d.x + d.y * d.y + d.z * d.z), fabsf(d.w)) * 180 / pi;
}

static void
initialize(Observer &o, Vec a = Vec(0, 0, 1), Vec g = Vec(), uint32_t start = 1000000u) {
    o.kp = 2;
    o.beginCalibration();
    for (uint32_t i = 0; i <= 150; ++i) {
        o.sample(a, g, start + i * 20000u, true);
    }
    expect(o.ready, "stationary calibration 3s");
}

int main() {
    expect(signedWord(0x7F, 0xFF) == 32767, "decode positive limit");
    expect(signedWord(0x80, 0) == -32768, "decode negative limit");
    expect(signedWord(0xFF, 0xFF) == -1, "decode minus one");
    uint8_t bytes[14] = {0x40, 0, 0x80, 0, 0, 0, 0x01, 0x54, 0, 0x83, 0xFF, 0x7D, 0, 0};
    Raw r = decode(bytes);
    Calibration c;
    expect(r.a[0] == 16384 && r.a[1] == -32768 && r.temp == 340 && r.g[0] == 131 && r.g[1] == -131,
           "packet order");
    vectorNear(c.accel(r), Vec(1, -2, 0), 1e-6f, "nominal counts to g");
    vectorNear(c.gyro(r), Vec(pi / 180, -pi / 180, 0), 1e-6f, "counts to rad/s");
    expect(saturated(r), "saturation");
    c.offset = Vec(10, 20, 30);
    r.a[0] = 16394;
    r.a[1] = 20;
    r.a[2] = 30;
    vectorNear(c.accel(r), Vec(1, 0, 0), 1e-6f, "offset before scaling");
    c.sensorToBody = Quat::increment(Vec(0, 0, pi / 2));
    vectorNear(c.accel(r), Vec(0, 1, 0), 1e-6f, "proper mounting rotation");
    c.countsPerG.x = 0;
    expect(!c.valid(), "invalid gain");
    c = Calibration();
    c.sensorToBody = Quat(2, 0, 0, 0);
    expect(!c.valid(), "nonunit mount rejected");
    Quat z90 = Quat::increment(Vec(0, 0, pi / 2));
    vectorNear(z90.rotate(Vec(1, 0, 0)), Vec(0, 1, 0), 1e-6f, "right handed z rotation");
    vectorNear((z90 * z90.inverse()).rotate(Vec(1, 2, 3)), Vec(1, 2, 3), 1e-5f, "inverse identity");
    vectorNear(Quat(-z90.w, -z90.x, -z90.y, -z90.z).rotate(Vec(1, 0, 0)),
               Vec(0, 1, 0),
               1e-6f,
               "double cover");
    Quat zero(0, 0, 0, 0);
    expect(!zero.normalize(), "zero quaternion rejected");
    Quat tiny = Quat::increment(Vec(1e-8f, 0, 0));
    expect(near(tiny.x, 5e-9f, 1e-12f), "small angle");
    Vec orientations[] = {Vec(1, 0, 0),
                          Vec(-1, 0, 0),
                          Vec(0, 1, 0),
                          Vec(0, -1, 0),
                          Vec(0, 0, 1),
                          Vec(0, 0, -1),
                          Vec(.6f, 0, .8f)};
    for (Vec u : orientations) {
        Observer o;
        initialize(o, u);
        vectorNear(o.up(), u, 1e-5f, "six faces and tilted starts");
    }
    Vec nearInverted(.0005f, 0, -sqrtf(1 - .0005f * .0005f));
    vectorNear(Quat::fromUp(nearInverted).inverse().rotate(Vec(0, 0, 1)),
               nearInverted,
               1e-6f,
               "near antipodal initialization");
    expect(near(inclination(Vec(1, 0, 0)).tilt, 0), "historic +X up");
    expect(near(inclination(Vec(-1, 0, 0)).y, 180), "atan2 quadrant");
    expect(!inclination(Vec(0, 1, 0)).yDefined, "angle branch singularity");
    expect(isnan(inclination(Vec()).x), "free fall static undefined");
    Observer o;
    initialize(o);
    expect(o.tare(), "tare initialized");
    expect(near(o.angles().x, 0) && near(o.angles().y, 0), "relative coordinate differences");
    expect(near(wrapDegrees(359), -1), "wrapped difference");
    o.beginCalibration();
    expect(!o.tare() && !o.hasTare, "reset clears reference");
    initialize(o, Vec(0, 0, 1), Vec(.01f, -.02f, .03f));
    vectorNear(o.bias, Vec(.01f, -.02f, .03f), 1e-6f, "stationary gyro bias");
    o.sample(Vec(0, 0, 1), Vec(), o.last + 20000u, false);
    expect(o.state == InvalidCalibration && !o.ready, "no valid-looking uncalibrated output");
    o.beginCalibration();
    for (uint32_t i = 0; i < 200; ++i) {
        o.sample(Vec(0, 0, 1), Vec(), 1000000u + i * 20000u, false, true);
    }
    expect(o.ready && o.state == LabUncalibrated, "explicit lab override");
    o.sample(Vec(), Vec(), o.last + 20000u, false, true);
    expect(o.state == GyroOnly, "zero accel propagation");
    o.sample(Vec(0, 0, 1), Vec(), o.last + 100000u, false, true);
    expect(o.state == TimingFault && !o.ready, "long gap invalidates, no stale integration");
    initialize(o);
    expect(o.stale(o.last + 30001u), "stale timeout");
    o.sample(Vec(NAN, 0, 0), Vec(), o.last + 20000u, true);
    expect(o.state == SensorFault, "NaN fault");
    initialize(o, Vec(0, 0, 1), Vec(), 0xFFF00000u);
    for (int i = 0; i < 100; ++i) {
        o.sample(Vec(0, 0, 1), Vec(), o.last + 20000u, true);
    }
    expect(o.ready, "unsigned timer rollover");
    Observer moving;
    moving.beginCalibration();
    for (uint32_t i = 0; i < 250; ++i) {
        moving.sample(Vec(0, 0, 1), Vec(.1f, 0, 0), i * 20000u, true);
    }
    expect(!moving.ready, "moving initialization rejected");
    moving.beginCalibration();
    for (uint32_t i = 0; i < 250; ++i) {
        moving.sample(Vec(0, 0, 1), Vec(i % 2 ? .06f : -.06f, 0, 0), i * 20000u, true);
    }
    expect(!moving.ready, "gyro variance rejects vibration");
    initialize(o);
    o.q = Quat::increment(Vec(.1f, 0, 0));
    for (int i = 0; i < 300; ++i) {
        o.sample(Vec(0, 0, 1), Vec(), o.last + 20000u, true);
    }
    expect(error(o.q, Quat()) < 0.01f, "positive feedback sign converges");
    // Analytic body-rate constant rotations, with accelerometer from independent Rodrigues formula.
    const Vec axis = Vec(1, 2, 3) * (1 / sqrtf(14));
    float maxError = 0;
    for (int hz : {50, 100}) {
        initialize(o);
        float dt = 1.0f / static_cast<float>(hz);
        o.kp = 2;
        for (int i = 1; i <= hz * 20; ++i) {
            float angle = static_cast<float>(i) * dt;
            Vec u = Vec(0, 0, 1) * cosf(angle) - axis.cross(Vec(0, 0, 1)) * sinf(angle) +
                    axis * (axis.z * (1 - cosf(angle)));
            o.sample(u, axis, o.last + static_cast<uint32_t>(1000000 / hz), true);
            Quat truth(cosf(angle / 2),
                       axis.x * sinf(angle / 2),
                       axis.y * sinf(angle / 2),
                       axis.z * sinf(angle / 2));
            float e = error(o.q, truth);
            if (e > maxError) {
                maxError = e;
            }
        }
        expect(maxError < 0.02f, "analytic constant body-rate 20s full revolutions");
    }
    // Noncommuting body X then Y: expected matrix Rx(90)*Ry(90), maps eZ to eX.
    initialize(o);
    o.kp = 0;
    for (int axisIndex = 0; axisIndex < 2; ++axisIndex) {
        for (int i = 0; i < 100; ++i) {
            o.sample(Vec(),
                     axisIndex == 0 ? Vec(pi / 2, 0, 0) : Vec(0, pi / 2, 0),
                     o.last + 10000u,
                     true);
        }
    }
    vectorNear(
        o.q.rotate(Vec(0, 0, 1)), Vec(1, 0, 0), 1e-5f, "body quaternion multiplication order");
    initialize(o);
    for (int i = 0; i < 500; ++i) {
        o.sample(Vec(0, 0, 1), Vec(0, 0, .01f), o.last + 20000u, true);
    }
    expect(error(o.q, Quat()) > 5.7f && o.up().z > .999f, "yaw drifts, vertical does not");
    initialize(o);
    o.sample(Vec(.6f, 0, .8f), Vec(), o.last + 20000u, true);
    expect(o.state == GyroOnly, "near 1g large innovation rejected");
    initialize(o);
    o.sample(Vec(.1f, 0, sqrtf(.99f)), Vec(), o.last + 20000u, true);
    expect(o.state == Tracking && error(o.q, Quat()) > .1f,
           "near 1g small innovation FALSE ACCEPTANCE documented");
    initialize(o);
    Vec saved = o.bias;
    for (int i = 0; i < 100; ++i) {
        o.sample(Vec(0, 0, 2), Vec(.01f, 0, 0), o.last + 20000u, true);
    }
    vectorNear(saved, o.bias, 1e-8f, "bias never learns acceleration disturbance");
    // Original Z-clipping mismatch: all observations -2540, shifted center 0, lower clip -76.62.
    float rawZ = -2540, shifted = rawZ + 2540;
    float historical = clamp(rawZ, shifted - 2 * 38.31f, shifted + 2 * 38.31f);
    expect(near(historical, -76.62f, .001f), "historic Z corruption regression");
    Moments m;
    for (int i = 0; i < 400; ++i) {
        m.add(Vec(1000, 0, -2540));
    }
    expect(near(m.mean.x, 1000) && near(m.mean.z, -2540), "correct static streaming comparator");
    float contaminated = (1000 + 400 * 1000) / 400.0f;
    expect(near(contaminated, 1002.5f), "historic accumulator retained mean regression");
    std::printf("PASS %d checks; analytic_max_attitude_error_deg=%.8f\n",
                checks,
                static_cast<double>(maxError));
}
