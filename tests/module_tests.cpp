#include "../src/DynamicIMU/src/processing/Instrument.h"
#include "../src/DynamicIMU/src/telemetry/Packet.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
int checks = 0;

void expect(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

// Fixture compartilhada com o teste do decodificador Python: bytes do firmware real.
int emitPacket() {
    telemetry::Frame frame{};
    frame.raw = imu::Raw{{16384, -32768, 0}, 340, {131, -131, 0}};
    frame.attitude = imu::Quat(1, 0, 0, 0);
    frame.bias = imu::Vec(0.1f, -0.2f, 0.3f);
    frame.state = imu::GyroOnly;
    frame.sequence = 123;
    frame.timestamp = 0xFFFFFFF0u;
    frame.faults = 1;
    frame.estimatedMisses = 2;
    frame.maxRenderUs = 4;
    frame.calibrationId = 42;
    frame.acquisitionUs = 500;
    frame.ready = frame.measured = frame.allowNominal = frame.hasTare = true;
    uint8_t packet[telemetry::packetSize];
    telemetry::encodePacket(frame, 3, packet);
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    return std::fwrite(packet, 1, sizeof packet, stdout) == sizeof packet ? 0 : 1;
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::strcmp(argv[1], "--packet") == 0) {
        return emitPacket();
    }

    imu::Calibration calibration;
    calibration.measured = true;
    processing::Instrument instrument(calibration, false);
    const imu::Raw stationary{{16384, 0, 0}, 0, {0, 0, 0}};
    instrument.initialize(true);
    expect(instrument.observer().state == imu::Calibrating, "startup requests calibration");
    for (uint32_t time = 20000; time <= 3040000; time += 20000) {
        instrument.accept(stationary, time);
    }
    expect(instrument.observer().ready, "stationary raw samples initialize observer");
    expect(instrument.observer().state == imu::Tracking, "calibrated samples enter fusion");
    expect(fabsf(instrument.observer().angles().tilt) < 0.01f, "+X reference preserved");
    instrument.tare(3040000);
    expect(instrument.observer().hasTare, "current attitude permits tare");
    instrument.clearTare();
    expect(!instrument.observer().hasTare, "clear tare");
    instrument.tare(3080000);
    expect(!instrument.observer().hasTare, "stale attitude rejects tare");
    instrument.checkFreshness(3080000);
    expect(instrument.observer().state == imu::TimingFault, "freshness timeout invalidates output");
    expect(instrument.faults() == 1, "freshness fault counted once");
    instrument.checkFreshness(3100000);
    expect(instrument.faults() == 1, "inactive output does not repeat timing fault");
    instrument.accept(stationary, 3120000);
    expect(!instrument.observer().ready, "new data does not silently recover a fault");
    instrument.requestCalibration(false);
    expect(instrument.observer().state == imu::SensorFault, "failed sensor recovery reported");
    expect(instrument.faults() == 2, "sensor failure counted");
    instrument.requestCalibration(true);
    expect(instrument.observer().state == imu::Calibrating, "explicit recovery starts calibration");
    imu::Raw saturated = stationary;
    saturated.a[1] = 32767;
    instrument.accept(saturated, 3140000);
    expect(instrument.observer().state == imu::Saturation, "raw saturation checked before fusion");
    expect(instrument.faults() == 3, "saturation counted");
    instrument.reset();
    expect(instrument.observer().state == imu::Uninitialized,
           "reset requires explicit calibration");
    expect(instrument.faults() == 3, "user reset is not a sensor failure");

    calibration.measured = false;
    processing::Instrument nominal(calibration, false);
    nominal.initialize(true);
    nominal.accept(stationary, 20000);
    expect(nominal.observer().state == imu::InvalidCalibration,
           "nominal calibration stays invalid");
    nominal.accept(stationary, 40000);
    expect(nominal.faults() == 1, "repeated rejected nominal samples count one failure entry");
    calibration.countsPerG.x = 0;
    processing::Instrument invalid(calibration, true);
    invalid.initialize(true);
    expect(invalid.observer().state == imu::InvalidCalibration,
           "invalid scale rejected at startup");
    expect(invalid.faults() == 1, "invalid configuration counted");

    // Esta lacuna é detectada dentro de Observer::sample, antes do watchdog da aplicação.
    calibration = imu::Calibration();
    calibration.measured = true;
    processing::Instrument internal(calibration, false);
    internal.initialize(true);
    for (uint32_t time = 20000; time <= 3040000; time += 20000) {
        internal.accept(stationary, time);
    }
    internal.accept(stationary, 3100000);
    expect(internal.observer().state == imu::TimingFault, "sample itself detects timing fault");
    expect(internal.faults() == 1, "internal observer failure reaches telemetry counter");
    internal.checkFreshness(3120000);
    expect(internal.faults() == 1, "watchdog does not double-count internal failure");
    internal.sensorFault();
    expect(internal.faults() == 2, "different failure state counts another occurrence");
    internal.sensorFault();
    expect(internal.faults() == 2, "same consecutive failure counted once");
    internal.reset();
    expect(internal.faults() == 2, "reset preserves lifetime diagnostic count");
    internal.requestCalibration(true);
    internal.sensorFault();
    expect(internal.faults() == 3, "failure after explicit retry counts again");
    processing::Instrument nominalProfile(imu::Calibration(), true);
    nominalProfile.initialize(true);
    for (uint32_t time = 20000; time <= 3040000; time += 20000) {
        nominalProfile.accept(stationary, time);
    }
    expect(nominalProfile.observer().ready &&
               nominalProfile.observer().state == imu::NominalCalibration,
           "nominal profile operates without claiming measured accelerometer coefficients");
    expect(!nominalProfile.calibration().measured && nominalProfile.calibration().id == 0,
           "nominal provenance remains explicit");
    std::printf("PASS %d module checks\n", checks);
}
