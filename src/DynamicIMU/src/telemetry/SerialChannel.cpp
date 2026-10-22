#include "SerialChannel.h"
#include <Arduino.h>

namespace telemetry {
void SerialChannel::queue(const Frame &frame) {
    if (position_ < packetSize) {
        ++drops_;
        return;
    }
    encodePacket(frame, drops_, buffer_);
    position_ = 0;
}

void SerialChannel::flush() {
    int space = Serial.availableForWrite();
    if (position_ < packetSize && space > 0) {
        uint8_t count = static_cast<uint8_t>(space);
        if (count > packetSize - position_) {
            count = packetSize - position_;
        }
        Serial.write(buffer_ + position_, count);
        position_ += count;
    }
}

char SerialChannel::readCommand() {
    return Serial.available() ? static_cast<char>(Serial.read()) : '\0';
}
} // namespace telemetry
