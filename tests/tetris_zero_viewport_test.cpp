#include "../firmware/flappy/tetris_zero_viewport.h"
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <vector>

static uint16_t sample(int x, int y) { return uint16_t((y * 257 + x) ^ 0x5a5a); }

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
    assert(pixels[219 * stride + 44] == sample(104, 208));
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
  std::cout << "Zero portrait boards, HUD, stripe boundaries and buffer guards passed.\n";
}
