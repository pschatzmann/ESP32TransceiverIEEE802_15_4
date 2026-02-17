#pragma once

#include "Buffers.h"
#include "ESP32TransceiverIEEE802_15_4.h"

namespace ieee802154 {

/**
 * @brief Arduino Stream interface for IEEE 802.15.4 transceiver.
 *
 * Allows using ESP32TransceiverIEEE802_15_4 as a Stream for easy integration
 * with Arduino APIs and libraries that expect a Stream object.
 * Provides buffered read/write access to the transceiver.
 */
class ESP32TransceiverStream : public Stream {
 public:
  /**
   * @brief Construct a new ESP32TransceiverStream object.
   * @param transceiver Reference to the IEEE802.15.4 transceiver instance.
   */
  ESP32TransceiverStream(ESP32TransceiverIEEE802_15_4& transceiver)
      : transceiver(transceiver) {
    transceiver.setRxCallback(onReceive, this);
  }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin() {
    transceiver.begin();
    return true;
  }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin(FrameControlField fcf) {
    return begin(fcf);
  } 

  /**
   * @brief Deinitialize the stream and underlying transceiver.
   */
  void end() { transceiver.end(); }

  /**
   * @brief Write a buffer of bytes to the transceiver.
   * @param buffer Pointer to data buffer.
   * @param size Number of bytes to write.
   * @return Number of bytes written.
   */
  size_t write(const uint8_t* buffer, size_t size) override {
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
      if (write(buffer[i]) == 1) {
        written++;
      } else {
        break;  // Stop if we can't write more
      }
    }
    if (size < MTU) flush();
    return written;
  }

  /**
   * @brief Write a single byte to the transceiver.
   * @param byte Byte to write.
   * @return 1 if written, 0 otherwise.
   */
  size_t write(const uint8_t byte) override {
    tx_buffer.push(byte);
    if (tx_buffer.isFull()) {
      flush();
    }
    return 1;
  }

  /**
   * @brief Read a single byte from the receive buffer.
   * @return The byte read, or -1 if no data is available.
   */
  virtual int read() override {
    uint8_t c = 0;
    int len = readBytes(&c, 1);
    return len == 1 ? c : -1;
  }

  /**
   * @brief Read multiple bytes from the receive buffer.
   * @param buffer Pointer to destination buffer.
   * @param size Number of bytes to read.
   * @return Number of bytes actually read.
   */
  size_t readBytes(uint8_t* buffer, size_t size) {
    return rx_buffer.readArray(buffer, size);
  }

  /**
   * @brief Peek at the next byte in the receive buffer (not implemented).
   * @return The next byte, or -1 if no data is available.
   */
  int peek() {
    uint8_t c = 0;
    bool rc = rx_buffer.peek(c);
    return rc ? c : -1;
  }

  /**
   * @brief Get the number of bytes available to read.
   * @return Number of bytes available in the receive buffer.
   */
  virtual int available() override { return rx_buffer.available(); }

  /**
   * @brief Get the number of bytes available for writing.
   * @return Always returns 1024.
   */
  int availableForWrite() override { return 1024; }

  /**
   * @brief Flush the transmit buffer and send its contents as a frame.
   *
   * Sends all buffered data via the transceiver and clears the buffer.
   */
  void flush() override {
    if (!transceiver.send(tx_buffer.data(), tx_buffer.available())) {
      ESP_LOGE(TAG, "Failed to send frame: size %d", tx_buffer.available());
    }
    tx_buffer.clear();
  }

  /**
   * @brief Set the size of the receive buffer.
   * @param size New size of the receive buffer in bytes.
   * @note This will resize the internal buffer used for storing received
   * frames. Be cautious when setting this value, as it may lead to increased
   * memory usage.
   */
  void setReadBufferSize(size_t size) { rx_buffer.resize(size); }

 protected:
  static constexpr int MTU = 116;
  ESP32TransceiverIEEE802_15_4& transceiver;
  BufferRTOS<uint8_t> rx_buffer{MTU * 10};
  Buffer<MTU> tx_buffer;

  /**
   * @brief Static callback invoked when a frame is received by the transceiver.
   * @param frame Reference to the received frame.
   * @param frame_info Frame information from ESP-IDF.
   * @param user_data Pointer to the ESP32TransceiverStream instance.
   */
  static void onReceive(Frame& frame, esp_ieee802154_frame_info_t& frame_info,
                        void* user_data) {
    ESP32TransceiverStream* stream =
        static_cast<ESP32TransceiverStream*>(user_data);
    static uint8_t lastSeq = 0xFF; // 0xFF means uninitialized
    uint8_t seq = frame.sequenceNumber;
    if (lastSeq != 0xFF) {
      uint8_t expected = (lastSeq + 1) & 0xFF;
      if (seq != expected) {
        ESP_LOGE(TAG, "Frame out of sequence: expected %u, got %u", expected, seq);
      }
    }
    lastSeq = seq;
    if (stream->rx_buffer.writeArray(frame.payload, frame.payloadLen) !=
        frame.payloadLen) {
      ESP_LOGE(
          TAG,
          "RX buffer overflow: frame payload size %d exceeds available space",
          frame.payloadLen);
    }
  }

 protected:
  static constexpr const char* TAG = "ESP32TransceiverStream";
};

}  // namespace ieee802154