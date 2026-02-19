#pragma once
// make sure that logging works in Arduino environment
#ifdef ARDUINO
#include "Arduino.h"
#endif

#include <esp_ieee802154.h>
#include <esp_log.h>
#include <stdint.h>

#include "Frame.h"  // From shoderico/ieee802154_frame
#include "esp_err.h"
#include "esp_ieee802154.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace ieee802154 {

// forward declaration
class ESP32TransceiverIEEE802_15_4;
extern ESP32TransceiverIEEE802_15_4* pt_transceiver;

/**
 * @brief Enum for IEEE 802.15.4 channel numbers (11-26).
 */
enum class channel_t : uint8_t {
  UNDEFINED = 0,
  CHANNEL_11 = 11,
  CHANNEL_12 = 12,
  CHANNEL_13 = 13,
  CHANNEL_14 = 14,
  CHANNEL_15 = 15,
  CHANNEL_16 = 16,
  CHANNEL_17 = 17,
  CHANNEL_18 = 18,
  CHANNEL_19 = 19,
  CHANNEL_20 = 20,
  CHANNEL_21 = 21,
  CHANNEL_22 = 22,
  CHANNEL_23 = 23,
  CHANNEL_24 = 24,
  CHANNEL_25 = 25,
  CHANNEL_26 = 26
};

/**
 * @brief Callback function type for received IEEE 802.15.4 frames.
 *
 * @param frame Parsed IEEE 802.15.4 frame.
 * @param frame_info Frame information (e.g., RSSI, LQI, channel).
 * @param user_data User-defined data passed to the callback.
 */
typedef void (*ieee802154_transceiver_rx_callback_t)(
    Frame& frame, esp_ieee802154_frame_info_t& frame_info, void* user_data);

/**
 * @brief Callback function type for successful IEEE 802.15.4 frame
 * transmission.
 *
 * @param frame Pointer to transmitted frame data.
 * @param ack Pointer to acknowledgment frame data (if any).
 * @param ack_frame_info Frame info for the acknowledgment.
 * @param user_data User-defined data passed to the callback.
 */
typedef void (*ieee802154_transceiver_tx_done_callback_t)(
    const uint8_t* frame, const uint8_t* ack,
    esp_ieee802154_frame_info_t* ack_frame_info, void* user_data);

/**
 * @brief Callback function type for failed IEEE 802.15.4 frame transmission.
 *
 * @param frame Pointer to transmitted frame data.
 * @param error Transmission error code.
 * @param user_data User-defined data passed to the callback.
 */
typedef void (*ieee802154_transceiver_tx_failed_callback_t)(
    const uint8_t* frame, esp_ieee802154_tx_error_t error, void* user_data);

/**
 * @brief Callback function type for SFD (Start Frame Delimiter) received event.
 *
 * @param user_data User-defined data passed to the callback.
 */
typedef void (*ieee802154_transceiver_sfd_callback_t)(void* user_data);

/**
 * @brief Callback function type for SFD transmitted event.
 *
 * @param frame Pointer to frame data.
 * @param user_data User-defined data passed to the callback.
 */
typedef void (*ieee802154_transceiver_sfd_tx_callback_t)(uint8_t* frame,
                                                         void* user_data);
/// Broadcast address constant
inline Address BROADCAST_ADDRESS((uint8_t[2]){0xFF, 0xFF});

/**
 * @brief Class to manage an IEEE 802.15.4 transceiver using the ESP-IDF API.
 * On the sending side we support broadcast and direct addressing.
 * On the receiving side we support promiscuous mode, and reveiving only frames
 * that are destinated to the device or broadcast frames.
 *
 * @warning Only one instance of this class should be created, as the ESP-IDF
 * callbacks are global and will reference the single instance through a global
 * pointer. Creating multiple instances may lead to undefined behavior.
 */

class ESP32TransceiverIEEE802_15_4 {
  // Friend declarations for global callback functions
  friend void receive_packet_task(void*);
  friend void ::esp_ieee802154_receive_done(
      uint8_t* frame, esp_ieee802154_frame_info_t* frame_info);
  friend void ::esp_ieee802154_transmit_done(
      const uint8_t* frame, const uint8_t* ack,
      esp_ieee802154_frame_info_t* ack_frame_info);
  friend void ::esp_ieee802154_transmit_failed(const uint8_t* frame,
                                               esp_ieee802154_tx_error_t error);
  friend void ::esp_ieee802154_receive_sfd_done(void);
  friend void ::esp_ieee802154_transmit_sfd_done(uint8_t* frame);

 public:
  /**
   * @brief Construct a new ESP32TransceiverIEEE802_15_4 object.
   * @param panID The Personal Area Network Identifier to use for the
   * transceiver.
   * @param localAddress The local address for the device.
   */
  ESP32TransceiverIEEE802_15_4(channel_t channel, int16_t panID,
                               Address localAddress) {
    this->channel = channel;
    this->panID = panID;
    this->local_address = localAddress;
    pt_transceiver = this;
  }

  /**
   * @brief Initialize the IEEE 802.15.4 transceiver with a specified channel.
   *
   * @return ESP_OK on success, or an error code on failure.
   */
  bool begin();

  /**
   * @brief Initialize the IEEE 802.15.4 transceiver with a specified channel.
   * @param fcf The Frame Control Field to use for the transceiver.
   * transceiver.
   *
   * @return ESP_OK on success, or an error code on failure.
   */
  bool begin(FrameControlField fcf) {
    setFrameControlField(fcf);
    return begin();
  }

  /**
   * @brief Deinitialize the IEEE 802.15.4 transceiver.
   *
   * @return ESP_OK on success, or an error code on failure.
   */
  bool end(void);

  /**
   * @brief Set the callback function for received frames.
   *
   * @param callback Callback function to invoke on frame reception.
   * @param user_data User-defined data to pass to the callback.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool setRxCallback(ieee802154_transceiver_rx_callback_t callback,
                     void* user_data);

  /**
   * @brief Set the callback function for successful frame transmission.
   *
   * @param callback Callback function to invoke when a frame is successfully
   * transmitted.
   * @param user_data User-defined data to pass to the callback.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool setTxDoneCallback(ieee802154_transceiver_tx_done_callback_t callback,
                         void* user_data);

  /**
   * @brief Set the callback function for failed frame transmission.
   *
   * @param callback Callback function to invoke when a frame transmission
   * fails.
   * @param user_data User-defined data to pass to the callback.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool setTxFailedCallback(ieee802154_transceiver_tx_failed_callback_t callback,
                           void* user_data);

  /**
   * @brief Set the callback function for SFD (Start Frame Delimiter) received
   * event.
   *
   * @param callback Callback function to invoke when SFD is received.
   * @param user_data User-defined data to pass to the callback.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool setSfdCallback(ieee802154_transceiver_sfd_callback_t callback,
                      void* user_data);

  /**
   * @brief Set the callback function for SFD transmitted event.
   *
   * @param callback Callback function to invoke when SFD is transmitted.
   * @param user_data User-defined data to pass to the callback.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool setSfdTxCallback(ieee802154_transceiver_sfd_tx_callback_t callback,
                        void* user_data);
  /**
   * @brief Transmit an IEEE 802.15.4 frame on the current channel.
   *
   * @param data payload data to tramsit.
   * @param len length of the payload data.
   * @return ESP_OK on success, or an error code on failure.
   */
  bool send(uint8_t* data, size_t len);

  /**
   * @brief Transmit an IEEE 802.15.4 frame. You need to setup up
   * all values in the frame object before calling this method. The channel and
   * destination address will be used from the frame object.
   *
   * @param frame Pointer to the frame to transmit.
   * @return True on success, false on failure.
   * @note if the the frame frame does not have a PAN, source or destination
   * address, we will use the information defined in the transceiver object.
   */
  bool send(Frame& frame);

  /**
   * @brief Change the IEEE 802.15.4 channel.
   * @param channel Channel number (11-26).
   * @return ESP_OK on success, or an error code on failure.
   * @note This method can be called at any time to change the channel of the
   * transceiver.
   */
  bool setChannel(channel_t channel);

  /**
   * @brief Get the current pending mode of the transceiver.
   * @return The pending mode value from the ESP-IDF driver.
   */
  esp_ieee802154_pending_mode_t getPendingMode() {
    return ::esp_ieee802154_get_pending_mode();
  }

  /**
   * @brief Get the current transmit power of the transceiver.
   * @return The transmit power value from the ESP-IDF driver.
   */
  int8_t getTxPower() { return ::esp_ieee802154_get_txpower(); }

  /**
   * @brief Set the transmit power of the transceiver.
   * @param power Transmit power value to set (in dBm). -24 dBm  to  +20 dBm
   * @return True if the power was set successfully, false otherwise.
   */
  bool setTxPower(int power);

  /**
   * @brief Check if the transceiver is currently set as coordinator.
   * @return True if coordinator mode is active, false otherwise.
   */
  bool isCoordinatorActive() const { return is_coordinator; }

  /**
   * @brief Set the coordinator mode for the transceiver.
   * @param coordinator True to enable coordinator mode, false to disable.
   * @return True if the mode was set successfully, false otherwise.
   * @note This method must be called before begin() to take effect!
   */
  bool setCoordinatorActive(bool coordinator);

  /**
   * @brief Check if promiscuous mode is active.
   * @return True if promiscuous mode is enabled, false otherwise.
   */
  bool isPromiscuousModeActive() const { return is_promiscuous_mode; }

  /**
   * @brief Set promiscuous mode for the transceiver.
   * @param promiscuous True to enable promiscuous mode, false to disable.
   * @return True if the mode was set successfully, false otherwise.
   * @note This method must be called before begin() to take effect!
   */
  bool setPromiscuousModeActive(bool promiscuous);

  /**
   * @brief Check if RX when idle mode is active.
   * @return True if RX when idle is enabled, false otherwise.
   */
  bool isRxWhenIdleActive() const { return is_rx_when_idle; }

  /**
   * @brief Set RX when idle mode for the transceiver.
   * @param rx_when_idle True to enable RX when idle, false to disable.
   * @return True if the mode was set successfully, false otherwise.
   */
  bool setRxWhenIdleActive(bool rx_when_idle);

  /**
   * @brief Set the destination address for outgoing frames.
   * @param address Destination address to use for outgoing frames.
   */
  void setDestinationAddress(const Address& address) {
    destination_address = address;
  }

  /**
   * @brief Set the destination address to the broadcast address (0xFFFF).
   */
  void setBroadcast() { setDestinationAddress(BROADCAST_ADDRESS); }

  /**
   * @brief Set the Frame Control Field (FCF) for outgoing frames.
   * @param fcf The frame control field structure to use.
   * @note This method must be called before begin() to take effect!
   */
  void setFrameControlField(const FrameControlField& fcf) {
    frame_control_field = fcf;
  }

  /**
   * @brief Get a reference to the Frame Control Field (FCF) for outgoing frames.
   * @return Reference to the current Frame Control Field structure.
   */

  FrameControlField& frameControlField() { return frame_control_field; }  

  /***
   * @brief Get a reference to the actual frame object that is used for
   * sending..
   * @return Reference to the current Frame object.
   */
  Frame& getFrame() { return frame; }

  /**
   * @brief Set a custom FreeRTOS task function for processing received packets.
   * @param task Pointer to the task function to use for processing received
   */
  void setReceiveTask(void (*task)(void* pvParameters)) {
    receive_packet_task = task;
  }

  /***
   * @brief Defines the receive buffer size for incoming frames.
   * @param size The size of the receive buffer in bytes.
   * @note This method must be called before begin() to take effect!
   */
  void setReceiveBufferSize(int size);
  /**
   * @brief Get the FreeRTOS message buffer handle for received frames.
   * @return StreamBufferHandle_t for the RX message buffer.
   */
  StreamBufferHandle_t getMessageBuffer() const { return message_buffer; }

  /**
   * @brief Increment the sequence number in the current frame by a
   * specified value.
   * @param n The value to add to the current sequence number.
   * @note this is automatically called after each successful transmission, but
   * you can call it
   */
  void incrementSequenceNumber(int n = 1) {
    frame.sequenceNumber += n;
    // Ensure the sequence number wraps around at 255
    frame.sequenceNumber = frame.sequenceNumber % 256;
  }

  /**
   * @brief Enable or disable automatic incrementing of the sequence number
   * after each successful transmission.
   * @param auto_increment True to enable automatic incrementing, false to
   */
  void setAutoIncrementSequenceNumber(bool auto_increment) {
    auto_increment_sequence_number = auto_increment;
  }

  /**
   * @brief  Set the time in us to wait for the ack frame.
   *
   * @param[in]  timeout  The time to wait for the ack frame, in us.
   *                      It Should be a multiple of 16.
   */
  void setAckTimeout(uint32_t timeout_us) {
    ack_timeout_us = timeout_us / 16 * 16;  // Round to nearest multiple of 16
  }

 protected:
  bool is_promiscuous_mode = false;
  bool is_coordinator = false;
  bool is_rx_when_idle = true;
  bool is_active = false;
  // current frame
  Frame frame;
  channel_t channel = channel_t::UNDEFINED;
  int16_t panID;          // Personal Area Network Identifier
  Address local_address;  // Local address for filtering (0, 2, or 8 bytes)
  Address destination_address = BROADCAST_ADDRESS;
  FrameControlField frame_control_field{};
  uint8_t transmit_buffer[MAX_FRAME_LEN] = {0};
  StreamBufferHandle_t message_buffer = nullptr;
  TaskHandle_t rx_task_handle = nullptr;
  bool radio_enabled = false;
  ieee802154_transceiver_rx_callback_t rx_callback_ = nullptr;
  void* rx_callback_user_data_ = nullptr;
  ieee802154_transceiver_tx_done_callback_t tx_done_callback_ = nullptr;
  void* tx_done_callback_user_data_ = nullptr;
  ieee802154_transceiver_tx_failed_callback_t tx_failed_callback_ = nullptr;
  void* tx_failed_callback_user_data_ = nullptr;
  ieee802154_transceiver_sfd_callback_t sfd_callback_ = nullptr;
  void* sfd_callback_user_data_ = nullptr;
  ieee802154_transceiver_sfd_tx_callback_t sfd_tx_callback_ = nullptr;
  void* sfd_tx_callback_user_data_ = nullptr;
  static void default_receive_packet_task(void* pvParameters);
  void (*receive_packet_task)(void* pvParameters) = default_receive_packet_task;
  int receive_msg_buffer_size = sizeof(frame_data_t) + 4;
  uint32_t ack_timeout_us = (2016 * 16);
  bool auto_increment_sequence_number = true;

  esp_err_t transmit_frame(Frame* frame);
  void onRxDone(uint8_t* frame, esp_ieee802154_frame_info_t* frame_info);
  void onTransmitDone(const uint8_t* frame, const uint8_t* ack,
                      esp_ieee802154_frame_info_t* ack_frame_info);
  void onTransmitFailed(const uint8_t* frame, esp_ieee802154_tx_error_t error);
  void onStartFrameDelimiterReceived();
  void onStartFrameDelimiterTransmitDone(uint8_t* frame);
};

}  // namespace ieee802154

#ifdef ARDUINO
using namespace ieee802154;
#endif