#include "../firmware/flappy/buddy_input.h"
#include <assert.h>
#include <iostream>

using namespace flappy;

int main() {
  BuddyControls controls;
  BuddyControlSample sample;
  sample.jumpHeld = true;
  sample.jumpPressed = true;
  assert(!controls.input(sample).jump && !controls.armed());
  sample = BuddyControlSample();
  controls.input(sample);
  assert(controls.armed());
  sample.tiltFresh = true;
  sample.tilt = -450;
  assert(controls.input(sample).lane == 0);
  sample.tilt = -270;
  assert(controls.input(sample).lane == 0);
  sample.tilt = -200;
  assert(controls.input(sample).lane == 1);
  sample.tilt = 600;
  assert(controls.input(sample).lane == 2);
  sample.tiltFresh = false;
  assert(controls.input(sample).lane == -1);
  sample.controller = true;
  sample.step = -1;
  sample.rollPressed = true;
  auto input = controls.input(sample);
  assert(input.lane == -1 && input.step == -1 && input.roll && !input.jump && input.pressed);
  controls.blockAction();
  sample = BuddyControlSample();
  sample.directionHeld = true;
  assert(controls.input(sample).step == 0 && !controls.armed());
  sample.directionHeld = false;
  controls.input(sample);
  sample.jumpHeld = sample.jumpPressed = true;
  input = controls.input(sample);
  assert(input.jump && !input.roll && input.pressed);
  std::cout << "Buddy controls: held-input isolation, separate actions and tilt hysteresis passed\n";
}
