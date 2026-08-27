// Thin wrapper over the ESP32's built-in TWAI (CAN 2.0B) peripheral.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace transport {

struct Frame {
  uint32_t id = 0;
  uint8_t len = 0;
  uint8_t data[8] = {};
};

enum class Mode {
  // Receives everything, never transmits and never acknowledges. Physically
  // incapable of disturbing the car's bus, which makes it the right mode for
  // the first connection and for passively sniffing broadcast frames.
  ListenOnly,
  // Full participation. Required to send diagnostic requests.
  Normal,
};

class TwaiBus {
 public:
  bool Begin(Mode mode);
  void End();
  bool Restart(Mode mode);

  bool Send(uint32_t id, const uint8_t* data, uint8_t len, uint32_t timeout_ms);
  bool Receive(Frame& out, uint32_t timeout_ms);

  // Clears queued frames so a request does not match a stale response.
  void FlushRx();

  bool running() const { return running_; }
  Mode mode() const { return mode_; }

  // Bus-off is the symptom of a wiring fault: swapped CANH/CANL, a missing
  // ground, or an extra termination resistor still fitted to the transceiver.
  bool RecoverIfBusOff();

 private:
  bool running_ = false;
  Mode mode_ = Mode::ListenOnly;
};

}  // namespace transport
