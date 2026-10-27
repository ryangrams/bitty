#include "../firmware/flappy/tetris_viewport.h"

#include <assert.h>
#include <algorithm>
#include <iostream>
#include <vector>

static void dirtyRowsMatchFinalPixels() {
  const int stride = 144;
  std::vector<uint16_t> source(256 * 240, 0), pixels(stride * 240, 0xa55a);
  for (bool large : {false, true}) for (int rows : {1, 8, 120, 240}) {
    flappy::TetrisViewport view;
    view.setLargeCells(large);
    const auto draw = [&]() {
      for (int y = 0; y < 240; y += rows)
        assert(view.consume(0, y, 255, y + rows - 1, source.data() + y * 256,
                            pixels.data(), true, stride) == (y + rows == 240));
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
    draw();
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    auto previous = pixels;
    draw(); verify(previous);
    for (uint8_t held : {uint8_t(18), uint8_t(10), uint8_t(7), uint8_t(255)}) {
      previous = pixels; view.setHeld(held); draw(); verify(previous);
      previous = pixels; draw(); verify(previous);
    }
    for (uint8_t next : {uint8_t(18), uint8_t(10), uint8_t(255)}) {
      previous = pixels; view.setNext(next); draw(); verify(previous);
    }
    previous = pixels; view.setStats(1234567, 1200, 29); draw(); verify(previous);
    previous = pixels; view.setStats(1, 0, 0); draw(); verify(previous);
    previous = pixels; source[48 * 256 + 96] ^= 0x1234; draw(); verify(previous);
    previous = pixels; draw(); verify(previous);
    std::fill(pixels.begin(), pixels.end(), 0xa55a);
    view.invalidate(); draw();
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    previous = pixels; draw(); verify(previous);
  }
}

static void clippedNativeRowsAndLayoutChanges() {
  const int stride = 144;
  std::vector<uint16_t> source(256 * 240);
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 256; ++x) source[y * 256 + x] = uint16_t(y * 257 + x);
  for (int width : {131, 135}) {
    flappy::TetrisViewport view;
    assert(view.configure(width, 240));
    std::vector<uint16_t> pixels(stride * 240, 0xa55a);
    std::vector<uint16_t> clipped(41 * 240);
    for (int y = 0; y < 240; ++y) memcpy(clipped.data() + y * 41, source.data() + y * 256 + 80, 41 * sizeof(uint16_t));
    assert(view.consume(80, 0, 120, 239, clipped.data(), pixels.data(), true, stride));
    const int left = (width - 96) / 2;
    for (int y = 40; y < 216; ++y) for (int x = 88; x <= 120; ++x)
      assert(pixels[(64 + y - 40) * stride + left + x - 88] == source[y * 256 + x]);
    view.setLargeCells(true);
    view.consume(0, 0, 255, 239, source.data(), pixels.data(), true, stride);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    view.setLargeCells(false);
    view.consume(0, 0, 255, 239, source.data(), pixels.data(), true, stride);
    for (int y = 0; y < 240; ++y) assert(view.rowChanged(y));
    view.consume(0, 0, 255, 239, source.data(), pixels.data(), false, stride);
    for (int y = 0; y < 240; ++y) {
      assert(view.rowChanged(y));
      for (int x = width; x < stride; ++x) assert(pixels[y * stride + x] == 0xa55a);
    }
  }
}

int main() {
  const int stride = 144, guard = 32;
  const uint16_t sentinel = 0xa55a;
  std::vector<uint16_t> source(256 * 240, 0);
  const uint16_t fill = 0x001f, light = 0x7dff, dark = 0x000b;
  for (int row = 0; row < 20; ++row) for (int col = 0; col < 10; ++col) {
    if ((row + col) % 3 == 0) continue;
    for (int y = 1; y < 7; ++y) for (int x = 1; x < 7; ++x)
      source[(48 + row * 8 + y) * 256 + 96 + col * 8 + x] =
          x == 1 || y == 1 ? light : x == 6 || y == 6 ? dark : fill;
  }
  std::vector<uint16_t> reference;
  for (int rows : {1, 8, 120, 240}) {
    flappy::TetrisViewport viewport;
    viewport.setLargeCells(true);
    viewport.setStats(123456, 17, 18);
    viewport.setHeld(18);
    viewport.setNext(10);
    std::vector<uint16_t> storage(stride * 240 + guard * 2, sentinel);
    uint16_t* pixels = storage.data() + guard;
    for (int y = 0; y < 240; y += rows)
      assert(viewport.consume(0, y, 255, y + rows - 1, source.data() + y * 256,
                              pixels, true, stride) == (y + rows == 240));
    for (int i = 0; i < guard; ++i) assert(storage[i] == sentinel && storage[storage.size() - 1 - i] == sentinel);
    for (int y = 0; y < 240; ++y) for (int x = 135; x < stride; ++x) assert(pixels[y * stride + x] == sentinel);
    for (int row = 0; row < 20; ++row) for (int col = 0; col < 10; ++col) {
      const int x = col * 11, y = 20 + row * 11;
      for (int edge = 0; edge < 11; ++edge) {
        assert(pixels[y * stride + x + edge] == 0);
        assert(pixels[(y + 10) * stride + x + edge] == 0);
        assert(pixels[(y + edge) * stride + x] == 0);
        assert(pixels[(y + edge) * stride + x + 10] == 0);
      }
      const bool empty = (row + col) % 3 == 0;
      assert(pixels[(y + 5) * stride + x + 5] == (empty ? 0 : fill));
      assert(pixels[(y + 1) * stride + x + 5] == (empty ? 0 : light));
      assert(pixels[(y + 9) * stride + x + 5] == (empty ? 0 : dark));
    }
    assert(pixels[41 * stride + 120] == 0xffff);
    assert(pixels[85 * stride + 121] == 0xffff);
    if (reference.empty()) reference = storage;
    else assert(reference == storage);
    viewport.setHeld(255);
    viewport.setNext(255);
    viewport.consume(0, 0, 255, 239, source.data(), pixels, true, stride);
    assert(pixels[41 * stride + 120] == 0);
    assert(pixels[85 * stride + 121] == 0);
  }
  flappy::TetrisViewport viewport;
  std::vector<uint16_t> pixels(stride * 240, sentinel);
  viewport.consume(0, 0, 255, 239, source.data(), pixels.data(), true, stride);
  assert(pixels[72 * stride + 27] == source[48 * 256 + 96]);
  viewport.setHeld(18);
  viewport.consume(0, 0, 255, 239, source.data(), pixels.data(), true, stride);
  assert(pixels[20 * stride + 58] == 0xffff);
  viewport.setHeld(255);
  viewport.consume(0, 0, 255, 239, source.data(), pixels.data(), true, stride);
  assert(pixels[20 * stride + 58] == 0);
  viewport.setLargeCells(true);
  assert(!viewport.consume(0, 0, 255, 239, source.data(), pixels.data(), true, 134));
  viewport.consume(0, 0, 255, 239, source.data(), pixels.data(), false, stride);
  assert(pixels[69 * stride] == source[0]);
  assert(sizeof(viewport) <= 320);
  dirtyRowsMatchFinalPixels();
  clippedNativeRowsAndLayoutChanges();
  std::cout << "Solo Tetris uniform 11-pixel cells, native colors, previews, stripes and buffer guards passed.\n";
}
