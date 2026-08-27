#include "lbc_client.h"

#include <Arduino.h>

#include "../leaf/pids.h"

namespace transport {

const char* ToString(RequestStatus s) {
  switch (s) {
    case RequestStatus::Ok: return "ok";
    case RequestStatus::NotRunning: return "bus not running (or listen-only)";
    case RequestStatus::SendFailed: return "send failed";
    case RequestStatus::Timeout: return "timeout";
    case RequestStatus::ProtocolError: return "iso-tp protocol error";
    case RequestStatus::NegativeResponse: return "negative response";
  }
  return "unknown";
}

RequestStatus LbcClient::RequestGroup(uint8_t group, const uint8_t*& payload,
                                      size_t& payload_len, uint32_t timeout_ms) {
  payload = nullptr;
  payload_len = 0;
  last_nrc_ = 0;

  if (!bus_.running() || bus_.mode() != Mode::Normal) return RequestStatus::NotRunning;

  rx_.Reset();
  bus_.FlushRx();

  uint8_t req[8];
  isotp::BuildGroupRequest(group, req);
  if (!bus_.Send(leaf::kLbcRequestId, req, 8, 100)) return RequestStatus::SendFailed;

  const uint32_t deadline = millis() + timeout_ms;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
    Frame f;
    if (!bus_.Receive(f, 50)) continue;
    if (f.id != leaf::kLbcResponseId) continue;

    // A negative response (7F <service> <nrc>) arrives as a single frame.
    if (f.len >= 4 && (f.data[0] >> 4) == 0x0 && f.data[1] == 0x7F) {
      last_nrc_ = f.data[3];
      return RequestStatus::NegativeResponse;
    }

    switch (rx_.Feed(f.data, f.len)) {
      case isotp::Result::Complete:
        payload = rx_.payload();
        payload_len = rx_.size();
        return RequestStatus::Ok;

      case isotp::Result::NeedFlowControl: {
        // 30 00 00: clear to send, no block limit, no separation delay. The
        // ELM327 equivalent of ATFCSH/ATFCSD/ATFCSM1 — and the step that,
        // omitted, produces a truncated six-byte response.
        uint8_t fc[8];
        isotp::BuildFlowControl(0x00, 0x00, fc);
        if (!bus_.Send(leaf::kLbcRequestId, fc, 8, 100)) return RequestStatus::SendFailed;
        break;
      }

      case isotp::Result::InProgress:
      case isotp::Result::Ignored:
        break;

      case isotp::Result::Error:
        return RequestStatus::ProtocolError;
    }
  }
  return RequestStatus::Timeout;
}

}  // namespace transport
