#include "console.h"

#include <Arduino.h>

#include "../../include/config.h"
#include "../leaf/decode.h"
#include "../leaf/pids.h"
#include "../power/sleep_manager.h"

namespace debugconsole {
namespace {

// Prints the payload twice over: once with offsets counted from the start of
// the payload (including the `61 xx` echo), once from the first data byte.
// Published Leaf decode tables use both conventions without saying which, and
// the resulting off-by-two is the single most common way to waste an evening.
void DumpPayload(const uint8_t* p, size_t len) {
  Serial.printf("payload %u bytes\n", static_cast<unsigned>(len));
  if (len >= 2) {
    Serial.printf("  echo: %02X %02X%s\n", p[0], p[1],
                  p[0] == 0x61 ? "  (positive response)" : "  (UNEXPECTED)");
  }

  Serial.println("  offsets from payload start (offset 0 = the 0x61 byte):");
  for (size_t i = 0; i < len; i += 16) {
    Serial.printf("    %3u:", static_cast<unsigned>(i));
    for (size_t j = i; j < i + 16 && j < len; ++j) Serial.printf(" %02X", p[j]);
    Serial.println();
  }

  if (len > leaf::kResponseEchoLen) {
    Serial.println("  offsets from first data byte (echo stripped):");
    const uint8_t* d = p + leaf::kResponseEchoLen;
    const size_t dlen = len - leaf::kResponseEchoLen;
    for (size_t i = 0; i < dlen; i += 16) {
      Serial.printf("    %3u:", static_cast<unsigned>(i));
      for (size_t j = i; j < i + 16 && j < dlen; ++j) Serial.printf(" %02X", d[j]);
      Serial.println();
    }
  }

  // One flat line to paste straight into find_offsets.
  Serial.print("  copy: \"");
  for (size_t i = 0; i < len; ++i) Serial.printf("%02X%s", p[i], i + 1 < len ? " " : "");
  Serial.println("\"");
}

void RequestAndDump(transport::LbcClient& lbc, uint8_t group) {
  const uint8_t* payload = nullptr;
  size_t len = 0;
  Serial.printf("\n-- group 0x%02X --\n", group);

  const auto status = lbc.RequestGroup(group, payload, len, config::kRequestTimeoutMs);
  if (status != transport::RequestStatus::Ok) {
    Serial.printf("  request failed: %s", transport::ToString(status));
    if (status == transport::RequestStatus::NegativeResponse)
      Serial.printf(" (NRC 0x%02X)", lbc.last_nrc());
    Serial.println();
    if (status == transport::RequestStatus::Timeout)
      Serial.println("  the LBC only answers when the car is in Ready or charging");
    return;
  }
  DumpPayload(payload, len);
}

void ShowCells(transport::LbcClient& lbc) {
  const uint8_t* payload = nullptr;
  size_t len = 0;
  const auto status =
      lbc.RequestGroup(leaf::kGroup2Cells, payload, len, config::kRequestTimeoutMs);
  if (status != transport::RequestStatus::Ok) {
    Serial.printf("\ncell request failed: %s\n", transport::ToString(status));
    return;
  }

  leaf::CellVoltages cv;
  if (!leaf::DecodeCellVoltages(payload, len, cv)) {
    Serial.println("\ncell decode failed");
    return;
  }

  Serial.printf("\n%u cells", cv.count);
  if (cv.count < leaf::kCellCount)
    Serial.printf("  (expected %u — likely a flow-control problem)", leaf::kCellCount);
  Serial.println();

  for (uint8_t i = 0; i < cv.count; ++i) {
    if (i % 8 == 0) Serial.printf("  %2u:", i);
    Serial.printf(" %4u", cv.mv[i]);
    if (i % 8 == 7) Serial.println();
  }
  if (cv.count % 8 != 0) Serial.println();

  Serial.printf("  min %u mV  max %u mV  spread %u mV  pack %.1f V\n", cv.MinMv(),
                cv.MaxMv(), cv.SpreadMv(), cv.SumMv() / 1000.0);
}

void ShowStatus(transport::LbcClient& lbc) {
  const uint8_t* payload = nullptr;
  size_t len = 0;
  const auto status =
      lbc.RequestGroup(leaf::kGroup1Status, payload, len, config::kRequestTimeoutMs);
  if (status != transport::RequestStatus::Ok) {
    Serial.printf("\nstatus request failed: %s\n", transport::ToString(status));
    return;
  }

  leaf::BatteryStatus st;
  if (!leaf::DecodeGroup1(payload, len, leaf::kGroup1Unverified, st)) {
    Serial.println("\ngroup 1 offsets are not filled in yet — see pids.h.");
    Serial.println("Capture the payload with '1', read the value off LeafSpy, then:");
    Serial.println("  make find-offsets && ./build/find_offsets \"<payload>\" --target <value>");
    return;
  }
  Serial.printf("\nSOC %.1f%%  SOH %.1f%%  %.2f Ah  %.1f V  %.1f A\n", st.soc_percent,
                st.soh_percent, st.ahr, st.pack_voltage, st.pack_current);
}

void Monitor(transport::TwaiBus& bus) {
  Serial.println("\nmonitoring — any key stops");
  Serial.println("(0x5B3 carries GIDs, 0x5BC carries SOH)");
  const bool was_normal = bus.mode() == transport::Mode::Normal;
  if (was_normal) bus.Restart(transport::Mode::ListenOnly);

  while (!Serial.available()) {
    transport::Frame f;
    if (!bus.Receive(f, 200)) continue;
    Serial.printf("  %03X [%u]", f.id, f.len);
    for (uint8_t i = 0; i < f.len; ++i) Serial.printf(" %02X", f.data[i]);
    if (f.id == leaf::kBroadcastGids) Serial.print("   <- GIDs");
    if (f.id == leaf::kBroadcastSoh) Serial.print("   <- SOH");
    Serial.println();
  }
  while (Serial.available()) Serial.read();
  if (was_normal) bus.Restart(transport::Mode::Normal);
  Serial.println("stopped");
}

}  // namespace

void PrintBanner() {
  Serial.println();
  Serial.println("Leaf CAN logger — 2015 24 kWh");
  Serial.printf("wake: %s\n", power::ToString(power::LastWakeReason()));
  PrintHelp();
}

void PrintHelp() {
  Serial.println();
  Serial.println("  1 2 4 6  request group and dump raw hex");
  Serial.println("           (1 status, 2 cell voltages, 4 temps, 6 shunts)");
  Serial.println("  c        decoded cell voltages");
  Serial.println("  s        decoded status (needs offsets in pids.h)");
  Serial.println("  m        monitor broadcast frames (listen-only)");
  Serial.println("  n        switch bus to normal mode (can transmit)");
  Serial.println("  l        switch bus to listen-only (cannot disturb the bus)");
  Serial.println("  i        bus info");
  Serial.println("  z        deep sleep now");
  Serial.println("  ?        this help");
}

bool Poll(transport::TwaiBus& bus, transport::LbcClient& lbc) {
  if (!Serial.available()) return false;
  const int c = Serial.read();

  switch (c) {
    case '1': RequestAndDump(lbc, leaf::kGroup1Status); break;
    case '2': RequestAndDump(lbc, leaf::kGroup2Cells); break;
    case '4': RequestAndDump(lbc, leaf::kGroup4Temps); break;
    case '6': RequestAndDump(lbc, leaf::kGroup6Shunts); break;
    case 'c': ShowCells(lbc); break;
    case 's': ShowStatus(lbc); break;
    case 'm': Monitor(bus); break;
    case 'n':
      Serial.println(bus.Restart(transport::Mode::Normal) ? "\nnormal mode"
                                                          : "\nfailed to restart");
      break;
    case 'l':
      Serial.println(bus.Restart(transport::Mode::ListenOnly) ? "\nlisten-only"
                                                              : "\nfailed to restart");
      break;
    case 'i':
      Serial.printf("\nbus %s, mode %s, %u kbit/s, tx=GPIO%d rx=GPIO%d rs=%s\n",
                    bus.running() ? "running" : "stopped",
                    bus.mode() == transport::Mode::Normal ? "normal" : "listen-only",
                    config::kCanBitrateKbps, config::kPinCanTx, config::kPinCanRx,
                    config::kHasRsControl ? "controlled" : "not available");
      break;
    case 'z':
      Serial.println("\nsleeping");
      power::EnterDeepSleep(bus);
      break;
    case '?': PrintHelp(); break;
    case '\r':
    case '\n': return false;
    default: Serial.println("\n? for help"); break;
  }
  return true;
}

}  // namespace debugconsole
