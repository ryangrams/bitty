#include "../firmware/flappy/async_display.h"

#include <assert.h>
#include <iostream>
#include <thread>
#include <vector>

static void copiesBoundsAndInFlightOwnership() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 1u);
  std::vector<uint16_t> source(244 * 8 + 2, 0xa55a);
  for (unsigned row = 0; row < 8; ++row)
    for (unsigned x = 0; x < 240; ++x) source[1 + row * 244 + x] = uint16_t(row * 240 + x);
  assert(!fifo.tryPush(0, 0, 241, 8, source.data(), 244));
  assert(!fifo.tryPush(0, 0, 240, 9, source.data(), 244));
  assert(!fifo.tryPush(0, 0, 0, 8, source.data(), 244));
  assert(!fifo.tryPush(-1, 0, 240, 8, source.data(), 244));
  assert(!fifo.tryPush(0, -1, 240, 8, source.data(), 244));
  assert(!fifo.tryPush(INT16_MAX, 0, 240, 8, source.data(), 244));
  assert(!fifo.tryPush(0, 0, 240, 8, source.data(), 239));
  assert(!fifo.tryPush(0, 0, 240, 8, nullptr, 244));
  assert(!fifo.pending() && !fifo.front() && !fifo.complete());
  for (unsigned i = 0; i < flappy::DisplayStripFifo::Capacity; ++i) assert(fifo.tryPush(0, i * 8, 240, 8, source.data() + 1, 244));
  assert(fifo.pending() == flappy::DisplayStripFifo::Capacity && !fifo.tryPush(0, 24, 240, 8, source.data() + 1, 244));
  std::fill(source.begin(), source.end(), 0);
  for (unsigned i = 0; i < flappy::DisplayStripFifo::Capacity; ++i) {
    const auto* strip = fifo.front();
    assert(strip && strip->x == 0 && strip->y == int(i * 8) && strip->width == 240 && strip->height == 8);
    for (unsigned p = 0; p < 240 * 8; ++p) assert(strip->pixels[p] == p);
    // Reading the front does not release an active transfer's storage.
    if (i == 0) assert(!fifo.tryPush(0, 24, 240, 8, source.data(), 244));
    assert(fifo.complete());
  }
  assert(!fifo.pending() && !fifo.front());
  assert(fifo.tryPush(0, 0, 1, 1, source.data(), 244));
  assert(fifo.front()->pixels[0] == 0 && fifo.complete());
}

static void wireOrderIsExactForEveryRgb565Color() {
  flappy::DisplayStrip strip;
  constexpr unsigned width = 239, rows = 8, stride = 241;
  std::vector<uint16_t> source(1 + stride * rows + 1, 0xa55a);
  for (unsigned first = 0; first < 65536; first += width * rows) {
    for (unsigned row = 0; row < rows; ++row)
      for (unsigned col = 0; col < width; ++col)
        source[1 + row * stride + col] = uint16_t(first + row * width + col);
    const auto original = source;
    std::fill(strip.pixels, strip.pixels + flappy::DisplayStrip::MaxWidth * flappy::DisplayStrip::MaxRows, uint16_t(0xbeef));
    assert(strip.copy(1, 2, width, rows, source.data() + 1, stride, true));
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(strip.pixels);
    for (unsigned row = 0; row < rows; ++row) for (unsigned col = 0; col < width; ++col) {
      const unsigned target = (row * width + col) * 2;
      const uint16_t color = source[1 + row * stride + col];
      assert(bytes[target] == uint8_t(color >> 8));
      assert(bytes[target + 1] == uint8_t(color));
    }
    for (unsigned p = width * rows; p < flappy::DisplayStrip::MaxWidth * flappy::DisplayStrip::MaxRows; ++p)
      assert(strip.pixels[p] == 0xbeef);
    assert(source == original);
    assert(strip.copy(1, 2, width, rows, source.data() + 1, stride));
    strip.encodeWireOrder();
    for (unsigned row = 0; row < rows; ++row) for (unsigned col = 0; col < width; ++col) {
      const unsigned target = (row * width + col) * 2;
      const uint16_t color = source[1 + row * stride + col];
      assert(bytes[target] == uint8_t(color >> 8));
      assert(bytes[target + 1] == uint8_t(color));
    }
    assert(source == original);
  }
}

static void frameBoundariesStayOrderedAndReserved() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 2u);
  const uint16_t first = 0x1234, second = 0xabcd;
  assert(fifo.tryPush(0, 0, 1, 1, &first, 1));
  assert(fifo.tryPushEndFrame());
  assert(fifo.tryPush(0, 1, 1, 1, &second, 1));
  for (unsigned index = 3; index < flappy::DisplayStripFifo::Capacity; ++index) assert(fifo.tryPushEndFrame());
  assert(!fifo.tryPushEndFrame());
  assert(fifo.front() && !fifo.front()->isFrameBoundary());
  assert(fifo.front()->pixels[0] == first);
  assert(fifo.complete());
  assert(fifo.front() && fifo.front()->isFrameBoundary());
  assert(fifo.tryPushEndFrame());
  assert(!fifo.tryPush(0, 2, 1, 1, &first, 1));
  assert(fifo.front()->isFrameBoundary());
  assert(fifo.complete());
  assert(fifo.front() && !fifo.front()->isFrameBoundary());
  assert(fifo.front()->pixels[0] == second);
  assert(fifo.complete());
  for (unsigned index = 0; index < flappy::DisplayStripFifo::Capacity - 2; ++index)
    assert(fifo.front()->isFrameBoundary() && fifo.complete());
  for (unsigned index = 0; index < flappy::DisplayStripFifo::Capacity; ++index) assert(fifo.tryPushEndFrame());
  for (unsigned index = 0; index < flappy::DisplayStripFifo::Capacity; ++index) {
    assert(fifo.front()->isFrameBoundary());
    assert(fifo.complete());
  }
  assert(fifo.tryPush(0, 0, 1, 1, &first, 1));
  assert(!fifo.front()->isFrameBoundary());
  assert(fifo.front()->pixels[0] == first && fifo.complete());
  assert(!fifo.front() && !fifo.pending());
}

static void concurrentFramesCompleteAfterTheirPixels() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 7u);
  constexpr unsigned frames = 2000;
  unsigned completedFrames = 0;
  std::thread consumer([&fifo, &completedFrames]() {
    for (unsigned frame = 0; frame < frames; ++frame) {
      const unsigned strips = frame % 17u == 0 ? 0 : 2;
      for (unsigned part = 0; part < strips; ++part) {
        flappy::DisplayStrip* strip;
        while (!(strip = fifo.front())) std::this_thread::yield();
        assert(!strip->isFrameBoundary());
        strip->encodeWireOrder();
        assert(strip->y == int(part * 8) && strip->width == 1 + frame % 240u && strip->height == 8);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(strip->pixels);
        for (unsigned p = 0; p < unsigned(strip->width) * strip->height; ++p) {
          const uint16_t expected = uint16_t(frame * 131u + part * 17u + p);
          assert(bytes[p * 2] == uint8_t(expected >> 8));
          assert(bytes[p * 2 + 1] == uint8_t(expected));
        }
        if (frame % 11u == 0) std::this_thread::yield();
        assert(fifo.complete());
      }
      const flappy::DisplayStrip* marker;
      while (!(marker = fifo.front())) std::this_thread::yield();
      assert(marker->isFrameBoundary());
      ++completedFrames;
      assert(fifo.complete());
    }
  });
  uint16_t source[240 * 8];
  for (unsigned frame = 0; frame < frames; ++frame) {
    const unsigned strips = frame % 17u == 0 ? 0 : 2;
    for (unsigned part = 0; part < strips; ++part) {
      const unsigned width = 1 + frame % 240u;
      for (unsigned p = 0; p < width * 8; ++p) source[p] = uint16_t(frame * 131u + part * 17u + p);
      while (!fifo.tryPush(0, part * 8, width, 8, source, width)) std::this_thread::yield();
      std::fill(source, source + width * 8, uint16_t(0xffff));
    }
    while (!fifo.tryPushEndFrame()) std::this_thread::yield();
  }
  consumer.join();
  assert(completedFrames == frames && !fifo.pending());
}

static void concurrentFifoAndDelayedConsumer() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 100u);
  constexpr unsigned count = 10000;
  std::thread consumer([&fifo]() {
    for (unsigned n = 0; n < count; ++n) {
      flappy::DisplayStrip* strip;
      while (!(strip = fifo.front())) std::this_thread::yield();
      assert(strip->y == int(n % 240u));
      assert(strip->width == 1u + n % 240u && strip->height == 1u + n % 8u);
      if (n % 97u == 0) std::this_thread::yield();
      if (!(n & 1u)) strip->encodeWireOrder();
      for (unsigned p = 0; p < unsigned(strip->width) * strip->height; ++p) {
        const uint16_t expected = uint16_t(n + p);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(strip->pixels);
        assert(bytes[p * 2] == uint8_t(expected >> 8));
        assert(bytes[p * 2 + 1] == uint8_t(expected));
      }
      assert(fifo.complete());
    }
  });
  uint16_t source[240 * 8];
  for (unsigned n = 0; n < count; ++n) {
    const unsigned width = 1u + n % 240u, height = 1u + n % 8u;
    for (unsigned p = 0; p < width * height; ++p) source[p] = uint16_t(n + p);
    while (!fifo.tryPush(0, n % 240u, width, height, source, width, (n & 1u) != 0)) std::this_thread::yield();
    std::fill(source, source + width * height, 0xffff);
  }
  consumer.join();
  assert(!fifo.pending());
}

static void borrowedFrameBoundsAndWireOrder() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 1u);
  constexpr unsigned count = 240 * 135;
  std::vector<uint16_t> guarded(count + 4, 0xa55a);
  uint16_t* surface = guarded.data() + 2;
  assert(!fifo.tryPushBorrowedFrame(nullptr, 240, 135));
  assert(!fifo.tryPushBorrowedFrame(surface, 0, 135));
  assert(!fifo.tryPushBorrowedFrame(surface, 240, -1));
  assert(!fifo.tryPushBorrowedFrame(surface, 241, 135));
  assert(!fifo.tryPushBorrowedFrame(surface, 240, 241));
  assert(!fifo.tryPushBorrowedFrame(surface, INT_MAX, INT_MAX));
  assert(!fifo.tryPushBorrowedFrame(reinterpret_cast<uint16_t*>(uintptr_t(surface) + 1), 1, 1));
  assert(!fifo.tryPushBorrowedFrame(reinterpret_cast<uint16_t*>(UINTPTR_MAX - 1u), 240, 135));
  assert(!fifo.pending());
  for (unsigned p = 0; p < count; ++p) surface[p] = uint16_t(p * 17u + 0x1234);
  assert(fifo.tryPushBorrowedFrame(surface, 240, 135));
  const uint16_t copiedPixel = 0xa13f;
  assert(fifo.tryPush(0, 0, 1, 1, &copiedPixel, 1));
  assert(fifo.tryPushEndFrame());
  flappy::DisplayStrip* frame = fifo.front();
  assert(frame && frame->isBorrowed() && !frame->isFrameBoundary());
  assert(frame->pixelData() == surface && frame->x == 0 && frame->y == 0);
  assert(frame->width == 240 && frame->height == 135);
  const flappy::DisplayStrip* immutableFrame = frame;
  assert(immutableFrame->pixelData() == surface);
  std::fill(frame->pixels, frame->pixels + 240 * 8, uint16_t(0xbeef));
  frame->encodeWireOrder();
  frame->encodeWireOrder();
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(surface);
  for (unsigned p = 0; p < count; ++p) {
    const uint16_t expected = uint16_t(p * 17u + 0x1234);
    assert(bytes[p * 2] == uint8_t(expected >> 8));
    assert(bytes[p * 2 + 1] == uint8_t(expected));
  }
  for (unsigned p = 0; p < 240 * 8; ++p) assert(frame->pixels[p] == 0xbeef);
  assert(guarded[0] == 0xa55a && guarded[1] == 0xa55a);
  assert(guarded[count + 2] == 0xa55a && guarded[count + 3] == 0xa55a);
  assert(fifo.pending() == 3 && fifo.front() == frame);
  assert(fifo.complete());
  assert(!fifo.front()->isBorrowed() && fifo.front()->pixelData() == fifo.front()->pixels);
  assert(fifo.front()->pixels[0] == copiedPixel && fifo.complete());
  assert(fifo.front()->isFrameBoundary() && !fifo.front()->isBorrowed() && fifo.complete());
  // Slot reuse clears references before a copied strip or a marker is published.
  for (unsigned n = 0; n < flappy::DisplayStripFifo::Capacity; ++n) {
    assert(fifo.tryPush(0, 0, 1, 1, &copiedPixel, 1));
    assert(!fifo.front()->isBorrowed() && fifo.front()->pixelData() == fifo.front()->pixels);
    fifo.front()->encodeWireOrder();
    const uint8_t* copiedBytes = reinterpret_cast<const uint8_t*>(fifo.front()->pixelData());
    assert(copiedBytes[0] == 0xa1 && copiedBytes[1] == 0x3f);
    assert(fifo.complete());
  }
}

static void concurrentBorrowedFrameCompletionAndReuse() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 3u);
  constexpr unsigned frames = 180, maxPixels = 240 * 240;
  std::vector<uint16_t> guarded(maxPixels + 4, 0x6cc6);
  uint16_t* surface = guarded.data() + 2;
  unsigned completedFrames = 0;
  std::thread consumer([&fifo, surface, &completedFrames]() {
    for (unsigned frame = 0; frame < frames; ++frame) {
      const unsigned width = frame % 3u == 0 ? 135 : 240;
      const unsigned height = frame % 3u == 1 ? 135 : 240;
      flappy::DisplayStrip* image;
      while (!(image = fifo.front())) std::this_thread::yield();
      assert(image->isBorrowed() && !image->isFrameBoundary());
      assert(image->pixelData() == surface && image->width == width && image->height == height);
      image->encodeWireOrder();
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(image->pixelData());
      // A delayed DMA reader retains this surface until the image completes.
      for (unsigned p = 0; p < width * height; ++p) {
        if (!(p % 4096u)) std::this_thread::yield();
        const uint16_t expected = uint16_t(frame * 131u + p * 17u);
        assert(bytes[p * 2] == uint8_t(expected >> 8));
        assert(bytes[p * 2 + 1] == uint8_t(expected));
      }
      assert(fifo.front() == image && fifo.complete());
      const flappy::DisplayStrip* marker;
      while (!(marker = fifo.front())) std::this_thread::yield();
      assert(marker->isFrameBoundary() && !marker->isBorrowed());
      ++completedFrames;
      assert(fifo.complete());
    }
  });
  for (unsigned frame = 0; frame < frames; ++frame) {
    // This is the portable equivalent of drain() before repainting one canvas.
    while (fifo.pending()) std::this_thread::yield();
    const unsigned width = frame % 3u == 0 ? 135 : 240;
    const unsigned height = frame % 3u == 1 ? 135 : 240;
    const bool wireOrder = (frame & 1u) != 0;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(surface);
    for (unsigned p = 0; p < width * height; ++p) {
      const uint16_t color = uint16_t(frame * 131u + p * 17u);
      if (wireOrder) { bytes[p * 2] = uint8_t(color >> 8); bytes[p * 2 + 1] = uint8_t(color); }
      else surface[p] = color;
    }
    assert(fifo.tryPushBorrowedFrame(surface, width, height, wireOrder));
    while (!fifo.tryPushEndFrame()) std::this_thread::yield();
    // The producer can run unrelated CPU work while the consumer reads pixels.
    uint32_t cpuState = frame;
    for (unsigned p = 0; p < 4096; ++p) cpuState = cpuState * 1664525u + 1013904223u;
    assert(cpuState != frame);
  }
  consumer.join();
  assert(completedFrames == frames && !fifo.pending());
  assert(guarded[0] == 0x6cc6 && guarded[1] == 0x6cc6);
  assert(guarded[maxPixels + 2] == 0x6cc6 && guarded[maxPixels + 3] == 0x6cc6);
}

static void preencodedBorrowedFramesKeepEveryRgb565Color() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 1u);
  constexpr unsigned width = 240, height = 135, count = width * height;
  std::vector<uint16_t> guarded(count + 4, 0xa55a);
  uint16_t* surface = guarded.data() + 2;
  uint8_t* bytes = reinterpret_cast<uint8_t*>(surface);
  for (unsigned first = 0; first < 65536; first += count) {
    for (unsigned p = 0; p < count; ++p) {
      const uint16_t color = uint16_t(first + p);
      bytes[p * 2] = uint8_t(color >> 8);
      bytes[p * 2 + 1] = uint8_t(color);
    }
    const auto original = guarded;
    assert(fifo.tryPushBorrowedFrame(surface, width, height, true));
    assert(fifo.tryPushEndFrame());
    auto* image = fifo.front();
    assert(image && image->isBorrowed() && image->pixelData() == surface);
    image->encodeWireOrder();
    image->encodeWireOrder();
    assert(guarded == original);
    assert(fifo.complete());
    assert(fifo.front()->isFrameBoundary() && fifo.complete());
  }
  // Reuse of an encoded job still converts native colors on the default path.
  for (unsigned n = 0; n < flappy::DisplayStripFifo::Capacity; ++n) {
    surface[0] = 0x12ab;
    assert(fifo.tryPushBorrowedFrame(surface, 1, 1));
    fifo.front()->encodeWireOrder();
    assert(bytes[0] == 0x12 && bytes[1] == 0xab);
    assert(fifo.complete());
  }
}

int main() {
  static_assert(sizeof(flappy::DisplayStripFifo) < 24000, "At most six display strips must remain bounded");
  copiesBoundsAndInFlightOwnership();
  wireOrderIsExactForEveryRgb565Color();
  frameBoundariesStayOrderedAndReserved();
  concurrentFramesCompleteAfterTheirPixels();
  concurrentFifoAndDelayedConsumer();
  borrowedFrameBoundsAndWireOrder();
  concurrentBorrowedFrameCompletionAndReuse();
  preencodedBorrowedFramesKeepEveryRgb565Color();
  std::cout << "Async display copying, borrowed frames, wire order, frame boundaries, bounds, in-flight ownership and concurrent wraparound passed at capacity "
            << flappy::DisplayStripFifo::Capacity << ".\n";
}
