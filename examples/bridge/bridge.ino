/*
 * IEEE 802.15.4 Bridge Example for ESP32
 *
 * This sketch demonstrates how to use the ESP32TransceiverIEEE802_15_4 library
 * to bridge frames between two IEEE 802.15.4 channels.
 *
 * Features:
 * - Receives frames on channel 11 and retransmits them on channel 13
 * - Measures and logs performance statistics for frame processing
 * - Handles transmission and reception callbacks for robust operation
 * - Logs average execution time for performance monitoring
 *
 * Usage:
 * - Connect ESP32 to serial monitor at 115200 baud
 * - Observe logs for bridged frames and performance metrics
 * - Useful for protocol bridging and channel analysis
 */
#include "ESP32TransceiverIEEE802_15_4.h"

#define TAG "IEEE802154_BRIDGE"

const channel_t RX_CHANNEL = channel_t::CHANNEL_11;
const channel_t TX_CHANNEL = channel_t::CHANNEL_13;
Address local({0xAB, 0xCD});
ESP32TransceiverIEEE802_15_4 transceiver(RX_CHANNEL, 0x1234, local);
const int NUM_SAMPLES = 100;
Frame frame;
static bool transmitting = false;

int64_t total_time_us = 0;
uint32_t sample_count = 0;
int64_t start_time = 0;
QueueHandle_t log_queue = NULL;
float average_time_us;

void capture_start() { start_time = millis(); }

void capture_done() {
  int64_t end_time = millis();
  int64_t elapsed_time = end_time - start_time;

  total_time_us += elapsed_time;
  sample_count++;

  if (sample_count >= NUM_SAMPLES) {
    float average_time_us = (float)total_time_us / NUM_SAMPLES;
    if (log_queue != NULL) {
      xQueueSendFromISR(log_queue, &average_time_us, NULL);
    }
    total_time_us = 0;
    sample_count = 0;
  }
}

//=========================================================================================
// Callbacks
void onRxDone(Frame& frame, esp_ieee802154_frame_info_t& frame_info,
              void* user_data) {
  // ESP_LOGI(TAG, "Received.");
  // ESP_LOGI(TAG, "payloadLen: %d", frame.payloadLen);
  capture_start();

  // Send to another channel
  transmitting = true;
  if (!transceiver.send(TX_CHANNEL, frame.payload, frame.payloadLen)) {
    ESP_LOGE(TAG, "transmit failed.");
    transmitting = false;
  }

  // Wait for the end of transmitting
  while (transmitting) {
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  // Back to receiving channel
  if (!transceiver.setChannel(RX_CHANNEL)) {
    ESP_LOGE(TAG, "recover channel failed.");
  }
}

void onTxDone(const uint8_t* frame, const uint8_t* ack,
              esp_ieee802154_frame_info_t* ack_frame_info, void* user_data) {
  transmitting = false;
  capture_done();
  // ESP_LOGI(TAG, "tx OK, sent %d bytes, ack %d", frame[0], ack != NULL);
}

void onTxFailed(const uint8_t* frame, esp_ieee802154_tx_error_t error,
                void* user_data) {
  transmitting = false;
  capture_done();
  ESP_LOGW(TAG, "tx failed, error %d", error);
}

//=========================================================================================

void setup() {
  Serial.begin(115200);

  // Prepare performance log
  log_queue = xQueueCreate(10, sizeof(float));
  if (log_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create log queue");
    return;
  }

  // Set callbacks
  transceiver.setRxCallback(onRxDone, NULL);
  transceiver.setTxDoneCallback(onTxDone, NULL);
  transceiver.setTxFailedCallback(onTxFailed, NULL);

  // Initialize transceiver with channel
  if (!transceiver.begin()) {
    ESP_LOGE(TAG, "Failed to initialize transceiver");
    return;
  }

  ESP_LOGI(TAG, "esp_ieee802154_get_pending_mode: %d",
           transceiver.getPendingMode());
  ESP_LOGI(TAG, "esp_ieee802154_get_txpower: %d", transceiver.getTxPower());
}

void loop() {
  if (xQueueReceive(log_queue, &average_time_us, portMAX_DELAY)) {
    ESP_LOGI(TAG, "Average execution time over %d samples: %.2f us",
             NUM_SAMPLES, average_time_us);
  }
}
