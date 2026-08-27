#include "decode.h"

namespace leaf {

bool ReadUnsigned(const uint8_t* p, size_t len, size_t off, size_t width, uint32_t& out) {
  if (p == nullptr || width == 0 || width > 4) return false;
  if (off + width > len) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < width; ++i) v = (v << 8) | p[off + i];
  out = v;
  return true;
}

bool ReadSigned(const uint8_t* p, size_t len, size_t off, size_t width, int32_t& out) {
  uint32_t raw = 0;
  if (!ReadUnsigned(p, len, off, width, raw)) return false;
  if (width < 4) {
    // Sign-extend from the field's own most significant bit.
    const uint32_t sign_bit = 1u << (width * 8 - 1);
    if (raw & sign_bit) raw |= ~((1u << (width * 8)) - 1u);
  }
  out = static_cast<int32_t>(raw);
  return true;
}

bool ReadU16(const uint8_t* p, size_t len, size_t off, uint16_t& out) {
  uint32_t v = 0;
  if (!ReadUnsigned(p, len, off, 2, v)) return false;
  out = static_cast<uint16_t>(v);
  return true;
}

bool ReadU24(const uint8_t* p, size_t len, size_t off, uint32_t& out) {
  return ReadUnsigned(p, len, off, 3, out);
}

bool ReadU32(const uint8_t* p, size_t len, size_t off, uint32_t& out) {
  return ReadUnsigned(p, len, off, 4, out);
}

bool ReadI16(const uint8_t* p, size_t len, size_t off, int16_t& out) {
  int32_t v = 0;
  if (!ReadSigned(p, len, off, 2, v)) return false;
  out = static_cast<int16_t>(v);
  return true;
}

bool ReadI32(const uint8_t* p, size_t len, size_t off, int32_t& out) {
  return ReadSigned(p, len, off, 4, out);
}

uint16_t CellVoltages::MinMv() const {
  if (count == 0) return 0;
  uint16_t m = mv[0];
  for (uint8_t i = 1; i < count; ++i)
    if (mv[i] < m) m = mv[i];
  return m;
}

uint16_t CellVoltages::MaxMv() const {
  if (count == 0) return 0;
  uint16_t m = mv[0];
  for (uint8_t i = 1; i < count; ++i)
    if (mv[i] > m) m = mv[i];
  return m;
}

uint16_t CellVoltages::SpreadMv() const {
  if (count == 0) return 0;
  return static_cast<uint16_t>(MaxMv() - MinMv());
}

uint32_t CellVoltages::SumMv() const {
  uint32_t s = 0;
  for (uint8_t i = 0; i < count; ++i) s += mv[i];
  return s;
}

bool IsPositiveResponse(const uint8_t* payload, size_t len, uint8_t group) {
  if (payload == nullptr || len < kResponseEchoLen) return false;
  return payload[0] == 0x61 && payload[1] == group;
}

bool DecodeCellVoltages(const uint8_t* payload, size_t len, CellVoltages& out) {
  out.count = 0;
  if (!IsPositiveResponse(payload, len, kGroup2Cells)) return false;

  const size_t data_len = len - kResponseEchoLen;
  size_t readings = data_len / 2;
  if (readings > kCellCount) readings = kCellCount;
  if (readings == 0) return false;

  for (size_t i = 0; i < readings; ++i) {
    uint16_t v = 0;
    if (!ReadU16(payload, len, kResponseEchoLen + i * 2, v)) break;
    out.mv[i] = v;
    out.count = static_cast<uint8_t>(i + 1);
  }
  return out.count > 0;
}

bool DecodeGroup1(const uint8_t* payload, size_t len, const Group1Layout& layout,
                  BatteryStatus& out) {
  out = BatteryStatus{};
  if (!IsPositiveResponse(payload, len, kGroup1Status)) return false;

  // An all-zero layout means the offsets have not been resolved yet. Decoding
  // against it would produce confident-looking zeros, which is worse than an
  // explicit failure.
  if (layout.soc_width == 0 && layout.ahr_width == 0 && layout.soh_width == 0 &&
      layout.pack_voltage_width == 0 && layout.pack_current_width == 0) {
    return false;
  }

  uint32_t u = 0;
  int32_t s = 0;

  if (layout.soc_width &&
      ReadUnsigned(payload, len, layout.soc_offset, layout.soc_width, u)) {
    out.soc_percent = u * layout.soc_scale;
  }
  if (layout.ahr_width &&
      ReadUnsigned(payload, len, layout.ahr_offset, layout.ahr_width, u)) {
    out.ahr = u * layout.ahr_scale;
  }
  if (layout.soh_width &&
      ReadUnsigned(payload, len, layout.soh_offset, layout.soh_width, u)) {
    out.soh_percent = u * layout.soh_scale;
  }
  if (layout.pack_voltage_width &&
      ReadUnsigned(payload, len, layout.pack_voltage_offset, layout.pack_voltage_width, u)) {
    out.pack_voltage = u * layout.pack_voltage_scale;
  }
  if (layout.pack_current_width &&
      ReadSigned(payload, len, layout.pack_current_offset, layout.pack_current_width, s)) {
    out.pack_current = s * layout.pack_current_scale;
  }

  out.valid = true;
  return true;
}

}  // namespace leaf
