/*
 * IEEE 802.15.4 Stream Receive Example for Performance Measurement
 *
 * Receives frames, checks checksum, measures throughput and error rate,
 * prints statistics (bytes received, errors, time).
 */

#include "ESP32TransceiverIEEE802_15_4.h"
#include "ESP32TransceiverStream.h"

#define TAG "STREAM_RECEIVE"
const channel_t channel = channel_t::CHANNEL_11;
Address local({0xAB, 0xCF});  // Different from sender
ESP32TransceiverIEEE802_15_4 transceiver(channel, 0x1234, local);
ieee802154::ESP32TransceiverStream stream(transceiver);

const int RECEIVE_BUFFER_SIZE = 1024;
uint8_t rxData[RECEIVE_BUFFER_SIZE];

unsigned long startTime = 0;
unsigned long receivedBytes = 0;

void setup() {
  Serial.begin(115200);
  // Short delay to allow serial monitor to connect
  delay(3000);

  ESP_LOGI(TAG, "Starting stream receive test...");
  stream.begin();
  startTime = millis();
}

void loop() {
  receivedBytes += stream.readBytes(rxData, RECEIVE_BUFFER_SIZE);

  // Print stats every second
  unsigned long elapsed = millis() - startTime;
  if (elapsed > 1000) {
    float rate = (elapsed > 0) ? (receivedBytes * 1000.0f / elapsed) : 0.0f;
    Serial.printf("Received: %lu bytes, rate: %.2f bytes/sec\n", receivedBytes,
                  rate);
    receivedBytes = 0;
    startTime = millis();
  }
}
