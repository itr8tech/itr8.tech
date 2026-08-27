// Nissan Leaf LBC (Li-ion Battery Controller) diagnostic addressing and the
// field offsets for a 2015 24 kWh pack.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace leaf {

// The LBC answers diagnostic requests at 0x79B and replies on 0x7BB.
constexpr uint32_t kLbcRequestId = 0x79B;
constexpr uint32_t kLbcResponseId = 0x7BB;

// Broadcast frames worth sniffing passively (no request needed).
constexpr uint32_t kBroadcastGids = 0x5B3;
constexpr uint32_t kBroadcastSoh = 0x5BC;

// Request groups for service 0x21.
constexpr uint8_t kGroup1Status = 0x01;   // SOC, SOH, AHr, pack voltage/current.
constexpr uint8_t kGroup2Cells = 0x02;    // 96 individual cell voltages.
constexpr uint8_t kGroup4Temps = 0x04;    // Pack temperature sensors.
constexpr uint8_t kGroup6Shunts = 0x06;   // Cell shunt / balancing state.

// Every ISO-TP payload from the LBC starts with the positive-response echo
// `61 <group>`. Community-published offsets are quoted inconsistently — some
// relative to the start of the payload (including this echo), some to the
// first data byte after it. Everything in this file is relative to the START
// OF THE PAYLOAD, i.e. offset 0 is the 0x61 byte. The debug dump prints both
// framings side by side so you can tell which convention a source is using.
constexpr size_t kResponseEchoLen = 2;

// ---------------------------------------------------------------------------
// UNVERIFIED — these offsets must be confirmed against your own car.
//
// The 2011-2015 LBC layout is the best documented of any Leaf generation, but
// published tables disagree on framing (see the note above) and some are for
// later model years. Rather than trust them blind, capture a payload with the
// `1` command in the debug console, read the corresponding value off LeafSpy,
// and run tools/find_offsets to resolve the encoding empirically:
//
//     make find-offsets
//     ./build/find_offsets "61 01 00 00 ..." --target 87.5 --tol 0.1
//
// It reports every (offset, width, signedness, scale) tuple that yields your
// target value, which is usually a single candidate for a distinctive number.
// Fill the results in here, then delete this banner.
// ---------------------------------------------------------------------------
struct Group1Layout {
  size_t soc_offset;
  size_t soc_width;
  double soc_scale;

  size_t ahr_offset;
  size_t ahr_width;
  double ahr_scale;

  size_t soh_offset;
  size_t soh_width;
  double soh_scale;

  size_t pack_voltage_offset;
  size_t pack_voltage_width;
  double pack_voltage_scale;

  size_t pack_current_offset;
  size_t pack_current_width;
  double pack_current_scale;  // Signed: negative is discharge.
};

// Placeholder values. These are deliberately zeroed rather than filled with
// plausible-looking guesses: a wrong offset that decodes to a believable
// number is far more expensive to debug than one that obviously reads zero.
constexpr Group1Layout kGroup1Unverified = {
    /*soc*/ 0, 0, 0.0,
    /*ahr*/ 0, 0, 0.0,
    /*soh*/ 0, 0, 0.0,
    /*pack_voltage*/ 0, 0, 0.0,
    /*pack_current*/ 0, 0, 0.0,
};

}  // namespace leaf
