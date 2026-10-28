#pragma once

#include <stdint.h>

namespace flappy {

// Distance policy is independent of rendering, input and the flight integrator.
class JetpackProgression {
 public:
  enum class Kind : uint8_t {
    None, Coins, Zap, RotatingZap, Missile, Laser, Vehicle, Power,
    SpinToken, SamLetter, Scientist, StreakBegin, StreakEnd,
    VehicleAcquire, VehicleEject, PowerAcquire, PowerExpire, Recovery
  };
  enum class Streak : uint8_t { Default, Missiles, Coins, Vehicle, Sam };
  enum { TraceCount = 16 };
  struct Event {
    uint32_t serial = 0, nativeDistance = 0, encounterDistance = 0, dueMeters = 0;
    Kind kind = Kind::None;
    uint8_t detail = 0;
    Streak streak = Streak::Default;
    uint32_t meters() const { return nativeDistance / 18; }
  };
  struct Context {
    uint32_t nativeDistance = 0;
    int32_t speedQ8 = 286;
    uint8_t vehicle = 0, samLetters = 0, powerMask = 0, powerUnlockedMask = 15;
    bool safePower = false, hazardsClear = true;
    bool coinSlot = true, zapSlot = true, missileSlot = true, laserSlot = true;
    bool vehicleSlot = true, powerSlot = true, spinSlot = true, samSlot = true;
    bool scientistSlot = true;
  };
  struct Batch {
    Event events[4];
    uint8_t count = 0;
  };

  void reset(uint32_t seed) {
    random_ = seed ? seed : 1;
    nextContent_ = 280;
    powerDue_ = vehicleDue_ = 0;
    vehicleBlockedUntil_ = recoveryUntil_ = 0;
    nextScientist_ = 130;
    lastSpinMeters_ = int32_t(200 + roll(250)) - 1050;
    spinCount_ = 0;
    lastSamMeters_ = int32_t(roll(50)) - 300;
    samCompensation_ = 0; samSpawns_ = 0;
    firstPower_ = firstVehicle_ = true;
    waiting_ = false;
    enabled_ = true;
    streak_ = Streak::Default;
    streakRemaining_ = 0;
    if (!historyInitialized_) {
      for (uint8_t& multiplier : vehicleMultiplier_) multiplier = 4;
      historyInitialized_ = true;
    }
    currentVehicle_ = 0;
    streakStartNative_ = 0;
    serial_ = 0;
    for (Event& event : trace_) event = Event{};
  }

  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  uint32_t serial() const { return serial_; }
  bool event(uint32_t serial, Event& result) const {
    if (!serial || serial > serial_ || serial_ - serial >= TraceCount) return false;
    const Event& value = trace_[(serial - 1) % TraceCount];
    if (value.serial != serial) return false;
    result = value;
    return true;
  }
  uint32_t nextContent() const { return nextContent_; }
  uint32_t powerDue() const { return powerDue_; }
  uint32_t vehicleDue() const { return vehicleDue_; }
  uint32_t vehicleBlockedUntil() const { return vehicleBlockedUntil_; }
  uint32_t recoveryUntil() const { return recoveryUntil_; }
  Streak streak() const { return streak_; }
  uint8_t streakRemaining() const { return streakRemaining_; }
  uint32_t streakMeters(uint32_t native) const {
    return (currentVehicle_ && native >= streakStartNative_ ? native - streakStartNative_ : native) / 18;
  }
  void setSamCompensation(uint32_t meters) { samCompensation_ = meters; }
  uint32_t samCompensation() const { return samCompensation_; }
  uint32_t samCompensationOnDeath(uint32_t native) const {
    const int64_t elapsed = int64_t(native / 18) - lastSamMeters_;
    const uint64_t total = samCompensation_ + uint64_t(elapsed > 0 ? elapsed : 0);
    return total > UINT32_MAX ? UINT32_MAX : uint32_t(total);
  }
  uint8_t vehicleMultiplier(unsigned index) const { return vehicleMultiplier_[index < 6 ? index : 0]; }

  static uint32_t nativeDistance(uint32_t screenQ8) {
    return uint32_t(uint64_t(screenQ8) * 60 / (23 * 256));
  }
  static uint32_t screenQ8(uint32_t native) {
    return uint32_t(uint64_t(native) * 23 * 256 / 60);
  }
  static uint32_t safeGapNative(int32_t speedQ8) {
    const uint32_t pixelsQ8 = uint32_t(speedQ8 > 0 ? speedQ8 : 0) * 70u + 32u * 256u;
    return uint32_t((uint64_t(pixelsQ8) * 60 + 23 * 256 - 1) / (23 * 256));
  }
  static void intervalRange(uint32_t meters, uint16_t& minimum, uint16_t& maximum) {
    static const uint16_t lower[] = {280,250,220,200,200,200,200,200,190,190,180,180,170,170,160,150};
    static const uint16_t upper[] = {470,420,390,350,320,280,275,270,265,260,255,250,245,240,235,230};
    unsigned index = meters < 2500 ? 0 : 1 + unsigned((meters - 2500) / 500);
    if (index > 15) index = 15;
    minimum = lower[index]; maximum = upper[index];
  }

  Batch step(const Context& context) {
    Batch batch;
    if (!enabled_ || waiting_) return batch;
    const uint32_t d = context.nativeDistance, meters = d / 18;
    ensureManagers(meters);
    if (d >= nextScientist_ && context.scientistSlot) {
      append(batch, Kind::Scientist, d);
      nextScientist_ = d + 180 + roll(260);
    }
    if (d < nextContent_) return batch;

    Kind kind = Kind::None;
    uint8_t detail = 0;
    uint32_t encounter = addSaturated(d, 577);
    uint32_t dueMeters = 0;
    if (context.safePower) {
      return batch;
    } else if (d < recoveryUntil_) {
      if (context.coinSlot) kind = Kind::Coins;
    } else if (!context.vehicle && context.powerSlot && meters + 100 > powerDue_) {
      kind = Kind::Power;
      uint8_t eligible[4]; unsigned count = 0;
      for (uint8_t i = 0; i < 4; ++i) if (!(context.powerMask & uint8_t(1u << i)) && (context.powerUnlockedMask & uint8_t(1u << i))) eligible[count++] = uint8_t(i + 1);
      if (!count) return batch;
      detail = eligible[roll(count)];
      dueMeters = powerDue_;
      powerDue_ = 0;
      firstPower_ = false;
    } else if (!context.vehicle && context.vehicleSlot && context.powerSlot &&
        meters > vehicleBlockedUntil_ && meters + 100 > vehicleDue_) {
      kind = Kind::Vehicle;
      detail = chooseVehicle();
      dueMeters = vehicleDue_;
      vehicleDue_ = 0;
    } else {
      kind = ordinary(context);
    }
    if (kind == Kind::None) return batch;
    append(batch, kind, d, detail);
    batch.events[batch.count - 1].encounterDistance = encounter;
    batch.events[batch.count - 1].dueMeters = dueMeters;
    trace_[(serial_ - 1) % TraceCount].encounterDistance = encounter;
    trace_[(serial_ - 1) % TraceCount].dueMeters = dueMeters;
    waiting_ = true;
    if (context.spinSlot) maybeSpin(batch, d);
    if (context.samSlot && context.vehicle != 8 && !context.safePower) maybeSam(batch, d, context.samLetters);
    return batch;
  }

  void completeContent(uint32_t native, uint32_t span, int32_t speedQ8, bool protectedRun = false) {
    if (!waiting_) return;
    waiting_ = false;
    uint16_t minimum = 0, maximum = 0;
    intervalRange(native / 18, minimum, maximum);
    switch (currentVehicle_) {
      case 2: minimum = maximum = 400; break;
      case 3: minimum = 600; maximum = 800; break;
      case 4: minimum = 345; maximum = 512; break;
      case 5: minimum = 220; maximum = 270; break;
      case 6: minimum = 220; maximum = 470; break;
      case 7: minimum = 460; maximum = 520; break;
      default: break;
    }
    if (streak_ == Streak::Missiles) minimum = maximum = 128;
    if (streak_ == Streak::Coins || protectedRun) minimum = maximum = 0;
    const uint32_t interval = minimum + roll(uint32_t(maximum - minimum));
    uint32_t gap = span + interval;
    if (!protectedRun && streak_ != Streak::Coins) {
      const uint32_t safe = safeGapNative(speedQ8);
      if (gap < safe) gap = safe;
    }
    if (gap < 90) gap = 90;
    nextContent_ = addSaturated(native, gap);
    if (streakRemaining_ && !--streakRemaining_) {
      record(Kind::StreakEnd, native, uint8_t(streak_));
      streak_ = Streak::Default;
    }
  }
  void recover(uint32_t native, uint32_t nativeGap) {
    recoveryUntil_ = addSaturated(native, nativeGap);
    if (nextContent_ < recoveryUntil_) nextContent_ = recoveryUntil_;
    waiting_ = false;
    record(Kind::Recovery, native);
  }
  void acquireVehicle(uint32_t native, uint8_t vehicle) {
    currentVehicle_ = vehicle;
    streakStartNative_ = native;
    streak_ = vehicle == 8 ? Streak::Sam : Streak::Vehicle;
    streakRemaining_ = 0;
    record(Kind::StreakBegin, native, uint8_t(streak_));
    record(Kind::VehicleAcquire, native, vehicle);
    recover(native, 444);
  }
  void ejectVehicle(uint32_t native, uint8_t vehicle) {
    record(Kind::StreakEnd, native, uint8_t(streak_));
    streak_ = Streak::Default; streakRemaining_ = 0; currentVehicle_ = 0;
    vehicleBlockedUntil_ = native / 18 + 470;
    record(Kind::VehicleEject, native, vehicle);
    recover(native, 444);
  }
  void acquirePower(uint32_t native, uint8_t power) { record(Kind::PowerAcquire, native, power); }
  void expirePower(uint32_t native, uint8_t power) {
    record(Kind::PowerExpire, native, power);
    recover(native, 444);
  }
  void record(Kind kind, uint32_t native, uint8_t detail = 0) {
    Event event;
    event.serial = ++serial_; event.nativeDistance = native; event.encounterDistance = native; event.kind = kind;
    event.detail = detail; event.streak = streak_;
    trace_[(event.serial - 1) % TraceCount] = event;
  }

 private:
  friend struct JetpackProgressionTestAccess;
  static uint32_t addSaturated(uint32_t a, uint32_t b) { return a > UINT32_MAX - b ? UINT32_MAX : a + b; }
  uint32_t roll(uint32_t range) {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return range ? random_ % range : 0;
  }
  static uint32_t bucket(uint32_t meters, uint32_t length) {
    return uint32_t((uint64_t(meters) + length - 1) / length * length);
  }
  void ensureManagers(uint32_t meters) {
    if (!powerDue_) {
      const uint32_t length = firstPower_ ? 300 : 500 + roll(500);
      powerDue_ = addSaturated(bucket(meters, length), roll(length));
    }
    if (!vehicleDue_) {
      vehicleDue_ = firstVehicle_ && meters <= 650 ? roll(650) : addSaturated(bucket(meters, 650), roll(650));
      firstVehicle_ = false;
    }
  }
  uint8_t chooseVehicle() {
    unsigned total = 0;
    for (uint8_t value : vehicleMultiplier_) total += value;
    unsigned choice = roll(total);
    unsigned selected = 0;
    for (; selected < 5 && choice >= vehicleMultiplier_[selected]; ++selected) choice -= vehicleMultiplier_[selected];
    for (unsigned i = 0; i < 6; ++i)
      vehicleMultiplier_[i] = i == selected ? 0 : uint8_t(vehicleMultiplier_[i] < 4 ? vehicleMultiplier_[i] + 1 : 4);
    return uint8_t(selected + 2);
  }
  Kind ordinary(const Context& context) {
    const uint32_t meters = context.vehicle ? streakMeters(context.nativeDistance) : context.nativeDistance / 18;
    if (!context.vehicle && (context.powerMask & 8)) return context.hazardsClear && context.missileSlot ? Kind::Missile : Kind::None;
    if (streak_ == Streak::Default && context.hazardsClear) {
      if (roll(10000) < 35) {
        streak_ = Streak::Missiles; streakRemaining_ = uint8_t(4 + roll(4));
        record(Kind::StreakBegin, context.nativeDistance, uint8_t(streak_));
      } else if (roll(10000) < 15) {
        streak_ = Streak::Coins; streakRemaining_ = uint8_t(2 + roll(2));
        record(Kind::StreakBegin, context.nativeDistance, uint8_t(streak_));
      }
    }
    if (streak_ == Streak::Missiles) return context.hazardsClear && context.missileSlot ? Kind::Missile : Kind::None;
    if (streak_ == Streak::Coins) return context.coinSlot ? Kind::Coins : Kind::None;
    const unsigned coinWeight = context.vehicle == 8 ? 10 : context.vehicle == 3 ? 60 :
        (context.vehicle == 2 || context.vehicle == 4 || context.vehicle == 6) ? 40 : 140;
    const bool specialCoins = context.vehicle == 2 || context.vehicle == 3 || context.vehicle == 4 || context.vehicle == 6 || context.vehicle == 8;
    const unsigned coins = context.coinSlot && (specialCoins || meters >= 50) ? coinWeight : 0;
    const unsigned gates = context.zapSlot && context.vehicle != 8 ? 800 : 0;
    const unsigned missileWeight = context.vehicle == 2 || context.vehicle == 7 ? 250 : context.vehicle == 4 ? 200 :
        context.vehicle == 8 ? (meters < 200 ? 100 : 200) : context.vehicle == 3 ? 0 : meters >= 200 ? 150 : 0;
    const unsigned missiles = context.hazardsClear && context.missileSlot ? missileWeight : 0;
    const unsigned laser = !context.vehicle && context.hazardsClear && context.laserSlot && meters >= 400 ?
        (meters < 2000 ? 100 : meters < 3000 ? 60 : meters < 4000 ? 40 : meters < 5000 ? 20 : meters < 6000 ? 10 : 5) : 0;
    const unsigned total = coins + gates + missiles + laser;
    if (!total) return Kind::None;
    unsigned choice = roll(total);
    if (choice < coins) return Kind::Coins;
    choice -= coins;
    if (choice < gates) return meters >= 400 && roll(10) == 0 ? Kind::RotatingZap : Kind::Zap;
    choice -= gates;
    if (choice < missiles) return Kind::Missile;
    return Kind::Laser;
  }
  void maybeSpin(Batch& batch, uint32_t native) {
    const int32_t meters = int32_t(native / 18);
    const bool guarantee = meters - lastSpinMeters_ > 1050;
    if (!guarantee && (spinCount_ >= 3 || roll(1000) >= 28)) return;
    const int32_t previous = lastSpinMeters_;
    lastSpinMeters_ = meters;
    spinCount_ = previous / 1000 == meters / 1000 ? uint8_t(spinCount_ + 1) : 1;
    append(batch, Kind::SpinToken, native);
  }
  void maybeSam(Batch& batch, uint32_t native, uint8_t letters) {
    const int32_t meters = int32_t(native / 18);
    const int64_t elapsed = int64_t(meters) - lastSamMeters_;
    if (samSpawns_ && elapsed < 150) return;
    const bool forced = elapsed + samCompensation_ > 300 || (!samSpawns_ && meters >= 50);
    if (!forced && roll(1000) >= 28) return;
    const uint8_t letter = !(letters & 1) ? 1 : !(letters & 2) ? 2 : 3;
    append(batch, Kind::SamLetter, native, letter);
    lastSamMeters_ = meters;
    samCompensation_ = 0;
    if (samSpawns_ < UINT16_MAX) ++samSpawns_;
  }
  void append(Batch& batch, Kind kind, uint32_t native, uint8_t detail = 0) {
    if (batch.count >= 4) return;
    record(kind, native, detail);
    batch.events[batch.count++] = trace_[(serial_ - 1) % TraceCount];
  }

  Event trace_[TraceCount];
  uint32_t random_ = 1, serial_ = 0, nextContent_ = 280, powerDue_ = 0, vehicleDue_ = 0;
  uint32_t vehicleBlockedUntil_ = 0, recoveryUntil_ = 0, nextScientist_ = 130;
  uint32_t streakStartNative_ = 0;
  uint32_t samCompensation_ = 0;
  int32_t lastSpinMeters_ = -800, lastSamMeters_ = -300;
  uint16_t samSpawns_ = 0;
  uint8_t vehicleMultiplier_[6] = {4,4,4,4,4,4}, spinCount_ = 0, streakRemaining_ = 0, currentVehicle_ = 0;
  Streak streak_ = Streak::Default;
  bool enabled_ = true, waiting_ = false, firstPower_ = true, firstVehicle_ = true, historyInitialized_ = false;
};

}  // namespace flappy
