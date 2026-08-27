#include "sleep_manager.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "../../include/config.h"

namespace power {

WakeReason LastWakeReason() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0: return WakeReason::CanActivity;
    case ESP_SLEEP_WAKEUP_TIMER: return WakeReason::Timer;
    case ESP_SLEEP_WAKEUP_UNDEFINED: return WakeReason::ColdBoot;
    default: return WakeReason::Other;
  }
}

const char* ToString(WakeReason r) {
  switch (r) {
    case WakeReason::ColdBoot: return "cold boot";
    case WakeReason::CanActivity: return "CAN activity";
    case WakeReason::Timer: return "timer";
    case WakeReason::Other: return "other";
  }
  return "unknown";
}

void InitTransceiverControl() {
  if (!config::kHasRsControl) return;
  pinMode(config::kPinCanRs, OUTPUT);
  digitalWrite(config::kPinCanRs, LOW);  // Active.
}

void SetTransceiverActive(bool active) {
  if (!config::kHasRsControl) return;
  digitalWrite(config::kPinCanRs, active ? LOW : HIGH);
}

bool BusIsActive(transport::TwaiBus& bus, uint32_t window_ms) {
  if (!bus.running()) return false;
  transport::Frame f;
  return bus.Receive(f, window_ms);
}

[[noreturn]] void EnterDeepSleep(transport::TwaiBus& bus) {
  bus.End();

  // Standby: driver off, receiver still live so bus traffic can wake us.
  SetTransceiverActive(false);

  if (config::kHasRsControl) {
    // The CAN RX line idles high (recessive) and is pulled low by the first
    // dominant bit, so wake on level 0.
    const gpio_num_t wake_pin = static_cast<gpio_num_t>(config::kPinCanRx);
    rtc_gpio_pullup_en(wake_pin);
    rtc_gpio_pulldown_dis(wake_pin);
    esp_sleep_enable_ext0_wakeup(wake_pin, 0);
  }

  // Armed either as the backstop alongside ext0, or as the only wake source
  // when the transceiver has no RS pin broken out.
  esp_sleep_enable_timer_wakeup(config::kTimerWakeIntervalUs);

  Serial.flush();
  esp_deep_sleep_start();
}

}  // namespace power
