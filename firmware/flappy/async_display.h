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

// A continuous 16-bit read of command 0x45 captures the panel's scanline.
// Continuous samples qualify the period and active row cadence before DMA.
class DisplayScanProbe {
 public:
  static uint32_t lineFromWireWord(uint32_t word) {
    if (word > 0xffffu) return 512;
    const uint16_t line = uint16_t((word >> 8) | (word << 8));
    return line < 512 ? line : 512;
  }
  static uint32_t phaseFromWireWord(uint32_t word) {
    const uint32_t line = lineFromWireWord(word);
    return line < 512 ? unsigned(line >> 8) : 2;
  }
  bool observeLine(uint32_t line, uint32_t nowUs) {
    requireLines_ = true;
    if (failed_ || complete()) return complete();
    if (line > 351 || (haveLine_ && line < previousLine_ &&
        (previousLine_ < 300 || line > 31))) {
      failed_ = true;
      return false;
    }
    if (haveLine_ && line < previousLine_) rowAnchor_ = false;
    if (line >= 64 && line <= 80 && !rowAnchor_) {
      rowAnchor_ = true;
      rowMeasured_ = false;
      anchorLine_ = line;
      anchorUs_ = nowUs;
    }
    if (rowAnchor_ && !rowMeasured_ && line >= 224 && line <= 280) {
      const uint32_t rows = line - anchorLine_;
      const uint32_t elapsed = nowUs - anchorUs_;
      // Counter quantization costs one row. Read timing gets another 20 us.
      if (elapsed < (rows + 1) * 46 + 20 || elapsed + 20 > (rows - 1) * 52) {
        failed_ = true;
        return false;
      }
      const uint32_t minimum = (elapsed - 20) * 1000 / (rows + 1);
      const uint32_t maximum = ((elapsed + 20) * 1000 + rows - 2) / (rows - 1);
      if (!rowSamples_ || minimum < rowMinNs_) rowMinNs_ = minimum;
      if (maximum > rowMaxNs_) rowMaxNs_ = maximum;
      ++rowSamples_;
      rowMeasured_ = true;
    }
    if (line > maxLine_) maxLine_ = line;
    previousLine_ = line;
    haveLine_ = true;
    return observe(line >> 8, nowUs);
  }
  bool observe(uint32_t phase, uint32_t nowUs) {
    if (failed_ || complete()) return complete();
    if (phase > 1 || (sampled_ && uint32_t(nowUs - previousSample_) > 1000)) {
      failed_ = true;
      return false;
    }
    previousSample_ = nowUs;
    if (!sampled_) { sampled_ = true; high_ = phase; return false; }
    if (high_ == bool(phase)) return false;
    if (haveTransition_) {
      const uint32_t duration = nowUs - transitionUs_;
      if (high_) { highUs_ = duration; haveHigh_ = true; }
      else { lowUs_ = duration; haveLow_ = true; }
    }
    transitionUs_ = nowUs;
    haveTransition_ = true;
    high_ = phase;
    if (high_) {
      if (haveRise_ && haveLow_ && haveHigh_) {
        periodUs_ = nowUs - riseUs_;
        if (periodUs_ < (requireLines_ ? 16000u : 14000u) ||
            periodUs_ > (requireLines_ ? 17500u : 20000u) || lowUs_ < 12500 || lowUs_ > 19000 ||
            highUs_ < 1000 || highUs_ > 6500) failed_ = true;
        else ++cycles_;
      }
      riseUs_ = nowUs;
      haveRise_ = true;
    }
    return complete();
  }
  bool complete() const {
    return cycles_ >= 2 && !failed_ && (!requireLines_ || (rowSamples_ >= 2 && maxLine_ >= 320));
  }
  bool failed() const { return failed_; }
  uint32_t periodUs() const { return periodUs_; }
  uint32_t lowUs() const { return lowUs_; }
  uint32_t highUs() const { return highUs_; }
  uint32_t rowMinNs() const { return rowMinNs_; }
  uint32_t rowMaxNs() const { return rowMaxNs_; }

 private:
  uint32_t previousSample_ = 0, transitionUs_ = 0, riseUs_ = 0;
  uint32_t periodUs_ = 0, lowUs_ = 0, highUs_ = 0;
  uint32_t anchorUs_ = 0, rowMinNs_ = 0, rowMaxNs_ = 0;
  uint16_t previousLine_ = 0, anchorLine_ = 0, maxLine_ = 0;
  uint8_t cycles_ = 0;
  uint8_t rowSamples_ = 0;
  bool sampled_ = false, high_ = false, haveTransition_ = false, haveRise_ = false;
  bool haveLow_ = false, haveHigh_ = false, failed_ = false;
  bool requireLines_ = false, haveLine_ = false, rowAnchor_ = false, rowMeasured_ = false;
};

class DisplayScanGate {
 public:
  enum class Result : uint8_t { Waiting, Ready, Invalid, TimedOut };
  static constexpr uint32_t TimeoutUs = 20000;
  static constexpr uint32_t FirstLine = 64, LastLine = 320;
  static constexpr uint32_t MaxDecisionAgeUs = 250, MaxLaunchUs = 350;
  explicit DisplayScanGate(uint32_t startedUs) : startedUs_(startedUs) {}
  Result observe(uint32_t line, uint32_t nowUs) const {
    if (line > 351) return Result::Invalid;
    if (uint32_t(nowUs - startedUs_) >= TimeoutUs) return Result::TimedOut;
    return line >= FirstLine && line <= LastLine ? Result::Ready : Result::Waiting;
  }
 private:
  uint32_t startedUs_;
};

struct DisplayStrip {
  enum { MaxWidth = 240, MaxRows = 8, MaxFrameRows = 240 };
  int16_t x = 0, y = 0;
  uint16_t width = 0, height = 0;
  uint16_t pixels[MaxWidth * MaxRows] = {};

  bool isFrameBoundary() const { return width == 0 && height == 0; }
  bool isBorrowed() const { return borrowed_ != nullptr; }
  bool scanSynchronized() const { return scanSynchronized_; }
  uint16_t* pixelData() { return borrowed_ ? borrowed_ : pixels; }
  const uint16_t* pixelData() const { return borrowed_ ? borrowed_ : pixels; }
  void markFrameBoundary() {
    x = y = 0; width = height = 0;
    borrowed_ = nullptr;
    wireEncoded_ = false;
    scanSynchronized_ = false;
  }

  static bool validBorrowedFrame(const uint16_t* surface, int columns, int rows) {
    if (!surface || columns < 1 || columns > MaxWidth || rows < 1 || rows > MaxFrameRows ||
        reinterpret_cast<uintptr_t>(surface) % alignof(uint16_t)) return false;
    const size_t count = size_t(columns) * size_t(rows);
    if (count > SIZE_MAX / sizeof(uint16_t)) return false;
    const size_t bytes = count * sizeof(uint16_t);
    return reinterpret_cast<uintptr_t>(surface) <= UINTPTR_MAX - (bytes - 1);
  }

  bool borrowFrame(uint16_t* surface, int columns, int rows, bool wireOrder = false, bool scanSync = false) {
    if (!validBorrowedFrame(surface, columns, rows)) return false;
    if (scanSync && (columns != 135 || rows != 240)) return false;
    x = y = 0;
    width = uint16_t(columns); height = uint16_t(rows);
    borrowed_ = surface;
    wireEncoded_ = wireOrder;
    scanSynchronized_ = scanSync;
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
    scanSynchronized_ = false;
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
  bool scanSynchronized_ = false;
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

  bool tryPushBorrowedFrame(uint16_t* surface, int width, int height, bool wireOrder = false, bool scanSync = false) {
    const uint32_t written = written_.load(std::memory_order_relaxed);
    if (uint32_t(written - completed_.load(std::memory_order_acquire)) >= Capacity) return false;
    if (!strips_[writeSlot_].borrowFrame(surface, width, height, wireOrder, scanSync)) return false;
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
    return count > Capacity ? unsigned(Capacity) : unsigned(count);
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
    uint64_t scanWaitUs = 0, scanProbeUs = 0;
    uint32_t scanFrames = 0, scanWaits = 0, scanTimeouts = 0, scanInvalidReads = 0;
    uint32_t scanProbeAttempts = 0, scanProbePasses = 0, scanProbeReads = 0;
    uint32_t scanPeriodUs = 0, scanLowUs = 0, scanHighUs = 0, maxScanWaitUs = 0;
    uint32_t scanRowMinNs = 0, scanRowMaxNs = 0, maxScanLaunchUs = 0, scanStaleReads = 0;
    bool scanSyncEnabled = false;
    unsigned pending = 0, maxPending = 0;
  };

  AsyncDisplay() = default;
  ~AsyncDisplay() { end(); }
  AsyncDisplay(const AsyncDisplay&) = delete;
  AsyncDisplay& operator=(const AsyncDisplay&) = delete;

  bool begin() {
    if (task_) return true;
    scanSyncEnabled_.store(false, std::memory_order_relaxed);
    stopping_.store(false, std::memory_order_relaxed);
    stopped_.store(false, std::memory_order_relaxed);
    void* storage = heap_caps_malloc(sizeof(DisplayStripFifo), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!storage) return false;
    fifo_ = new (storage) DisplayStripFifo;
    free_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, DisplayStripFifo::Capacity, &freeStorage_);
    ready_ = xSemaphoreCreateCountingStatic(DisplayStripFifo::Capacity, 0, &readyStorage_);
    completed_ = xSemaphoreCreateBinaryStatic(&completedStorage_);
    if (!free_ || !ready_ || !completed_ || xTaskCreatePinnedToCore(runTask, "M5 LCD", 4096, this, 1, &task_, 0) != pdPASS) {
      if (free_) vSemaphoreDelete(free_);
      if (ready_) vSemaphoreDelete(ready_);
      if (completed_) vSemaphoreDelete(completed_);
      free_ = ready_ = completed_ = nullptr;
      task_ = nullptr;
      freeFifo();
      return false;
    }
    return true;
  }

  bool enabled() const { return task_ != nullptr; }

  // Call after setting the physical portrait orientation. Qualification keeps
  // all display access on the main task while the worker queue is empty.
  bool configureScanSync(bool enabled) {
    drain();
    scanSyncEnabled_.store(false, std::memory_order_release);
    if (!enabled || !task_ || M5.Display.width() != 135 || M5.Display.height() != 240 ||
        M5.Display.getRotation() != 0) return false;
    DisplayScanProbe probe;
    const uint32_t started = micros();
    uint32_t reads = 0, attempts = 0;
    auto* bus = M5.Display.panel()->getBus();
    const uint32_t priorClock = bus->getReadClock();
    bus->setReadClock(5000000);
    do {
      probe = DisplayScanProbe();
      const uint32_t attemptStarted = micros();
      ++attempts;
      while (uint32_t(micros() - attemptStarted) < 60000 && !probe.complete() && !probe.failed()) {
        const uint32_t line = readScanLine();
        ++reads;
        probe.observeLine(line, micros());
        if (!probe.complete() && !probe.failed()) delayMicroseconds(50);
      }
    } while (!probe.complete() && attempts < 3);
    bus->setReadClock(priorClock);
    const uint32_t elapsed = micros() - started;
    portENTER_CRITICAL(&statsMux_);
    stats_.scanProbeAttempts += attempts;
    stats_.scanProbePasses += probe.complete();
    stats_.scanProbeReads += reads;
    stats_.scanProbeUs += elapsed;
    stats_.scanPeriodUs = probe.periodUs();
    stats_.scanLowUs = probe.lowUs();
    stats_.scanHighUs = probe.highUs();
    stats_.scanRowMinNs = probe.rowMinNs();
    stats_.scanRowMaxNs = probe.rowMaxNs();
    portEXIT_CRITICAL(&statsMux_);
    scanSyncEnabled_.store(probe.complete(), std::memory_order_release);
    return probe.complete();
  }

  bool scanSyncEnabled() const { return scanSyncEnabled_.load(std::memory_order_acquire); }

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
  bool submitBorrowedFrame(uint16_t* surface, int width, int height, bool wireOrder = false, bool scanSync = false) {
    if (!task_ || !DisplayStrip::validBorrowedFrame(surface, width, height) ||
        reinterpret_cast<uintptr_t>(surface) % 4) return false;
    if (scanSync && (width != 135 || height != 240 || M5.Display.getRotation() != 0)) return false;
    const size_t bytes = size_t(width) * size_t(height) * sizeof(uint16_t);
    const uintptr_t last = reinterpret_cast<uintptr_t>(surface) + bytes - 1;
    if (!esp_ptr_dma_capable(surface) || !esp_ptr_dma_capable(reinterpret_cast<const void*>(last))) return false;
    uint32_t waitUs = 0;
    bool waited = false;
    if (!takeFree(waited, waitUs)) return false;
    if (!fifo_->tryPushBorrowedFrame(surface, width, height, wireOrder, scanSync && scanSyncEnabled())) {
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
    while (fifo_ && fifo_->pending()) xSemaphoreTake(completed_, portMAX_DELAY);
  }

  Stats stats() const {
    portENTER_CRITICAL(&statsMux_);
    Stats result = stats_;
    portEXIT_CRITICAL(&statsMux_);
    result.pending = fifo_ ? fifo_->pending() : 0;
    result.scanSyncEnabled = scanSyncEnabled();
    return result;
  }

  void end() {
    if (!fifo_) return;
    drain();
    scanSyncEnabled_.store(false, std::memory_order_release);
    if (task_) {
      stopping_.store(true, std::memory_order_release);
      xSemaphoreGive(ready_);
      while (!stopped_.load(std::memory_order_acquire)) vTaskDelay(1);
    }
    if (free_) vSemaphoreDelete(free_);
    if (ready_) vSemaphoreDelete(ready_);
    if (completed_) vSemaphoreDelete(completed_);
    task_ = nullptr; free_ = ready_ = completed_ = nullptr;
    freeFifo();
  }

 private:
  DisplayStripFifo* fifo_ = nullptr;
  SemaphoreHandle_t free_ = nullptr, ready_ = nullptr, completed_ = nullptr;
  StaticSemaphore_t freeStorage_, readyStorage_, completedStorage_;
  TaskHandle_t task_ = nullptr;
  std::atomic<bool> stopping_{false}, stopped_{false};
  std::atomic<bool> scanSyncEnabled_{false};
  mutable portMUX_TYPE statsMux_ = portMUX_INITIALIZER_UNLOCKED;
  Stats stats_;
  uint32_t probeTarget_ = 0;
  bool probeAwaitingStart_ = false;

  static uint32_t readScanLine(uint32_t* sampleUs = nullptr) {
    auto* bus = M5.Display.panel()->getBus();
    M5.Display.startWrite();
    bus->writeCommand(0x45, 8);
    bus->wait();
    bus->beginRead(1);
    const uint32_t stamp = micros();
    const uint32_t word = bus->readData(16);
    bus->endRead();
    M5.Display.endWrite();
    if (sampleUs) *sampleUs = stamp;
    return DisplayScanProbe::lineFromWireWord(word);
  }

  uint32_t waitScanPhase() {
    const uint32_t started = micros();
    DisplayScanGate gate(started);
    uint32_t reads = 0;
    auto* bus = M5.Display.panel()->getBus();
    const uint32_t priorClock = bus->getReadClock();
    bus->setReadClock(5000000);
    uint32_t sampleUs = started;
    DisplayScanGate::Result result;
    do {
      const uint32_t line = readScanLine(&sampleUs);
      ++reads;
      result = gate.observe(line, micros());
      if (result == DisplayScanGate::Result::Waiting) delayMicroseconds(50);
    } while (result == DisplayScanGate::Result::Waiting);
    bus->setReadClock(priorClock);
    const uint32_t elapsed = micros() - started;
    const bool ready = result == DisplayScanGate::Result::Ready;
    if (!ready) scanSyncEnabled_.store(false, std::memory_order_release);
    portENTER_CRITICAL(&statsMux_);
    stats_.scanFrames += ready;
    stats_.scanWaits += reads > 1;
    stats_.scanWaitUs += elapsed;
    stats_.scanTimeouts += result == DisplayScanGate::Result::TimedOut;
    stats_.scanInvalidReads += result == DisplayScanGate::Result::Invalid;
    if (elapsed > stats_.maxScanWaitUs) stats_.maxScanWaitUs = elapsed;
    portEXIT_CRITICAL(&statsMux_);
    return sampleUs;
  }

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
        bool synchronized = strip->scanSynchronized() && scanSyncEnabled();
        uint32_t sampleUs = synchronized ? waitScanPhase() : 0;
        synchronized = synchronized && scanSyncEnabled();
        for (;;) {
          M5.Display.startWrite();
          if (!synchronized || uint32_t(micros() - sampleUs) <= DisplayScanGate::MaxDecisionAgeUs) break;
          if (uint32_t(micros() - started) >= DisplayScanGate::TimeoutUs) {
            scanSyncEnabled_.store(false, std::memory_order_release);
            synchronized = false;
            break;
          }
          M5.Display.endWrite();
          portENTER_CRITICAL(&statsMux_);
          ++stats_.scanStaleReads;
          portEXIT_CRITICAL(&statsMux_);
          sampleUs = waitScanPhase();
          synchronized = scanSyncEnabled();
        }
        M5.Display.pushImageDMA(strip->x, strip->y, strip->width, strip->height,
            reinterpret_cast<const lgfx::swap565_t*>(strip->pixelData()));
        const uint32_t launchUs = synchronized ? uint32_t(micros() - sampleUs) : 0;
        if (launchUs > DisplayScanGate::MaxLaunchUs) scanSyncEnabled_.store(false, std::memory_order_release);
        M5.Display.endWrite();
        const uint32_t workUs = uint32_t(micros() - started);
        portENTER_CRITICAL(&statsMux_);
        ++stats_.strips;
        stats_.bytes += uint32_t(strip->width) * strip->height * sizeof(uint16_t);
        stats_.workUs += workUs;
        stats_.encodeUs += encodeUs;
        if (launchUs > stats_.maxScanLaunchUs) stats_.maxScanLaunchUs = launchUs;
        if (workUs > stats_.maxWorkUs) stats_.maxWorkUs = workUs;
        portEXIT_CRITICAL(&statsMux_);
      }
      fifo_->complete();
      xSemaphoreGive(free_);
      if (!fifo_->pending()) xSemaphoreGive(completed_);
    }
  }
};

}  // namespace flappy
#endif
