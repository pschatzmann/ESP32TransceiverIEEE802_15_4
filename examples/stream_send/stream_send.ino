/*
 * IEEE 802.15.4 Stream Send Example for Performance Measurement
 *
 * Fills the transmit buffer with maximum data (MTU), adds a checksum,
 * sends frames as fast as possible, and prints timing/throughput.
 */
#include "ESP32TransceiverIEEE802_15_4.h"
#include "ESP32TransceiverStream.h"

#define TAG "STREAM_SEND"
const channel_t channel = channel_t::CHANNEL_11;
Address local({0xAB, 0xCE});
ESP32TransceiverIEEE802_15_4 transceiver(channel, 0x1234, local);
ieee802154::ESP32TransceiverStream stream(transceiver);

const int MTU = 116;
uint8_t txData[MTU];
unsigned long startTime = 0;
unsigned long sentBytes = 0;
unsigned long sentFrames = 0;

uint8_t calcChecksum(const uint8_t* data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) sum ^= data[i];
  return sum;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  ESP_LOGI(TAG, "Starting stream send test...");

  FrameControlField fcf;
  fcf.ackRequest = true;
  stream.begin(fcf);

  startTime = millis();
  ESP_LOGI(TAG, "Started");

}

void loop() {
  // Fill data with incrementing pattern
  for (int i = 0; i < MTU - 1; ++i) txData[i] = sentFrames % 256;
  // Add checksum as the last byte
  txData[MTU - 1] = calcChecksum(txData, MTU - 1);

  // Send the frame
  size_t written = stream.write(txData, MTU);
  stream.flush();
  sentBytes += written;
  sentFrames++;

  // Print stats every second
  if (millis() - startTime > 1000) {
    Serial.printf("Sent: %lu frames, %lu bytes, rate: %lu bytes/sec\n",
                  sentFrames, sentBytes, sentBytes);
    sentBytes = 0;
    sentFrames = 0;
    startTime = millis();
  }
}
