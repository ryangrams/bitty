#include "../firmware/flappy/geometry_input.h"

#include <assert.h>
#include <iostream>

using flappy::GeometryButtons;
using flappy::GeometryInput;

static void expect(GeometryInput input, uint8_t held = 0, uint8_t pressed = 0) {
  assert(input.held == held && input.pressed == pressed);
  assert(((input.held | input.pressed) & uint8_t(~(1 | 64 | 128))) == 0);
}

static void jumpAndShipUseTheSameAction() {
  GeometryButtons buttons;
  expect(buttons.update(false, false, false, true, 0));
  expect(buttons.update(true, false, false, true, 10), 1, 1);
  for (uint32_t elapsed : {uint32_t(1), uint32_t(699), uint32_t(700), uint32_t(701), uint32_t(60000)})
    expect(buttons.update(true, false, false, true, 10 + elapsed), 1);
  expect(buttons.update(false, false, false, true, 60011));
  expect(buttons.update(false, false, false, true, 60012));
  expect(buttons.update(true, false, false, true, 60013), 1, 1);
}

static void levelNavigationWorksWhileHoldingJump() {
  GeometryButtons buttons;
  expect(buttons.update(false, false, false, true, 0));
  expect(buttons.update(true, false, true, true, 1), 1, 65);
  expect(buttons.update(true, false, false, true, 2), 1);
  expect(buttons.update(true, true, false, true, 3), 1);
  expect(buttons.update(true, false, false, true, 100), 1, 128);
  expect(buttons.update(true, false, false, true, 101), 1);
  expect(buttons.update(false, true, false, true, 200));
  expect(buttons.update(false, false, false, true, 201), 0, 128);
  expect(buttons.update(false, false, true, true, 202), 0, 64);
}

static void menuHoldNeverBecomesNavigation() {
  for (uint32_t start : {uint32_t(1000), uint32_t(UINT32_MAX - 100)}) {
    for (uint32_t duration : {uint32_t(699), uint32_t(700), uint32_t(701), uint32_t(60000)}) {
      for (bool pollWhileHeld : {false, true}) {
        GeometryButtons buttons;
        expect(buttons.update(false, false, false, true, uint32_t(start - 1)));
        expect(buttons.update(false, true, false, true, start));
        if (pollWhileHeld)
          expect(buttons.update(false, true, false, true, uint32_t(start + duration)));
        expect(buttons.update(false, false, false, true, uint32_t(start + duration)),
               0, duration < GeometryButtons::MenuHoldMs ? 128 : 0);
        expect(buttons.update(false, false, false, true, uint32_t(start + duration + 1)));
      }
    }
  }
}

static void entryAndResetConsumeHeldButtonsUntilNeutral() {
  for (uint8_t held = 1; held < 4; ++held) {
    const bool front = held & 1, right = held & 2;
    GeometryButtons buttons;
    for (unsigned reset = 0; reset < 2; ++reset) {
      buttons.reset();
      expect(buttons.update(front, right, true, true, 0));
      expect(buttons.update(front, right, false, true, 10000));
      expect(buttons.update(true, false, false, true, 10001));
      expect(buttons.update(false, true, false, true, 10002));
      expect(buttons.update(false, false, true, true, 10003));
      expect(buttons.update(false, true, false, true, 10004));
      expect(buttons.update(false, false, false, true, 10005));
      expect(buttons.update(true, false, false, true, 10006), 1, 1);
      expect(buttons.update(true, true, false, true, 10007), 1);
      expect(buttons.update(true, false, false, true, 10008), 1, 128);
    }
  }
}

static void menuOrControllerTakeoverDropsPendingInput() {
  for (bool front : {false, true}) {
    for (bool right : {false, true}) {
      GeometryButtons buttons;
      buttons.update(false, false, false, true, 0);
      buttons.update(front, right, false, true, 10);
      expect(buttons.update(front, right, true, false, 11));
      expect(buttons.update(front, right, false, false, 1000));
      expect(buttons.update(true, true, true, true, 1001));
      expect(buttons.update(false, true, false, true, 1002));
      expect(buttons.update(true, false, false, true, 1003));
      expect(buttons.update(false, false, true, true, 1004));
      expect(buttons.update(false, false, false, true, 1005));
      expect(buttons.update(false, false, true, true, 1006), 0, 64);
      expect(buttons.update(true, true, false, true, 1007), 1, 1);
      expect(buttons.update(true, false, false, true, 1008), 1, 128);
      expect(buttons.update(false, false, false, false, 1009));
      expect(buttons.update(false, false, false, true, 1010));
      expect(buttons.update(true, false, false, true, 1011), 1, 1);
    }
  }
}

int main() {
  flappy::GeometryController controller;
  for (unsigned buttons = 0; buttons < 256; ++buttons) {
    controller.reset();
    const auto input = controller.update(uint8_t(buttons));
    assert(input.buttons == ((buttons & 3) ? 1 : 0));
    assert(input.openSettings == bool(buttons & 8));
    assert(!controller.update(uint8_t(buttons)).openSettings);
    controller.update(0);
    assert(controller.update(8).openSettings);
  }
  static_assert(GeometryButtons::MenuHoldMs == 700, "Menu hold threshold changed");
  jumpAndShipUseTheSameAction();
  levelNavigationWorksWhileHoldingJump();
  menuHoldNeverBecomesNavigation();
  entryAndResetConsumeHeldButtonsUntilNeutral();
  menuOrControllerTakeoverDropsPendingInput();
  std::cout << "Geometry jump, ship holds, navigation and menu input isolation checks passed.\n";
}
