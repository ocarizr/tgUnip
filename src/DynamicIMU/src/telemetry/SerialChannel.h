#ifndef DYNAMIC_IMU_TELEMETRY_SERIAL_CHANNEL_H
#define DYNAMIC_IMU_TELEMETRY_SERIAL_CHANNEL_H

#include "Packet.h"

namespace telemetry {
// Buffer de um pacote, envio parcial e descarte contado, sem bloquear a aquisição.
class SerialChannel {
  public:
    void queue(const Frame &frame);
    void flush();
    char readCommand();

  private:
    uint8_t buffer_[packetSize] = {};
    uint8_t position_ = packetSize;
    uint32_t drops_ = 0;
};
} // namespace telemetry

#endif
