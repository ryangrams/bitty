#pragma once
#include <stdint.h>
#include <string.h>
#include "tetris_viewport.h"

namespace flappy {

// Layout coordinates verified against the MMC3_118_v2 cartridge.
struct TetrisZeroViewport {
  int width = 135;
  int height = 240;

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 135 || screenHeight < 240) return false;
    width = 135;
    height = 240;
    return true;
  }

  // Source rectangles are inclusive. Each completed frame returns true.
  bool consume(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* output, bool gameplay, bool twoPlayers = true, int stride = 0) const {
    if (!gameplay || !twoPlayers) {
      TetrisViewport single;
      single.width = width;
      single.height = height;
      return single.consume(x1, y1, x2, y2, source, output, gameplay, stride);
    }
    if (!source || !output || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    const int left = (width - 135) / 2, top = (height - 240) / 2;
    if (x1 == 0 && y1 == 0) {
      for (int y = 0; y < height; ++y) memset(output + y * stride, 0, width * sizeof(uint16_t));
      for (int board = 0; board < 2; ++board) {
        const int x = left + 3 + board * 67;
        for (int y = 86; y <= 207; ++y) {
          output[(top + y) * stride + x] = 0x7bef;
          output[(top + y) * stride + x + 61] = 0x7bef;
        }
        for (int dx = 0; dx < 62; ++dx) {
          output[(top + 86) * stride + x + dx] = 0x7bef;
          output[(top + 207) * stride + x + dx] = 0x7bef;
        }
      }
    }
    const Region regions[] = {
      {16, 48, 80, 160, 4, 87, true},
      {160, 48, 80, 160, 71, 87, true},
      {8, 216, 16, 8, 2, 7, false},
      {32, 216, 48, 8, 19, 7, false},
      {232, 216, 16, 8, 69, 7, false},
      {176, 216, 48, 8, 86, 7, false},
      {8, 16, 64, 24, 2, 25, false},
      {184, 16, 64, 24, 69, 25, false},
      {80, 8, 48, 32, 10, 52, false},
      {128, 8, 48, 32, 77, 52, false},
      {104, 208, 48, 16, 44, 219, false},
    };
    const int sourceWidth = x2 - x1 + 1;
    for (const auto& region : regions) {
      const int w = region.small ? region.w * 3 / 4 : region.w;
      const int h = region.small ? region.h * 3 / 4 : region.h;
      for (int dy = 0; dy < h; ++dy) {
        const int sy = region.sy + (region.small ? dy * 4 / 3 : dy);
        if (sy < y1 || sy > y2) continue;
        uint16_t* target = output + (top + region.dy + dy) * stride + left + region.dx;
        for (int dx = 0; dx < w; ++dx) {
          const int sx = region.sx + (region.small ? dx * 4 / 3 : dx);
          if (sx >= x1 && sx <= x2) target[dx] = source[(sy - y1) * sourceWidth + sx - x1];
        }
      }
    }
    return y2 == 239;
  }

 private:
  struct Region { int sx, sy, w, h, dx, dy; bool small; };
};

}  // namespace flappy
