// Leaf CAN logger — 2015 Nissan Leaf, 24 kWh.
//
// Milestone 1 of the build: bus up, ISO-TP request/response working, and a
// serial console for the driveway session that resolves the unknown byte
// offsets against LeafSpy. Charge-session logging and the web UI build on top
// of this once the decode is confirmed.
#include <Arduino.h>

#include "../include/config.h"
#include "debug/console.h"
#include "leaf/decode.h"
#include "power/sleep_manager.h"
#include "transport/lbc_client.h"
#include "transport/twai_bus.h"

namespace {

transport::TwaiBus g_bus;
transport::LbcClient g_lbc(g_bus);

uint32_t g_last_traffic_ms = 0;
bool g_interactive = false;  // Set by any console input; suppresses auto-sleep.

}  // namespace

void setup() {
  Serial.begin(config::kSerialBaud);
  delay(200);

  power::InitTransceiverControl();
  power::SetTransceiverActive(true);

  // Start listen-only. Until the wiring is proven — CANH/CANL not swapped, the
  // transceiver's 120 ohm termination resistor actually removed — the firmware
  // should be physically unable to transmit onto the car's bus. Press 'n' to
  // go active once monitoring shows sane traffic.
  if (!g_bus.Begin(transport::Mode::ListenOnly)) {
    Serial.println("TWAI init failed — check the pins in include/config.h");
  }

  debugconsole::PrintBanner();
  Serial.println("\nStarted in listen-only mode. Press 'm' to watch the bus,");
  Serial.println("then 'n' to enable transmit once the traffic looks right.");

  g_last_traffic_ms = millis();
}

void loop() {
  if (debugconsole::Poll(g_bus, g_lbc)) {
    g_interactive = true;
    g_last_traffic_ms = millis();
  }

  // Drain the receive queue so an idle bus is distinguishable from a busy one,
  // and so the monitor never falls behind during a cell sweep.
  transport::Frame f;
  while (g_bus.Receive(f, 0)) g_last_traffic_ms = millis();

  g_bus.RecoverIfBusOff();

  // Parked and quiet: the LBC will not answer anyway, so there is nothing to
  // stay awake for. Skipped while someone is on the console.
  if (!g_interactive && millis() - g_last_traffic_ms > config::kBusIdleSleepMs) {
    Serial.println("bus idle — sleeping");
    power::EnterDeepSleep(g_bus);
  }

  delay(5);
}
