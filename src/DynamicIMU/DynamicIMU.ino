// Revisão de 2026. Hardware e pinagem preservados da implementação de 2018.
#include "Config.h"
#include "src/application/Application.h"

namespace {
application::Application instrument(instrumentCalibration(), ALLOW_NOMINAL_CALIBRATION, ENABLE_TFT);
}

void setup() {
    instrument.begin();
}

void loop() {
    instrument.update();
}
