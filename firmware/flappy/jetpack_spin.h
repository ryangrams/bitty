#pragma once

#include <stdint.h>
#include "jetpack_art.h"
#include "jetpack_rewards.h"

namespace flappy {

// Compact reel presentation uses authored stop timing on the native pixel grid.
class JetpackSpin {
 public:
  enum { Duration = 60, WinHold = 150, LossHold = 8 };
  void reset() { age_ = Duration; started_ = false; }
  void begin(const JetpackRewards::Outcome& outcome, uint32_t seed) {
    started_ = true; age_ = 0; seed_ = seed;
    const uint8_t target = outcome.hasPrize ? awardSymbol(outcome.award) : uint8_t(seed % 8);
    symbols_[0] = target;
    symbols_[1] = outcome.hasPrize || outcome.matches >= 2 ? target : uint8_t((target + 1 + (seed >> 8) % 7) % 8);
    symbols_[2] = outcome.hasPrize ? target : uint8_t((target + 1 + (seed >> 16) % 7) % 8);
  }
  void tick() { if (started_ && age_ < Duration) ++age_; }
  bool running() const { return started_ && age_ < Duration; }
  bool started() const { return started_; }
  uint8_t age() const { return age_; }
  static uint8_t stopTick(unsigned reel) { return uint8_t(36 + (reel < 3 ? reel : 2) * 12); }
  bool stopped(unsigned reel) const { return !running() || age_ >= stopTick(reel); }
  uint8_t symbol(unsigned reel) const { return symbols_[reel < 3 ? reel : 2]; }

  template<class Canvas>
  void draw(Canvas& canvas, int left = 66, int top = 55) const {
    using namespace JetpackArt;
    for (unsigned reel = 0; reel < 3; ++reel) {
      const int x = left + int(reel) * 38;
      canvas.rect(x, top, 32, 39, color(MetalShade));
      canvas.rect(x + 1, top + 1, 30, 37, color(BrassShade));
      canvas.rect(x + 2, top + 2, 28, 35, color(PanelShade));
      Clip<Canvas> clip(canvas, x + 3, top + 3, 26, 33);
      const bool stationary = stopped(reel);
      const unsigned phase = unsigned(age_) * (5 + reel) + ((seed_ >> (reel * 5)) & 31u);
      const int offset = stationary ? 0 : int(phase % 22) - 11;
      const uint8_t current = stationary ? symbols_[reel] : uint8_t((phase / 22) % 8);
      for (int row = -1; row <= 1; ++row)
        drawSymbol(clip, x + 16, top + 19 + offset + row * 22,
                   uint8_t((current + row + 8) % 8), age_);
      canvas.pixel(x + 1, top + 19, color(Eye));
      canvas.pixel(x + 30, top + 19, color(Eye));
    }
  }

 private:
  template<class Canvas> class Clip {
   public:
    Clip(Canvas& canvas, int x, int y, int width, int height)
        : canvas_(canvas), x_(x), y_(y), right_(x + width), bottom_(y + height) {}
    void pixel(int x, int y, uint16_t ink) {
      if (x >= x_ && y >= y_ && x < right_ && y < bottom_) canvas_.pixel(x, y, ink);
    }
    void rect(int x, int y, int width, int height, uint16_t ink) {
      const int left = x > x_ ? x : x_, top = y > y_ ? y : y_;
      const int right = x + width < right_ ? x + width : right_, bottom = y + height < bottom_ ? y + height : bottom_;
      if (right > left && bottom > top) canvas_.rect(left, top, right - left, bottom - top, ink);
    }
    void line(int x0, int y0, int x1, int y1, uint16_t ink) {
      const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
      const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x0, y0, ink);
        if (x0 == x1 && y0 == y1) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
      }
    }
   private:
    Canvas& canvas_;
    int x_, y_, right_, bottom_;
  };
  static uint8_t awardSymbol(const JetpackRewards::Award& award) {
    using Kind = JetpackRewards::Kind;
    return award.kind == Kind::Coins ? uint8_t(award.amount >= 2000 ? 2 : award.amount >= 1000 ? 1 : 0)
         : award.kind == Kind::Blast ? 3 : award.kind == Kind::Revive ? 4
         : award.kind == Kind::Tokens ? 5 : award.kind == Kind::DoubleCoins ? 6 : 7;
  }
  template<class Canvas> static void drawSymbol(Canvas& c, int x, int y, uint8_t symbol, uint32_t frame) {
    using namespace JetpackArt;
    if (symbol <= 2) {
      drawCoin(c, x, y, frame);
      if (symbol >= 1) drawCoin(c, x - 4, y + 3, frame + 3);
      if (symbol >= 2) drawCoin(c, x + 4, y - 3, frame + 6);
    } else if (symbol == 3) drawExplosion(c, x, y, 10, 0);
    else if (symbol == 4) {
      c.rect(x - 5, y - 3, 4, 4, color(Red)); c.rect(x + 1, y - 3, 4, 4, color(Red));
      c.line(x - 5, y, x, y + 5, color(Red)); c.line(x + 5, y, x, y + 5, color(Red));
      c.rect(x - 3, y, 7, 3, color(Red)); c.pixel(x - 3, y - 2, color(Eye));
    } else drawPickupIcon(c, x, y, frame, symbol == 5 ? 4 : symbol == 6 ? 3 : 6);
  }
  uint32_t seed_ = 0;
  uint8_t age_ = Duration, symbols_[3] = {};
  bool started_ = false;
};

}  // namespace flappy
