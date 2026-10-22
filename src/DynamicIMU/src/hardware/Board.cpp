#include "Board.h"
#include <Arduino.h>
#include <Wire.h>

namespace hardware {
void beginCommunication() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000);
    Wire.setWireTimeout(3000, true); // Arduino AVR core 1.8.6; timeout em microssegundos.
}
} // namespace hardware
