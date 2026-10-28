#include "../firmware/flappy/jetpack_progression.h"

#include <algorithm>
#include <array>
#include <assert.h>
#include <iostream>
#include <vector>

namespace flappy {
struct JetpackProgressionTestAccess {
  static void managers(JetpackProgression& p, uint32_t meters) { p.ensureManagers(meters); }
  static void due(JetpackProgression& p, uint32_t power, uint32_t vehicle) {
    p.powerDue_ = power; p.vehicleDue_ = vehicle; p.nextContent_ = 0;
  }
  static uint8_t vehicle(JetpackProgression& p) { return p.chooseVehicle(); }
  static JetpackProgression::Kind ordinary(JetpackProgression& p, const JetpackProgression::Context& c) {
    return p.ordinary(c);
  }
  static void completeOrdinary(JetpackProgression& p, uint32_t d) {
    p.waiting_ = true; p.completeContent(d, 84, 286);
  }
  static void noManagers(JetpackProgression& p) { p.powerDue_ = p.vehicleDue_ = UINT32_MAX / 2; }
};
}
using Policy = flappy::JetpackProgression;
using Access = flappy::JetpackProgressionTestAccess;
using Kind = Policy::Kind;
using Streak = Policy::Streak;

static bool content(Kind kind) { return kind >= Kind::Coins && kind <= Kind::Power; }
static const Policy::Event* find(const Policy::Batch& batch, Kind kind) {
  for (unsigned i = 0; i < batch.count; ++i) if (batch.events[i].kind == kind) return &batch.events[i];
  return nullptr;
}

static void nativeUnitsAndManagerTargets() {
  for (uint32_t native : {0u,18u,280u,470u,18000u,1000000u,18000000u}) {
    const uint32_t pixels = Policy::screenQ8(native);
    const uint32_t roundTrip = Policy::nativeDistance(pixels);
    assert(roundTrip <= native && native - roundTrip <= 1);
  }
  uint16_t minimum = 0, maximum = 0;
  Policy::intervalRange(2499, minimum, maximum); assert(minimum == 280 && maximum == 470);
  Policy::intervalRange(2500, minimum, maximum); assert(minimum == 250 && maximum == 420);
  Policy::intervalRange(9500, minimum, maximum); assert(minimum == 150 && maximum == 230);
  Policy::intervalRange(100000, minimum, maximum); assert(minimum == 150 && maximum == 230);
  assert(Policy::safeGapNative(286) >= 280);
  assert(Policy::safeGapNative(903) > Policy::safeGapNative(286));
  unsigned earlyPower = 0, latePower = 0, earlyVehicle = 0, lateVehicle = 0;
  for (uint32_t seed = 1; seed <= 1024; ++seed) {
    Policy p; p.reset(seed); Access::managers(p, 0);
    assert(p.powerDue() < 300 && p.vehicleDue() < 650);
    earlyPower += p.powerDue() < 50; latePower += p.powerDue() > 250;
    earlyVehicle += p.vehicleDue() < 50; lateVehicle += p.vehicleDue() > 600;
  }
  assert(earlyPower > 50 && latePower > 50 && earlyVehicle > 20 && lateVehicle > 20);

  Policy p; p.reset(87); Access::due(p, 200, 210);
  Policy::Context c; c.nativeDistance = 101 * 18; c.scientistSlot = c.samSlot = c.spinSlot = false;
  const auto first = p.step(c);
  const auto* power = find(first, Kind::Power);
  assert(power && !find(first, Kind::Vehicle));
  assert(power->encounterDistance == 101 * 18 + 577 && power->nativeDistance == 101 * 18 && power->dueMeters == 200);
  p.completeContent(c.nativeDistance, 48, c.speedQ8);
  c.nativeDistance = p.nextContent(); c.powerSlot = false;
  const auto occupied = p.step(c);
  assert(!find(occupied, Kind::Power) && !find(occupied, Kind::Vehicle));
}

static void vehicleHistoryAndRecovery() {
  std::array<unsigned, 6> counts{};
  Policy p; p.reset(7193);
  uint8_t previous = 0;
  for (unsigned i = 0; i < 100000; ++i) {
    const uint8_t vehicle = Access::vehicle(p);
    assert(vehicle >= 2 && vehicle <= 7 && vehicle != previous);
    assert(p.vehicleMultiplier(vehicle - 2) == 0);
    ++counts[vehicle - 2]; previous = vehicle;
  }
  for (unsigned count : counts) assert(count > 16000 && count < 17300);
  const uint8_t suppressed = previous;
  p.reset(871); assert(p.vehicleMultiplier(suppressed - 2) == 0);
  assert(Access::vehicle(p) != suppressed);

  p.acquireVehicle(1000 * 18, 4);
  assert(p.streak() == Streak::Vehicle && p.recoveryUntil() >= 1000 * 18 + 444);
  p.ejectVehicle(1100 * 18, 4);
  assert(p.streak() == Streak::Default && p.vehicleBlockedUntil() == 1570);
  Policy::Context c; c.scientistSlot = c.samSlot = c.spinSlot = false;
  Access::due(p, 100000, 1100);
  c.nativeDistance = 1500 * 18;
  auto batch = p.step(c); assert(!find(batch, Kind::Vehicle));
  p.completeContent(c.nativeDistance, 84, c.speedQ8);
  Access::due(p, 100000, 1100); c.nativeDistance = 1570 * 18;
  batch = p.step(c); assert(!find(batch, Kind::Vehicle));
  p.completeContent(c.nativeDistance, 84, c.speedQ8);
  Access::due(p, 100000, 1100); c.nativeDistance = 1571 * 18;
  batch = p.step(c); assert(find(batch, Kind::Vehicle));
}

static void eligibilityAndFamilies() {
  std::array<unsigned, 7> counts{};
  for (uint32_t seed = 1; seed <= 256; ++seed) {
    Policy p; p.reset(seed);
    Policy::Context c; c.scientistSlot = c.samSlot = c.spinSlot = false;
    Access::noManagers(p);
    for (unsigned i = 0; i < 1000; ++i) {
      c.nativeDistance = i < 250 ? 49 * 18 : i < 500 ? 199 * 18 : i < 750 ? 399 * 18 : 500 * 18;
      const Kind kind = Access::ordinary(p, c);
      if (p.streak() == Streak::Default) {
        if (i < 250) assert(kind == Kind::Zap);
        if (i < 500) assert(kind != Kind::Missile && kind != Kind::Laser);
        if (i < 750) assert(kind != Kind::Laser);
      }
      ++counts[unsigned(kind)]; Access::completeOrdinary(p, c.nativeDistance);
    }
    c.nativeDistance = 1000 * 18; c.hazardsClear = false;
    for (unsigned i = 0; i < 100; ++i) {
      const Kind kind = Access::ordinary(p, c);
      assert(kind != Kind::Missile && kind != Kind::Laser);
      Access::completeOrdinary(p, c.nativeDistance);
    }
    Access::due(p, 1, 1); c.vehicle = 4;
    auto batch = p.step(c); assert(!find(batch, Kind::Power) && !find(batch, Kind::Vehicle));
    p.completeContent(c.nativeDistance, 84, c.speedQ8);
    Access::due(p, 1, 1); c.vehicle = 0; c.safePower = true;
    batch = p.step(c); for (unsigned i = 0; i < batch.count; ++i) assert(!content(batch.events[i].kind));
  }
  assert(counts[unsigned(Kind::Coins)] > 1000 && counts[unsigned(Kind::Missile)] > 1000);
  assert(counts[unsigned(Kind::Laser)] > 1000 && counts[unsigned(Kind::RotatingZap)] > 1000);
}

static void rareStreaksAndFullTrace() {
  unsigned missiles = 0, coins = 0, opportunities = 0;
  uint64_t hash = 1469598103934665603ull;
  for (uint32_t seed = 1; seed <= 32; ++seed) {
    Policy p; p.reset(seed);
    Policy::Context c; c.nativeDistance = 1000 * 18;
    for (unsigned i = 0; i < 10000; ++i) {
      const bool idle = p.streak() == Streak::Default;
      const Kind kind = Access::ordinary(p, c);
      opportunities += idle;
      if (idle && p.streak() == Streak::Missiles) {
        ++missiles; assert(p.streakRemaining() >= 4 && p.streakRemaining() <= 7);
      }
      if (idle && p.streak() == Streak::Coins) {
        ++coins; assert(p.streakRemaining() >= 2 && p.streakRemaining() <= 3);
      }
      hash ^= unsigned(kind); hash *= 1099511628211ull;
      Access::completeOrdinary(p, c.nativeDistance);
    }
  }
  assert(missiles * 1000 > opportunities * 2 && missiles * 1000 < opportunities * 5);
  assert(coins * 10000 > opportunities * 9 && coins * 10000 < opportunities * 21);
  assert(hash != 0);

  Policy a, b; a.reset(9183); b.reset(9183);
  Policy::Context c; c.scientistSlot = false;
  unsigned powers = 0, vehicles = 0, spins = 0, sams = 0, contents = 0;
  uint32_t previousSpin = 0, previousSam = 0;
  for (uint32_t native = 0; native < 18 * 20000; native += 6) {
    c.nativeDistance = native;
    const auto x = a.step(c), y = b.step(c);
    assert(x.count == y.count);
    for (unsigned i = 0; i < x.count; ++i) {
      const auto& event = x.events[i];
      assert(event.kind == y.events[i].kind && event.detail == y.events[i].detail && event.serial == y.events[i].serial);
      Policy::Event replay; assert(a.event(event.serial, replay) && replay.kind == event.kind);
      powers += event.kind == Kind::Power; vehicles += event.kind == Kind::Vehicle;
      if (event.kind == Kind::SpinToken) { ++spins; assert(!previousSpin || event.meters() - previousSpin <= 1110); previousSpin = event.meters(); }
      if (event.kind == Kind::SamLetter) { ++sams; assert(!previousSam || event.meters() - previousSam >= 150); previousSam = event.meters(); }
      if (content(event.kind)) { ++contents; a.completeContent(native, 100, 600); b.completeContent(native, 100, 600); }
    }
  }
  assert(powers >= 10 && powers < 60 && vehicles >= 10 && vehicles < 60);
  assert(spins >= 18 && spins < 100 && sams >= 60 && sams < 140 && contents > 500);
  Policy::Event old; assert(!a.event(1, old)); assert(a.event(a.serial(), old));
  std::cout << "Progression 20km trace: " << contents << " slots, " << powers << " powers, " << vehicles
      << " vehicles, " << spins << " spin tokens, " << sams << " SAM letters\n";
}

int main() {
  nativeUnitsAndManagerTargets(); vehicleHistoryAndRecovery(); eligibilityAndFamilies(); rareStreaksAndFullTrace();
  std::cout << "Jetpack progression tests passed\n";
}
