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
Address local({0xAB, 0xCF}); // Different from sender
ESP32TransceiverIEEE802_15_4 transceiver(channel, 0x1234, local);
ieee802154::ESP32TransceiverStream stream(transceiver);

const int MTU = 116;
uint8_t rxData[MTU];

unsigned long startTime = 0;
unsigned long receivedBytes = 0;
unsigned long receivedFrames = 0;
unsigned long errorFrames = 0;


uint8_t calcChecksum(const uint8_t* data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) sum ^= data[i];
  return sum;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  ESP_LOGI(TAG, "Starting stream receive test...");
  stream.begin();
  startTime = millis();
}

void loop() {
  // Read full MTU-sized frame

  if (stream.available() >= MTU) {
    size_t read = stream.readBytes(rxData, MTU);
    receivedBytes += read;
    receivedFrames++;
    // Check checksum
    uint8_t expected = calcChecksum(rxData, MTU - 1);
    if (rxData[MTU - 1] != expected) errorFrames++;
  }

  // Print stats every second
  if (millis() - startTime > 1000) {
  Serial.printf("Received: %lu frames, %lu bytes, errors: %lu, rate: %lu bytes/sec\n",
          receivedFrames, receivedBytes, errorFrames, receivedBytes);
  receivedBytes = 0;
  receivedFrames = 0;
  errorFrames = 0;
  startTime = millis();
  }
}
