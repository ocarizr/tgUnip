#include "Application.h"
#include "../hardware/Board.h"
#include <Arduino.h>

namespace application {
Application::Application(const imu::Calibration &calibration, bool allowLab, bool enableScreen)
    : instrument_(calibration, allowLab), screen_(enableScreen), allowLab_(allowLab) {}

void Application::begin() {
    hardware::beginCommunication();
    screen_.begin();
    delay(100);
    instrument_.initialize(sensor_.configure());
}

void Application::handleCommand(char command) {
    switch (command) {
    case 'c':
        instrument_.requestCalibration(sensor_.configure());
        break;
    case 't':
        instrument_.tare(micros());
        break;
    case 'u':
        instrument_.clearTare();
        break;
    case 'r':
        instrument_.reset();
        break;
    default:
        break;
    }
}

void Application::publish(const sensors::Sample &sample) {
    const imu::Observer &observer = instrument_.observer();
    const imu::Calibration &calibration = instrument_.calibration();
    telemetry::Frame frame;
    frame.raw = sample.raw;
    frame.timestamp = sample.timestamp;
    frame.acquisitionUs = sample.acquisitionUs;
    frame.attitude = observer.q;
    frame.bias = observer.bias;
    frame.state = observer.state;
    frame.sequence = sensor_.sequence();
    frame.faults = instrument_.faults();
    frame.estimatedMisses = sensor_.estimatedMisses();
    frame.maxRenderUs = screen_.maxRenderUs();
    frame.calibrationId = calibration.id;
    frame.ready = observer.ready;
    frame.measured = calibration.measured;
    frame.allowLab = allowLab_;
    frame.hasTare = observer.hasTare;
    serial_.queue(frame); // Os dados brutos continuam disponíveis antes da calibração.
}

ui::ViewModel Application::viewModel(uint32_t now) const {
    const imu::Observer &observer = instrument_.observer();
    return ui::ViewModel{
        observer.state, observer.angles(), observer.lab, !observer.stale(now), observer.hasTare};
}

void Application::update() {
    const uint32_t now = micros();
    sensors::Sample sample;
    const sensors::PollResult result = sensor_.poll(now, sample);
    if (result == sensors::PollResult::Fault) {
        instrument_.sensorFault();
    } else if (result == sensors::PollResult::SampleReady) {
        instrument_.accept(sample.raw, sample.timestamp);
        publish(sample);
    }

    if (!sensor_.checkConfiguration(now)) {
        instrument_.sensorFault();
    }
    instrument_.checkFreshness(micros());

    serial_.flush();
    handleCommand(serial_.readCommand());

    const uint32_t displayTime = micros();
    // Calcular os ângulos só quando houver uma linha a desenhar, como no ciclo original.
    if (!screen_.due(displayTime)) {
        return;
    }
    const ui::Action action = screen_.service(viewModel(displayTime), displayTime);
    if (action == ui::Action::Tare) {
        handleCommand('t');
    } else if (action == ui::Action::Calibrate) {
        handleCommand('c');
    }
}
} // namespace application
