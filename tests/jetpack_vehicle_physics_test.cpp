#include "../firmware/flappy/jetpack_vehicle_physics.h"

#include <assert.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace V = flappy::JetpackVehiclePhysics;

static void launchAndHold() {
  const double fit = 92.0 / 240.0;
  for (bool mech : {false, true}) {
    V::State state;
    int32_t velocity = 0;
    double reference = 0;
    for (unsigned tick = 0; tick < 100; ++tick) {
      const unsigned duration = mech ? 20 : 12;
      const double boost = tick == 0 ? (mech ? 6 : 6.5) : tick < duration ? (mech ? .45 : .6) : mech ? .29 : 0;
      reference = (reference + fit * ((mech ? .36 : .3) - boost)) * .98;
      const bool jumped = V::hop(state, mech, true, tick == 0, tick == 0, velocity);
      assert(jumped == (tick == 0));
      assert(std::abs(double(velocity) / V::Unit - reference) < .001);
    }
    // Releasing cancels the remaining held-boost window.
    state = V::State{}; velocity = 0;
    V::hop(state, mech, true, true, true, velocity);
    V::hop(state, mech, false, false, false, velocity);
    assert(!state.boostTicks);
    const int32_t previous = velocity;
    V::hop(state, mech, true, true, false, velocity);
    const int expectedBoost = mech ? 29 : 0;
    const double expected = (double(previous) / V::Unit + fit * ((mech ? .36 : .3) - expectedBoost / 100.0)) * .98;
    assert(std::abs(double(velocity) / V::Unit - expected) < .0001);
  }
}

static void bufferedLanding() {
  for (bool mech : {false, true}) {
    const unsigned window = mech ? 20 : 30;
    V::State state; int32_t velocity = 0;
    assert(!V::hop(state, mech, true, true, false, velocity));
    for (unsigned tick = 1; tick < window - 1; ++tick)
      assert(!V::hop(state, mech, false, false, false, velocity));
    assert(V::hop(state, mech, false, false, true, velocity));
    assert(!V::hop(state, mech, false, false, true, velocity));
    state = V::State{};
    V::hop(state, mech, true, true, false, velocity);
    for (unsigned tick = 1; tick <= window; ++tick)
      assert(!V::hop(state, mech, false, false, tick == window, velocity));
  }
}

static void additiveBirdAndCooldown() {
  V::State state;
  int32_t velocity = V::Unit;
  assert(V::bird(state, true, true, velocity));
  double expected = (1 + (0.2 - 5) * 92 / 240.0) * .98;
  assert(std::abs(double(velocity) / V::Unit - expected) < .0001);
  for (unsigned i = 0; i < 60; ++i) {
    assert(!V::bird(state, true, true, velocity));
    assert(state.birdCooldown == 10);
  }
  for (unsigned i = 0; i < 10; ++i) assert(!V::bird(state, false, false, velocity));
  assert(!state.birdCooldown);
  const int32_t previous = velocity;
  assert(V::bird(state, true, true, velocity));
  expected = (double(previous) / V::Unit + (0.2 - 5) * 92 / 240.0) * .98;
  assert(std::abs(double(velocity) / V::Unit - expected) < .0001);
}

static void markerArcsAndTeleportDelay() {
  V::State state;
  int upper = 0, lower = 0;
  int32_t minimum = 200 * V::Unit, maximum = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    V::advanceMarker(state);
    const int32_t y = V::markerY(state);
    if (y < minimum) minimum = y;
    if (y > maximum) maximum = y;
    upper += y < 35 * V::Unit;
    lower += y > 101 * V::Unit;
    assert(y >= 22 * V::Unit && y <= 114 * V::Unit);
  }
  assert(minimum < 23 * V::Unit && maximum > 113 * V::Unit);
  assert(upper > 250 && lower > 250);
  state = V::State{};
  const int32_t before = V::markerY(state);
  auto event = V::teleporter(state, true);
  assert(event.started && !event.arrived);
  for (unsigned i = 1; i < 12; ++i) {
    event = V::teleporter(state, true);
    assert(!event.started && !event.arrived && V::markerY(state) == before);
  }
  event = V::teleporter(state, false);
  assert(event.arrived && state.teleportStage == 2 && V::markerY(state) == before);
  for (unsigned i = 0; i < 11; ++i) {
    event = V::teleporter(state, true);
    assert(!event.started && !event.arrived);
  }
  event = V::teleporter(state, false);
  assert(!state.teleportStage && V::markerY(state) != before);
}

static void horizontalSpeedProfiles() {
  const unsigned starts[] = {250,400,200,250,250,300,250};
  const unsigned maxima[] = {1000,6000,1000,1000,1000,750,800};
  const double increments[] = {.25,.40,.22,.09,.25,.15,.06};
  for (unsigned ride = 2; ride <= 8; ++ride) {
    V::State state; V::beginSpeed(state, uint8_t(ride), 175 * V::Unit);
    double expected = starts[ride - 2];
    for (unsigned tick = 1; tick <= 1500; ++tick) {
      if (tick > 1) expected = std::min(double(maxima[ride - 2]),expected + std::sin(std::min((tick - 1) * 3.0,90.0) * std::acos(-1.0) / 180) * increments[ride - 2]);
      V::advanceSpeed(state,uint8_t(ride));
      assert(std::abs(double(state.nativeSpeed) / V::Unit - expected) < .012);
      assert(V::fittedSpeed(state,115) <= 1000 * V::Unit);
    }
    assert(!state.slowing && state.speedAge == 30);
    V::beginSpeed(state,uint8_t(ride),480 * V::Unit);
    expected = 480;
    bool decelerating = true;
    unsigned age = 0;
    for (unsigned tick = 0; tick < 120; ++tick) {
      if (decelerating) {
        expected = std::max(double(starts[ride - 2]),expected * .976);
        decelerating = expected > starts[ride - 2];
      } else {
        ++age;
        expected = std::min(double(maxima[ride - 2]),expected + std::sin(std::min(age * 3.0,90.0) * std::acos(-1.0) / 180) * increments[ride - 2]);
      }
      V::advanceSpeed(state,uint8_t(ride));
      assert(std::abs(double(state.nativeSpeed) / V::Unit - expected) < .002);
    }
    for (unsigned tick = 0; tick < 30000; ++tick) V::advanceSpeed(state,uint8_t(ride));
    assert(state.nativeSpeed <= int32_t(maxima[ride - 2]) * V::Unit);
  }
  V::State hog; V::beginSpeed(hog,3,175 * V::Unit);
  for (unsigned i = 0; i < 600; ++i) V::advanceSpeed(hog,3);
  assert(std::abs(double(hog.nativeSpeed) / V::Unit - 635.437691859) < .004);
}

int main() {
  launchAndHold(); bufferedLanding(); additiveBirdAndCooldown(); markerArcsAndTeleportDelay(); horizontalSpeedProfiles();
  std::cout << "Jetpack vehicle physics tests passed\n";
}
