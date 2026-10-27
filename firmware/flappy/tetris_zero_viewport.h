#pragma once
#include <stdint.h>
#include <string.h>
#include "tetris_viewport.h"

namespace flappy {

// Layout coordinates verified against the MMC3_118_v2 cartridge.
struct TetrisZeroViewport {
  int width = 135;
  int height = 240;

  void invalidate() const { damage_.invalidate(); single_.invalidate(); }
  bool rowChanged(int row) const { return delegated_ ? single_.rowChanged(row) : damage_.changed(row); }
  void setLargeCells(bool enabled) { single_.setLargeCells(enabled); }
  void setHeld(uint8_t player, uint8_t orientation) {
    if (player >= 1 && player <= 2) held_[player - 1] = TetrisActions::spawnOrientation(orientation);
    if (player == 1) single_.setHeld(orientation);
  }
  void setNext(uint8_t player, uint8_t orientation) { if (player == 1) single_.setNext(orientation); }
  void setStats(uint8_t player, uint32_t score, uint16_t lines, uint8_t level) {
    if (player == 1) single_.setStats(score, lines, level);
  }

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 135 || screenHeight < 240) return false;
    width = 135;
    height = 240;
    invalidate();
    return true;
  }

  // Source rectangles are inclusive. Each completed frame returns true.
  bool consume(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* output, bool gameplay, bool twoPlayers = true, int stride = 0) const {
    if (!gameplay || !twoPlayers) {
      if (!delegated_) { single_.invalidate(); delegated_ = true; }
      single_.width = width;
      single_.height = height;
      return single_.consume(x1, y1, x2, y2, source, output, gameplay, stride);
    }
    if (!source || !output || width != 135 || height != 240 || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    const int left = (width - 135) / 2, top = (height - 240) / 2;
    if (y1 == 0 || !damage_.configured(output, width, height, stride, 1) || delegated_) {
      if (delegated_) { damage_.invalidate(); delegated_ = false; }
      damage_.begin(output, width, height, stride, 1);
      if (damage_.fresh()) for (int board = 0; board < 2; ++board) {
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
    };
    const int sourceWidth = x2 - x1 + 1;
    for (const auto& region : regions) {
      const int w = region.small ? region.w * 3 / 4 : region.w;
      const int h = region.small ? region.h * 3 / 4 : region.h;
      int firstY = y1 > region.sy ? y1 - region.sy : 0;
      int endY = y2 + 1 - region.sy;
      int firstX = x1 > region.sx ? x1 - region.sx : 0;
      int endX = x2 + 1 - region.sx;
      if (endY <= 0 || endX <= 0) continue;
      if (region.small) {
        firstY = (firstY * 3 + 3) / 4; endY = (endY * 3 + 3) / 4;
        firstX = (firstX * 3 + 3) / 4; endX = (endX * 3 + 3) / 4;
      }
      if (endY > h) endY = h;
      if (endX > w) endX = w;
      if (firstX >= endX || firstY >= endY) continue;
      for (int dy = firstY; dy < endY; ++dy) {
        const int sy = region.sy + (region.small ? dy + dy / 3 : dy);
        const uint16_t* row = source + (sy - y1) * sourceWidth;
        if (!region.small) {
          damage_.copy(top + region.dy + dy, left + region.dx + firstX,
                       row + region.sx + firstX - x1, endX - firstX);
        } else {
          static const uint8_t columns[] = {
            0,1,2,4,5,6,8,9,10,12,13,14,16,17,18,20,21,22,24,25,26,28,29,30,
            32,33,34,36,37,38,40,41,42,44,45,46,48,49,50,52,53,54,56,57,58,60,61,62,
            64,65,66,68,69,70,72,73,74,76,77,78
          };
          uint16_t pixels[60];
          for (int dx = firstX; dx < endX; ++dx) pixels[dx - firstX] = row[region.sx + columns[dx] - x1];
          damage_.copy(top + region.dy + dy, left + region.dx + firstX, pixels, endX - firstX);
        }
      }
    }
    if (y2 == 239) for (uint8_t player = 0; player < 2; ++player) {
      const int x = left + 3 + player * 67;
      if (damage_.fresh()) TetrisViewport::label(output, width, height, stride, "Hold", x, top + 215);
      if (damage_.fresh() || previousHeld_[player] != held_[player])
        TetrisViewport::trackedPreview(damage_, held_[player], x + 32, top + 217);
      previousHeld_[player] = held_[player];
    }
    return y2 == 239;
  }

 private:
  struct Region { int sx, sy, w, h, dx, dy; bool small; };
  uint8_t held_[2] = {255, 255};
  mutable uint8_t previousHeld_[2] = {255, 255};
  mutable bool delegated_ = true;
  mutable TetrisRowDamage damage_;
  mutable TetrisViewport single_;
};

}  // namespace flappy
