#pragma once
#include <stdint.h>

namespace flappy {

struct GeometryInput {
  uint8_t held = 0;
  uint8_t pressed = 0;
};

struct GeometryPlayInput {
  uint8_t buttons = 0;
  bool openSettings = false;
};

class GeometryController {
 public:
  void reset() { startHeld_ = false; }
  GeometryPlayInput update(uint8_t nativeButtons) {
    GeometryPlayInput result;
    const bool start = (nativeButtons & 8) != 0;
    result.openSettings = start && !startHeld_;
    startHeld_ = start;
    result.buttons = (nativeButtons & 3) ? 1 : 0;
    return result;
  }
 private:
  bool startHeld_ = false;
};

class GeometryButtons {
 public:
  static constexpr uint32_t MenuHoldMs = 700;

  void reset() {
    armed_ = front_ = right_ = false;
    rightAt_ = 0;
  }

  GeometryInput update(bool frontHeld, bool rightHeld, bool leftClicked,
                       bool enabled, uint32_t nowMs) {
    GeometryInput result;
    if (!enabled) { reset(); return result; }
    if (!armed_) {
      if (!frontHeld && !rightHeld && !leftClicked) armed_ = true;
      return result;
    }
    if (frontHeld) {
      result.held = 1;
      if (!front_) result.pressed |= 1;
    }
    if (leftClicked) result.pressed |= 64;
    if (rightHeld && !right_) rightAt_ = nowMs;
    if (!rightHeld && right_ && uint32_t(nowMs - rightAt_) < MenuHoldMs)
      result.pressed |= 128;
    front_ = frontHeld;
    right_ = rightHeld;
    return result;
  }

 private:
  uint32_t rightAt_ = 0;
  bool armed_ = false, front_ = false, right_ = false;
};

}  // namespace flappy
