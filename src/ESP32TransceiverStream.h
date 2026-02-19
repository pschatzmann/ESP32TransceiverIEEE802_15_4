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
   * @brief Get the current Frame Control Field (FCF) in use.
   * @return The current FrameControlField structure.
   */
  FrameControlField& getFrameControlField() { return transceiver.getFrameControlField(); }

  /**
   * @brief Get the current receive buffer size in bytes.
   * @return The size of the receive buffer.
   */
  size_t getRxBufferSize() const { return rx_buffer.size(); }

  /**
   * @brief Get the current message buffer size for receiving frames.
   * @return The size of the message buffer in bytes.
   */
  int getRxMessageBufferSize() const { return receive_msg_buffer_size; }

  /**
   * @brief Get the delay between send retries in milliseconds.
   * @return The retry delay in ms.
   */
  int getSendDelay() const { return send_delay_ms; }

  /**
   * @brief Set RX when idle mode for the transceiver.
   * @param rx_when_idle True to enable RX when idle, false to disable.
   * @return True if the mode was set successfully, false otherwise.
   */
  bool setRxWhenIdleActive(bool rx_when_idle) {
    return transceiver.setRxWhenIdleActive(rx_when_idle);
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
   * @brief Get a reference to the Frame Control Field (FCF) for outgoing frames.
   * @return Reference to the current Frame Control Field structure.
   */
  FrameControlField& frameControlField() {
    return transceiver.getFrameControlField();
  }

  /**
   * @brief Set the destination address for the stream.
   * @param address The destination address.
   */
  void setDestinationAddress(const Address& address) {
    transceiver.setDestinationAddress(address);
  }

  /**
   * @brief  Set the time in us to wait for the ack frame.
   *
   * @param[in]  timeout  The time to wait for the ack frame, in us.
   *                      It Should be a multiple of 16.
   */
  void setAckTimeoutUs(uint32_t timeout_us) {
    transceiver.setAckTimeoutUs(timeout_us);
  }

  /**
   * @brief Get the current acknowledgment timeout in microseconds.
   * @return The acknowledgment timeout in microseconds.
   */
  uint32_t getAckTimeoutUs() const { return transceiver.getAckTimeoutUs(); }

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
   * @brief Set the delay between sends or send retries.
   * @param delay_ms Delay in milliseconds.
   */
  void setSendDelay(int delay_ms) { send_delay_ms = delay_ms; }

  /**
   * @brief Defines the retry count for faild send requests
   * @param count Number of retries.
   */
  void setSendRetryCount(int count) { send_retry_count = count; }

  /**
   * @brief Enable or disable CCA (Clear Channel Assessment).
   * @param cca_enabled True to enable CCA (Clear Channel Assessment), false to
   * disable.
   */
  void setCCAActive(bool cca_enabled) { transceiver.setCCAActive(cca_enabled); }

  /**
   * @brief Check if CCA (Clear Channel Assessment) is enabled.
   * @return True if CCA is enabled, false otherwise.
   */
  bool isCCAActive() const { return transceiver.isCCAActive(); }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin() {
    is_open_frame = false;
    last_seq = -1;
    // use no separate task!
    transceiver.setAutoIncrementSequenceNumber(false);
    transceiver.setReceiveTask(nullptr);
    transceiver.setReceiveBufferSize(
        receive_msg_buffer_size);  // Set default message buffer size
    setRxBufferSize(1024);
    transceiver.setTxDoneCallback(ieee802154_transceiver_tx_done_callback,
                                  this);
    transceiver.setTxFailedCallback(ieee802154_transceiver_tx_failed_callback,
                                    this);
    // start with 1;
    transceiver.incrementSequenceNumber(1);

    return transceiver.begin();
  }

  /**
   * @brief Initialize the stream and underlying transceiver.
   * @return True on success, false otherwise.
   */
  bool begin(FrameControlField fcf) {
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
    uint32_t end = millis() + _timeout;
    while (receive() && millis() < end);
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
    if (isSendConfirmations()) {
      sendWithConfirmations();
    } else {
      sendWithoutConfirmations();
    }
  }

 protected:
  static constexpr const char* TAG = "ESP32TransceiverStream";
  static constexpr int MTU = 116;
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
  /// Delay after sending a frame when confirmations are not used
  int send_delay_ms = 10;
  int last_seq = -1;
  int send_retry_count = 2;
  esp_ieee802154_tx_error_t last_tx_error = ESP_IEEE802154_TX_ERR_NONE;

  bool isSendConfirmations() { return getFrameControlField().ackRequest == 1; }

  bool isSequenceNumbers() {
    return getFrameControlField().sequenceNumberSuppression == 0;
  }

  /**
   * @brief Internal method to receive frames and fill the receive buffer.
   * @return True if a frame was received and processed, false otherwise.
   */
  bool receive() {
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
                                       sizeof(frame_data_t), pdMS_TO_TICKS(20));
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

    ESP_LOGI(TAG, "Received frame: len=%d, seq=%d", frame.payloadLen,
             frame.sequenceNumber);

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
      // will be made availabe with next call
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
   * @brief Internal method to send a frame with confirmation handling.
   * Retries sending the frame if confirmation fails, with a delay between
   * attempts.
   */
  void sendWithConfirmations() {
    uint8_t tmp[tx_buffer.available()];
    int len = tx_buffer.readArray(tmp, tx_buffer.available());
    int retry = send_retry_count;
    // send frame
    int attempt = 0;
    do {
      send_confirmation_state = WAITING_FOR_CONFIRMATION;
      ESP_LOGD(TAG, "Attempt %d: Sending frame, len: %d", attempt, len);
      if (!transceiver.send(tmp, len)) {
        ESP_LOGE(TAG, "Failed to send frame: size %d", len);
        send_confirmation_state = CONFIRMATION_ERROR;
      }
      // wait for confirmations
      uint32_t timeout =
          millis() + getAckTimeoutUs() / 1000 + 100;  // Add some margin
      while (send_confirmation_state == WAITING_FOR_CONFIRMATION &&
             millis() < timeout) {
        delay(10);
      }

      // on error retry sending the same frame
      switch (send_confirmation_state) {
        case CONFIRMATION_ERROR: {
          ESP_LOGI(TAG, "Send failed with rc=%d, retrying...", last_tx_error);
          retry--;
          if (retry <= 0) {
            transceiver.incrementSequenceNumber(1);
            return;
          }
          delay(send_delay_ms);  // Short delay before retrying if needed
          break;
        }
        case CONFIRMATION_RECEIVED: {
          transceiver.incrementSequenceNumber(1);
          break;
        }
        default:
          retry--;
          delay(send_delay_ms);  // Short delay before retrying if needed
          break;
      }
      ++attempt;
    } while (send_confirmation_state == CONFIRMATION_ERROR);
    delay(send_delay_ms);  // Short delay before retrying if needed
  }

  /**
   * @brief Internal method to send a frame without waiting for confirmations.
   * Adds a delay after sending to allow the transceiver to process the frame.
   */
  void sendWithoutConfirmations() {
    uint8_t tmp[tx_buffer.available()];
    int len = tx_buffer.readArray(tmp, tx_buffer.available());
    ESP_LOGD(TAG, "Sending frame, len: %d", len);
    if (transceiver.send(tmp, len)) {
      transceiver.incrementSequenceNumber(1);
    } else {
      ESP_LOGE(TAG, "Failed to send frame: size %d", len);
    }
    delay(send_delay_ms);
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
    self.last_tx_error = ESP_IEEE802154_TX_ERR_NONE;
  }

  /**
   * @brief Callback for failed frame transmission.
   */
  static void ieee802154_transceiver_tx_failed_callback(
      const uint8_t* frame, esp_ieee802154_tx_error_t error, void* user_data) {
    ESP32TransceiverStream& self =
        *static_cast<ESP32TransceiverStream*>(user_data);
    self.send_confirmation_state = CONFIRMATION_ERROR;
    self.last_tx_error = error;
  }
};

}  // namespace ieee802154