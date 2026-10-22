#include "Packet.h"
#include <string.h>

namespace telemetry {
namespace {
uint16_t crc16(const uint8_t *p, uint8_t count) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < count; ++i) {
        crc ^= static_cast<uint16_t>(p[i]) << 8;
        for (uint8_t j = 0; j < 8; ++j) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

void put32(uint8_t *tx, uint8_t pos, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) {
        tx[pos + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

void put16(uint8_t *tx, uint8_t pos, uint16_t value) {
    tx[pos] = static_cast<uint8_t>(value);
    tx[pos + 1] = static_cast<uint8_t>(value >> 8);
}

} // namespace

void encodePacket(const Frame &frame, uint32_t drops, uint8_t tx[packetSize]) {
    memset(tx, 0, packetSize);
    tx[0] = 0xA5;
    tx[1] = 0x5A;
    tx[2] = 1;
    tx[3] = frame.state;
    put32(tx, 4, frame.sequence);
    put32(tx, 8, frame.timestamp);
    for (uint8_t i = 0; i < 3; ++i) {
        put16(tx, 12 + 2 * i, static_cast<uint16_t>(frame.raw.a[i]));
        put16(tx, 20 + 2 * i, static_cast<uint16_t>(frame.raw.g[i]));
    }
    put16(tx, 18, static_cast<uint16_t>(frame.raw.temp));
    const float values[] = {frame.attitude.w,
                            frame.attitude.x,
                            frame.attitude.y,
                            frame.attitude.z,
                            frame.bias.x,
                            frame.bias.y,
                            frame.bias.z};
    static_assert(sizeof(float) == 4, "Telemetry requires IEEE binary32");
    memcpy(tx + 26, values, sizeof values);
    put16(tx, 54, frame.calibrationId);
    put32(tx, 56, frame.faults);
    put32(tx, 60, frame.estimatedMisses);
    put32(tx, 64, drops);
    put32(tx, 68, frame.maxRenderUs);
    put16(tx,
          72,
          (frame.ready ? 1 : 0) | (frame.measured ? 2 : 0) | (frame.allowNominal ? 4 : 0) |
              (frame.hasTare ? 8 : 0));
    put16(tx, 74, 1);
    put16(tx, 76, frame.acquisitionUs);
    put16(tx, 78, crc16(tx, 78));
}

} // namespace telemetry
