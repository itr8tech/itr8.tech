// Request/response client for the Leaf's battery controller.
//
// Combines the TWAI driver with ISO-TP reassembly: ask for a group, get back
// the fully reassembled payload with the `61 <group>` echo intact.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "isotp.h"
#include "twai_bus.h"

namespace transport {

enum class RequestStatus {
  Ok,
  NotRunning,       // Bus not started, or started in listen-only mode.
  SendFailed,
  Timeout,          // No response, or the response stopped part way.
  ProtocolError,    // Malformed ISO-TP framing or a sequence gap.
  NegativeResponse, // The LBC answered 0x7F.
};

const char* ToString(RequestStatus s);

class LbcClient {
 public:
  explicit LbcClient(TwaiBus& bus) : bus_(bus) {}

  // Sends `02 21 <group>` to 0x79B and reassembles the reply from 0x7BB.
  // On Ok, `payload`/`payload_len` point at internal storage valid until the
  // next call.
  RequestStatus RequestGroup(uint8_t group, const uint8_t*& payload,
                             size_t& payload_len, uint32_t timeout_ms);

  // The negative response code, valid after NegativeResponse.
  uint8_t last_nrc() const { return last_nrc_; }

 private:
  TwaiBus& bus_;
  isotp::Reassembler rx_;
  uint8_t last_nrc_ = 0;
};

}  // namespace transport
