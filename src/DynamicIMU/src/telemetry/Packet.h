#ifndef DYNAMIC_IMU_TELEMETRY_PACKET_H
#define DYNAMIC_IMU_TELEMETRY_PACKET_H

#include "../sensors/RawSample.h"
#include "../processing/Attitude.h"

namespace telemetry {
constexpr uint8_t packetSize = 80;

// Fotografia dos dados de uma aquisição; não contém referências a hardware.
struct Frame {
    imu::Raw raw;
    imu::Quat attitude;
    imu::Vec bias;
    imu::State state;
    uint32_t sequence, timestamp, faults, estimatedMisses, maxRenderUs;
    uint16_t calibrationId, acquisitionUs;
    bool ready, measured, allowLab, hasTare;
};

// Binário v1 little-endian, com CRC-16/CCITT-FALSE. Não acessa a UART.
void encodePacket(const Frame &frame, uint32_t drops, uint8_t packet[packetSize]);
} // namespace telemetry

#endif
