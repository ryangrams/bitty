#include "../firmware/flappy/nes_system_button.h"

#include <assert.h>
#include <stdint.h>
#include <iostream>

using flappy::NesSystemButton;

static void arm(NesSystemButton& button, uint32_t now = 0) {
  assert(button.update(false, true, now) == 0);
}

static void shortRelease() {
  NesSystemButton button;
  arm(button);
  assert(button.update(true, true, 10) == 0);
  assert(button.update(true, true, 100) == 0);
  assert(button.update(true, true, 709) == 0);
  assert(button.update(false, true, 709) == 8);
  assert(button.update(false, true, 800) == 0);
  assert(button.update(true, true, 900) == 0);
  assert(button.update(false, true, 901) == 8);
}

static void holdSelectOnce() {
  NesSystemButton button;
  arm(button);
  assert(button.update(true, true, 100) == 0);
  assert(button.update(true, true, 799) == 0);
  assert(button.update(true, true, 800) == 4);
  assert(button.update(true, true, 801) == 0);
  assert(button.update(true, true, 9000) == 0);
  assert(button.update(false, true, 9001) == 0);
  assert(button.update(false, true, 9002) == 0);
  assert(button.update(true, true, 10000) == 0);
  assert(button.update(false, true, 10001) == 8);
}

static void unpolledHoldRelease() {
  NesSystemButton button;
  arm(button);
  assert(button.update(true, true, 10) == 0);
  assert(button.update(false, true, 710) == 4);
  assert(button.update(false, true, 711) == 0);
  assert(button.update(true, true, 1000) == 0);
  assert(button.update(false, true, 2000) == 4);
}

static void disabledInputsDoNothing() {
  NesSystemButton button;
  assert(button.update(false, false, 0) == 0);
  assert(button.update(true, false, 10) == 0);
  assert(button.update(true, false, 10000) == 0);
  assert(button.update(false, false, 10001) == 0);
  arm(button, 10002);
  assert(button.update(true, true, 10003) == 0);
  assert(button.update(false, true, 10004) == 8);
}

static void heldMenuEntryIsBlocked() {
  NesSystemButton button;
  assert(button.update(true, false, 0) == 0);
  assert(button.update(true, true, 1) == 0);
  assert(button.update(true, true, 10000) == 0);
  assert(button.update(false, true, 10001) == 0);
  assert(button.update(true, true, 10002) == 0);
  assert(button.update(false, true, 10003) == 8);

  button.reset();
  assert(button.update(true, true, 11000) == 0);
  assert(button.update(true, true, 12000) == 0);
  assert(button.update(false, true, 12001) == 0);
  assert(button.update(true, true, 12002) == 0);
  assert(button.update(false, true, 12003) == 8);
}

static void deactivationDropsPendingEvents() {
  NesSystemButton button;
  arm(button);
  assert(button.update(true, true, 10) == 0);
  assert(button.update(true, false, 20) == 0);
  assert(button.update(true, true, 1000) == 0);
  assert(button.update(false, true, 1001) == 0);
  assert(button.update(true, true, 2000) == 0);
  assert(button.update(false, false, 2010) == 0);
  arm(button, 2011);
  assert(button.update(true, true, 2012) == 0);
  assert(button.update(false, true, 2013) == 8);

  assert(button.update(true, true, 3000) == 0);
  assert(button.update(true, true, 3700) == 4);
  assert(button.update(true, false, 3701) == 0);
  assert(button.update(true, true, 3702) == 0);
  assert(button.update(false, true, 3703) == 0);
}

static void resetCancelsPendingAndBlocksHeldButton() {
  NesSystemButton button;
  arm(button);
  assert(button.update(true, true, 10) == 0);
  button.reset(true);
  assert(button.update(true, true, 1000) == 0);
  assert(button.update(true, true, 2000) == 0);
  assert(button.update(false, true, 2001) == 0);
  assert(button.update(true, true, 3000) == 0);
  assert(button.update(false, true, 3001) == 8);
  assert(button.update(true, true, 4000) == 0);
  button.reset();
  assert(button.update(false, true, 5000) == 0);
  assert(button.update(true, true, 5001) == 0);
  assert(button.update(true, true, 5701) == 4);
}

static void clockWrapIsSafe() {
  NesSystemButton button;
  const uint32_t start = UINT32_MAX - 300;
  arm(button, start - 1);
  assert(button.update(true, true, start) == 0);
  assert(button.update(true, true, uint32_t(start + 699)) == 0);
  assert(button.update(true, true, uint32_t(start + 700)) == 4);
  assert(button.update(false, true, uint32_t(start + 701)) == 0);

  button.reset();
  arm(button, start - 1);
  assert(button.update(true, true, start) == 0);
  assert(button.update(false, true, uint32_t(start + 500)) == 8);

  button.reset();
  arm(button, start - 1);
  assert(button.update(true, true, start) == 0);
  assert(button.update(false, true, uint32_t(start + 700)) == 4);
}

int main() {
  static_assert(NesSystemButton::HoldMs == 700, "Select hold threshold changed");
  shortRelease();
  holdSelectOnce();
  unpolledHoldRelease();
  disabledInputsDoNothing();
  heldMenuEntryIsBlocked();
  deactivationDropsPendingEvents();
  resetCancelsPendingAndBlocksHeldButton();
  clockWrapIsSafe();
  std::cout << "NES system button checks passed.\n";
}
