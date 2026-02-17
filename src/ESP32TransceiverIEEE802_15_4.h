#pragma once
// make sure that logging works in Arduino environment
#ifdef ARDUINO
#include "Arduino.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_ieee802154.h>
#include <esp_log.h>
#include <stdint.h>

#include "Frame.h"  // From shoderico/ieee802154_frame
#include "esp_err.h"
#include "esp_ieee802154.h"
#include "nvs_flash.h"

#define MAX_FRAME_LEN 128

namespace ieee802154 {

// forward declaration
class ESP32TransceiverIEEE802_15_4;
extern ESP32TransceiverIEEE802_15_4* pt_transceiver;

/**
 * @brief Enum for IEEE 802.15.4 channel numbers (11-26).
 */
enum class channel_t : uint8_t {
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
   * @brief Transmit an IEEE 802.15.4 frame on a specified channel.
   *
   * @param channel Channel number (11-26) to use for transmission.
   * @param data Pointer to payload data to transmit.
   * @param len Length of the payload data.
   * @return True on success, false on failure.
   */
  bool send(channel_t channel, uint8_t* data, size_t len);

  /**
   * @brief Set the IEEE 802.15.4 channel.
   *
   * @param channel Channel number (11-26).
   * @return ESP_OK on success, or an error code on failure.
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
   */
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
   */
  void setFrameControlField(const FrameControlField& fcf) {
    frame_control_field = fcf;
  }

  /***
   * @brief Get a reference to the actual frame object that is used for
   * sending..
   * @return Reference to the current Frame object.
   */
  Frame& getFrame() { return frame; }

 protected:
  bool is_promiscuous_mode = false;
  bool is_coordinator = false;
  bool is_rx_when_idle = true;
  bool is_active = false;
  // current frame
  Frame frame;
  channel_t channel = channel_t::CHANNEL_11;
  int16_t panID;          // Personal Area Network Identifier
  Address local_address;  // Local address for filtering (0, 2, or 8 bytes)
  Address destination_address = BROADCAST_ADDRESS;
  FrameControlField frame_control_field{};
  uint8_t transmit_buffer[MAX_FRAME_LEN] = {0};
  StreamBufferHandle_t message_buffer = NULL;
  TaskHandle_t rx_task_handle = NULL;
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

  esp_err_t transmit_channel(Frame* frame, int8_t channel, bool change_channel);

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