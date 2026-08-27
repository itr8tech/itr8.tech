// ISO 15765-2 (ISO-TP) receive-side reassembly.
//
// Pure C++ with no Arduino or ESP-IDF dependency so it can be unit-tested on
// the host. The TWAI driver feeds raw 8-byte CAN frames in; this produces the
// reassembled multi-frame payload.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace isotp {

enum class Result : uint8_t {
  Ignored,          // Not part of a message we are assembling (e.g. flow control).
  NeedFlowControl,  // First frame consumed; caller must send a flow-control frame.
  InProgress,       // Consecutive frame consumed, more expected.
  Complete,         // Full message assembled; payload() / size() are valid.
  Error,            // Protocol violation: bad sequence number, overflow, malformed PCI.
};

// A Leaf group-1 response is ~40 bytes; group 2 (96 cell voltages) is ~200.
// 512 gives headroom without putting real pressure on ESP32 RAM.
constexpr size_t kMaxPayload = 512;

class Reassembler {
 public:
  void Reset();

  // `len` is the CAN frame's DLC (1..8).
  Result Feed(const uint8_t* frame, uint8_t len);

  const uint8_t* payload() const { return buf_; }
  size_t size() const { return len_; }          // Valid once Complete.
  size_t expected() const { return expected_; }

 private:
  uint8_t buf_[kMaxPayload] = {};
  size_t len_ = 0;
  size_t expected_ = 0;
  uint8_t next_seq_ = 0;
  bool active_ = false;
};

// Builds the single-frame request `02 21 <group> 00 00 00 00 00`.
// Nissan service 0x21 ("read data by group"); the LBC answers with 0x61.
void BuildGroupRequest(uint8_t group, uint8_t out[8]);

// Builds the flow-control frame `30 <block_size> <st_min> 00 ...`.
// 30 00 00 = clear to send, no block limit, no separation delay.
void BuildFlowControl(uint8_t block_size, uint8_t st_min, uint8_t out[8]);

}  // namespace isotp
