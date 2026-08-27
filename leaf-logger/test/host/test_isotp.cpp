#include "../../src/transport/isotp.h"

#include <cstring>
#include <vector>

#include "harness.h"

using isotp::Reassembler;
using isotp::Result;

void test_isotp() {
  std::printf("isotp\n");

  {
    TEST("single frame");
    Reassembler r;
    const uint8_t f[8] = {0x03, 0x61, 0x01, 0xAA, 0, 0, 0, 0};
    CHECK(r.Feed(f, 8) == Result::Complete);
    CHECK_EQ(r.size(), (size_t)3);
    CHECK_EQ(r.payload()[0], 0x61);
    CHECK_EQ(r.payload()[2], 0xAA);
  }

  {
    TEST("single frame with zero length is malformed");
    Reassembler r;
    const uint8_t f[8] = {0x00, 0, 0, 0, 0, 0, 0, 0};
    CHECK(r.Feed(f, 8) == Result::Error);
  }

  {
    TEST("single frame claiming more than the DLC carries is malformed");
    Reassembler r;
    const uint8_t f[4] = {0x07, 1, 2, 3};
    CHECK(r.Feed(f, 4) == Result::Error);
  }

  {
    TEST("first frame requests flow control");
    Reassembler r;
    const uint8_t ff[8] = {0x10, 0x20, 0x61, 0x02, 0x0C, 0x80, 0x0C, 0x81};
    CHECK(r.Feed(ff, 8) == Result::NeedFlowControl);
    CHECK_EQ(r.expected(), (size_t)0x20);
    CHECK_EQ(r.size(), (size_t)6);
  }

  {
    TEST("first frame that would fit in a single frame is malformed");
    Reassembler r;
    const uint8_t ff[8] = {0x10, 0x05, 1, 2, 3, 4, 5, 6};
    CHECK(r.Feed(ff, 8) == Result::Error);
  }

  {
    TEST("first frame larger than the buffer is rejected");
    Reassembler r;
    // 0x0FFF = 4095, well past kMaxPayload.
    const uint8_t ff[8] = {0x1F, 0xFF, 1, 2, 3, 4, 5, 6};
    CHECK(r.Feed(ff, 8) == Result::Error);
  }

  {
    TEST("multi-frame reassembly preserves byte order");
    Reassembler r;
    // 14 bytes total: 6 in the first frame, 7 + 1 in two consecutive frames.
    const uint8_t ff[8] = {0x10, 0x0E, 1, 2, 3, 4, 5, 6};
    CHECK(r.Feed(ff, 8) == Result::NeedFlowControl);
    const uint8_t cf1[8] = {0x21, 7, 8, 9, 10, 11, 12, 13};
    CHECK(r.Feed(cf1, 8) == Result::InProgress);
    const uint8_t cf2[8] = {0x22, 14, 0, 0, 0, 0, 0, 0};
    CHECK(r.Feed(cf2, 8) == Result::Complete);

    CHECK_EQ(r.size(), (size_t)14);
    for (size_t i = 0; i < 14; ++i) CHECK_EQ(r.payload()[i], (uint8_t)(i + 1));
  }

  {
    TEST("a sequence gap is an error, not silent corruption");
    Reassembler r;
    const uint8_t ff[8] = {0x10, 0x0E, 1, 2, 3, 4, 5, 6};
    CHECK(r.Feed(ff, 8) == Result::NeedFlowControl);
    const uint8_t cf2[8] = {0x22, 7, 8, 9, 10, 11, 12, 13};  // Expected 0x21.
    CHECK(r.Feed(cf2, 8) == Result::Error);
  }

  {
    TEST("sequence numbers wrap past 15");
    Reassembler r;
    // 6 + 16*7 = 118 bytes, so the run needs sequence numbers 1..15 and then
    // a wrap to 0 for the sixteenth consecutive frame.
    const uint8_t ff[8] = {0x10, 0x76, 0, 0, 0, 0, 0, 0};
    CHECK(r.Feed(ff, 8) == Result::NeedFlowControl);
    for (int i = 1; i <= 15; ++i) {
      uint8_t cf[8] = {(uint8_t)(0x20 | (i & 0x0F)), 0, 0, 0, 0, 0, 0, 0};
      CHECK(r.Feed(cf, 8) == Result::InProgress);
    }
    uint8_t cf16[8] = {0x20, 0, 0, 0, 0, 0, 0, 0};  // Wrapped to 0.
    CHECK(r.Feed(cf16, 8) == Result::Complete);
    CHECK_EQ(r.size(), (size_t)118);
  }

  {
    TEST("consecutive frame outside a message is ignored");
    Reassembler r;
    const uint8_t cf[8] = {0x21, 1, 2, 3, 4, 5, 6, 7};
    CHECK(r.Feed(cf, 8) == Result::Ignored);
  }

  {
    TEST("flow control from the bus is ignored");
    Reassembler r;
    const uint8_t fc[8] = {0x30, 0, 0, 0, 0, 0, 0, 0};
    CHECK(r.Feed(fc, 8) == Result::Ignored);
  }

  {
    TEST("request and flow-control builders");
    uint8_t req[8];
    isotp::BuildGroupRequest(0x01, req);
    CHECK_EQ(req[0], 0x02);
    CHECK_EQ(req[1], 0x21);
    CHECK_EQ(req[2], 0x01);
    CHECK_EQ(req[7], 0x00);

    uint8_t fc[8];
    isotp::BuildFlowControl(0x00, 0x00, fc);
    CHECK_EQ(fc[0], 0x30);
    CHECK_EQ(fc[1], 0x00);
    CHECK_EQ(fc[2], 0x00);
  }
}
