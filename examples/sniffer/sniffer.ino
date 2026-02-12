/*
 * IEEE 802.15.4 Sniffer Example for ESP32
 *
 * This sketch demonstrates how to use the ESP32TransceiverIEEE802_15_4 library
 * to capture and log IEEE 802.15.4 frames.
 *
 * Features:
 * - Initializes the ESP32 transceiver in sniffer mode on channel 11
 * - Logs detailed information about each received frame, including payload and
 * metadata
 * - Formats received data as hexadecimal for easy inspection
 *
 * Usage:
 * - Connect ESP32 to serial monitor at 115200 baud
 * - Observe logs for captured frames
 * - Useful for debugging and protocol analysis
 */
#include "ESP32TransceiverIEEE802_15_4.h"

#define TAG "IEEE802154_SNIFFER"

Address local({0xAB, 0xCD});
ESP32TransceiverIEEE802_15_4 transceiver(channel_t::CHANNEL_11, 0x1234, local);

// Function to format a byte buffer as a hexadecimal string in one line
int hex_dump_in_oneline(char* line, size_t line_size, const uint8_t* buffer,
                        size_t len) {
  size_t line_pos = 0;  // Current position in the output line

  // Check if the output buffer size is valid
  if (line_size == 0) {
    return 0;  // Return 0 if no space is available
  }

  // Iterate through each byte in the input buffer
  for (size_t i = 0; i < len; i++) {
    // Append the byte as a two-digit hexadecimal value
    int written =
        snprintf(line + line_pos, line_size - line_pos, "%02x", buffer[i]);
    if (written < 0 || (size_t)written >= line_size - line_pos) {
      ESP_LOGE(TAG, "Buffer overflow at byte %zu", i);
      break;  // Stop if there's not enough space
    }
    line_pos += written;

    // Add a double space every 8 bytes, except for the last byte
    if ((i + 1) % 8 == 0 && i != len - 1) {
      written = snprintf(line + line_pos, line_size - line_pos, "  ");
      if (written < 0 || (size_t)written >= line_size - line_pos) {
        ESP_LOGE(TAG, "Buffer overflow at delimiter %zu", i);
        break;  // Stop if there's not enough space
      }
      line_pos += written;
    } else if (i != len - 1) {
      // Add a single space between bytes, except for the last byte
      written = snprintf(line + line_pos, line_size - line_pos, " ");
      if (written < 0 || (size_t)written >= line_size - line_pos) {
        ESP_LOGE(TAG, "Buffer overflow at space %zu", i);
        break;  // Stop if there's not enough space
      }
      line_pos += written;
    }
  }

  // Ensure the output string is null-terminated
  if (line_pos < line_size) {
    line[line_pos] = '\0';
  } else {
    line[line_size - 1] = '\0';  // Truncate if buffer is full
  }

  return line_pos;  // Return the number of characters written
}


// Callback for received frames.
void rx_callback(Frame& frame,
                 esp_ieee802154_frame_info_t& frame_info, void* user_data) {
  char buff[512] = {0};
  int pos = 0;
  pos += sprintf(&buff[pos], "frameType: %s",
                 frame.to_str(frame.fcf.frameType));
  pos += sprintf(&buff[pos], ", seqNum: %02x", frame.sequenceNumber);
  pos += sprintf(&buff[pos], ", dstPanId: %04x", frame.destPanId);

  pos += sprintf(&buff[pos], ", dstAddr: ");
  pos += hex_dump_in_oneline(&buff[pos], sizeof(buff) - pos, frame.destAddress,
                             frame.destAddrLen);

  pos += sprintf(&buff[pos], ", srcAddr: ");
  pos += hex_dump_in_oneline(&buff[pos], sizeof(buff) - pos, frame.srcAddress,
                             frame.srcAddrLen);

  pos += sprintf(&buff[pos], ", payload: ");
  pos += hex_dump_in_oneline(&buff[pos], sizeof(buff) - pos, frame.payload,
                             frame.payloadLen);

  ESP_LOGI(TAG, "%s", buff);
}

void setup() {
  Serial.begin(115200);

  // Set receive callback
  transceiver.setRxCallback(rx_callback, NULL);

  // Initialize transceiver with channel
  if (!transceiver.begin()) {
    ESP_LOGE(TAG, "Failed to initialize transceiver");
    return;
  }
}

void loop() { delay(1000); }
