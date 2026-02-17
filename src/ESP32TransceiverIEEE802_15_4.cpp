#include "ESP32TransceiverIEEE802_15_4.h"

#include <stdio.h>
#include <string.h>

#include "esp_ieee802154.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"

// tag for logging
#define TAG "IEEE802154_TRANSCEIVER"

namespace ieee802154 {

/// Structure to hold frame data and frame info
struct frame_data_t {
  uint8_t frame[MAX_FRAME_LEN];            // Raw frame data
  esp_ieee802154_frame_info_t frame_info;  // Frame info (RSSI, LQI, etc.)
};

/// accessible by global callback functions
ESP32TransceiverIEEE802_15_4* pt_transceiver = nullptr;

/// Forward declarations
void receive_packet_task(void* pvParameters);

bool ESP32TransceiverIEEE802_15_4::begin() {
  esp_err_t ret;

  if (is_active) {
    ESP_LOGW(TAG, "Transceiver is already active");
    return true;
  }

  // Initialize NVS flash
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NVS: %d", ret);
    return false;
  }

  // Validate channel
  if (static_cast<uint8_t>(channel) < 11 ||
      static_cast<uint8_t>(channel) > 26) {
    ESP_LOGE(TAG, "Invalid channel: %d", channel);
    return false;
  }

  // Create message buffer
  message_buffer = xMessageBufferCreate(sizeof(frame_data_t) + 4);
  if (!message_buffer) {
    ESP_LOGE(TAG, "Failed to create message buffer");
    end();
    return false;
  }

  // Initialize IEEE 802.15.4 radio
  ret = esp_ieee802154_enable();
  ESP_LOGI(TAG, "Enabling IEEE 802.15.4 radio");
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable IEEE 802.15.4 radio: %d", ret);
    end();
    return false;
  }
  radio_enabled = true;

  ret = esp_ieee802154_set_coordinator(is_coordinator);
  ESP_LOGI(TAG, "Setting coordinator mode to %s",
           is_coordinator ? "true" : "false");
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set coordinator mode to false: %d", ret);
    end();
    return false;
  }

  ret = esp_ieee802154_set_promiscuous(is_promiscuous_mode);
  ESP_LOGI(TAG, "Setting promiscuous mode to %s",
           is_promiscuous_mode ? "true" : "false");
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable promiscuous mode: %d", ret);
    end();
    return false;
  }

  ret = esp_ieee802154_set_rx_when_idle(is_rx_when_idle);
  ESP_LOGI(TAG, "Setting rx when idle to %s",
           is_rx_when_idle ? "true" : "false");
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set rx when idle: %d", ret);
    end();
    return false;
  }

  ret = esp_ieee802154_set_channel(static_cast<uint8_t>(channel));
  ESP_LOGI(TAG, "Setting channel to %d", (int) channel);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set channel %d: %d", channel, ret);
    end();
    return false;
  }

  ret = esp_ieee802154_set_panid(panID);
  ESP_LOGI(TAG, "Setting PAN ID to 0x%04X", panID);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set PAN ID: %d", ret);
    end();
    return false;
  }

  ESP_LOGI(TAG, "Setting local address: %s", local_address.to_str());
  if (local_address.mode() == addr_mode_t::SHORT) {
    ret = esp_ieee802154_set_short_address(*(uint16_t*)local_address.data());
  } else if (local_address.mode() == addr_mode_t::EXTENDED) {
    ret = esp_ieee802154_set_extended_address(local_address.data());
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set local address: %d", ret);
    end();
    return false;
  }

  ret = esp_ieee802154_receive();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start receiving: %d", ret);
    end();
    return false;
  }

  // Start receive task
  if (xTaskCreate(receive_packet_task, "RX", 1024 * 5, this, 5,
                  &rx_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create receive task");
    end();
    return false;
  }
  is_active = true;
  ESP_LOGI(TAG, "IEEE 802.15.4 transceiver initialized on channel %d", channel);
  return true;
}

bool ESP32TransceiverIEEE802_15_4::end(void) {
  esp_err_t ret;

  // Stop receive task
  if (rx_task_handle) {
    vTaskDelete(rx_task_handle);
    rx_task_handle = NULL;
  }

  // Free message buffer
  if (message_buffer) {
    vMessageBufferDelete(message_buffer);
    message_buffer = NULL;
  }

  // Disable radio
  if (radio_enabled) {
    ret = esp_ieee802154_disable();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable IEEE 802.15.4 radio: %d", ret);
      return false;
    }
    radio_enabled = false;
  }
  is_active = false;
  ESP_LOGI(TAG, "IEEE 802.15.4 transceiver deinitialized");
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setRxCallback(
    ieee802154_transceiver_rx_callback_t callback, void* user_data) {
  rx_callback_ = callback;
  rx_callback_user_data_ = user_data;
  ESP_LOGI(TAG, "Receive callback set");
  return true;
}

// Internal: Transmit an IEEE 802.15.4 frame
esp_err_t ESP32TransceiverIEEE802_15_4::transmit_channel(Frame* frame,
                                                         int8_t channel,
                                                         bool change_channel) {
  if (!is_active) {
    ESP_LOGE(TAG, "Transceiver is not active");
    return ESP_ERR_INVALID_STATE;
  }

  bool verbose = false;
  esp_err_t ret;

  if (!frame) {
    ESP_LOGE(TAG, "Invalid frame pointer");
    return ESP_ERR_INVALID_ARG;
  }

  if (change_channel) {
    if (channel < 11 || channel > 26) {
      ESP_LOGE(TAG, "Invalid channel: %d", channel);
      return ESP_ERR_INVALID_ARG;
    }
  }

  // Prepare buffer
  memset(transmit_buffer, 0, MAX_FRAME_LEN);  // Clear

  // Build frame into a byte array
  size_t len = frame->build(transmit_buffer, false);
  if (len == 0) {
    ESP_LOGE(TAG, "Failed to build frame");
    return ESP_FAIL;
  }
  // ESP_LOGI(TAG, "len: %d, buffer[0]: %d", len, buffer[0]);
  // ESP_LOG_BUFFER_HEX(TAG, buffer, buffer[0]);

  if (change_channel) {
    // Set channel
    ret = esp_ieee802154_set_channel(channel);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set channel %d: %d", channel, ret);
      return ret;
    }
  }

  // Transmit frame
  ret = esp_ieee802154_transmit(transmit_buffer, false);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit frame: %d", ret);
    return ret;
  }

  if (verbose) {
    if (change_channel)
      ESP_LOGI(TAG, "Transmitted frame of %zu bytes on channel %d", len,
               channel);
    else
      ESP_LOGI(TAG, "Transmitted frame of %zu bytes", len);
  }
  // Increment sequence number for next transmission
  frame->sequenceNumber++;
  return ESP_OK;
}

bool ESP32TransceiverIEEE802_15_4::send(uint8_t* data, size_t len) {
  return send(channel, data, len);
}

bool ESP32TransceiverIEEE802_15_4::send(channel_t toChannel, uint8_t* data,
                                        size_t len) {

  ESP_LOGI(TAG, "Sending frame on channel %d to address %s, len: %d", toChannel, destination_address.to_str(), len);
  frame.fcf = frame_control_field;
  frame.setPAN(panID);                    // Ensure PAN ID is set and compressed
  frame.setSourceAddress(local_address);  // Ensure source address is set
  frame.setDestinationAddress(
      destination_address);  // Ensure destination address is set
  frame.setPayload(data, len);
  return transmit_channel(&frame, static_cast<int8_t>(toChannel), true) ==
         ESP_OK;
}

bool ESP32TransceiverIEEE802_15_4::setChannel(channel_t channel) {
  if (static_cast<uint8_t>(channel) < 11 ||
      static_cast<uint8_t>(channel) > 26) {
    ESP_LOGE(TAG, "Invalid channel: %d", channel);
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret;

  // Set channel
  ret = esp_ieee802154_set_channel(static_cast<uint8_t>(channel));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set channel %d: %d", channel, ret);
    return false;
  }

  // Start receiving
  ret = esp_ieee802154_receive();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start receiving: %d", ret);
    return false;
  }

  // ESP_LOGI(TAG, "Channel set to %d", channel);
  return true;
}

void ESP32TransceiverIEEE802_15_4::onRxDone(
    uint8_t* frame, esp_ieee802154_frame_info_t* frame_info) {
  if (!message_buffer) {
    esp_ieee802154_receive_handle_done(frame);
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;

  // Prepare packet
  frame_data_t packet;
  memcpy(packet.frame, frame, frame[0]);
  packet.frame_info = *frame_info;

  // Send to message buffer
  size_t bytes_sent =
      xMessageBufferSendFromISR(message_buffer, &packet, sizeof(frame_data_t),
                                &higher_priority_task_woken);
  if (bytes_sent == 0) {
    ESP_EARLY_LOGW(TAG, "Message buffer full, packet discarded");
  }

  esp_ieee802154_receive_handle_done(frame);

  if (higher_priority_task_woken) {
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

bool ESP32TransceiverIEEE802_15_4::setTxDoneCallback(
    ieee802154_transceiver_tx_done_callback_t callback, void* user_data) {
  tx_done_callback_ = callback;
  tx_done_callback_user_data_ = user_data;
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setTxFailedCallback(
    ieee802154_transceiver_tx_failed_callback_t callback, void* user_data) {
  tx_failed_callback_ = callback;
  tx_failed_callback_user_data_ = user_data;
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setSfdCallback(
    ieee802154_transceiver_sfd_callback_t callback, void* user_data) {
  sfd_callback_ = callback;
  sfd_callback_user_data_ = user_data;
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setSfdTxCallback(
    ieee802154_transceiver_sfd_tx_callback_t callback, void* user_data) {
  sfd_tx_callback_ = callback;
  sfd_tx_callback_user_data_ = user_data;
  return true;
}

void ESP32TransceiverIEEE802_15_4::onTransmitDone(
    const uint8_t* frame, const uint8_t* ack,
    esp_ieee802154_frame_info_t* ack_frame_info) {
  if (tx_done_callback_) {
    tx_done_callback_(frame, ack, ack_frame_info, tx_done_callback_user_data_);
  }
}

void ESP32TransceiverIEEE802_15_4::onTransmitFailed(
    const uint8_t* frame, esp_ieee802154_tx_error_t error) {
  if (tx_failed_callback_) {
    tx_failed_callback_(frame, error, tx_failed_callback_user_data_);
  }
}

void ESP32TransceiverIEEE802_15_4::onStartFrameDelimiterReceived() {
  if (sfd_callback_) {
    sfd_callback_(sfd_callback_user_data_);
  }
}

void ESP32TransceiverIEEE802_15_4::onStartFrameDelimiterTransmitDone(
    uint8_t* frame) {
  if (sfd_tx_callback_) {
    sfd_tx_callback_(frame, sfd_tx_callback_user_data_);
  }
}

void receive_packet_task(void* pvParameters) {
  frame_data_t packet;
  Frame frame{};
  ESP32TransceiverIEEE802_15_4& transceiver =
      *static_cast<ESP32TransceiverIEEE802_15_4*>(pvParameters);

  ESP_LOGI(TAG, "Receive packet task started");

  while (1) {
    // Receive packet from message buffer
    size_t read_bytes =
        xMessageBufferReceive(transceiver.message_buffer, &packet,
                              sizeof(frame_data_t), pdMS_TO_TICKS(10));
    if (read_bytes != sizeof(frame_data_t)) {
      if (read_bytes != 0) {
        ESP_LOGE(TAG, "Invalid packet size received: %d", read_bytes);
      }
      continue;
    }

    // Parse frame
    if (!frame.parse(packet.frame, false)) {
      ESP_LOGE(TAG, "Failed to parse frame");
      continue;
    }

    // Invoke callback if set
    ESP32TransceiverIEEE802_15_4* self = pt_transceiver;
    if (self && self->rx_callback_) {
      //self->frame = frame;  // Update frame info for callback
      self->rx_callback_(frame, packet.frame_info,
                         self->rx_callback_user_data_);
    }

    // Short delay to yield CPU
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// Class member implementations for mode setters
bool ESP32TransceiverIEEE802_15_4::setCoordinatorActive(bool coordinator) {
  if (is_active) {
    ESP_LOGW(TAG, "Cannot change coordinator mode while active");
    return false;
  }
  is_coordinator = coordinator;
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setPromiscuousModeActive(bool promiscuous) {
  if (is_active) {
    ESP_LOGW(TAG, "Cannot change promiscuous mode while active");
    return false;
  }
  is_promiscuous_mode = promiscuous;

  return true;
}

bool ESP32TransceiverIEEE802_15_4::setRxWhenIdleActive(bool rx_when_idle) {
  if (is_active) {
    ESP_LOGW(TAG, "Cannot change rx when idle while active");
    return false;
  }
  is_rx_when_idle = rx_when_idle;
  return true;
}

bool ESP32TransceiverIEEE802_15_4::setTxPower(int power) {
  if (::esp_ieee802154_set_txpower(power) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set transmit power to %d", power);
    return false;
  }
  return true;
}

}  // namespace ieee802154

// The SFD (Start Frame Delimiter) of the frame was received.
extern "C" void esp_ieee802154_receive_sfd_done(void) {
  if (pt_transceiver) pt_transceiver->onStartFrameDelimiterReceived();
}

// Callback for received IEEE 802.15.4 frames.
extern "C" void esp_ieee802154_receive_done(
    uint8_t* frame, esp_ieee802154_frame_info_t* frame_info) {
  if (pt_transceiver) pt_transceiver->onRxDone(frame, frame_info);
}

// The Frame Transmission succeeded.
extern "C" void esp_ieee802154_transmit_done(
    const uint8_t* frame, const uint8_t* ack,
    esp_ieee802154_frame_info_t* ack_frame_info) {
  if (pt_transceiver)
    pt_transceiver->onTransmitDone(frame, ack, ack_frame_info);
}

// The Frame Transmission failed.
extern "C" void esp_ieee802154_transmit_failed(
    const uint8_t* frame, esp_ieee802154_tx_error_t error) {
  if (pt_transceiver) pt_transceiver->onTransmitFailed(frame, error);
}

// The SFD field of the frame was transmitted.
extern "C" void esp_ieee802154_transmit_sfd_done(uint8_t* frame) {
  if (pt_transceiver) pt_transceiver->onStartFrameDelimiterTransmitDone(frame);
}
