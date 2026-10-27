#pragma once
#include <stdint.h>
#include <string.h>

namespace flappy {

// Copies the cartridge's pixels into a portrait layout without a full NES framebuffer.
struct TetrisViewport {
  int width = 135;
  int height = 240;

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 131 || screenHeight < 240) return false;
    width = screenWidth < 135 ? screenWidth : 135;
    height = 240;
    return true;
  }

  // Source rectangles are inclusive. Return true after the lower half completes the frame.
  bool consume(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* output, bool gameplay, int stride = 0) const {
    if (!source || !output || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    if (y1 == 0) for (int y = 0; y < height; ++y) memset(output + y * stride, 0, width * sizeof(uint16_t));
    if (!gameplay) {
      const int pictureHeight = width * 3 / 4;
      const int top = (height - pictureHeight) / 2;
      const int sourceWidth = x2 - x1 + 1;
      for (int y = 0; y < pictureHeight; ++y) {
        const int sy = y * 240 / pictureHeight;
        if (sy < y1 || sy > y2) continue;
        for (int x = 0; x < width; ++x) {
          const int sx = x * 256 / width;
          if (sx >= x1 && sx <= x2) output[(top + y) * stride + x] = source[(sy - y1) * sourceWidth + sx - x1];
        }
      }
      return y2 == 239;
    }
    const Rect regions[] = {
      {88, 40, 96, 176, (width - 96) / 2, height - 176},
      {192, 56, 48, 16, 4, 4},
      {184, 104, 48, 48, width - 52, 4},
      {104, 24, 72, 8, 4, 32},
      {192, 160, 40, 16, 4, 44},
    };
    const int sourceWidth = x2 - x1 + 1;
    for (const auto& region : regions) {
      for (int dy = 0; dy < region.h; ++dy) {
        const int sy = region.sy + dy;
        if (sy < y1 || sy > y2) continue;
        const int outY = region.dy + dy;
        if (outY < 0 || outY >= height) continue;
        for (int dx = 0; dx < region.w; ++dx) {
          const int sx = region.sx + dx, outX = region.dx + dx;
          if (sx >= x1 && sx <= x2 && outX >= 0 && outX < width)
            output[outY * stride + outX] = source[(sy - y1) * sourceWidth + sx - x1];
        }
      }
    }
    return y2 == 239;
  }

 private:
  struct Rect { int sx, sy, w, h, dx, dy; };
};

}  // namespace flappy
