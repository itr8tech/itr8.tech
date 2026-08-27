#include "isotp.h"

#include <string.h>

namespace isotp {
namespace {

constexpr uint8_t kPciSingle = 0x0;
constexpr uint8_t kPciFirst = 0x1;
constexpr uint8_t kPciConsecutive = 0x2;
constexpr uint8_t kPciFlowControl = 0x3;

}  // namespace

void Reassembler::Reset() {
  len_ = 0;
  expected_ = 0;
  next_seq_ = 0;
  active_ = false;
}

Result Reassembler::Feed(const uint8_t* frame, uint8_t len) {
  if (frame == nullptr || len == 0 || len > 8) return Result::Error;

  const uint8_t pci = frame[0] >> 4;

  switch (pci) {
    case kPciSingle: {
      const uint8_t n = frame[0] & 0x0F;
      // A zero-length single frame is malformed, as is one claiming more bytes
      // than the frame actually carries.
      if (n == 0 || n > len - 1) return Result::Error;
      memcpy(buf_, frame + 1, n);
      len_ = n;
      expected_ = n;
      active_ = false;
      return Result::Complete;
    }

    case kPciFirst: {
      if (len < 2) return Result::Error;
      const size_t total = (static_cast<size_t>(frame[0] & 0x0F) << 8) | frame[1];
      // Anything that fits in a single frame should have been sent as one.
      if (total <= 7) return Result::Error;
      if (total > kMaxPayload) return Result::Error;

      const size_t n = (len - 2) < 6 ? (len - 2) : 6;
      memcpy(buf_, frame + 2, n);
      len_ = n;
      expected_ = total;
      next_seq_ = 1;
      active_ = true;
      return Result::NeedFlowControl;
    }

    case kPciConsecutive: {
      if (!active_) return Result::Ignored;

      const uint8_t seq = frame[0] & 0x0F;
      if (seq != next_seq_) {
        // A gap means we have silently lost data; the payload is unusable.
        Reset();
        return Result::Error;
      }
      next_seq_ = (next_seq_ + 1) & 0x0F;

      const size_t remaining = expected_ - len_;
      const size_t avail = len - 1;
      const size_t n = avail < remaining ? avail : remaining;
      memcpy(buf_ + len_, frame + 1, n);
      len_ += n;

      if (len_ >= expected_) {
        active_ = false;
        return Result::Complete;
      }
      return Result::InProgress;
    }

    case kPciFlowControl:
      // We are the tester; we send flow control, we do not consume it.
      return Result::Ignored;

    default:
      return Result::Error;
  }
}

void BuildGroupRequest(uint8_t group, uint8_t out[8]) {
  memset(out, 0, 8);
  out[0] = 0x02;  // Single frame, 2 payload bytes.
  out[1] = 0x21;  // Nissan "read data by group".
  out[2] = group;
}

void BuildFlowControl(uint8_t block_size, uint8_t st_min, uint8_t out[8]) {
  memset(out, 0, 8);
  out[0] = 0x30;  // Flow control, clear to send.
  out[1] = block_size;
  out[2] = st_min;
}

}  // namespace isotp
