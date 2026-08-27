// Board wiring and tunables.
#pragma once

#include <stdint.h>

namespace config {

// --- Pins (ESP32 DevKit V1, 30-pin) ---------------------------------------
// D4/D5 avoid the strapping pins (0, 2, 12, 15), the input-only pins
// (34, 35, 36, 39) and the USB serial console (1, 3).
//
// CAN_RX must be an RTC-capable GPIO: it doubles as the ext0 deep-sleep wake
// source, so bus activity can wake the chip. GPIO4 is RTC_GPIO10.
constexpr int kPinCanTx = 5;   // -> transceiver CTX (D)
constexpr int kPinCanRx = 4;   // -> transceiver CRX (R), also ext0 wake
constexpr int kPinCanRs = 25;  // -> transceiver RS: LOW = active, HIGH = standby

// Set false if your transceiver board does not break out RS. The common
// Waveshare/VP230 board ties RS to ground through a slope resistor, in which
// case standby is unavailable and the sleep manager falls back to timer wake.
// See README "Wake on CAN".
constexpr bool kHasRsControl = true;

// --- Bus -------------------------------------------------------------------
constexpr uint32_t kCanBitrateKbps = 500;  // ISO 15765-4, 11-bit at 500 kbit/s.

// --- Timing ----------------------------------------------------------------
constexpr uint32_t kRequestTimeoutMs = 1000;
constexpr uint32_t kBusIdleSleepMs = 60000;   // Idle this long -> deep sleep.
constexpr uint32_t kBusProbeWindowMs = 2000;  // Listen this long after a timer wake.
constexpr uint32_t kTimerWakeIntervalUs = 60ULL * 1000000ULL;  // Fallback wake cadence.

// --- Sampling --------------------------------------------------------------
// A charge session is hours long; 1 Hz on status and a cell sweep every 30 s
// is ample resolution for a charge curve and keeps bus load negligible.
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr uint32_t kCellSweepIntervalMs = 30000;

constexpr uint32_t kSerialBaud = 115200;

}  // namespace config
