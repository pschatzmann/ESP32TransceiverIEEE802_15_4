#include "Frame.h"

#include <esp_log.h>
#include <string.h>

namespace ieee802154 {

static const char* TAG = "IEEE802154";

// Internal: Process FCF for debugging
static void process_fcf(const FrameControlField* fcf, bool verbose) {
  if (!verbose) return;

  // Check Frame Type
  switch (fcf->frameType) {
    case static_cast<uint8_t>(Frameype_t::BEACON):
      ESP_LOGI(TAG, "Frame type: Beacon");
      break;
    case static_cast<uint8_t>(Frameype_t::DATA):
      ESP_LOGI(TAG, "Frame type: Data");
      break;
    case static_cast<uint8_t>(Frameype_t::ACK):
      ESP_LOGI(TAG, "Frame type: ACK");
      break;
    case static_cast<uint8_t>(Frameype_t::MAC_CMD):
      ESP_LOGI(TAG, "Frame type: MAC Command");
      break;
    default:
      ESP_LOGW(TAG, "Frame type: Reserved (0x%x)", fcf->frameType);
      break;
  }

  // Check Destination Addressing Mode
  switch (fcf->destAddrMode) {
    case static_cast<uint8_t>(addr_mode_t::NONE):
      ESP_LOGI(TAG, "Destination address: None");
      break;
    case static_cast<uint8_t>(addr_mode_t::SHORT):
      ESP_LOGI(TAG, "Destination address: 16-bit short");
      break;
    case static_cast<uint8_t>(addr_mode_t::EXTENDED):
      ESP_LOGI(TAG, "Destination address: 64-bit extended");
      break;
    default:
      ESP_LOGW(TAG, "Destination address: Reserved (0x%x)", fcf->destAddrMode);
      break;
  }

  // Check Source Addressing Mode
  switch (fcf->srcAddrMode) {
    case static_cast<uint8_t>(addr_mode_t::NONE):
      ESP_LOGI(TAG, "Source address: None");
      break;
    case static_cast<uint8_t>(addr_mode_t::SHORT):
      ESP_LOGI(TAG, "Source address: 16-bit short");
      break;
    case static_cast<uint8_t>(addr_mode_t::EXTENDED):
      ESP_LOGI(TAG, "Source address: 64-bit extended");
      break;
    default:
      ESP_LOGW(TAG, "Source address: Reserved (0x%x)", fcf->srcAddrMode);
      break;
  }

  // Check Frame Version
  switch (fcf->frameVersion) {
    case static_cast<uint8_t>(frame_version_t::V_2003):
      ESP_LOGI(TAG, "Frame version: IEEE 802.15.4-2003");
      break;
    case static_cast<uint8_t>(frame_version_t::V_2006):
      ESP_LOGI(TAG, "Frame version: IEEE 802.15.4-2006");
      break;
    case static_cast<uint8_t>(frame_version_t::V_RESERVED1):
    case static_cast<uint8_t>(frame_version_t::V_RESERVED2):
      ESP_LOGW(TAG, "Frame version: Reserved (0x%x)", fcf->frameVersion);
      break;
    default:
      ESP_LOGW(TAG, "Frame version: Unknown (0x%x)", fcf->frameVersion);
      break;
  }

  // Check 1-bit fields (log both 1 and 0 states)
  if (fcf->securityEnabled) {
    ESP_LOGI(TAG, "Security: Enabled");
  } else {
    ESP_LOGI(TAG, "Security: Disabled");
  }

  if (fcf->framePending) {
    ESP_LOGI(TAG, "Frame pending: Yes");
  } else {
    ESP_LOGI(TAG, "Frame pending: No");
  }

  if (fcf->ackRequest) {
    ESP_LOGI(TAG, "Acknowledgment: Requested");
  } else {
    ESP_LOGI(TAG, "Acknowledgment: Not requested");
  }

  if (fcf->panIdCompression) {
    ESP_LOGI(TAG, "PAN ID compression: Enabled");
  } else {
    ESP_LOGI(TAG, "PAN ID compression: Disabled");
  }

  if (fcf->sequenceNumberSuppression) {
    ESP_LOGI(TAG, "Sequence number suppression: Enabled");
  } else {
    ESP_LOGI(TAG, "Sequence number suppression: Disabled");
  }

  if (fcf->informationElementsPresent) {
    ESP_LOGI(TAG, "Information elements: Present");
  } else {
    ESP_LOGI(TAG, "Information elements: Not present");
  }

  if (fcf->reserved) {
    ESP_LOGI(TAG, "Reserved bit: Set");
  } else {
    ESP_LOGI(TAG, "Reserved bit: Cleared");
  }
}

// Parse IEEE 802.15.4 frame
bool Frame::parse(const uint8_t* data, bool verbose) {
  Frame* frame = this;
  if (!data || !frame) {
    return false;
  }

  // Get frame length from first byte (excluding trailing 0x00)
  size_t frame_len = data[0] - 1;
  size_t offset = 1;  // Skip length byte

  // Parse FCF
  if (offset + IEEE802154_FCF_SIZE > frame_len) {
    return false;
  }
  memcpy(&frame->fcf, data + offset, IEEE802154_FCF_SIZE);
  offset += IEEE802154_FCF_SIZE;

  // Process FCF for debugging
  process_fcf(&frame->fcf, verbose);

  // Parse Sequence Number
  if (!frame->fcf.sequenceNumberSuppression) {
    if (offset + 1 > frame_len) {
      return false;
    }
    frame->sequenceNumber = data[offset];
    if (verbose) {
      ESP_LOGI(TAG, "Sequence number: 0x%02x", frame->sequenceNumber);
    }
    offset += 1;
  } else {
    frame->sequenceNumber = 0;  // Not present
    if (verbose) {
      ESP_LOGI(TAG, "Sequence number: Suppressed");
    }
  }

  // Parse Destination PAN ID
  bool hasDestAddr = (frame->fcf.destAddrMode != static_cast<uint8_t>(addr_mode_t::NONE));
  bool hasSrcAddr = (frame->fcf.srcAddrMode != static_cast<uint8_t>(addr_mode_t::NONE));
  if (hasDestAddr) {
    if (offset + IEEE802154_PAN_ID_LEN > frame_len) {
      return false;
    }
    frame->destPanId = (data[offset + 1] << 8) | data[offset];  // Little-endian
    if (verbose) {
      ESP_LOGI(TAG, "Destination PAN ID: 0x%04x", frame->destPanId);
    }
    offset += IEEE802154_PAN_ID_LEN;
  } else {
    frame->destPanId = 0;
    if (verbose) {
      ESP_LOGI(TAG, "Destination PAN ID: None");
    }
  }

  // Parse Destination Address
  size_t destAddrLen = 0;
  if (frame->fcf.destAddrMode == static_cast<uint8_t>(addr_mode_t::SHORT)) {
    destAddrLen = 2;
  } else if (frame->fcf.destAddrMode == static_cast<uint8_t>(addr_mode_t::EXTENDED)) {
    destAddrLen = 8;
  }
  frame->destAddrLen = destAddrLen;  // Set destination address length
  if (destAddrLen > 0) {
    if (offset + destAddrLen > frame_len) {
      return false;
    }
    memcpy(frame->destAddress, data + offset, destAddrLen);
    if (verbose) {
      if (destAddrLen == 2) {
        uint16_t addr = (frame->destAddress[1] << 8) | frame->destAddress[0];
        ESP_LOGI(TAG, "Destination address: 0x%04x (short)", addr);
      } else {
        ESP_LOGI(TAG,
                 "Destination address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
                 "(extended)",
                 frame->destAddress[0], frame->destAddress[1],
                 frame->destAddress[2], frame->destAddress[3],
                 frame->destAddress[4], frame->destAddress[5],
                 frame->destAddress[6], frame->destAddress[7]);
      }
    }
    offset += destAddrLen;
  } else {
    memset(frame->destAddress, 0, IEEE802154_MAX_ADDR_LEN);
  }

  // Parse Source PAN ID
  if (hasSrcAddr && !frame->fcf.panIdCompression) {
    if (offset + IEEE802154_PAN_ID_LEN > frame_len) {
      return false;
    }
    frame->srcPanId = (data[offset + 1] << 8) | data[offset];  // Little-endian
    if (verbose) {
      ESP_LOGI(TAG, "Source PAN ID: 0x%04x", frame->srcPanId);
    }
    offset += IEEE802154_PAN_ID_LEN;
  } else if (hasSrcAddr && frame->fcf.panIdCompression) {
    frame->srcPanId = frame->destPanId;  // PAN ID compression
    if (verbose) {
      ESP_LOGI(TAG, "Source PAN ID: 0x%04x (compressed)", frame->srcPanId);
    }
  } else {
    frame->srcPanId = 0;
    if (verbose) {
      ESP_LOGI(TAG, "Source PAN ID: None");
    }
  }

  // Parse Source Address
  size_t srcAddrLen = 0;
  if (frame->fcf.srcAddrMode == static_cast<uint8_t>(addr_mode_t::SHORT)) {
    srcAddrLen = 2;
  } else if (frame->fcf.srcAddrMode == static_cast<uint8_t>(addr_mode_t::EXTENDED)) {
    srcAddrLen = 8;
  }
  frame->srcAddrLen = srcAddrLen;  // Set source address length
  if (srcAddrLen > 0) {
    if (offset + srcAddrLen > frame_len) {
      return false;
    }
    memcpy(frame->srcAddress, data + offset, srcAddrLen);
    if (verbose) {
      if (srcAddrLen == 2) {
        uint16_t addr = (frame->srcAddress[1] << 8) | frame->srcAddress[0];
        ESP_LOGI(TAG, "Source address: 0x%04x (short)", addr);
      } else {
        ESP_LOGI(TAG,
                 "Source address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
                 "(extended)",
                 frame->srcAddress[0], frame->srcAddress[1],
                 frame->srcAddress[2], frame->srcAddress[3],
                 frame->srcAddress[4], frame->srcAddress[5],
                 frame->srcAddress[6], frame->srcAddress[7]);
      }
    }
    offset += srcAddrLen;
  } else {
    memset(frame->srcAddress, 0, IEEE802154_MAX_ADDR_LEN);
  }

  // Parse Payload
  frame->payloadLen = frame_len - offset;
  frame->payload = (frame->payloadLen > 0) ? (uint8_t*)(data + offset) : NULL;
  if (verbose) {
    ESP_LOGI(TAG, "Payload length: %zu bytes", frame->payloadLen);
    if (frame->payloadLen > 0) {
      ESP_LOG_BUFFER_HEX(TAG, frame->payload,
                         frame->payloadLen > 16 ? 16 : frame->payloadLen);
    }
  }

  return true;
}

// Build IEEE 802.15.4 frame with length byte at start and 0x00 at end
size_t Frame::build(uint8_t* buffer, bool verbose) const {
  const Frame* frame = this;
  if (!frame || !buffer) {
    ESP_LOGE(TAG, "Invalid input");
    return 0;
  }

  if (verbose) {
    ESP_LOGI(TAG,
             "FCF: frameType=%d, securityEnabled=%d, framePending=%d, "
             "ackRequest=%d, panIdCompression=%d, reserved=%d, "
             "sequenceNumberSuppression=%d, informationElementsPresent=%d, "
             "destAddrMode=%d, frameVersion=%d, srcAddrMode=%d",
             frame->fcf.frameType, frame->fcf.securityEnabled,
             frame->fcf.framePending, frame->fcf.ackRequest,
             frame->fcf.panIdCompression, frame->fcf.reserved,
             frame->fcf.sequenceNumberSuppression,
             frame->fcf.informationElementsPresent, frame->fcf.destAddrMode,
             frame->fcf.frameVersion, frame->fcf.srcAddrMode);
  }

  size_t offset = 1;  // Reserve space for length byte

  // Write FCF
  memcpy(buffer + offset, &frame->fcf, IEEE802154_FCF_SIZE);
  offset += IEEE802154_FCF_SIZE;

  // Write Sequence Number
  if (!frame->fcf.sequenceNumberSuppression) {
    buffer[offset] = frame->sequenceNumber;
    offset += 1;
  }

  // Write Destination PAN ID
  bool hasDestAddr = (frame->fcf.destAddrMode != static_cast<uint8_t>(addr_mode_t::NONE));
  bool hasSrcAddr = (frame->fcf.srcAddrMode != static_cast<uint8_t>(addr_mode_t::NONE));
  if (hasDestAddr) {
    buffer[offset] = frame->destPanId & 0xFF;
    buffer[offset + 1] = (frame->destPanId >> 8) & 0xFF;
    offset += IEEE802154_PAN_ID_LEN;
  }

  // Write Destination Address
  size_t destAddrLen = 0;
  if (frame->fcf.destAddrMode == static_cast<uint8_t>(addr_mode_t::SHORT)) {
    destAddrLen = 2;
  } else if (frame->fcf.destAddrMode == static_cast<uint8_t>(addr_mode_t::EXTENDED)) {
    destAddrLen = 8;
  }
  if (destAddrLen > 0) {
    memcpy(buffer + offset, frame->destAddress, destAddrLen);
    offset += destAddrLen;
  }

  // Write Source PAN ID
  if (hasSrcAddr && !frame->fcf.panIdCompression) {
    buffer[offset] = frame->srcPanId & 0xFF;
    buffer[offset + 1] = (frame->srcPanId >> 8) & 0xFF;
    offset += IEEE802154_PAN_ID_LEN;
  }

  // Write Source Address
  size_t srcAddrLen = 0;
  if (frame->fcf.srcAddrMode == static_cast<uint8_t>(addr_mode_t::SHORT)) {
    srcAddrLen = 2;
  } else if (frame->fcf.srcAddrMode == static_cast<uint8_t>(addr_mode_t::EXTENDED)) {
    srcAddrLen = 8;
  }
  if (srcAddrLen > 0) {
    memcpy(buffer + offset, frame->srcAddress, srcAddrLen);
    offset += srcAddrLen;
  }

  // Write Payload
  if (frame->payloadLen > 0 && frame->payload) {
    memcpy(buffer + offset, frame->payload, frame->payloadLen);
    offset += frame->payloadLen;
  }

  // Write trailing 0x00
  buffer[offset] = 0x00;
  offset += 1;

  // Write length byte at start (total length including length byte and trailing
  // 0x00)
  buffer[0] = offset;

  if (verbose) {
    ESP_LOGI(TAG, "Built frame of %zu bytes", offset);
  }
  return offset;
}

// Debug function to convert frame type to string
const char* Frame::to_str(uint8_t frameType) {
  switch (frameType) {
    case static_cast<uint8_t>(Frameype_t::BEACON):
      return "Beacon";
    case static_cast<uint8_t>(Frameype_t::DATA):
      return "Data";
    case static_cast<uint8_t>(Frameype_t::ACK):
      return "ACK";
    case static_cast<uint8_t>(Frameype_t::MAC_CMD):
      return "MAC Command";
    default:
      return "Reserved";
  }
}

}  // namespace ieee802154