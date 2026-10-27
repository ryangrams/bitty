#pragma once

#include <atomic>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef M5FLAP_DISPLAY_QUEUE_CAPACITY
#define M5FLAP_DISPLAY_QUEUE_CAPACITY 6
#endif

namespace flappy {

struct DisplayStrip {
  enum { MaxWidth = 240, MaxRows = 8, MaxFrameRows = 240 };
  int16_t x = 0, y = 0;
  uint16_t width = 0, height = 0;
  uint16_t pixels[MaxWidth * MaxRows] = {};

  bool isFrameBoundary() const { return width == 0 && height == 0; }
  bool isBorrowed() const { return borrowed_ != nullptr; }
  uint16_t* pixelData() { return borrowed_ ? borrowed_ : pixels; }
  const uint16_t* pixelData() const { return borrowed_ ? borrowed_ : pixels; }
  void markFrameBoundary() {
    x = y = 0; width = height = 0;
    borrowed_ = nullptr;
    wireEncoded_ = false;
  }

  static bool validBorrowedFrame(const uint16_t* surface, int columns, int rows) {
    if (!surface || columns < 1 || columns > MaxWidth || rows < 1 || rows > MaxFrameRows ||
        reinterpret_cast<uintptr_t>(surface) % alignof(uint16_t)) return false;
    const size_t count = size_t(columns) * size_t(rows);
    if (count > SIZE_MAX / sizeof(uint16_t)) return false;
    const size_t bytes = count * sizeof(uint16_t);
    return reinterpret_cast<uintptr_t>(surface) <= UINTPTR_MAX - (bytes - 1);
  }

  bool borrowFrame(uint16_t* surface, int columns, int rows, bool wireOrder = false) {
    if (!validBorrowedFrame(surface, columns, rows)) return false;
    x = y = 0;
    width = uint16_t(columns); height = uint16_t(rows);
    borrowed_ = surface;
    wireEncoded_ = wireOrder;
    return true;
  }

  // The consumer converts a native slot once before its DMA transfer.
  void encodeWireOrder() {
    if (wireEncoded_ || isFrameBoundary()) return;
    uint16_t* input = pixelData();
    uint8_t* output = reinterpret_cast<uint8_t*>(input);
    const unsigned count = unsigned(width) * height;
    for (unsigned index = 0; index < count; ++index) {
      const uint16_t color = input[index];
      output[index * 2] = uint8_t(color >> 8);
      output[index * 2 + 1] = uint8_t(color);
    }
    wireEncoded_ = true;
  }

  bool copy(int left, int top, int columns, int rows, const uint16_t* source, int stride, bool wireOrder = false) {
    if (!source || columns < 1 || columns > MaxWidth || rows < 1 || rows > MaxRows ||
        left < 0 || top < 0 || left > INT16_MAX - columns || top > INT16_MAX - rows ||
        stride < columns || stride > INT16_MAX) return false;
    x = int16_t(left); y = int16_t(top);
    width = uint16_t(columns); height = uint16_t(rows);
    borrowed_ = nullptr;
    wireEncoded_ = wireOrder;
    for (int row = 0; row < rows; ++row) {
      const uint16_t* input = source + size_t(row) * unsigned(stride);
      if (wireOrder) {
        uint8_t* output = reinterpret_cast<uint8_t*>(pixels + row * columns);
        for (int col = 0; col < columns; ++col) {
          const uint16_t color = input[col];
          output[col * 2] = uint8_t(color >> 8);
          output[col * 2 + 1] = uint8_t(color);
        }
      } else memcpy(pixels + row * columns, input, size_t(columns) * sizeof(uint16_t));
    }
    return true;
  }

 private:
  uint16_t* borrowed_ = nullptr;
  bool wireEncoded_ = false;
};

// One producer and one consumer own opposite ends. A front strip remains
// reserved until complete(), including while its SPI transfer is in flight.
class DisplayStripFifo {
 public:
  enum { Capacity = M5FLAP_DISPLAY_QUEUE_CAPACITY };
  static_assert(Capacity >= 3 && Capacity <= 6, "Display queue capacity must be between 3 and 6");
  explicit DisplayStripFifo(uint32_t sequence = 0) : written_(sequence), completed_(sequence) {}

  bool tryPush(int x, int y, int width, int height, const uint16_t* pixels, int stride, bool wireOrder = false) {
    const uint32_t written = written_.load(std::memory_order_relaxed);
    if (uint32_t(written - completed_.load(std::memory_order_acquire)) >= Capacity) return false;
    if (!strips_[writeSlot_].copy(x, y, width, height, pixels, stride, wireOrder)) return false;
    writeSlot_ = uint8_t((writeSlot_ + 1u) % Capacity);
    written_.store(written + 1u, std::memory_order_release);
    return true;
  }

  bool tryPushEndFrame() {
    const uint32_t written = written_.load(std::memory_order_relaxed);
    if (uint32_t(written - completed_.load(std::memory_order_acquire)) >= Capacity) return false;
    strips_[writeSlot_].markFrameBoundary();
    writeSlot_ = uint8_t((writeSlot_ + 1u) % Capacity);
    written_.store(written + 1u, std::memory_order_release);
    return true;
  }

  bool tryPushBorrowedFrame(uint16_t* surface, int width, int height, bool wireOrder = false) {
    const uint32_t written = written_.load(std::memory_order_relaxed);
    if (uint32_t(written - completed_.load(std::memory_order_acquire)) >= Capacity) return false;
    if (!strips_[writeSlot_].borrowFrame(surface, width, height, wireOrder)) return false;
    writeSlot_ = uint8_t((writeSlot_ + 1u) % Capacity);
    written_.store(written + 1u, std::memory_order_release);
    return true;
  }

  DisplayStrip* front() {
    return const_cast<DisplayStrip*>(static_cast<const DisplayStripFifo*>(this)->front());
  }

  const DisplayStrip* front() const {
    if (completed_.load(std::memory_order_relaxed) == written_.load(std::memory_order_acquire)) return nullptr;
    return &strips_[readSlot_];
  }

  bool complete() {
    const uint32_t completed = completed_.load(std::memory_order_relaxed);
    if (completed == written_.load(std::memory_order_acquire)) return false;
    readSlot_ = uint8_t((readSlot_ + 1u) % Capacity);
    completed_.store(completed + 1u, std::memory_order_release);
    return true;
  }

  unsigned pending() const {
    const uint32_t completed = completed_.load(std::memory_order_acquire);
    const uint32_t count = written_.load(std::memory_order_acquire) - completed;
    return count > Capacity ? Capacity : unsigned(count);
  }

 private:
  DisplayStrip strips_[Capacity];
  std::atomic<uint32_t> written_, completed_;
  uint8_t writeSlot_ = 0, readSlot_ = 0;
};

}  // namespace flappy

#if defined(ARDUINO_ARCH_ESP32)
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <new>

namespace flappy {

// After begin(), drain before any main-task display operation. The worker owns
// each complete startWrite/pushImage/endWrite transaction on core 0.
class AsyncDisplay {
 public:
  struct Stats {
    uint64_t bytes = 0, workUs = 0, waitUs = 0, copyUs = 0, encodeUs = 0, frameIntervalUs = 0;
    uint64_t probeElapsedUs = 0;
    uint32_t strips = 0, submits = 0, waits = 0;
    uint32_t submittedFrames = 0, completedFrames = 0, probeIntervals = 0;
    bool probeComplete = false;
    uint32_t firstFrameUs = 0, lastFrameUs = 0;
    uint32_t minFrameUs = UINT32_MAX, maxFrameUs = 0;
    uint32_t maxWaitUs = 0, maxCopyUs = 0, maxWorkUs = 0;
    unsigned pending = 0, maxPending = 0;
  };

  AsyncDisplay() = default;
  ~AsyncDisplay() { end(); }
  AsyncDisplay(const AsyncDisplay&) = delete;
  AsyncDisplay& operator=(const AsyncDisplay&) = delete;

  bool begin() {
    if (task_) return true;
    stopping_.store(false, std::memory_order_relaxed);
    stopped_.store(false, std::memory_order_relaxed);
    void* storage = heap_caps_malloc(sizeof(DisplayStripFifo), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!storage) return false;
    fifo_ = new (storage) DisplayStripFifo;
    free_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, DisplayStripFifo::Capacity, &freeStorage_);
    ready_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, 0, &readyStorage_);
    if (!free_ || !ready_ || xTaskCreatePinnedToCore(runTask, "M5 LCD", 4096, this, 1, &task_, 0) != pdPASS) {
      if (free_) vSemaphoreDelete(free_);
      if (ready_) vSemaphoreDelete(ready_);
      free_ = ready_ = nullptr;
      task_ = nullptr;
      freeFifo();
      return false;
    }
    return true;
  }

  bool enabled() const { return task_ != nullptr; }

  bool submit(int x, int y, int width, int height, const uint16_t* pixels, int stride) {
    if (!task_ || !pixels || width < 1 || width > DisplayStrip::MaxWidth ||
        height < 1 || height > DisplayStrip::MaxRows || x < 0 || y < 0 ||
        x > INT16_MAX - width || y > INT16_MAX - height || stride < width || stride > INT16_MAX) return false;
    uint32_t waitUs = 0;
    bool waited = false;
    if (!takeFree(waited, waitUs)) return false;
    const uint32_t copyStarted = micros();
    if (!fifo_->tryPush(x, y, width, height, pixels, stride)) {
      xSemaphoreGive(free_);
      return false;
    }
    const uint32_t copyUs = uint32_t(micros() - copyStarted);
    recordEnqueue(waited, waitUs, copyUs, false);
    xSemaphoreGive(ready_);
    return true;
  }

  // Keep the complete surface alive and untouched until drain() returns. The
  // worker converts native colors in place. Set wireOrder for preencoded pixels.
  bool submitBorrowedFrame(uint16_t* surface, int width, int height, bool wireOrder = false) {
    if (!task_ || !DisplayStrip::validBorrowedFrame(surface, width, height) ||
        reinterpret_cast<uintptr_t>(surface) % 4) return false;
    const size_t bytes = size_t(width) * size_t(height) * sizeof(uint16_t);
    const uintptr_t last = reinterpret_cast<uintptr_t>(surface) + bytes - 1;
    if (!esp_ptr_dma_capable(surface) || !esp_ptr_dma_capable(reinterpret_cast<const void*>(last))) return false;
    uint32_t waitUs = 0;
    bool waited = false;
    if (!takeFree(waited, waitUs)) return false;
    if (!fifo_->tryPushBorrowedFrame(surface, width, height, wireOrder)) {
      xSemaphoreGive(free_);
      return false;
    }
    recordEnqueue(waited, waitUs, 0, false);
    xSemaphoreGive(ready_);
    return true;
  }

  // Completion is recorded after all earlier image DMA transfers have finished.
  bool submitEndFrame() {
    if (!task_) return false;
    uint32_t waitUs = 0;
    bool waited = false;
    if (!takeFree(waited, waitUs)) return false;
    if (!fifo_->tryPushEndFrame()) {
      xSemaphoreGive(free_);
      return false;
    }
    recordEnqueue(waited, waitUs, 0, true);
    xSemaphoreGive(ready_);
    return true;
  }

  // The next completed boundary starts the probe and contributes no interval.
  void startFrameProbe(uint32_t intervals = 500) {
    portENTER_CRITICAL(&statsMux_);
    stats_.probeIntervals = 0;
    stats_.probeElapsedUs = 0;
    stats_.probeComplete = intervals == 0;
    probeTarget_ = intervals;
    probeAwaitingStart_ = intervals != 0;
    portEXIT_CRITICAL(&statsMux_);
  }

  void drain() {
    while (fifo_ && fifo_->pending()) vTaskDelay(1);
  }

  Stats stats() const {
    portENTER_CRITICAL(&statsMux_);
    Stats result = stats_;
    portEXIT_CRITICAL(&statsMux_);
    result.pending = fifo_ ? fifo_->pending() : 0;
    return result;
  }

  void end() {
    if (!fifo_) return;
    drain();
    if (task_) {
      stopping_.store(true, std::memory_order_release);
      xSemaphoreGive(ready_);
      while (!stopped_.load(std::memory_order_acquire)) vTaskDelay(1);
    }
    if (free_) vSemaphoreDelete(free_);
    if (ready_) vSemaphoreDelete(ready_);
    task_ = nullptr; free_ = ready_ = nullptr;
    freeFifo();
  }

 private:
  DisplayStripFifo* fifo_ = nullptr;
  SemaphoreHandle_t free_ = nullptr, ready_ = nullptr;
  StaticSemaphore_t freeStorage_, readyStorage_;
  TaskHandle_t task_ = nullptr;
  std::atomic<bool> stopping_{false}, stopped_{false};
  mutable portMUX_TYPE statsMux_ = portMUX_INITIALIZER_UNLOCKED;
  Stats stats_;
  uint32_t probeTarget_ = 0;
  bool probeAwaitingStart_ = false;

  bool takeFree(bool& waited, uint32_t& waitUs) {
    waited = xSemaphoreTake(free_, 0) != pdTRUE;
    if (!waited) return true;
    const uint32_t started = micros();
    if (xSemaphoreTake(free_, portMAX_DELAY) != pdTRUE) return false;
    waitUs = uint32_t(micros() - started);
    return true;
  }

  void recordEnqueue(bool waited, uint32_t waitUs, uint32_t copyUs, bool boundary) {
    const unsigned pending = fifo_->pending();
    portENTER_CRITICAL(&statsMux_);
    if (boundary) ++stats_.submittedFrames;
    else ++stats_.submits;
    stats_.waits += waited;
    stats_.waitUs += waitUs;
    stats_.copyUs += copyUs;
    if (waitUs > stats_.maxWaitUs) stats_.maxWaitUs = waitUs;
    if (copyUs > stats_.maxCopyUs) stats_.maxCopyUs = copyUs;
    if (pending > stats_.maxPending) stats_.maxPending = pending;
    portEXIT_CRITICAL(&statsMux_);
  }

  void freeFifo() {
    if (!fifo_) return;
    fifo_->~DisplayStripFifo();
    heap_caps_free(fifo_);
    fifo_ = nullptr;
  }

  static void runTask(void* context) { static_cast<AsyncDisplay*>(context)->run(); }

  void run() {
    for (;;) {
      if (xSemaphoreTake(ready_, portMAX_DELAY) != pdTRUE) continue;
      if (stopping_.load(std::memory_order_acquire)) {
        stopped_.store(true, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
      }
      DisplayStrip* strip = fifo_->front();
      if (!strip) continue;
      if (strip->isFrameBoundary()) {
        portENTER_CRITICAL(&statsMux_);
        const uint32_t stamp = micros();
        if (!stats_.completedFrames) stats_.firstFrameUs = stamp;
        else {
          const uint32_t interval = stamp - stats_.lastFrameUs;
          stats_.frameIntervalUs += interval;
          if (interval < stats_.minFrameUs) stats_.minFrameUs = interval;
          if (interval > stats_.maxFrameUs) stats_.maxFrameUs = interval;
        }
        if (probeTarget_) {
          if (probeAwaitingStart_) probeAwaitingStart_ = false;
          else {
            stats_.probeElapsedUs += uint32_t(stamp - stats_.lastFrameUs);
            if (++stats_.probeIntervals == probeTarget_) {
              stats_.probeComplete = true;
              probeTarget_ = 0;
            }
          }
        }
        stats_.lastFrameUs = stamp;
        ++stats_.completedFrames;
        portEXIT_CRITICAL(&statsMux_);
      } else {
        const uint32_t started = micros();
        strip->encodeWireOrder();
        const uint32_t encodeUs = uint32_t(micros() - started);
        M5.Display.startWrite();
        M5.Display.pushImageDMA(strip->x, strip->y, strip->width, strip->height,
            reinterpret_cast<const lgfx::swap565_t*>(strip->pixelData()));
        M5.Display.endWrite();
        const uint32_t workUs = uint32_t(micros() - started);
        portENTER_CRITICAL(&statsMux_);
        ++stats_.strips;
        stats_.bytes += uint32_t(strip->width) * strip->height * sizeof(uint16_t);
        stats_.workUs += workUs;
        stats_.encodeUs += encodeUs;
        if (workUs > stats_.maxWorkUs) stats_.maxWorkUs = workUs;
        portEXIT_CRITICAL(&statsMux_);
      }
      fifo_->complete();
      xSemaphoreGive(free_);
    }
  }
};

}  // namespace flappy
#endif
