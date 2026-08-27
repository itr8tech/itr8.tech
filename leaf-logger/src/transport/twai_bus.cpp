#include "twai_bus.h"

#include <Arduino.h>
#include <driver/twai.h>

#include "../../include/config.h"

namespace transport {

bool TwaiBus::Begin(Mode mode) {
  if (running_) End();

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(config::kPinCanTx),
      static_cast<gpio_num_t>(config::kPinCanRx),
      mode == Mode::ListenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
  g.rx_queue_len = 32;  // A 96-cell response arrives as ~28 frames back to back.
  g.tx_queue_len = 8;

  const twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  const twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }
  running_ = true;
  mode_ = mode;
  return true;
}

void TwaiBus::End() {
  if (!running_) return;
  twai_stop();
  twai_driver_uninstall();
  running_ = false;
}

bool TwaiBus::Restart(Mode mode) {
  End();
  return Begin(mode);
}

bool TwaiBus::Send(uint32_t id, const uint8_t* data, uint8_t len, uint32_t timeout_ms) {
  if (!running_ || mode_ != Mode::Normal || len > 8) return false;

  twai_message_t msg = {};
  msg.identifier = id;
  msg.data_length_code = len;
  msg.flags = TWAI_MSG_FLAG_NONE;  // 11-bit standard ID.
  for (uint8_t i = 0; i < len; ++i) msg.data[i] = data[i];

  return twai_transmit(&msg, pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
}

bool TwaiBus::Receive(Frame& out, uint32_t timeout_ms) {
  if (!running_) return false;

  twai_message_t msg = {};
  if (twai_receive(&msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) return false;

  out.id = msg.identifier;
  out.len = msg.data_length_code > 8 ? 8 : msg.data_length_code;
  for (uint8_t i = 0; i < out.len; ++i) out.data[i] = msg.data[i];
  return true;
}

void TwaiBus::FlushRx() {
  if (!running_) return;
  twai_message_t msg;
  while (twai_receive(&msg, 0) == ESP_OK) {
  }
}

bool TwaiBus::RecoverIfBusOff() {
  if (!running_) return false;
  twai_status_info_t status;
  if (twai_get_status_info(&status) != ESP_OK) return false;
  if (status.state != TWAI_STATE_BUS_OFF) return false;

  twai_initiate_recovery();
  delay(100);
  twai_start();
  return true;
}

}  // namespace transport
