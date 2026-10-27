#pragma once

#include <atomic>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {

struct DisplayStrip {
  enum { MaxWidth = 240, MaxRows = 8 };
  int16_t x = 0, y = 0;
  uint16_t width = 0, height = 0;
  uint16_t pixels[MaxWidth * MaxRows] = {};

  bool copy(int left, int top, int columns, int rows, const uint16_t* source, int stride) {
    if (!source || columns < 1 || columns > MaxWidth || rows < 1 || rows > MaxRows ||
        left < 0 || top < 0 || left > INT16_MAX - columns || top > INT16_MAX - rows ||
        stride < columns || stride > INT16_MAX) return false;
    x = int16_t(left); y = int16_t(top);
    width = uint16_t(columns); height = uint16_t(rows);
    for (int row = 0; row < rows; ++row)
      memcpy(pixels + row * columns, source + size_t(row) * unsigned(stride), size_t(columns) * sizeof(uint16_t));
    return true;
  }
};

// One producer and one consumer own opposite ends. A front strip remains
// reserved until complete(), including while its SPI transfer is in flight.
class DisplayStripFifo {
 public:
  enum { Capacity = 3 };
  explicit DisplayStripFifo(uint32_t sequence = 0) : written_(sequence), completed_(sequence) {}

  bool tryPush(int x, int y, int width, int height, const uint16_t* pixels, int stride) {
    const uint32_t written = written_.load(std::memory_order_relaxed);
    if (uint32_t(written - completed_.load(std::memory_order_acquire)) >= Capacity) return false;
    if (!strips_[writeSlot_].copy(x, y, width, height, pixels, stride)) return false;
    writeSlot_ = uint8_t((writeSlot_ + 1u) % Capacity);
    written_.store(written + 1u, std::memory_order_release);
    return true;
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
    uint64_t bytes = 0, workUs = 0;
    uint32_t strips = 0;
    unsigned pending = 0;
  };

  AsyncDisplay() = default;
  ~AsyncDisplay() { end(); }
  AsyncDisplay(const AsyncDisplay&) = delete;
  AsyncDisplay& operator=(const AsyncDisplay&) = delete;

  bool begin() {
    if (task_) return true;
    stopping_.store(false, std::memory_order_relaxed);
    stopped_.store(false, std::memory_order_relaxed);
    fifo_ = new (std::nothrow) DisplayStripFifo;
    if (!fifo_) return false;
    free_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, DisplayStripFifo::Capacity, &freeStorage_);
    ready_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, 0, &readyStorage_);
    if (!free_ || !ready_ || xTaskCreatePinnedToCore(runTask, "M5 LCD", 4096, this, 1, &task_, 0) != pdPASS) {
      if (free_) vSemaphoreDelete(free_);
      if (ready_) vSemaphoreDelete(ready_);
      free_ = ready_ = nullptr;
      task_ = nullptr;
      delete fifo_; fifo_ = nullptr;
      return false;
    }
    return true;
  }

  bool enabled() const { return task_ != nullptr; }

  bool submit(int x, int y, int width, int height, const uint16_t* pixels, int stride) {
    if (!task_ || !pixels || width < 1 || width > DisplayStrip::MaxWidth ||
        height < 1 || height > DisplayStrip::MaxRows || x < 0 || y < 0 ||
        x > INT16_MAX - width || y > INT16_MAX - height || stride < width || stride > INT16_MAX) return false;
    if (xSemaphoreTake(free_, portMAX_DELAY) != pdTRUE) return false;
    if (!fifo_->tryPush(x, y, width, height, pixels, stride)) {
      xSemaphoreGive(free_);
      return false;
    }
    xSemaphoreGive(ready_);
    return true;
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
    delete fifo_; fifo_ = nullptr;
  }

 private:
  DisplayStripFifo* fifo_ = nullptr;
  SemaphoreHandle_t free_ = nullptr, ready_ = nullptr;
  StaticSemaphore_t freeStorage_, readyStorage_;
  TaskHandle_t task_ = nullptr;
  std::atomic<bool> stopping_{false}, stopped_{false};
  mutable portMUX_TYPE statsMux_ = portMUX_INITIALIZER_UNLOCKED;
  Stats stats_;

  static void runTask(void* context) { static_cast<AsyncDisplay*>(context)->run(); }

  void run() {
    for (;;) {
      if (xSemaphoreTake(ready_, portMAX_DELAY) != pdTRUE) continue;
      if (stopping_.load(std::memory_order_acquire)) {
        stopped_.store(true, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
      }
      const DisplayStrip* strip = fifo_->front();
      if (!strip) continue;
      const uint32_t started = micros();
      M5.Display.startWrite();
      M5.Display.pushImage(strip->x, strip->y, strip->width, strip->height, strip->pixels);
      M5.Display.endWrite();
      portENTER_CRITICAL(&statsMux_);
      ++stats_.strips;
      stats_.bytes += uint32_t(strip->width) * strip->height * sizeof(uint16_t);
      stats_.workUs += uint32_t(micros() - started);
      portEXIT_CRITICAL(&statsMux_);
      fifo_->complete();
      xSemaphoreGive(free_);
    }
  }
};

}  // namespace flappy
#endif
