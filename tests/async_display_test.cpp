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
  for (unsigned i = 0; i < 3; ++i) assert(fifo.tryPush(0, i * 8, 240, 8, source.data() + 1, 244));
  assert(fifo.pending() == 3 && !fifo.tryPush(0, 24, 240, 8, source.data() + 1, 244));
  std::fill(source.begin(), source.end(), 0);
  for (unsigned i = 0; i < 3; ++i) {
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

static void concurrentFifoAndDelayedConsumer() {
  flappy::DisplayStripFifo fifo(UINT32_MAX - 100u);
  constexpr unsigned count = 10000;
  std::thread consumer([&fifo]() {
    for (unsigned n = 0; n < count; ++n) {
      const flappy::DisplayStrip* strip;
      while (!(strip = fifo.front())) std::this_thread::yield();
      assert(strip->y == int(n % 240u));
      assert(strip->width == 1u + n % 240u && strip->height == 1u + n % 8u);
      if (n % 97u == 0) std::this_thread::yield();
      for (unsigned p = 0; p < unsigned(strip->width) * strip->height; ++p)
        assert(strip->pixels[p] == uint16_t(n + p));
      assert(fifo.complete());
    }
  });
  uint16_t source[240 * 8];
  for (unsigned n = 0; n < count; ++n) {
    const unsigned width = 1u + n % 240u, height = 1u + n % 8u;
    for (unsigned p = 0; p < width * height; ++p) source[p] = uint16_t(n + p);
    while (!fifo.tryPush(0, n % 240u, width, height, source, width)) std::this_thread::yield();
    std::fill(source, source + width * height, 0xffff);
  }
  consumer.join();
  assert(!fifo.pending());
}

int main() {
  static_assert(sizeof(flappy::DisplayStripFifo) < 11700, "Three display strips must remain bounded");
  copiesBoundsAndInFlightOwnership();
  concurrentFifoAndDelayedConsumer();
  std::cout << "Async display copying, bounds, FIFO, in-flight ownership and concurrent wraparound passed\n";
}
