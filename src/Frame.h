#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "esp_assert.h"

// Constants
#define IEEE802154_FCF_SIZE 2
#define IEEE802154_MAX_ADDR_LEN 8
#define IEEE802154_PAN_ID_LEN 2
#define IEEE802154_RSSI_LQI_SIZE 1  // 1 byte for combined RSSI and LQI

namespace ieee802154 {

/// IEEE 802.15.4 FCF field value enumerations
enum class Frameype_t : uint8_t {
  BEACON = 0x0,   // Beacon frame
  DATA = 0x1,     // Data frame
  ACK = 0x2,      // Acknowledgment frame
  MAC_CMD = 0x3,  // MAC Command frame
                  // 0x4 to 0x7 are reserved
};

/// IEEE 802.15.4 address mode enumerations
enum class addr_mode_t : uint8_t {
  NONE = 0x0,      // No address
  RESERVED = 0x1,  // Reserved
  SHORT = 0x2,     // 16-bit short address
  EXTENDED = 0x3,  // 64-bit extended address
};

/// IEEE 802.15.4 frame version enumerations
enum class frame_version_t : uint8_t {
  V_2003 = 0x0,       // IEEE 802.15.4-2003
  V_2006 = 0x1,       // IEEE 802.15.4-2006
  V_RESERVED1 = 0x2,  // Reserved
  V_RESERVED2 = 0x3,  // Reserved
};

/**
 * @brief IEEE 802.15.4 Frame Control Field (FCF) structure
 * Bit fields are ordered LSB to MSB to match IEEE 802.15.4 specification
 */
struct FrameControlField {
  uint8_t frameType : 3 = 0x1;
  uint8_t securityEnabled : 1 = 0;   // Security Enabled (bit 3)
  uint8_t framePending : 1 = 0;      // Frame Pending (bit 4)
  uint8_t ackRequest : 1 = 0;        // Acknowledgment Request (bit 5)
  uint8_t panIdCompression : 1 = 0;  // PAN ID Compression (bit 6)
  uint8_t reserved : 1 = 0;          // Reserved (bit 7)
  uint8_t sequenceNumberSuppression : 1 =
      0;  // Sequence Number Suppression (bit 8)
  uint8_t informationElementsPresent : 1 =
      0;                         // Information Elements Present (bit 9)
  uint8_t destAddrMode : 2 = 0;  // Destination Address Mode (bits 10-11)
  uint8_t frameVersion : 2 =
      (uint8_t)frame_version_t::V_2006;  // Frame Version (bits 12-13)
  uint8_t srcAddrMode : 2 = 0;           // Source Address Mode (bits 14-15)
};

/**
 * @brief IEEE 802.15.4 Address abstraction.
 *
 * Represents a short (16-bit) or extended (64-bit) address for IEEE 802.15.4
 * frames. Provides constructors for both address types and utility methods for
 * access and string conversion.
 */
class Address {
 public:
  /**
   * @brief Default constructor. Initializes address mode to NONE.
   */
  Address() = default;
  /**
   * @brief Construct an Address from a pointer and mode.
   * @param addr Pointer to address bytes (2 or 8 bytes).
   * @param mode Addressing mode (SHORT or EXTENDED).
   */
  Address(const uint8_t* addr, addr_mode_t mode) : local_addr_mode(mode) {
    if (mode == addr_mode_t::SHORT) {
      memcpy(local_address, addr, 2);
    } else if (mode == addr_mode_t::EXTENDED) {
      memcpy(local_address, addr, 8);
    }
  }

  /**
   * @brief Template constructor to deduce address length and mode at compile
   * time.
   * @tparam N Address length (must be 2 or 8).
   * @param addr Array of address bytes.
   */
  template <size_t N>
  Address(const uint8_t (&addr)[N]) {
    static_assert(N == 2 || N == 8, "Address must be 2 or 8 bytes");
    if constexpr (N == 2) {
      local_addr_mode = addr_mode_t::SHORT;
      memcpy(local_address, addr, 2);
    } else if constexpr (N == 8) {
      local_addr_mode = addr_mode_t::EXTENDED;
      memcpy(local_address, addr, 8);
    }
  }

  /**
   * @brief Get a pointer to the address bytes.
   * @return Pointer to address data (2 or 8 bytes).
   */
  uint8_t* data() { return local_address; }

  /**
   * @brief Get the address mode (NONE, SHORT, EXTENDED).
   * @return Address mode.
   */
  addr_mode_t mode() { return local_addr_mode; }

  /**
   * @brief Get a human-readable string representation of the address.
   * @return Pointer to static string buffer.
   */
  const char* to_str() const { return to_str(local_address, local_addr_mode); }

  /**
   * @brief Get a human-readable string for a raw address and length.
   * @param addr Pointer to address bytes.
   * @param len Length of address (2 or 8).
   * @return Pointer to static string buffer.
   */
  static const char* to_str(const uint8_t* addr, int len) {
    static char str[25];
    if (len == 2) {
      snprintf(str, sizeof(str), "%02X:%02X", addr[0], addr[1]);
    } else if (len == 8) {
      snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
               addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6],
               addr[7]);
    } else {
      snprintf(str, sizeof(str), "Invalid");
    }
    return str;
  }

  /**
   * @brief Get a human-readable string for a raw address and mode.
   * @param addr Pointer to address bytes.
   * @param mode Address mode (NONE, SHORT, EXTENDED).
   * @return Pointer to static string buffer.
   */
  static const char* to_str(const uint8_t* addr, addr_mode_t mode) {
    switch (mode) {
      case addr_mode_t::NONE:
        return "None";
      case addr_mode_t::SHORT:
        return to_str(addr, 2);
      case addr_mode_t::EXTENDED:
        return to_str(addr, 8);
      default:
        return "Invalid";
    }
  }

 protected:
  /**
   * @brief Local address mode (NONE, SHORT, EXTENDED).
   */
  addr_mode_t local_addr_mode = addr_mode_t::NONE;
  /**
   * @brief Local address bytes (0, 2, or 8 bytes used).
   */
  uint8_t local_address[8] = {0};
};

/// IEEE 802.15.4 MAC frame structure
struct Frame {
  FrameControlField fcf{};     // Frame Control Field
  uint8_t sequenceNumber = 0;  // Sequence Number (if not suppressed)
  uint16_t destPanId = 0;      // Destination PAN ID (if present)
  uint8_t destAddress[8];      // Destination Address (0, 2, or 8 bytes)
  uint16_t srcPanId = 0;       // Source PAN ID (if present)
  uint8_t srcAddress[8];       // Source Address (0, 2, or 8 bytes)
  uint8_t destAddrLen = 0;     // Length of destination address
  uint8_t srcAddrLen = 0;      // Length of source address
  size_t payloadLen = 0;       // Length of payload
  uint8_t* payload = nullptr;  // Pointer to payload data
  uint8_t rssi_lqi = 0;        // RSSI and LQI (combined in 1 byte)

  /// parse frame from raw data
  bool parse(const uint8_t* data, bool verbose);

  /// Build IEEE 802.15.4 frame array with length byte at start and 0x00 at end
  size_t build(uint8_t* buffer, bool verbose) const;

  /// Get a human-readable string representation of the frame.
  const char* to_str(uint8_t frameType);

  /// Set the source address for the frame.
  void setSourceAddress(Address address) {
    memcpy(srcAddress, address.data(),
           address.mode() == addr_mode_t::SHORT ? 2 : 8);
    srcAddrLen = address.mode() == addr_mode_t::SHORT ? 2 : 8;
    fcf.srcAddrMode = static_cast<uint8_t>(address.mode());
  }

  /// Set the destination address for the frame.
  void setDestinationAddress(Address address) {
    memcpy(destAddress, address.data(),
           address.mode() == addr_mode_t::SHORT ? 2 : 8);
    destAddrLen = address.mode() == addr_mode_t::SHORT ? 2 : 8;
    fcf.destAddrMode = static_cast<uint8_t>(address.mode());
  }

  /// Set the payload for the frame.
  void setPayload(const uint8_t* data, size_t len) {
    if (buffer.size() < len) {
      buffer.resize(len);  // Resize buffer if needed
    }
    payload = buffer.data();
    memcpy(payload, data, len);
    payloadLen = len;
  }

  /// Defines the Personal Area Network Identifier (PAN ID) for the frame.
  void setPAN(uint16_t panId) {
    destPanId = panId;
    srcPanId = panId;
    fcf.panIdCompression = 1;  // Enable PAN ID Compression
  }

 protected:
  std::vector<uint8_t> buffer;  // Buffer for building frames
};

// Ensure FCF structure is exactly 2 bytes
ESP_STATIC_ASSERT(sizeof(FrameControlField) == IEEE802154_FCF_SIZE,
                  "ieee802154_fcf_t must be 2 bytes");

}  // namespace ieee802154