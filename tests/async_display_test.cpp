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

int main() {
  static_assert(sizeof(flappy::DisplayStripFifo) < 24000, "At most six display strips must remain bounded");
  copiesBoundsAndInFlightOwnership();
  wireOrderIsExactForEveryRgb565Color();
  frameBoundariesStayOrderedAndReserved();
  concurrentFramesCompleteAfterTheirPixels();
  concurrentFifoAndDelayedConsumer();
  std::cout << "Async display copying, wire order, frame boundaries, bounds, FIFO, in-flight ownership and concurrent wraparound passed at capacity "
            << flappy::DisplayStripFifo::Capacity << ".\n";
}
