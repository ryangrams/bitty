#pragma once
#include <stdint.h>

namespace flappy {

struct TetrisInput {
  uint8_t held = 0;
  uint8_t pressed = 0;
};

// The power key supplies clicks. The center and right keys supply held states.
class TetrisButtons {
 public:
  void reset() { armed_ = false; front_ = right_ = false; }

  TetrisInput update(bool front, bool right, bool leftClick, uint8_t preset,
                     bool enabled, uint32_t now) {
    TetrisInput result;
    if (!enabled) { reset(); return result; }
    if (!armed_) {
      if (!front && !right) armed_ = true;
      return result;
    }
    if (front && !front_) { result.pressed |= 32; frontAt_ = now; }
    if (front && uint32_t(now - frontAt_) >= 200) result.held |= 32;
    if (right && !right_) rightAt_ = now;
    if (leftClick) result.pressed |= preset == 1 ? 64 : 2;
    if (!right && right_ && uint32_t(now - rightAt_) < 700) result.pressed |= preset == 1 ? 128 : 1;
    front_ = front;
    right_ = right;
    return result;
  }

 private:
  uint32_t rightAt_ = 0, frontAt_ = 0;
  bool armed_ = false, front_ = false, right_ = false;
};

class TetrisTilt {
 public:
  void calibrate(float x) {
    if (!(x >= -2 && x <= 2)) return;
    center_ = filtered_ = x;
    direction_ = 0;
    rotationArmed_ = calibrated_ = true;
  }

  TetrisInput update(float x, bool enabled, uint8_t preset) {
    TetrisInput result;
    if (!enabled || !(x >= -2 && x <= 2)) {
      direction_ = 0;
      rotationArmed_ = false;
      return result;
    }
    if (!calibrated_) calibrate(x);
    filtered_ += (x - filtered_) * 0.25f;
    const float offset = filtered_ - center_;
    if (offset > 0.24f) direction_ = 1;
    else if (offset < -0.24f) direction_ = -1;
    else if (offset > -0.12f && offset < 0.12f) {
      direction_ = 0;
      if (x - center_ > -0.12f && x - center_ < 0.12f) rotationArmed_ = true;
    }
    if (preset == 1) {
      if (direction_ && rotationArmed_) {
        result.pressed = direction_ < 0 ? 2 : 1;
        rotationArmed_ = false;
      }
    } else result.held = direction_ < 0 ? 64 : direction_ > 0 ? 128 : 0;
    return result;
  }

  uint8_t update(float x, bool enabled) {
    return update(x, enabled, 0).held;
  }
 private:
  float center_ = 0, filtered_ = 0;
  int8_t direction_ = 0;
  bool calibrated_ = false, rotationArmed_ = false;
};

// Sample before each emulated frame. A tap ends after its first row movement.
class TetrisSoftDrop {
 public:
  void reset() { pending_ = 0; }
  bool update(bool tapped, bool held, uint8_t pieceY, bool active) {
    if (!active) { reset(); return false; }
    if (pending_ && pieceY != startY_) { --pending_; startY_ = pieceY; }
    if (tapped) {
      if (!pending_) startY_ = pieceY;
      if (pending_ < 255) ++pending_;
    }
    return pending_ || held;
  }
 private:
  uint8_t startY_ = 0, pending_ = 0;
};

}  // namespace flappy
