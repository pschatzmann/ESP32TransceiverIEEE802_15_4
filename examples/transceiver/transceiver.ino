
/*
 * Simple IEEE 802.15.4 Transceiver Example for ESP32
 *
 * This sketch demonstrates how to use the ESP32TransceiverIEEE802_15_4 library
 * to send and receive IEEE 802.15.4 frames.
 *
 * Features:
 * - Initializes the ESP32 transceiver on channel 11
 * - Sets up a callback to log detailed information about received frames
 * - Broadcasts a data frame with a simple payload every 5 seconds
 * - Logs transmission status and received frame details via ESP_LOG
 *
 * Usage:
 * - Connect ESP32 to serial monitor at 115200 baud
 * - Observe logs for transmitted and received frames
 * - Modify payload or frame parameters as needed for your application
 */

#include "ESP32TransceiverIEEE802_15_4.h"

#define TAG "SIMPLE_TRANSCEIVER"

Address local({0xAB, 0xCE});
const channel_t channel = channel_t::CHANNEL_11;
ESP32TransceiverIEEE802_15_4 transceiver(channel, 0x1234, local);
uint8_t payloadData[] = "Hello, IEEE 802.15.4!";

// Callback function for received frames (reference-based)
void rx_callback(Frame& frame,
                 esp_ieee802154_frame_info_t& frame_info, void* user_data) {
  // Frame Info (frame_info)
  ESP_LOGI(TAG, "Receiver Frame Info:");
  ESP_LOGI(TAG, "  Pending: %d", frame_info.pending);
  ESP_LOGI(TAG, "  Process: %d", frame_info.process);
  ESP_LOGI(TAG, "  Channel: %d", frame_info.channel);
  ESP_LOGI(TAG, "  RSSI: %d dBm", frame_info.rssi);
  ESP_LOGI(TAG, "  LQI: %d", frame_info.lqi);
  ESP_LOGI(TAG, "  Timestamp: %llu us", frame_info.timestamp);

  // Frame Info (frame)
  ESP_LOGI(TAG, "Frame Info:");
  ESP_LOGI(TAG, "  Payload Length: %zu bytes", frame.payloadLen);
  ESP_LOGI(TAG, "  RSSI_LQI: 0x%02x", frame.rssi_lqi);

  // Frame Control Field (FCF)
  ESP_LOGI(TAG, "Frame Control Field:");
  ESP_LOGI(TAG, "  Frame Type: %d (%s)", frame.fcf.frameType,
           frame.to_str(frame.fcf.frameType));
  ESP_LOGI(TAG, "  Security Enabled: %d", frame.fcf.securityEnabled);
  ESP_LOGI(TAG, "  Frame Pending: %d", frame.fcf.framePending);
  ESP_LOGI(TAG, "  ACK Request: %d", frame.fcf.ackRequest);
  ESP_LOGI(TAG, "  PAN ID Compression: %d", frame.fcf.panIdCompression);
  ESP_LOGI(TAG, "  Sequence Number Suppression: %d",
           frame.fcf.sequenceNumberSuppression);
  ESP_LOGI(TAG, "  Information Elements Present: %d",
           frame.fcf.informationElementsPresent);
  ESP_LOGI(TAG, "  Destination Address Mode: %d", frame.fcf.destAddrMode);
  ESP_LOGI(TAG, "  Frame Version: %d", frame.fcf.frameVersion);
  ESP_LOGI(TAG, "  Source Address Mode: %d", frame.fcf.srcAddrMode);

  // Sequence Number
  ESP_LOGI(TAG, "Sequence Number:");
  ESP_LOGI(TAG, "  Sequence Number: %d", frame.sequenceNumber);

  // Address Information
  ESP_LOGI(TAG, "Address Information:");
  ESP_LOGI(TAG, "  Destination PAN ID: 0x%04x", frame.destPanId);
  ESP_LOGI(TAG, "  Destination Address (len=%d): 0x%x 0x%x", frame.destAddrLen, frame.destAddress[0], frame.destAddress[1]  );
  ESP_LOGI(TAG, "  Source PAN ID: 0x%04x", frame.srcPanId);
  ESP_LOGI(TAG, "  Source Address (len=%d):  0x%x 0x%x", frame.srcAddrLen, frame.srcAddress[0], frame.srcAddress[1]);

  // Payload
  ESP_LOGI(TAG, "Payload: %s", frame.payload);
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  ESP_LOGI(TAG, "Starting...");

  // Set the receive callback
  transceiver.setRxCallback(rx_callback, NULL);

  // Initialize the IEEE 802.15.4 transceiver
  if (!transceiver.begin()) {
    ESP_LOGE(TAG, "Failed to initialize transceiver");
    return;
  }

  ESP_LOGI(TAG, "Simple transceiver started on channel %d", (int)channel);
}

void loop() {
  if (transceiver.send((uint8_t*)payloadData, sizeof(payloadData))) {
    ESP_LOGI(TAG, "Transmitted frame");
  } else {
    ESP_LOGE(TAG, "Transmit failed");
  }
  delay(5000);  // Transmit every 5 seconds
}
