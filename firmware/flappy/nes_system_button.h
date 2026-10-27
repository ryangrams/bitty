#pragma once
#include <stdint.h>

namespace flappy {

class NesSystemButton {
 public:
  static constexpr uint32_t HoldMs = 700;

  uint8_t update(bool down, bool enabled, uint32_t now) {
    if (!enabled) {
      enabled_ = false;
      pressed_ = down;
      blocked_ = down;
      selected_ = false;
      return 0;
    }
    if (!enabled_) {
      enabled_ = true;
      pressed_ = down;
      blocked_ = down;
      selected_ = false;
      return 0;
    }
    if (blocked_) {
      if (!down) {
        pressed_ = false;
        blocked_ = false;
      }
      return 0;
    }
    if (down) {
      if (!pressed_) {
        pressed_ = true;
        selected_ = false;
        pressedAt_ = now;
      }
      if (!selected_ && elapsed(now) >= HoldMs) {
        selected_ = true;
        return 4;
      }
      return 0;
    }
    if (!pressed_) return 0;
    pressed_ = false;
    if (selected_) {
      selected_ = false;
      return 0;
    }
    return elapsed(now) >= HoldMs ? 4 : 8;
  }

  void reset(bool down = false) {
    enabled_ = false;
    pressed_ = down;
    blocked_ = down;
    selected_ = false;
    pressedAt_ = 0;
  }

 private:
  uint32_t elapsed(uint32_t now) const { return uint32_t(now - pressedAt_); }
  uint32_t pressedAt_ = 0;
  bool enabled_ = false;
  bool pressed_ = false;
  bool blocked_ = false;
  bool selected_ = false;
};

}  // namespace flappy
