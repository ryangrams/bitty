#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "menu_font.h"

namespace flappy {

struct ArcadeInput {
  int16_t move = 0;
  bool action = false;
  bool pressed = false;
};

enum class ArcadeCue : uint8_t { None, Fire, Hit, Death, Power, Win };

class ArcadeControls {
 public:
  static constexpr uint32_t MenuHoldMs = 700;
  struct Events {
    bool pause = false;
    bool zero = false;
  };

  void reset() {
    blockAction();
    rightWasDown_ = false;
    rightPressEnabled_ = false;
    rightBlocked_ = true;
  }

  void blockAction() { armed_ = false; }
  bool armed() const { return armed_; }

  ArcadeInput input(int move, bool held, bool pressed) {
    ArcadeInput result;
    result.move = int16_t(move < -1024 ? -1024 : move > 1024 ? 1024 : move);
    if (!armed_) {
      if (!held && !pressed) armed_ = true;
      return result;
    }
    result.action = held;
    result.pressed = pressed;
    return result;
  }

  Events update(bool rightDown, bool leftTap, bool enabled, uint32_t nowMs) {
    Events events;
    events.zero = enabled && leftTap;
    if (!enabled) rightPressEnabled_ = false;
    if (rightBlocked_) {
      rightWasDown_ = rightDown;
      if (!rightDown) rightBlocked_ = false;
      return events;
    }
    if (rightDown && !rightWasDown_) {
      rightPressMs_ = nowMs;
      rightPressEnabled_ = enabled;
    }
    if (!rightDown && rightWasDown_) {
      events.pause = enabled && rightPressEnabled_ && uint32_t(nowMs - rightPressMs_) < MenuHoldMs;
      rightPressEnabled_ = false;
    }
    rightWasDown_ = rightDown;
    return events;
  }

 private:
  bool armed_ = false;
  bool rightWasDown_ = false;
  bool rightPressEnabled_ = false;
  bool rightBlocked_ = false;
  uint32_t rightPressMs_ = 0;
};

class ArcadeTilt {
 public:
  bool calibrate(float sample) {
    if (!isfinite(sample) || sample < -1.5f || sample > 1.5f) return false;
    neutral_ = sample;
    filtered_ = 0;
    return true;
  }
  void reset() { filtered_ = 0; }
  float neutral() const { return neutral_; }
  int16_t update(float sample, bool fresh, uint8_t sensitivity) {
    if (!fresh || !isfinite(sample)) { reset(); return 0; }
    const float span = sensitivity == 0 ? .55f : sensitivity == 2 ? .22f : .35f;
    const float offset = sample - neutral_;
    const float target = fabsf(offset) <= .035f ? 0.f :
        (offset > 0 ? offset - .035f : offset + .035f) / span;
    filtered_ += (target - filtered_) * .45f;
    if (fabsf(filtered_) < .015f) filtered_ = 0;
    const float bounded = filtered_ < -1 ? -1 : filtered_ > 1 ? 1 : filtered_;
    return int16_t(bounded * 1024);
  }
 private:
  float neutral_ = 0, filtered_ = 0;
};

class ArcadeCanvas {
 public:
  enum { Width = 135, Height = 240 };
  explicit ArcadeCanvas(uint16_t* pixels) : pixels_(pixels) {}
  static uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
  }
  void clear(uint16_t color) {
    for (unsigned i = 0; i < Width * Height; ++i) pixels_[i] = color;
  }
  void pixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < Width && y >= 0 && y < Height) pixels_[y * Width + x] = color;
  }
  void rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    const int right = x + w > Width ? Width : x + w;
    const int bottom = y + h > Height ? Height : y + h;
    for (int row = top; row < bottom; ++row)
      for (int col = left; col < right; ++col) pixels_[row * Width + col] = color;
  }
  void line(int x0, int y0, int x1, int y1, uint16_t color) {
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      pixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      const int twice = error * 2;
      if (twice >= dy) { error += dy; x0 += sx; }
      if (twice <= dx) { error += dx; y0 += sy; }
    }
  }
  void text(const char* label, int x, int y, uint16_t color) {
    MenuFont::draw(pixels_, Width, Height, label, x, y, color, 1, 24);
  }
  void number(uint32_t value, int x, int y, uint16_t color) {
    char label[12];
    snprintf(label, sizeof(label), "%lu", static_cast<unsigned long>(value));
    text(label, x, y, color);
  }
 private:
  uint16_t* pixels_;
};

}  // namespace flappy
