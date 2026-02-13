#pragma once
#include <freertos/stream_buffer.h>

#include "freertos/FreeRTOS.h"
#define ESP32X

namespace ieee802154 {

/**
 * @brief Simple byte buffer for storing frame data.
 *
 * Used internally by ESP32TransceiverStream for TX buffering.
 * @copyright GPLv3 *
 * @tparam N Maximum size of the buffer in bytes.
 */
template <size_t N = 128>
class Buffer {
 public:
  Buffer() : frame_buffer_len(0) {}

  bool push(uint8_t byte) {
    if (frame_buffer_len < N) {
      frame_buffer[frame_buffer_len++] = byte;
      return true;
    }
    return false;
  }

  uint8_t* data() { return frame_buffer; }

  int available() { return frame_buffer_len; }

  void clear(int n) {
    if (n <= frame_buffer_len) {
      memmove(frame_buffer, frame_buffer + n, frame_buffer_len - n);
      frame_buffer_len -= n;
    } else {
      clear();
    }
  }

  void clear() { frame_buffer_len = 0; }

  bool isFull() { return frame_buffer_len >= N; }

 private:
  uint8_t frame_buffer[N];      // Buffer for incoming frames
  size_t frame_buffer_len = 0;  // Length of data in the buffer
};

/**
 * @brief Buffer implementation which is using a FreeRTOS StreamBuffer.
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T>
class BufferRTOS {
 public:
  BufferRTOS(size_t streamBufferSize, size_t xTriggerLevel = 1,
             TickType_t writeMaxWait = portMAX_DELAY,
             TickType_t readMaxWait = portMAX_DELAY) {
    readWait = readMaxWait;
    writeWait = writeMaxWait;
    current_size_bytes = (streamBufferSize + 1) * sizeof(T);
    trigger_level = xTriggerLevel;

    if (streamBufferSize > 0) {
      setup();
    }
  }

  ~BufferRTOS() { end(); }

  /// Re-Allocats the memory and the queue
  bool resize(size_t size) {
    bool result = true;
    int req_size_bytes = (size + 1) * sizeof(T);
    if (current_size_bytes != req_size_bytes) {
      end();
      current_size_bytes = req_size_bytes;
      result = setup();
    }
    return result;
  }

  void setReadMaxWait(TickType_t ticks) { readWait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { writeWait = ticks; }

  void setWriteFromISR(bool active) { write_from_isr = active; }

  void setReadFromISR(bool active) { read_from_isr = active; }

  // reads a single value
  bool read(T& result) {
    T data = 0;
    return readArray(&data, 1) == 1;
  }

  // reads multiple values
  int readArray(T data[], int len) {
    // do not block if there is no data available
    if (isEmpty()) return 0;

    if (read_from_isr) {
      xHigherPriorityTaskWoken = pdFALSE;
      int result = xStreamBufferReceiveFromISR(xStreamBuffer, (void*)data,
                                               sizeof(T) * len,
                                               &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR();
#else
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
      return result / sizeof(T);
    } else {
      return xStreamBufferReceive(xStreamBuffer, (void*)data, sizeof(T) * len,
                                  readWait) /
             sizeof(T);
    }
  }

  int writeArray(const T data[], int len) {
    ESP_LOGD(TAG, "writeArray: %d", len);
    if (write_from_isr) {
      xHigherPriorityTaskWoken = pdFALSE;
      int result =
          xStreamBufferSendFromISR(xStreamBuffer, (void*)data, sizeof(T) * len,
                                   &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR();
#else
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
      return result / sizeof(T);
    } else {
      return xStreamBufferSend(xStreamBuffer, (void*)data, sizeof(T) * len,
                               writeWait) /
             sizeof(T);
    }
  }

  // peeks the actual entry from the buffer
  bool peek(T& result) {
    ESP_LOGE(TAG, "peek not implemented");
    return false;
  }

  // checks if the buffer is full
  bool isFull() { return xStreamBufferIsFull(xStreamBuffer) == pdTRUE; }

  bool isEmpty() { return xStreamBufferIsEmpty(xStreamBuffer) == pdTRUE; }

  // write add an entry to the buffer
  bool write(T data) {
    int len = sizeof(T);
    return writeArray(&data, len) == len;
  }

  // clears the buffer
  void reset() { xStreamBufferReset(xStreamBuffer); }

  // provides the number of entries that are available to read
  int available() {
    return xStreamBufferBytesAvailable(xStreamBuffer) / sizeof(T);
  }

  // provides the number of entries that are available to write
  int availableForWrite() {
    return xStreamBufferSpacesAvailable(xStreamBuffer) / sizeof(T);
  }

  size_t size() { return current_size_bytes / sizeof(T); }

  operator bool() { return xStreamBuffer != nullptr && size() > 0; }

 protected:
  StreamBufferHandle_t xStreamBuffer = nullptr;
  StaticStreamBuffer_t static_stream_buffer;
  uint8_t* p_data = nullptr;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // Initialised to pdFALSE.
  int readWait = portMAX_DELAY;
  int writeWait = portMAX_DELAY;
  bool read_from_isr = false;
  bool write_from_isr = false;
  size_t current_size_bytes = 0;
  size_t trigger_level = 0;
  const char* TAG = "BufferRTOS";

  /// The allocation has been postponed to be done here, so that we can e.g. use
  /// psram
  bool setup() {
    if (current_size_bytes == 0) return true;

    // allocate data if necessary
    int size = (current_size_bytes + 1) * sizeof(T);
    if (p_data == nullptr) {
      p_data = (uint8_t*)new T[size];
      // check allocation
      if (p_data == nullptr) {
        ESP_LOGE(TAG, "allocate failed for %d bytes", size);
        return false;
      }
    }

    // create stream buffer if necessary
    if (xStreamBuffer == nullptr) {
      xStreamBuffer = xStreamBufferCreateStatic(
          current_size_bytes, trigger_level, p_data, &static_stream_buffer);
    }
    if (xStreamBuffer == nullptr) {
      ESP_LOGE(TAG, "xStreamBufferCreateStatic failed");
      static constexpr const char* TAG = "BufferRTOS";
      return false;
    }
    // make sure that the data is empty
    reset();
    return true;
  }

  /// Release resurces: call resize to restart again
  void end() {
    if (xStreamBuffer != nullptr) vStreamBufferDelete(xStreamBuffer);
    delete[] (p_data);
    current_size_bytes = 0;
    p_data = nullptr;
    xStreamBuffer = nullptr;
  }
};

}  // namespace ieee802154