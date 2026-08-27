// Two-state power management.
//
// The Leaf's DC-DC converter runs whenever the car is in Ready OR plugged in
// and charging, so during every session worth logging the 12 V battery is
// being actively charged and power is effectively free. The only state that
// must be frugal is parked-and-idle.
//
//   Logging  - bus alive, transceiver active, poll freely.
//   Waiting  - deep sleep, transceiver in standby, woken by bus activity.
#pragma once

#include <stdint.h>

#include "../transport/twai_bus.h"

namespace power {

enum class WakeReason {
  ColdBoot,
  CanActivity,  // ext0 fired: the bus came alive.
  Timer,        // Fallback cadence when RS control is unavailable.
  Other,
};

WakeReason LastWakeReason();
const char* ToString(WakeReason r);

// Drives the transceiver's RS pin. Low = active, high = standby (driver off,
// receiver still live so it can wake us). No-op when kHasRsControl is false.
void InitTransceiverControl();
void SetTransceiverActive(bool active);

// Stops the bus, puts the transceiver in standby, arms the wake sources and
// does not return.
//
// ext0 wakes on the CAN RX line going low, which is the first dominant bit of
// the first frame after the bus comes alive — so a charge session is captured
// from its start rather than from the next timer tick. A timer wake is armed
// alongside it as a backstop.
[[noreturn]] void EnterDeepSleep(transport::TwaiBus& bus);

// True if any frame appears within `window_ms`. Used after a timer wake to
// decide whether to stay up or go straight back to sleep.
bool BusIsActive(transport::TwaiBus& bus, uint32_t window_ms);

}  // namespace power
