#include "../firmware/flappy/tetris_zero_viewport.h"
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <vector>

static uint16_t sample(int x, int y) { return uint16_t((y * 257 + x) ^ 0x5a5a); }

static void dirtyRowsAndPlayerTransitions() {
  const int stride = 144;
  std::vector<uint16_t> source(256 * 240, 0), pixels(stride * 240, 0xa55a);
  for (int rows : {1, 8, 120, 240}) {
    flappy::TetrisZeroViewport view;
    const auto draw = [&](bool twoPlayers) {
      for (int y = 0; y < 240; y += rows)
        assert(view.consume(0, y, 255, y + rows - 1, source.data() + y * 256,
                            pixels.data(), true, twoPlayers, stride) == (y + rows == 240));
    };
    const auto verify = [&](const std::vector<uint16_t>& previous) {
      for (int y = 0; y < 240; ++y) {
        const bool changed = memcmp(previous.data() + y * stride, pixels.data() + y * stride,
                                    135 * sizeof(uint16_t)) != 0;
        assert(view.rowChanged(y) == changed);
        for (int x = 135; x < stride; ++x) assert(pixels[y * stride + x] == 0xa55a);
      }
      assert(!view.rowChanged(-1) && !view.rowChanged(240));
    };
    draw(true);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    auto previous = pixels; draw(true); verify(previous);
    for (uint8_t player : {uint8_t(1), uint8_t(2)}) for (uint8_t held : {uint8_t(18), uint8_t(10), uint8_t(255)}) {
      previous = pixels; view.setHeld(player, held); draw(true); verify(previous);
      previous = pixels; draw(true); verify(previous);
    }
    previous = pixels; source[48 * 256 + 16] ^= 0x4567; draw(true); verify(previous);
    previous = pixels; source[48 * 256 + 160] ^= 0x1234; draw(true); verify(previous);
    draw(false);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    previous = pixels; draw(false); verify(previous);
    view.setLargeCells(true); draw(false);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    previous = pixels; draw(false); verify(previous);
    draw(true);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    previous = pixels; draw(true); verify(previous);
    std::fill(pixels.begin(), pixels.end(), 0xa55a);
    view.invalidate(); draw(true);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
  }
}

int main() {
  const int stride = 144, height = 240, guard = 32;
  const uint16_t sentinel = 0xa55a;
  flappy::TetrisZeroViewport view;
  assert(!view.configure(134, 240) && !view.configure(135, 239));
  assert(view.configure(240, 320) && view.width == 135 && view.height == 240);
  assert(view.configure(135, 240));
  flappy::TetrisViewport single;
  assert(single.configure(240, 320) && single.width == 135 && single.height == 240);
  std::vector<uint16_t> source(256 * 240);
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 256; ++x) source[y * 256 + x] = sample(x, y);
  std::vector<uint16_t> reference;
  for (int rows : {1, 8, 120, 240}) {
    std::vector<uint16_t> storage(stride * height + guard * 2, sentinel);
    view.invalidate();
    uint16_t* pixels = storage.data() + guard;
    for (int y = 0; y < 240; y += rows)
      assert(view.consume(0, y, 255, y + rows - 1, source.data() + y * 256, pixels, true, true, stride) == (y + rows == 240));
    for (int i = 0; i < guard; ++i) assert(storage[i] == sentinel && storage[storage.size() - 1 - i] == sentinel);
    for (int y = 0; y < height; ++y) for (int x = 135; x < stride; ++x) assert(pixels[y * stride + x] == sentinel);
    assert(pixels[87 * stride + 4] == sample(16, 48));
    assert(pixels[206 * stride + 63] == sample(94, 206));
    assert(pixels[87 * stride + 71] == sample(160, 48));
    assert(pixels[206 * stride + 130] == sample(238, 206));
    assert(pixels[7 * stride + 2] == sample(8, 216));
    assert(pixels[7 * stride + 86] == sample(176, 216));
    assert(pixels[25 * stride + 69] == sample(184, 16));
    assert(pixels[48 * stride + 69] == sample(184, 39));
    assert(pixels[52 * stride + 77] == sample(128, 8));
    assert(pixels[219 * stride + 44] == 0);
    assert(pixels[86 * stride + 3] == 0x7bef && pixels[207 * stride + 131] == 0x7bef);
    if (reference.empty()) reference = storage;
    else assert(reference == storage);
  }
  std::vector<uint16_t> pixels(stride * height, sentinel);
  assert(!view.consume(-1, 0, 255, 7, source.data(), pixels.data(), true, true, stride));
  assert(!view.consume(0, 0, 255, 240, source.data(), pixels.data(), true, true, stride));
  assert(!view.consume(0, 0, 255, 7, nullptr, pixels.data(), true, true, stride));
  assert(!view.consume(0, 0, 255, 7, source.data(), pixels.data(), true, true, 134));
  assert(std::all_of(pixels.begin(), pixels.end(), [](uint16_t c) { return c == sentinel; }));
  assert(view.consume(0, 0, 255, 239, source.data(), pixels.data(), true, false, stride));
  assert(pixels[64 * stride + 19] == sample(88, 40));
  assert(view.consume(0, 0, 255, 239, source.data(), pixels.data(), false, true, stride));
  assert(pixels[69 * stride] == sample(0, 0));
  assert(pixels[68 * stride] == 0);
  view.setHeld(1, 18);
  view.setHeld(2, 10);
  view.setLargeCells(true);
  view.consume(0, 0, 255, 239, source.data(), pixels.data(), true, true, stride);
  assert(pixels[219 * stride + 40] == 0xffff);
  assert(pixels[219 * stride + 111] == 0xffff);
  assert(pixels[87 * stride + 4] == sample(16, 48));
  assert(sizeof(view) <= 384);
  dirtyRowsAndPlayerTransitions();
  std::cout << "Zero portrait boards, HUD, stripe boundaries and buffer guards passed.\n";
}
