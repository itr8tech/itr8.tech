// Decoders for Nissan Leaf LBC responses.
//
// Pure C++ over byte buffers, deliberately free of Arduino and ESP-IDF, so the
// whole decode layer can be unit-tested on a laptop against captured payloads
// with no car and no hardware present.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pids.h"

namespace leaf {

// Bounds-checked big-endian field readers. All return false and leave `out`
// untouched if the field would run past the end of the payload.
bool ReadU16(const uint8_t* p, size_t len, size_t off, uint16_t& out);
bool ReadU24(const uint8_t* p, size_t len, size_t off, uint32_t& out);
bool ReadU32(const uint8_t* p, size_t len, size_t off, uint32_t& out);
bool ReadI16(const uint8_t* p, size_t len, size_t off, int16_t& out);
bool ReadI32(const uint8_t* p, size_t len, size_t off, int32_t& out);

// Reads an unsigned big-endian field of arbitrary width (1..4 bytes).
bool ReadUnsigned(const uint8_t* p, size_t len, size_t off, size_t width, uint32_t& out);
// Same, sign-extended from the field's own width.
bool ReadSigned(const uint8_t* p, size_t len, size_t off, size_t width, int32_t& out);

// A 2015 24 kWh pack is 48 modules of two series pairs: 96 series groups.
constexpr uint8_t kCellCount = 96;

struct CellVoltages {
  uint16_t mv[kCellCount] = {};
  uint8_t count = 0;

  uint16_t MinMv() const;
  uint16_t MaxMv() const;
  uint16_t SpreadMv() const;  // Max - min. The number that actually matters.
  uint32_t SumMv() const;
};

// Group 2 (022102). After the `61 02` echo the payload is a run of big-endian
// uint16 millivolt readings, one per series group.
//
// Accepts a short payload (fewer than 96 readings) and reports how many were
// present via `out.count` — a truncated response is worth seeing rather than
// rejecting outright, since it usually means a flow-control problem.
bool DecodeCellVoltages(const uint8_t* payload, size_t len, CellVoltages& out);

struct BatteryStatus {
  double soc_percent = 0.0;
  double ahr = 0.0;
  double soh_percent = 0.0;
  double pack_voltage = 0.0;
  double pack_current = 0.0;  // Negative is discharge.
  bool valid = false;         // False while the layout is unverified.
};

// Group 1 (022101). Returns false if `layout` has not been filled in — see the
// UNVERIFIED banner in pids.h.
bool DecodeGroup1(const uint8_t* payload, size_t len, const Group1Layout& layout,
                  BatteryStatus& out);

// Confirms a payload is the positive response to the group we asked for,
// i.e. that it begins `61 <group>`. A 0x7F byte here is a negative response.
bool IsPositiveResponse(const uint8_t* payload, size_t len, uint8_t group);

}  // namespace leaf
