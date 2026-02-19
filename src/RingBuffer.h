#pragma once
#include <freertos/stream_buffer.h>

#include "freertos/FreeRTOS.h"
#define ESP32X

namespace ieee802154 {

/**
 * @brief Efficient ring buffer for storing frame data.
 *
 * Implements a FIFO circular buffer for byte storage and retrieval.
 * Used internally by ESP32TransceiverStream for TX and RX buffering.
 * Supports push, read, peek, and bulk operations with minimal memory movement.
 *
 * - Data is written at the tail and read from the head.
 * - Buffer automatically wraps around when full.
 * - Provides methods for available space, bulk read/write, and peeking.
 *
 * @copyright GPLv3
 */
class RingBuffer {
public:
  RingBuffer(int size = 128) {
    resize(size);
  }

  void resize(size_t new_size) {
    buffer.resize(new_size);
    capacity = new_size;
    clear();
  }

  bool write(uint8_t byte) {
    if (isFull()) return false;
    buffer[tail] = byte;
    tail = (tail + 1) % capacity;
    ++count;
    return true;
  }

  int writeArray(const uint8_t* data, size_t len) {
    int written = 0;
    for (size_t i = 0; i < len; ++i) {
      if (write(data[i])) {
        ++written;
      } else {
        break;
      }
    }
    return written;
  }


  int available() const {
    return count;
  }

  void clear() {
    head = 0;
    tail = 0;
    count = 0;
  }

  bool isFull() const {
    return count == capacity;
  }

  bool isEmpty() const {
    return count == 0;
  } 

  size_t size() const {
    return capacity;
  }

  int read() {
    if (available() > 0) {
      uint8_t byte = buffer[head];
      head = (head + 1) % capacity;
      --count;
      return byte;
    }
    return 0;  // No data available
  }

  // Read up to len bytes into dest, returns number of bytes read
  int readArray(uint8_t* dest, size_t len) {
    int n = 0;
    while (n < (int)len && available() > 0) {
      dest[n++] = read();
    }
    return n;
  }

  // Peek at the next byte without removing it
  bool peek(uint8_t& out) const {
    if (available() > 0) {
      out = buffer[head];
      return true;
    }
    return false;
  }

  // Returns available space for writing
  int availableForWrite() const {
    return capacity - count;
  }

private:
  std::vector<uint8_t> buffer;
  size_t capacity = 0;
  size_t head = 0;
  size_t tail = 0;
  int count = 0;
};

}  // namespace ieee802154