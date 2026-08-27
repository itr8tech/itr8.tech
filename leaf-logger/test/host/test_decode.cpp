#include "../../src/leaf/decode.h"
#include "../../src/transport/isotp.h"

#include <cstring>
#include <vector>

#include "harness.h"

void test_decode() {
  std::printf("decode\n");

  {
    TEST("big-endian unsigned reads of each width");
    const uint8_t p[] = {0x12, 0x34, 0x56, 0x78};
    uint32_t v = 0;
    CHECK(leaf::ReadUnsigned(p, 4, 0, 1, v)); CHECK_EQ(v, 0x12u);
    CHECK(leaf::ReadUnsigned(p, 4, 0, 2, v)); CHECK_EQ(v, 0x1234u);
    CHECK(leaf::ReadUnsigned(p, 4, 0, 3, v)); CHECK_EQ(v, 0x123456u);
    CHECK(leaf::ReadUnsigned(p, 4, 0, 4, v)); CHECK_EQ(v, 0x12345678u);
  }

  {
    TEST("reads past the end of the payload fail rather than read garbage");
    const uint8_t p[] = {0x12, 0x34};
    uint32_t v = 0xDEAD;
    CHECK(!leaf::ReadUnsigned(p, 2, 1, 2, v));
    CHECK_EQ(v, 0xDEADu);  // Left untouched.
    CHECK(!leaf::ReadUnsigned(p, 2, 0, 5, v));
    CHECK(!leaf::ReadUnsigned(p, 2, 0, 0, v));
  }

  {
    TEST("signed reads sign-extend from the field width, not the word width");
    const uint8_t p[] = {0xFF, 0xFF, 0xF0, 0x60};
    int32_t v = 0;
    CHECK(leaf::ReadSigned(p, 4, 0, 1, v)); CHECK_EQ(v, -1);
    CHECK(leaf::ReadSigned(p, 4, 0, 2, v)); CHECK_EQ(v, -1);
    CHECK(leaf::ReadSigned(p, 4, 2, 2, v)); CHECK_EQ(v, -4000);

    const uint8_t pos[] = {0x0F, 0xA0};
    CHECK(leaf::ReadSigned(pos, 2, 0, 2, v)); CHECK_EQ(v, 4000);
  }

  {
    TEST("positive response echo is checked");
    const uint8_t good[] = {0x61, 0x01, 0x00};
    const uint8_t wrong_group[] = {0x61, 0x02, 0x00};
    const uint8_t negative[] = {0x7F, 0x21, 0x12};
    CHECK(leaf::IsPositiveResponse(good, 3, leaf::kGroup1Status));
    CHECK(!leaf::IsPositiveResponse(wrong_group, 3, leaf::kGroup1Status));
    CHECK(!leaf::IsPositiveResponse(negative, 3, leaf::kGroup1Status));
    CHECK(!leaf::IsPositiveResponse(good, 1, leaf::kGroup1Status));
  }

  {
    TEST("cell voltages decode as big-endian millivolts");
    std::vector<uint8_t> p = {0x61, 0x02};
    for (int i = 0; i < leaf::kCellCount; ++i) {
      uint16_t mv = 3800 + i;  // 3800..3895 mV
      p.push_back(mv >> 8);
      p.push_back(mv & 0xFF);
    }
    leaf::CellVoltages cv;
    CHECK(leaf::DecodeCellVoltages(p.data(), p.size(), cv));
    CHECK_EQ(cv.count, leaf::kCellCount);
    CHECK_EQ(cv.mv[0], 3800);
    CHECK_EQ(cv.mv[95], 3895);
    CHECK_EQ(cv.MinMv(), 3800);
    CHECK_EQ(cv.MaxMv(), 3895);
    CHECK_EQ(cv.SpreadMv(), 95);
  }

  {
    TEST("a truncated cell response decodes what arrived and reports the count");
    // Six readings only — the signature of a flow-control problem.
    std::vector<uint8_t> p = {0x61, 0x02};
    for (int i = 0; i < 6; ++i) { p.push_back(0x0E); p.push_back(0xD8); }
    leaf::CellVoltages cv;
    CHECK(leaf::DecodeCellVoltages(p.data(), p.size(), cv));
    CHECK_EQ(cv.count, 6);
    CHECK_EQ(cv.mv[0], 3800);
  }

  {
    TEST("cell decode rejects a response for another group");
    const uint8_t p[] = {0x61, 0x01, 0x0E, 0xD8};
    leaf::CellVoltages cv;
    CHECK(!leaf::DecodeCellVoltages(p, 4, cv));
    CHECK_EQ(cv.count, 0);
  }

  {
    TEST("group 1 refuses to decode against an unverified layout");
    // Guards against the worst failure mode: confident-looking zeros that
    // read as real telemetry.
    std::vector<uint8_t> p(40, 0x11);
    p[0] = 0x61; p[1] = 0x01;
    leaf::BatteryStatus st;
    CHECK(!leaf::DecodeGroup1(p.data(), p.size(), leaf::kGroup1Unverified, st));
    CHECK(!st.valid);
  }

  {
    TEST("group 1 decodes once a layout is supplied");
    std::vector<uint8_t> p(40, 0x00);
    p[0] = 0x61; p[1] = 0x01;
    p[4] = 0x03; p[5] = 0xE8;   // 1000
    p[10] = 0xFF; p[11] = 0x38; // -200
    leaf::Group1Layout layout = {};
    layout.soc_offset = 4; layout.soc_width = 2; layout.soc_scale = 0.1;
    layout.pack_current_offset = 10; layout.pack_current_width = 2;
    layout.pack_current_scale = 0.5;

    leaf::BatteryStatus st;
    CHECK(leaf::DecodeGroup1(p.data(), p.size(), layout, st));
    CHECK(st.valid);
    CHECK_NEAR(st.soc_percent, 100.0, 1e-9);
    CHECK_NEAR(st.pack_current, -100.0, 1e-9);
  }

  {
    TEST("end to end: 96 cells through ISO-TP reassembly into the decoder");
    // Build the payload the LBC would return, split it into ISO-TP frames the
    // way the bus would, feed them through the reassembler, and decode.
    std::vector<uint8_t> payload = {0x61, 0x02};
    for (int i = 0; i < leaf::kCellCount; ++i) {
      uint16_t mv = 3700 + (i % 40);
      payload.push_back(mv >> 8);
      payload.push_back(mv & 0xFF);
    }
    const size_t total = payload.size();  // 194 bytes

    isotp::Reassembler r;
    uint8_t ff[8] = {};
    ff[0] = 0x10 | ((total >> 8) & 0x0F);
    ff[1] = total & 0xFF;
    std::memcpy(ff + 2, payload.data(), 6);
    CHECK(r.Feed(ff, 8) == isotp::Result::NeedFlowControl);

    size_t sent = 6;
    uint8_t seq = 1;
    isotp::Result res = isotp::Result::InProgress;
    while (sent < total) {
      uint8_t cf[8] = {};
      cf[0] = 0x20 | (seq & 0x0F);
      const size_t n = (total - sent) < 7 ? (total - sent) : 7;
      std::memcpy(cf + 1, payload.data() + sent, n);
      res = r.Feed(cf, 8);
      sent += n;
      seq = (seq + 1) & 0x0F;
    }
    CHECK(res == isotp::Result::Complete);
    CHECK_EQ(r.size(), total);

    leaf::CellVoltages cv;
    CHECK(leaf::DecodeCellVoltages(r.payload(), r.size(), cv));
    CHECK_EQ(cv.count, leaf::kCellCount);
    CHECK_EQ(cv.mv[0], 3700);
    CHECK_EQ(cv.mv[39], 3739);
    CHECK_EQ(cv.mv[40], 3700);
    CHECK_EQ(cv.SpreadMv(), 39);
  }
}
