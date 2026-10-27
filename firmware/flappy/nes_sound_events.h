#pragma once

#include <stdint.h>

namespace flappy {

struct TetrisSoundSnapshot {
  bool playing = false;
  bool active = false;
  uint8_t pieceY = 0;
  uint16_t lines = 0;
  uint8_t level = 0;
  bool toppedOut = false;
};

enum NesSoundEvent : uint8_t {
  NesSoundNone = 0,
  NesSoundLock = 1,
  NesSoundLineClear = 2,
  NesSoundLevelUp = 4,
  NesSoundCrash = 8
};

class TetrisSoundTracker {
 public:
  void reset() { initialized_ = false; }

  // Sample once after each completed emulated frame, separately for each player.
  uint8_t sample(const TetrisSoundSnapshot& current) {
    uint8_t events = NesSoundNone;
    if (initialized_ && previous_.playing && current.playing && !previous_.toppedOut) {
      // B-Type counts remaining lines downward. Both directions represent a clear.
      if (current.lines != previous_.lines) events |= NesSoundLineClear;
      if (current.level > previous_.level) events |= NesSoundLevelUp;
      if (current.toppedOut) events |= NesSoundCrash;
      if (previous_.active && !current.active &&
          !(events & (NesSoundLineClear | NesSoundCrash))) events |= NesSoundLock;
    }
    previous_ = current;
    initialized_ = true;
    return events;
  }

  static uint8_t priority(uint8_t events) {
    if (events & NesSoundLineClear) return NesSoundLineClear;
    if (events & NesSoundLevelUp) return NesSoundLevelUp;
    if (events & NesSoundCrash) return NesSoundCrash;
    if (events & NesSoundLock) return NesSoundLock;
    return NesSoundNone;
  }

 private:
  TetrisSoundSnapshot previous_;
  bool initialized_ = false;
};

}  // namespace flappy
