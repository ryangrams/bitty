#include "../firmware/flappy/geometry_launcher.h"

#include <assert.h>
#include <iostream>

using namespace flappy;

static void nativeStartupAndRequestedLevel() {
  for (unsigned target = 0; target < 22; ++target) {
    GeometryLauncher l; l.request(uint8_t(target));
    assert(l.active() && !l.complete() && !l.failed());
    // A soft reset can initially expose the previous frame's game state.
    assert(l.update(2, 13).buttons == 0 && l.active());
    for (uint8_t state : {uint8_t(5), uint8_t(0), uint8_t(1)}) {
      bool pressed = false, releasedAfterPress = false;
      for (unsigned frame = 0; frame < 48; ++frame) {
        const auto a = l.update(state, 0);
        assert(!a.writeLevel && (a.buttons == 0 || a.buttons == 1));
        if (frame < 23) assert(a.buttons == 0);
        if (a.buttons) pressed = true;
        else if (pressed) releasedAfterPress = true;
      }
      assert(pressed && releasedAfterPress);
    }
    uint8_t level = 0;
    unsigned writes = 0;
    bool pressed = false, neutralAfterWrite = false;
    for (unsigned frame = 0; frame < 110; ++frame) {
      const auto a = l.update(6, level);
      if (frame < 89) assert(!a.buttons && !a.writeLevel);
      if (a.writeLevel) {
        assert(frame == 89 && a.level == target && !a.buttons);
        ++writes; level = a.level;
      }
      if (frame > 89 && frame < 93) {
        assert(!a.buttons); neutralAfterWrite = true;
      }
      if (a.buttons) { assert(frame >= 93 && neutralAfterWrite); pressed = true; }
    }
    assert(writes == (target ? 1u : 0u) && level == target && pressed);
    const auto a = l.update(2, level);
    assert(!a.buttons && !a.writeLevel && !l.active() && l.complete() && !l.failed());
    for (unsigned frame = 0; frame < 100; ++frame) {
      const auto stopped = l.update(uint8_t(frame & 7), 99);
      assert(!stopped.buttons && !stopped.writeLevel);
    }
  }
}

static void failuresLeaveGameMemoryUntouched() {
  GeometryLauncher l; l.request(9);
  for (unsigned i = 0; i < 89; ++i) assert(!l.update(6, 0).writeLevel);
  assert(l.update(6, 0).writeLevel);
  assert(!l.update(6, 0).writeLevel && l.failed() && !l.active());
  l.request(9);
  for (unsigned i = 0; i < 90; ++i) l.update(6, 9);
  assert(l.update(2, 10).buttons == 0 && l.failed() && !l.complete());
  l.request(99); assert(l.requestedLevel() == 0);
  for (unsigned i = 0; i < GeometryLauncher::MaxFrames; ++i) {
    const auto a = l.update(0xf1, 0);
    assert(!a.buttons && !a.writeLevel && l.active());
  }
  const auto timedOut = l.update(0xf1, 0);
  assert(!timedOut.buttons && !timedOut.writeLevel && l.failed() && !l.active());
  l.request(1); l.reset(); assert(!l.active() && !l.failed() && !l.complete());
  assert(!l.update(6, 0).writeLevel);
}

static void bootBatchNeverAdvancesGameplay() {
  struct Runtime {
    uint8_t state=6,level=0;unsigned steps=0,playSteps=0;
    uint8_t readRam(unsigned at) const { return at==0x498 ? state : level; }
    void writeRam(unsigned at,uint8_t value) { assert(at==GeometryLauncher::LevelAddress);level=value; }
    bool step(uint8_t buttons) { ++steps;if(state==2)++playSteps;else if(buttons&1)state=2;return true; }
  } runtime;
  GeometryLauncher launch;launch.request(4);
  for(unsigned i=0;i<40 && launch.active();++i)launch.advanceBoot(runtime);
  assert(launch.complete() && !launch.failed() && runtime.level==4 && runtime.playSteps==0);
  const unsigned steps=runtime.steps;
  launch.advanceBoot(runtime);assert(runtime.steps==steps);
}

int main() {
  bootBatchNeverAdvancesGameplay();
  nativeStartupAndRequestedLevel();
  failuresLeaveGameMemoryUntouched();
  std::cout << "Geometry launcher checks passed\n";
}
