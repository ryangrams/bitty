#pragma once
#include <stdint.h>

namespace flappy {

struct GeometryLaunchAction {
  uint8_t buttons = 0;
  bool writeLevel = false;
  uint8_t level = 0;
};

class GeometryLauncher {
 public:
  static constexpr uint16_t LevelAddress = 0x047b;
  static constexpr uint16_t MaxFrames = 2400;

  // Call after loading or soft-resetting the verified v1.2.8 main cartridge.
  void request(uint8_t level) {
    requested_ = level < 22 ? level : 0;
    frames_ = stateFrames_ = 0;
    lastState_ = 0xff;
    active_ = true; complete_ = failed_ = selected_ = false;
  }
  void reset() { active_ = complete_ = failed_ = selected_ = false; }
  bool active() const { return active_; }
  bool complete() const { return complete_; }
  bool failed() const { return failed_; }
  uint8_t requestedLevel() const { return requested_; }
  uint16_t frames() const { return frames_; }

  // Run a few silent menu frames between presentations. Stop at the first
  // gameplay state so level physics and music keep their normal NTSC cadence.
  template<class Runtime>
  void advanceBoot(Runtime& runtime) {
    for(unsigned i=0;i<3 && active_;++i) {
      const auto action=update(runtime.readRam(0x0498),runtime.readRam(LevelAddress));
      if(!active_) return;
      if(action.writeLevel) runtime.writeRam(LevelAddress,action.level);
      if(!runtime.step(action.buttons)) { active_=false; failed_=true; return; }
    }
  }

  // Apply writeLevel only to LevelAddress, before stepping the returned input.
  // Game state and physics remain under the native cartridge's control.
  GeometryLaunchAction update(uint8_t state, uint8_t observedLevel) {
    GeometryLaunchAction action;
    action.level = requested_;
    if (!active_) return action;
    if (++frames_ > MaxFrames) {
      active_ = false; failed_ = true;
      return action;
    }
    if (state != lastState_) { lastState_ = state; stateFrames_ = 0; }
    ++stateFrames_;
    if (state == 2) {
      if (selected_) {
        active_ = false;
        complete_ = observedLevel == requested_;
        failed_ = !complete_;
      }
      return action;
    }
    if (state == 6) {
      if (stateFrames_ < 90) return action;
      if (!selected_) {
        action.writeLevel = observedLevel != requested_;
        selected_ = true;
        return action;
      }
      if (observedLevel != requested_) {
        active_ = false; failed_ = true;
        return action;
      }
      if (stateFrames_ >= 94) action.buttons = pulse(stateFrames_ - 94);
      return action;
    }
    if (state == 5 || state == 0 || state == 1) {
      if (stateFrames_ >= 24) action.buttons = pulse(stateFrames_ - 24);
    }
    return action;
  }

 private:
  static uint8_t pulse(uint16_t frame) { return frame % 16 < 4 ? 1 : 0; }
  uint16_t frames_ = 0, stateFrames_ = 0;
  uint8_t requested_ = 0, lastState_ = 0xff;
  bool active_ = false, complete_ = false, failed_ = false, selected_ = false;
};

}  // namespace flappy
