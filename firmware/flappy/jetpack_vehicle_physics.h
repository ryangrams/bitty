#pragma once

#include <stdint.h>
#include <math.h>

namespace flappy {
namespace JetpackVehiclePhysics {

enum { Unit = 65536 };

inline int32_t nearest(int64_t value, int64_t divisor) {
  return int32_t(value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor);
}

inline int32_t fitHundredths(int32_t native) {
  return nearest(int64_t(native) * 23 * Unit, 6000);
}

struct State {
  uint8_t boostTicks = 0, tapBuffer = 0, birdCooldown = 0;
  int32_t markerNative = 0;
  uint16_t arcTenths = 0;
  uint8_t teleportStage = 0, teleportTicks = 0;
  bool markerUp = true;
  int32_t nativeSpeed = 0;
  uint8_t speedAge = 0;
  bool slowing = false;
};

struct SpeedProfile { uint16_t start, maximum; uint8_t increaseHundredths; };

inline SpeedProfile speedProfile(uint8_t ride) {
  switch (ride) {
    case 3: return {400, 6000, 40};
    case 4: return {200, 1000, 22};
    case 5: return {250, 1000, 9};
    case 7: return {300, 750, 15};
    case 8: return {250, 800, 6};
    default: return {250, 1000, 25};
  }
}

inline void beginSpeed(State& state, uint8_t ride, int32_t currentNative) {
  (void)ride;
  state.nativeSpeed = currentNative;
  state.speedAge = 0;
  state.slowing = true;
}

inline void advanceSpeed(State& state, uint8_t ride) {
  const SpeedProfile profile = speedProfile(ride);
  if (state.slowing) {
    state.nativeSpeed = nearest(int64_t(state.nativeSpeed) * 976, 1000);
    const int32_t start = int32_t(profile.start) * Unit;
    if (state.nativeSpeed <= start) { state.nativeSpeed = start; state.slowing = false; }
    return;
  }
  if (state.speedAge < 30) ++state.speedAge;
  const int32_t sine = state.speedAge == 30 ? Unit : int32_t(lroundf(sinf(float(state.speedAge) * 0.05235987755982989f) * Unit));
  state.nativeSpeed += nearest(int64_t(sine) * profile.increaseHundredths, 100);
  const int32_t maximum = int32_t(profile.maximum) * Unit;
  if (state.nativeSpeed > maximum) state.nativeSpeed = maximum;
}

inline int32_t fittedSpeed(const State& state, unsigned difficultyPercent) {
  int32_t native = nearest(int64_t(state.nativeSpeed) * difficultyPercent, 100);
  // The native Hog cap is 6000. The small display caps final scroll at 1000.
  if (native > 1000 * Unit) native = 1000 * Unit;
  return native;
}

// First-frame impulse, held boost and gravity share the same acceleration step.
inline bool hop(State& state, bool mech, bool held, bool pressed, bool grounded, int32_t& velocity) {
  if (state.tapBuffer) --state.tapBuffer;
  if (pressed) state.tapBuffer = uint8_t(mech ? 20 : 30);
  const bool jump = grounded && state.tapBuffer;
  int boost = 0;
  if (jump) {
    state.tapBuffer = 0;
    state.boostTicks = uint8_t((mech ? 20 : 12) - 1);
    boost = mech ? 600 : 650;
  } else if (held && state.boostTicks) {
    boost = mech ? 45 : 60;
    --state.boostTicks;
  } else {
    state.boostTicks = 0;
    if (held && mech && !grounded) boost = 29;
  }
  velocity = nearest(int64_t(velocity + fitHundredths((mech ? 36 : 30) - boost)) * 98, 100);
  return jump;
}

// The release cooldown prevents rapid taps from stacking unrestricted impulses.
inline bool bird(State& state, bool held, bool pressed, int32_t& velocity) {
  const bool flap = pressed && !state.birdCooldown;
  if (flap) {
    velocity -= fitHundredths(500);
    state.birdCooldown = 10;
  } else if (!held && state.birdCooldown) --state.birdCooldown;
  velocity = nearest(int64_t(velocity + fitHundredths(20)) * 98, 100);
  return flap;
}

inline int32_t markerY(const State& state) {
  return 68 * Unit - nearest(int64_t(state.markerNative) * 23, 60);
}

inline void advanceMarker(State& state) {
  const int32_t end = 88 * Unit;
  const bool upperArc = state.markerNative > end && state.arcTenths <= 1800;
  const bool lowerArc = state.markerNative < -end && state.arcTenths >= 1800;
  if (upperArc || lowerArc) {
    state.arcTenths = uint16_t(state.arcTenths + 16);
    const float radians = float(state.arcTenths) * 0.0017453292519943296f;
    state.markerNative = (upperArc ? end : -end) + int32_t(lroundf(sinf(radians) * 32 * Unit));
    state.markerUp = !upperArc;
    if (state.arcTenths >= 3600) state.arcTenths = 0;
  } else {
    state.markerNative += (state.markerUp ? 8 : -8) * Unit;
    state.arcTenths = state.markerUp ? 0 : 1800;
  }
}

struct Teleport {
  bool started = false, arrived = false;
};

inline Teleport teleporter(State& state, bool pressed) {
  Teleport event;
  if (state.teleportStage && state.teleportTicks && !--state.teleportTicks) {
    if (state.teleportStage == 1) {
      event.arrived = true;
      state.teleportStage = 2;
      state.teleportTicks = 12;
    } else state.teleportStage = 0;
  }
  if (!state.teleportStage && pressed) {
    event.started = true;
    state.teleportStage = 1;
    state.teleportTicks = 12;
  }
  if (state.teleportStage != 1 && !event.arrived) advanceMarker(state);
  return event;
}

}  // namespace JetpackVehiclePhysics
}  // namespace flappy
