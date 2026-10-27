#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "menu_font.h"
#include "tetris_actions.h"

namespace flappy {

class TetrisRowDamage {
 public:
  void invalidate() const { force_ = true; }
  bool configured(uint16_t* output, int width, int height, int stride, uint8_t layout) const {
    return output_ == output && width_ == width && height_ == height && stride_ == stride && layout_ == layout;
  }
  bool begin(uint16_t* output, int width, int height, int stride, uint8_t layout) const {
    memset(dirty_, 0, sizeof(dirty_));
    fresh_ = force_ || output_ != output || width_ != width || height_ != height ||
             stride_ != stride || layout_ != layout;
    output_ = output; width_ = width; height_ = height; stride_ = stride; layout_ = layout;
    if (fresh_) {
      for (int row = 0; row < height_; ++row) {
        memset(output_ + row * stride_, 0, size_t(width_) * sizeof(uint16_t));
        mark(row);
      }
    }
    force_ = false;
    return fresh_;
  }
  bool fresh() const { return fresh_; }
  bool changed(int row) const {
    return row >= 0 && row < height_ && (dirty_[unsigned(row) >> 5] & (uint32_t(1) << (row & 31)));
  }
  void copy(int row, int x, const uint16_t* source, int count) const {
    if (row < 0 || row >= height_ || !source || count <= 0) return;
    if (x < 0) { const int skip = -x; if (skip >= count) return; source += skip; count -= skip; x = 0; }
    if (count > width_ - x) count = width_ - x;
    if (count <= 0) return;
    uint16_t* target = output_ + row * stride_ + x;
    const size_t bytes = size_t(count) * sizeof(uint16_t);
    if (memcmp(target, source, bytes)) { memcpy(target, source, bytes); mark(row); }
  }
  void pixel(int x, int y, uint16_t color) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    uint16_t& target = output_[y * stride_ + x];
    if (target != color) { target = color; mark(y); }
  }
  uint16_t value(int x, int y) const { return output_[y * stride_ + x]; }

 private:
  mutable uint16_t* output_ = nullptr;
  mutable uint32_t dirty_[8] = {};
  mutable int width_ = 0, height_ = 0, stride_ = 0;
  mutable uint8_t layout_ = 255;
  mutable bool force_ = true, fresh_ = true;
  void mark(int row) const { dirty_[unsigned(row) >> 5] |= uint32_t(1) << (row & 31); }
};

// Copies the cartridge's pixels into a portrait layout without a full NES framebuffer.
struct TetrisViewport {
  int width = 135;
  int height = 240;

  void setLargeCells(bool enabled) { if (largeCells_ != enabled) invalidate(); largeCells_ = enabled; }
  void invalidate() const { damage_.invalidate(); }
  bool rowChanged(int row) const { return damage_.changed(row); }
  void setHeld(uint8_t orientation) { held_ = TetrisActions::spawnOrientation(orientation); }
  void setNext(uint8_t orientation) { next_ = TetrisActions::spawnOrientation(orientation); }
  void setStats(uint32_t score, uint16_t lines, uint8_t level) {
    score_ = score; lines_ = lines; level_ = level;
  }

  // These previews use cell geometry, so every mino has the same dimensions.
  static void preview(uint16_t* output, int width, int height, int stride,
                      uint8_t orientation, int x, int y, int size = 5) {
    orientation = TetrisActions::spawnOrientation(orientation);
    if (orientation == 255) return;
    int minX = 9, minY = 9, maxX = -9;
    for (uint8_t i = 0; i < 4; ++i) {
      int cx, cy; TetrisActions::cell(orientation, i, cx, cy);
      if (cx < minX) minX = cx;
      if (cx > maxX) maxX = cx;
      if (cy < minY) minY = cy;
    }
    x += (24 - (maxX - minX + 1) * size) / 2;
    for (uint8_t i = 0; i < 4; ++i) {
      int cx, cy; TetrisActions::cell(orientation, i, cx, cy);
      for (int dy = 0; dy < size; ++dy) for (int dx = 0; dx < size; ++dx) {
        const int px = x + (cx - minX) * size + dx, py = y + (cy - minY) * size + dy;
        if (px >= 0 && px < width && py >= 0 && py < height)
          output[py * stride + px] = dx && dy && dx < size - 1 && dy < size - 1 ? 0xffff : 0x7bef;
      }
    }
  }

  static void label(uint16_t* output, int width, int height, int stride,
                    const char* text, int x, int y) {
    for (int i = 0; text[i]; ++i) {
      const uint8_t* glyph = MenuFont::glyph(text[i]);
      for (int row = 0; row < MenuFont::Height; ++row)
        for (int col = 0; col < MenuFont::Width; ++col) {
          const int px = x + i * MenuFont::Advance + col, py = y + row;
          if ((glyph[row] & (1u << (MenuFont::Width - col - 1))) &&
              px >= 0 && px < width && py >= 0 && py < height) output[py * stride + px] = 0xffff;
        }
    }
  }

  static void trackedPreview(const TetrisRowDamage& damage, uint8_t orientation, int x, int y) {
    uint16_t pixels[24 * 10] = {};
    preview(pixels, 24, 10, 24, orientation, 0, 0);
    for (int row = 0; row < 10; ++row) damage.copy(y + row, x, pixels + row * 24, 24);
  }

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 131 || screenHeight < 240) return false;
    width = screenWidth < 135 ? screenWidth : 135;
    height = 240;
    invalidate();
    return true;
  }

  // Source rectangles are inclusive. Return true after the lower half completes the frame.
  bool consume(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* output, bool gameplay, int stride = 0) const {
    if (!source || !output || width < 1 || width > 135 || height != 240 || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    const uint8_t layout = !gameplay ? 0 : largeCells_ ? 2 : 1;
    if (y1 == 0 || !damage_.configured(output, width, height, stride, layout)) {
      damage_.begin(output, width, height, stride, layout);
      row_ = -1;
    }
    if (!gameplay) {
      const int pictureHeight = width * 3 / 4;
      const int top = (height - pictureHeight) / 2;
      const int sourceWidth = x2 - x1 + 1;
      for (int y = 0; y < pictureHeight; ++y) {
        const int sy = y * 240 / pictureHeight;
        if (sy < y1 || sy > y2) continue;
        for (int x = 0; x < width; ++x) {
          const int sx = x * 256 / width;
          if (sx >= x1 && sx <= x2) damage_.pixel(x, top + y, source[(sy - y1) * sourceWidth + sx - x1]);
        }
      }
      return y2 == 239;
    }
    if (largeCells_) {
      consumeLarge(x1, y1, x2, y2, source);
      if (y2 == 239) {
        const bool fresh = damage_.fresh();
        if (fresh || previousScore_ != score_ || previousLines_ != lines_ || previousLevel_ != level_) drawStats();
        if (fresh) {
          label(output, width, height, stride, "Hold", 111, 23);
          label(output, width, height, stride, "Next", 111, 67);
        }
        if (fresh || previousHeld_ != held_) trackedPreview(damage_, held_, 111, 39);
        if (fresh || previousNext_ != next_) trackedPreview(damage_, next_, 111, 83);
        rememberHud();
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
      int firstX = region.sx > x1 ? region.sx : x1;
      if (firstX < region.sx - region.dx) firstX = region.sx - region.dx;
      int endX = region.sx + region.w;
      if (endX > x2 + 1) endX = x2 + 1;
      if (endX > region.sx + width - region.dx) endX = region.sx + width - region.dx;
      if (firstX >= endX) continue;
      int firstY = region.sy > y1 ? region.sy : y1;
      if (firstY < region.sy - region.dy) firstY = region.sy - region.dy;
      int endY = region.sy + region.h;
      if (endY > y2 + 1) endY = y2 + 1;
      if (endY > region.sy + height - region.dy) endY = region.sy + height - region.dy;
      for (int sy = firstY; sy < endY; ++sy)
        damage_.copy(region.dy + sy - region.sy, region.dx + firstX - region.sx,
                     source + (sy - y1) * sourceWidth + firstX - x1, endX - firstX);
    }
    if (y2 == 239) {
      if (damage_.fresh()) label(output, width, height, stride, "Hold", 54, 3);
      if (damage_.fresh() || previousHeld_ != held_) trackedPreview(damage_, held_, 54, 18);
      rememberHud();
    }
    return y2 == 239;
  }

 private:
  struct Rect { int sx, sy, w, h, dx, dy; };
  struct ColorCount { uint16_t color; uint8_t count; };
  struct CellColors { ColorCount entries[4]; };
  bool largeCells_ = false;
  uint8_t held_ = 255, next_ = 255, level_ = 0;
  uint16_t lines_ = 0;
  uint32_t score_ = 0;
  mutable int row_ = -1;
  mutable CellColors colors_[10] = {};
  mutable TetrisRowDamage damage_;
  mutable uint8_t previousHeld_ = 255, previousNext_ = 255, previousLevel_ = 0;
  mutable uint16_t previousLines_ = 0;
  mutable uint32_t previousScore_ = 0;

  void rememberHud() const {
    previousHeld_ = held_; previousNext_ = next_; previousLevel_ = level_;
    previousLines_ = lines_; previousScore_ = score_;
  }
  void drawStats() const {
    char stats[32];
    snprintf(stats, sizeof(stats), "S%06lu Ln%03u Lv%u", static_cast<unsigned long>(score_),
             unsigned(lines_), unsigned(level_));
    uint16_t pixels[135];
    for (int row = 0; row < 20; ++row) {
      memset(pixels, 0, size_t(width) * sizeof(uint16_t));
      if (row >= 2 && row < 2 + MenuFont::Height) {
        for (int index = 0; stats[index] && index * MenuFont::Advance < width; ++index) {
          const uint8_t bits = MenuFont::glyph(stats[index])[row - 2];
          for (int column = 0; column < MenuFont::Width; ++column) {
            const int x = index * MenuFont::Advance + column;
            if (x < width && (bits & (1u << (MenuFont::Width - 1 - column)))) pixels[x] = 0xffff;
          }
        }
      }
      damage_.copy(row, 0, pixels, width);
    }
  }

  static int brightness(uint16_t color) {
    return ((color >> 11) & 31) * 2 + ((color >> 5) & 63) * 3 + (color & 31) * 2;
  }

  void paintCell(int column, int row) const {
    const CellColors& cell = colors_[column];
    uint16_t fill = 0, light = 0, dark = 0;
    uint8_t largest = 0;
    int brightest = -1, darkest = 999;
    for (const auto& entry : cell.entries) {
      if (!entry.count || !entry.color) continue;
      if (entry.count > largest) { largest = entry.count; fill = entry.color; }
      const int value = brightness(entry.color);
      if (value > brightest) { brightest = value; light = entry.color; }
      if (value < darkest) { darkest = value; dark = entry.color; }
    }
    if (!damage_.fresh() && damage_.value(column * 11 + 5, 20 + row * 11 + 5) == fill &&
        damage_.value(column * 11 + 1, 20 + row * 11 + 5) == light &&
        damage_.value(column * 11 + 9, 20 + row * 11 + 5) == dark) return;
    uint16_t pixels[11];
    for (int y = 0; y < 11; ++y) {
      for (int x = 0; x < 11; ++x) {
        uint16_t color = fill;
        if (!x || !y || x == 10 || y == 10) color = 0;
        else if (x == 1 || y == 1) color = light;
        else if (x == 9 || y == 9) color = dark;
        pixels[x] = color;
      }
      damage_.copy(20 + row * 11 + y, column * 11, pixels, 11);
    }
  }

  void consumeLarge(int x1, int y1, int x2, int y2, const uint16_t* source) const {
    const int sourceWidth = x2 - x1 + 1;
    for (int sy = y1 > 48 ? y1 : 48; sy <= y2 && sy < 208; ++sy) {
      const int row = (sy - 48) / 8;
      if (row != row_) { memset(colors_, 0, sizeof(colors_)); row_ = row; }
      for (int sx = x1 > 96 ? x1 : 96; sx <= x2 && sx < 176; ++sx) {
        const uint16_t color = source[(sy - y1) * sourceWidth + sx - x1];
        CellColors& cell = colors_[(sx - 96) / 8];
        for (auto& entry : cell.entries) {
          if (!entry.count || entry.color == color) { entry.color = color; ++entry.count; break; }
        }
      }
      if ((sy - 48) % 8 == 7) for (int column = 0; column < 10; ++column) paintCell(column, row);
    }
  }
};

}  // namespace flappy
