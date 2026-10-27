#include "../firmware/flappy/tetris_input.h"
#include <assert.h>
#include <limits>
#include <iostream>

static void buttons() {
  for (uint8_t preset = 0; preset < 2; ++preset) {
    flappy::TetrisButtons buttons;
    buttons.update(false, false, false, preset, true, 0);
    assert(buttons.update(false, false, true, preset, true, 1).pressed == (preset ? 64 : 2));
    auto center = buttons.update(true, false, false, preset, true, 10);
    assert(center.pressed == 32 && center.held == 0);
    center = buttons.update(true, false, false, preset, true, 20);
    assert(center.pressed == 0 && center.held == 0);
    center = buttons.update(false, false, false, preset, true, 30);
    assert(center.pressed == 0 && center.held == 0);
    auto right = buttons.update(false, true, false, preset, true, 40);
    assert(right.pressed == 0 && right.held == 0);
    right = buttons.update(false, false, false, preset, true, 50);
    assert(right.pressed == (preset ? 128 : 1) && right.held == 0);
    buttons.update(false, true, false, preset, true, 100);
    assert(buttons.update(false, true, false, preset, true, 800).held == 0);
    assert(buttons.update(false, false, false, preset, true, 810).pressed == 0);
    buttons.update(true, true, true, preset, false, 900);
    const auto heldOnEntry = buttons.update(true, true, true, preset, true, 1000);
    assert(heldOnEntry.held == 0 && heldOnEntry.pressed == 0);
    buttons.update(false, false, false, preset, true, 1010);
    assert(buttons.update(true, false, false, preset, true, 1020).pressed == 32);
    buttons.update(false, false, false, preset, true, 1030);
    const uint32_t start = UINT32_MAX - 100;
    buttons.update(false, true, false, preset, true, start);
    assert(buttons.update(false, false, false, preset, true, uint32_t(start + 200)).pressed == (preset ? 128 : 1));
  }
}

static void frontHoldThresholdAndWrap() {
  for (uint8_t preset = 0; preset < 2; ++preset) {
    for (uint32_t start : {uint32_t(1000), uint32_t(UINT32_MAX - 100)}) {
      flappy::TetrisButtons buttons;
      buttons.update(false, false, false, preset, true, uint32_t(start - 1));
      auto input = buttons.update(true, false, false, preset, true, start);
      assert(input.pressed == 32 && input.held == 0);
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 199));
      assert(input.pressed == 0 && input.held == 0);
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 200));
      assert(input.pressed == 0 && input.held == 32);
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 1000));
      assert(input.pressed == 0 && input.held == 32);
      input = buttons.update(false, false, false, preset, true, uint32_t(start + 1001));
      assert(input.pressed == 0 && input.held == 0);
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 1002));
      assert(input.pressed == 32 && input.held == 0);
      buttons.update(true, false, false, preset, false, uint32_t(start + 1003));
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 1400));
      assert(input.pressed == 0 && input.held == 0);
      buttons.update(false, false, false, preset, true, uint32_t(start + 1401));
      input = buttons.update(true, false, false, preset, true, uint32_t(start + 1402));
      assert(input.pressed == 32 && input.held == 0);
    }
  }
}

static void movementTilt() {
  flappy::TetrisTilt tilt;
  assert(tilt.update(0, true, 0).held == 0);
  uint8_t direction = 0;
  for (int i = 0; i < 20; ++i) direction = tilt.update(0.6f, true, 0).held;
  assert(direction == 128);
  for (int i = 0; i < 20; ++i) direction = tilt.update(0.18f, true, 0).held;
  assert(direction == 128);
  for (int i = 0; i < 20; ++i) direction = tilt.update(0, true, 0).held;
  assert(direction == 0);
  for (int i = 0; i < 20; ++i) direction = tilt.update(-0.6f, true, 0).held;
  assert(direction == 64);
  tilt.calibrate(-0.6f);
  assert(tilt.update(-0.6f, true, 0).held == 0);
  assert(tilt.update(0, false, 0).held == 0);
  assert(tilt.update(std::numeric_limits<float>::quiet_NaN(), true, 0).held == 0);
}

static void rotationTilt() {
  flappy::TetrisTilt tilt;
  tilt.calibrate(0);
  unsigned cw = 0, ccw = 0;
  for (int i = 0; i < 100; ++i) {
    const auto input = tilt.update(0.6f, true, 1);
    assert(input.held == 0);
    cw += input.pressed == 1;
    ccw += input.pressed == 2;
  }
  assert(cw == 1 && ccw == 0);
  for (int i = 0; i < 100; ++i) {
    const auto input = tilt.update(-0.6f, true, 1);
    assert(input.held == 0 && input.pressed == 0);
  }
  for (int i = 0; i < 20; ++i) assert(tilt.update(0, true, 1).pressed == 0);
  for (int i = 0; i < 100; ++i) {
    const auto input = tilt.update(-0.6f, true, 1);
    ccw += input.pressed == 2;
  }
  assert(ccw == 1);
  tilt.update(-0.6f, false, 1);
  for (int i = 0; i < 20; ++i) assert(tilt.update(0.6f, true, 1).pressed == 0);
  for (int i = 0; i < 20; ++i) tilt.update(0, true, 1);
  for (int i = 0; i < 20; ++i) cw += tilt.update(0.6f, true, 1).pressed == 1;
  assert(cw == 2);
  tilt.calibrate(std::numeric_limits<float>::infinity());
  assert(tilt.update(std::numeric_limits<float>::quiet_NaN(), true, 1).pressed == 0);
}

static void singleRowDrop() {
  flappy::TetrisSoftDrop drop;
  assert(!drop.update(true, false, 0, false));
  assert(!drop.update(false, false, 0, true));
  assert(drop.update(true, false, 3, true));
  assert(drop.update(false, false, 3, true));
  assert(drop.update(false, false, 3, true));
  assert(!drop.update(false, false, 4, true));
  assert(!drop.update(false, false, 4, true));
  assert(drop.update(true, true, 4, true));
  assert(drop.update(false, true, 5, true));
  assert(drop.update(false, true, 6, true));
  assert(!drop.update(false, false, 6, true));
  assert(drop.update(true, false, 6, true));
  assert(drop.update(true, false, 6, true));
  assert(drop.update(false, false, 7, true));
  assert(!drop.update(false, false, 8, true));
  assert(drop.update(true, false, 8, true));
  assert(!drop.update(false, false, 8, false));
  assert(!drop.update(false, false, 0, true));
}

int main() {
  buttons();
  frontHoldThresholdAndWrap();
  movementTilt();
  rotationTilt();
  singleRowDrop();
  std::cout << "Tetris two-mode controls, tilt hysteresis and single-row drop checks passed.\n";
}
