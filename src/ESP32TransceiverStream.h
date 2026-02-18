#pragma once

#include "ESP32TransceiverIEEE802_15_4.h"
#include "RingBuffer.h"

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
    transceiver.setReceiveTask(nullptr);
  }

  /**
   * @brief Set the Frame Control Field (FCF) for outgoing frames.
   * @param fcf The frame control field structure to use.
   * @note This method must be called before begin() to take effect!
   */
  void setFrameControlField(const FrameControlField& fcf) {
    transceiver.setFrameControlField(fcf);
  }

  /**
   * @brief Set the destination address for the stream.
   * @param address The destination address.
   */
  void setDestinationAddress(const Address& address) {
    transceiver.setDestinationAddress(address);
  }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin() {
    is_open_frame = false;
    // use no separate task!
    transceiver.setReceiveTask(nullptr);
    transceiver.setReceiveBufferSize(
        receive_msg_buffer_size);  // Set default message buffer size
    setRxBufferSize(1024);
    transceiver.setTxDoneCallback(ieee802154_transceiver_tx_done_callback,
                                  this);
    transceiver.setTxFailedCallback(ieee802154_transceiver_tx_failed_callback,
                                    this);
    transceiver.begin();
    return true;
  }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin(FrameControlField fcf) {
    this->fcf = fcf;
    transceiver.setFrameControlField(fcf);
    return begin();
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
    bool rc = tx_buffer.write(byte);
    if (tx_buffer.isFull()) {
      flush();
    }
    return rc ? 1 : 0;
  }
  /**
   * @brief Read a single byte from the receive buffer.
   * @return The byte read, or -1 if no data is available.
   */
  int read() override {
    receive();
    return rx_buffer.read();
  }

  /**
   * @brief Read multiple bytes from the receive buffer.
   * @param buffer Pointer to destination buffer.
   * @param size Number of bytes to read.
   * @return Number of bytes actually read.
   */
  size_t readBytes(uint8_t* buffer, size_t size) {
    // fill receive buffer
    while (receive());
    // provide data from receive buffer
    return rx_buffer.readArray(buffer, size);
  }

  /**
   * @brief Peek at the next byte in the receive buffer (not implemented).
   * @return The next byte, or -1 if no data is available.
   */
  int peek() {
    if (rx_buffer.isEmpty()) receive();
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
    // get data from buffer
    uint8_t tmp[tx_buffer.available()];
    int len = tx_buffer.readArray(tmp, tx_buffer.available());
    // send frame

    int attempt = 0;
    do {
      send_confirmation_state = isSendConfirmations() ? WAITING_FOR_CONFIRMATION
                                                      : CONFIRMATION_RECEIVED;
      ESP_LOGD(TAG, "Attempt %d: Sending frame, len: %d", attempt, len);
      if (!transceiver.send(tmp, len)) {
        ESP_LOGE(TAG, "Failed to send frame: size %d", len);
        send_confirmation_state = CONFIRMATION_ERROR;
      }
      // wait for confirmations
      while (send_confirmation_state == WAITING_FOR_CONFIRMATION) {
        delay(10);
      }

      // on error retry sending the same frame
      if (send_confirmation_state == CONFIRMATION_ERROR) {
        ESP_LOGI(TAG, "Send failed, retrying...");
        delay(send_retry_delay_ms);  // Short delay before retrying if needed
        // Decrement sequence number for retry
        transceiver.incrementSequenceNumber(-1);
      }
      ++attempt;
    } while (send_confirmation_state == CONFIRMATION_ERROR);
  }

  /**
   * @brief Set the size of the receive buffer. This defines how many bytes
   * we can get by calling readBytes();
   * @param size New size of the receive buffer.
   */
  void setRxBufferSize(size_t size) { rx_buffer.resize(size); }

  /**
   * @brief Set the size of the message buffer used for receiving frames.
   * @param size New size of the message buffer in bytes.
   * @note This method must be called before begin() to take effect.
   */
  void setRxMessageBufferSize(int size) {
    receive_msg_buffer_size = size;
    transceiver.setReceiveBufferSize(size);
  }

  /**
   * @brief Set the delay between send retries.
   * @param delay_ms Delay in milliseconds.
   */
  void setSendRetryDelay(int delay_ms) { send_retry_delay_ms = delay_ms; }

 protected:
  static constexpr const char* TAG = "ESP32TransceiverStream";
  static constexpr int MTU = 116;
  FrameControlField fcf;
  int receive_msg_buffer_size =
      (sizeof(frame_data_t) + 4) * 100;  // Default size for message buffer
  ESP32TransceiverIEEE802_15_4& transceiver;
  RingBuffer rx_buffer{1024};
  RingBuffer tx_buffer{MTU};
  Frame frame;  // For parsing and buffering received frames
  bool is_open_frame = false;
  enum send_confirmation_state_t {
    WAITING_FOR_CONFIRMATION,
    CONFIRMATION_RECEIVED,
    CONFIRMATION_ERROR,
  };
  volatile send_confirmation_state_t send_confirmation_state =
      WAITING_FOR_CONFIRMATION;
  bool is_send_confirations_enabled = false;
  int send_retry_delay_ms = 50;  // Delay between retries in milliseconds

  bool isSendConfirmations() { return fcf.ackRequest == 1; }

  bool isSequenceNumbers() { return fcf.sequenceNumberSuppression == 0; }

  /**
   * @brief Internal method to receive frames and fill the receive buffer.
   * @return True if a frame was received and processed, false otherwise.
   */
  bool receive() {
    static int last_seq = -1;
    frame_data_t packet;  // Temporary storage for received frame data
    if (is_open_frame) {
      // We have a pending frame that we haven't processed yet
      if (frame.payloadLen > rx_buffer.availableForWrite()) {
        delay(10);
        return false;
      }
      // Store payload in receive buffer
      rx_buffer.writeArray(frame.payload, frame.payloadLen);
      is_open_frame = false;  // Mark frame as processed
      return true;
    }

    // get next frame
    size_t read_bytes = 0;
    read_bytes = xMessageBufferReceive(transceiver.getMessageBuffer(), &packet,
                                       sizeof(frame_data_t), pdMS_TO_TICKS(10));
    if (read_bytes != sizeof(frame_data_t)) {
      if (read_bytes != 0) {
        ESP_LOGE(TAG, "Invalid packet size received: %d", read_bytes);
      }
      return false;
    }

    // Parse frame
    if (!frame.parse(packet.frame, false)) {
      ESP_LOGE(TAG, "Failed to parse frame");
      return false;
    }

    // Sequence number check (after successful parse, before buffer handling)
    if (isSequenceNumbers()) {
      int seq = frame.sequenceNumber;
      if (last_seq != -1) {
        int expected = (last_seq + 1) % 256;
        if (seq == last_seq) {
          ESP_LOGI(TAG, "Retransmission ignored: seq %d", seq);
          return false;  // Ignore duplicate
        } else if (seq != expected) {
          ESP_LOGI(TAG, "Frame sequence skipped: expected %d, got %d", expected,
                   seq);
          // Accept the new frame, but log the skip
        }
      }
      last_seq = seq;
    }

    if (frame.payloadLen > rx_buffer.availableForWrite()) {
      ESP_LOGD(TAG, "Received frame payload too large for buffer: %d bytes",
               frame.payloadLen);
      is_open_frame = true;
      return false;
    }

    // Store payload in receive buffer
    rx_buffer.writeArray(frame.payload, frame.payloadLen);
    delay(5);

    return true;
  }

  /**
   * @brief Callback for successful frame transmission.
   */
  static void ieee802154_transceiver_tx_done_callback(
      const uint8_t* frame, const uint8_t* ack,
      esp_ieee802154_frame_info_t* ack_frame_info, void* user_data) {
    ESP32TransceiverStream& self =
        *static_cast<ESP32TransceiverStream*>(user_data);
    self.send_confirmation_state = CONFIRMATION_RECEIVED;
  }

  /**
   * @brief Callback for failed frame transmission.
   */
  static void ieee802154_transceiver_tx_failed_callback(
      const uint8_t* frame, esp_ieee802154_tx_error_t error, void* user_data) {
    ESP32TransceiverStream& self =
        *static_cast<ESP32TransceiverStream*>(user_data);
    self.send_confirmation_state = CONFIRMATION_ERROR;
  }
};

}  // namespace ieee802154