#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {

// New level objects for the fingerprinted Japan/USA cartridge. Coordinates use
// its native 16-pixel columns. Every course has continuous ground and an exit.
class MarioClock {
 public:
  static constexpr size_t MaxLevelBytes = 96;
  static constexpr unsigned Tile = 8, ClockWidth = 17 * Tile, ClockHeight = 5 * Tile;
  static constexpr int ClockY = 128;
  struct Obstacle { uint16_t x; uint8_t width, height; };

  void generate(uint32_t seed) {
    seed_ = random_ = seed ? seed : 1;
    levelBytes_ = obstacleCount_ = clockCount_ = 0;
    pause_ = jump_ = 0;
    seenClocks_ = 0;
    frames_ = stalled_ = 0;
    previousX_ = 0;
    tileReady_ = false; pipeExit_ = false;
    next();
    pages_ = uint8_t(8 + next() % 3);
    append(0x50);  // Native 400-count timer and ground-level entry.
    append(uint8_t(((next() % 4) << 4) | 1));
    unsigned previousPage = 0;
    for (unsigned page = 0; page < pages_; ++page) {
      bool first = true;
      const auto object = [&](unsigned column, unsigned row, unsigned code) {
        const uint8_t advance = first && page != previousPage ? 0x80 : 0;
        append(uint8_t((column << 4) | row)); append(uint8_t(code | advance));
        first = false; previousPage = page;
      };
      if (page == pages_ - 1u) {
        object(2, 9, 0x43);  // Four coins on the approach.
        object(9, 13, 0x41); // Native flagpole and its solid base.
        object(13, 15, 0x26); // Small castle, starting at row six.
        finishX_ = uint16_t(page * 256 + 9 * 16);
      } else if (page % 3 == 1) {
        object(1, 9, 0x43);
        if (clockCount_ < 3) clocks_[clockCount_++] = uint16_t(page * 256 + 64);
      } else {
        const unsigned column = page % 3 == 2 ? 11 : 8 + next() % 3;
        const unsigned height = 1 + next() % 2;
        object(1, 7, 0x43);
        const bool pipe = (next() & 1) != 0;
        if (pipe) object(column, 11 - height, 0x70 | height);
        else object(column, 11 - height, 0x60 | (height - 1));
        if (obstacleCount_ < 12)
          obstacles_[obstacleCount_++] = {uint16_t(page * 256 + column * 16),
              uint8_t(pipe ? 32 : 16), uint8_t(height * 16)};
      }
    }
    append(0xfd);
  }

  // Four repeatable course recipes. The long empty pages are intentional
  // clearings for a portrait clock; obstacle pages have measured jump leads.
  void buildCourse(unsigned recipe) {
    recipe %= 4;
    generate(recipe + 1);
    levelBytes_ = obstacleCount_ = clockCount_ = 0;
    pages_ = 7; pipeExit_ = recipe < 2;
    append(0x50); append(recipe ? 0x01 : 0x21);
    for (unsigned page = 0; page < pages_; ++page) {
      bool first = true;
      const auto object = [&](unsigned column,unsigned row,unsigned code) {
        append(uint8_t(column * 16 + row));
        append(uint8_t(code | (first && page ? 0x80 : 0))); first = false;
      };
      const auto solid = [&](unsigned column,unsigned height,bool pipe) {
        object(column,11 - height,(pipe ? 0x70 : 0x60) | (pipe ? height : height - 1));
        obstacles_[obstacleCount_++] = {uint16_t(page * 256 + column * 16),uint8_t(pipe ? 32 : 16),uint8_t(height * 16)};
      };
      if (page == 1 || page == 4) {
        object(0,11,0x60); clocks_[clockCount_++] = uint16_t(page * 256 + 64);
      } else if (page == 0) {
        object(1,7,0x43); solid(9,recipe == 2 ? 3 : 2,true);
      } else if (page == 2) {
        object(1,7,0x43);
        solid(5,1,false); solid(6,2,false); solid(7,3,false);
      } else if (page == 3) {
        object(1,7,0x43); solid(4,1,true); solid(11,2,true);
      } else if (page == 5) {
        object(1,7,0x43); object(7,7,0x00); solid(11,recipe == 3 ? 3 : 2,false);
      } else {
        object(2,9,0x43);
        if (pipeExit_) {
          object(9,9,0x7a); // Enterable pipe, with a separate destination record.
          obstacles_[obstacleCount_++] = {uint16_t(page * 256 + 9 * 16),32,32};
        } else object(9,13,0x41);
        object(13,15,0x26);
        finishX_ = uint16_t(page * 256 + 9 * 16);
      }
    }
    append(0xfd);
  }

  void setPortrait(bool value) { portrait_ = value; }

  template<class Runtime>
  bool install(Runtime& runtime) const {
    const uint8_t endEnemies = 0xff;
    return runtime.setPrgCache(3) &&
        runtime.patchCachedPrg(0, 0x1f01, &endEnemies, 1) &&
        runtime.patchCachedPrg(1, 0x068e, level_, levelBytes_);
  }

  // The bot sends ordinary NES buttons. Jump height and movement remain the
  // original cartridge's physics. Clearings let the clock stay readable.
  template<class Runtime>
  uint8_t input(const Runtime& runtime) {
    ++frames_;
    const uint16_t x = uint16_t(runtime.readRam(0x6d) * 256u + runtime.readRam(0x86));
    if (x == previousX_) { if (stalled_ < 65535) ++stalled_; }
    else stalled_ = 0;
    previousX_ = x;
    if (runtime.readRam(0x770) != 1 || runtime.readRam(0x772) != 3 || runtime.readRam(0x0e) != 8) return 0;
    const bool grounded = runtime.readRam(0x1d) == 0;
    if ((grounded || portrait_) && !jump_) {
      for (unsigned i = 0; i < clockCount_; ++i) {
        if (!(seenClocks_ & (1u << i)) && x >= clocks_[i] + (portrait_ ? 5 : 40) && x < clocks_[i] + (portrait_ ? 37 : 72)) {
          seenClocks_ |= uint8_t(1u << i);
          pause_ = 180;
          break;
        }
      }
    }
    // Brake before stopping so the native run inertia cannot slide the clock
    // out of view. The remaining pause uses no horizontal input.
    if (pause_) { --pause_; return pause_ >= 172 ? 64 : 0; }
    if (grounded && !jump_) {
      for (unsigned i = 0; i < obstacleCount_; ++i) {
        const int distance = int(obstacles_[i].x) - int(x);
        if (distance > 0 && distance <= 54) { jump_ = 25; break; }
      }
      const int flagDistance = int(finishX_) - int(x);
      if (flagDistance > 0 && flagDistance <= 50) jump_ = 25;
    }
    const bool jumping = jump_ != 0;
    if (jump_) --jump_;
    return uint8_t(128 | 2 | (jumping ? 1 : 0));
  }

  template<class Runtime>
  bool finished(const Runtime& runtime) const {
    return runtime.readRam(0x75c) != 0 || runtime.readRam(0x75f) != 0 ||
        runtime.readRam(0x770) == 3 || runtime.readRam(0x0e) == 6 ||
        runtime.readRam(0x0e) == 11 || frames_ > 7200 || stalled_ > 720;
  }

  void setTime(uint8_t hour, uint8_t minute, bool twentyFour) {
    hour %= 24; minute %= 60;
    if (!twentyFour) { hour %= 12; if (!hour) hour = 12; }
    digits_[0] = hour / 10; digits_[1] = hour % 10;
    digits_[2] = minute / 10; digits_[3] = minute % 10;
  }

  // Capture a native floor tile from the user's running cartridge. No imported
  // artwork is stored in the source tree or a separate asset pack.
  void captureTile(int x1, int y1, int x2, int y2, const uint16_t* pixels) {
    if (!pixels || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 ||
        x1 > 16 || x2 < 23 || y1 > 208 || y2 < 215) return;
    const unsigned stride = unsigned(x2 - x1 + 1);
    for (unsigned y = 0; y < Tile; ++y)
      memcpy(tile_ + y * Tile, pixels + (208 + y - y1) * stride + 16 - x1, Tile * sizeof(uint16_t));
    tileReady_ = true;
  }

  // Compose into a borrowed strip's scratch copy, before viewport damage is
  // compared. Erasing and scrolling therefore use the normal dirty-row path.
  bool overlay(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* scratch, size_t capacity, uint16_t scrollX) const {
    if (!tileReady_ || !source || !scratch || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 ||
        x2 < x1 || y2 < y1 || y2 < ClockY || y1 >= ClockY + int(ClockHeight)) return false;
    const size_t width = size_t(x2 - x1 + 1), rows = size_t(y2 - y1 + 1);
    if (width * rows > capacity) return false;
    bool copied = false;
    for (unsigned clock = 0; clock < clockCount_; ++clock) {
      const int left = int(clocks_[clock]) - scrollX;
      if (left > x2 || left + int(ClockWidth) <= x1) continue;
      if (!copied) { memcpy(scratch, source, width * rows * sizeof(uint16_t)); copied = true; }
      for (unsigned gy = 0; gy < 5; ++gy) {
        const int tileY = ClockY + int(gy * Tile);
        if (tileY > y2 || tileY + int(Tile) <= y1) continue;
        for (unsigned gx = 0; gx < 17; ++gx) {
          const int tileX = left + int(gx * Tile);
          if (tileX > x2 || tileX + int(Tile) <= x1 || !ink(gx,gy)) continue;
          const int firstX = tileX < x1 ? x1 : tileX, lastX = tileX + int(Tile) - 1 > x2 ? x2 : tileX + int(Tile) - 1;
          const int firstY = tileY < y1 ? y1 : tileY, lastY = tileY + int(Tile) - 1 > y2 ? y2 : tileY + int(Tile) - 1;
          for (int y = firstY; y <= lastY; ++y)
            memcpy(scratch + size_t(y - y1) * width + firstX - x1,
                tile_ + (y - tileY) * Tile + firstX - tileX, size_t(lastX - firstX + 1) * sizeof(uint16_t));
        }
      }
    }
    return copied;
  }

  uint32_t seed() const { return seed_; }
  const uint8_t* level() const { return level_; }
  size_t levelBytes() const { return levelBytes_; }
  unsigned pages() const { return pages_; }
  unsigned clockCount() const { return clockCount_; }
  uint16_t clockX(unsigned index) const { return index < clockCount_ ? clocks_[index] : 0; }
  uint16_t pauseFrames() const { return pause_; }
  unsigned obstacleCount() const { return obstacleCount_; }
  const Obstacle& obstacle(unsigned i) const { return obstacles_[i]; }
  bool tileReady() const { return tileReady_; }
  uint16_t finishX() const { return finishX_; }
  bool pipeExit() const { return pipeExit_; }

 private:
  uint8_t level_[MaxLevelBytes] = {}, levelBytes_ = 0, obstacleCount_ = 0, clockCount_ = 0, pages_ = 0;
  Obstacle obstacles_[12] = {};
  uint16_t clocks_[3] = {}, tile_[Tile * Tile] = {}, finishX_ = 0;
  uint8_t digits_[4] = {1, 2, 0, 0}, jump_ = 0, seenClocks_ = 0;
  uint16_t pause_ = 0, stalled_ = 0, previousX_ = 0;
  uint32_t seed_ = 1, random_ = 1, frames_ = 0;
  bool tileReady_ = false, portrait_ = false, pipeExit_ = false;

  uint32_t next() { random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5; return random_; }
  void append(uint8_t value) { if (levelBytes_ < MaxLevelBytes) level_[levelBytes_++] = value; }
  bool ink(unsigned x, unsigned y) const {
    static const uint8_t glyphs[10][5] = {
      {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
      {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
    };
    if (x == 8) return y == 1 || y == 3;
    const unsigned start[] = {0, 4, 10, 14};
    for (unsigned d = 0; d < 4; ++d)
      if (x >= start[d] && x < start[d] + 3) return (glyphs[digits_[d]][y] & (4u >> (x - start[d]))) != 0;
    return false;
  }
};

}  // namespace flappy
