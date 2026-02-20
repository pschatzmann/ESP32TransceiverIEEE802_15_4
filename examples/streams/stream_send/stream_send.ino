/*
 * IEEE 802.15.4 Stream Send Example for Performance Measurement
 *
 * Fills the transmit buffer, adds a checksum,
 * sends frames as fast as possible and prints timing/throughput.
 */
#include "ESP32TransceiverStream.h"

const channel_t channel = channel_t::CHANNEL_11;
Address local({0xAB, 0xCF});
ESP32TransceiverStream stream(channel, 0x1234, local);

const int SEND_BUFFER_SIZE = 1024;
uint8_t txData[SEND_BUFFER_SIZE];
unsigned long startTime = 0;
unsigned long sentBytes = 0;

uint8_t calcChecksum(const uint8_t* data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) sum ^= data[i];
  return sum;
}

void setup() {
  Serial.begin(115200);
  // Short delay to allow serial monitor to connect
  delay(3000);
  Serial.println("Starting...");

  stream.setDestinationAddress(Address({0xAB, 0xCD}));
  stream.setSendDelay(10);
  stream.begin();

  startTime = millis();
}

void loop() {
  Serial.println("writing");
  // Fill data with incrementing pattern
  for (size_t i = 0; i < SEND_BUFFER_SIZE - 1; i++) {
    txData[i] = i % 256;
  }

  // Send the frame
  size_t written = stream.write(txData, SEND_BUFFER_SIZE);
  sentBytes += written;

  // Print stats every second
  unsigned long elapsed = millis() - startTime;
  if (elapsed > 1000) {
    float rate = (elapsed > 0) ? (sentBytes * 1000.0f / elapsed) : 0.0f;
    Serial.printf("Sent: %lu bytes, rate: %.2f bytes/sec\n", sentBytes, rate);
    sentBytes = 0;
    startTime = millis();
  }
}
