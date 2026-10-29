#pragma once

#include "buddy_game.h"

namespace flappy {

struct BuddyControlSample {
  bool controller = false;
  bool jumpHeld = false, rollHeld = false;
  bool jumpPressed = false, rollPressed = false;
  bool directionHeld = false;
  int8_t step = 0;
  int16_t tilt = 0;
  bool tiltFresh = false;
};

class BuddyControls {
 public:
  void blockAction() { armed_ = false; lane_ = 1; }
  bool armed() const { return armed_; }

  BuddyInput input(const BuddyControlSample& sample) {
    BuddyInput result;
    if (!armed_) {
      if (!sample.jumpHeld && !sample.rollHeld && !sample.jumpPressed &&
          !sample.rollPressed && !sample.directionHeld && !sample.step) armed_ = true;
      return result;
    }
    result.jump = sample.jumpHeld || sample.jumpPressed;
    result.roll = sample.rollHeld || sample.rollPressed;
    result.pressed = sample.jumpPressed || sample.rollPressed;
    if (sample.controller) {
      result.step = sample.step < 0 ? -1 : sample.step > 0 ? 1 : 0;
    } else if (sample.tiltFresh) {
      // Separate enter and exit thresholds prevent lane chatter near neutral.
      if (sample.tilt < -400) lane_ = 0;
      else if (sample.tilt > 400) lane_ = 2;
      else if ((lane_ == 0 && sample.tilt > -220) ||
               (lane_ == 2 && sample.tilt < 220)) lane_ = 1;
      result.lane = lane_;
    }
    return result;
  }

 private:
  bool armed_ = false;
  int8_t lane_ = 1;
};

}  // namespace flappy
