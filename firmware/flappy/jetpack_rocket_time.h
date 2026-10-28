#pragma once

#include <stdint.h>

namespace flappy {

class JetpackRocketTime {
 public:
  enum class Phase : uint8_t { Inactive, Enter, Hold, Exit };
  enum { LevelCount = 10, TicksPerSecond = 60, EaseTicks = 90 };
  static constexpr uint32_t One = 65536;

  struct Config {
    uint16_t durationTicks;
    uint8_t waveCount, choiceCount, trailChancePercent, premiumChancePercent;
  };
  struct Events {
    uint16_t waveMask = 0;
    uint8_t choiceCount = 0;
    bool release = false, ended = false;
  };

  static Config config(uint8_t level) {
    static const uint8_t choices[] = {1,2,2,3,3,4,4,5,5,6};
    static const uint8_t trails[] = {6,8,10,10,14,14,18,18,22,22};
    static const uint8_t premiums[] = {10,10,10,15,15,15,15,20,20,20};
    level = boundLevel(level);
    return Config{uint16_t(420 + 150 * level), uint8_t(level + 1), choices[level], trails[level], premiums[level]};
  }

  void start(uint8_t level) { level_ = boundLevel(level); elapsed_ = 0; active_ = true; }
  void cancel() { active_ = false; }
  void collectVehicle() { cancel(); }
  bool active() const { return active_; }
  uint16_t elapsedTicks() const { return elapsed_; }
  uint8_t level() const { return level_; }
  uint16_t choiceTick() const { return uint16_t(config(level_).durationTicks - 120); }
  uint16_t releaseTick() const { return uint16_t(config(level_).durationTicks + 150); }
  uint16_t endTick() const { return uint16_t(config(level_).durationTicks + 240); }
  static uint16_t waveTick(uint8_t wave) { return uint16_t(90 + 150 * wave); }

  Phase phase() const {
    return !active_ ? Phase::Inactive : elapsed_ < EaseTicks ? Phase::Enter
         : elapsed_ < releaseTick() ? Phase::Hold : Phase::Exit;
  }

  Events advance(uint16_t ticks = 1) {
    Events events;
    if (!active_ || !ticks) return events;
    const uint16_t before = elapsed_;
    const uint32_t next = uint32_t(elapsed_) + ticks;
    elapsed_ = uint16_t(next < endTick() ? next : endTick());
    const Config settings = config(level_);
    for (uint8_t wave = 0; wave < settings.waveCount; ++wave)
      if (before < waveTick(wave) && elapsed_ >= waveTick(wave)) events.waveMask |= uint16_t(1u << wave);
    if (before < choiceTick() && elapsed_ >= choiceTick()) events.choiceCount = settings.choiceCount;
    events.release = before < releaseTick() && elapsed_ >= releaseTick();
    events.ended = elapsed_ == endTick();
    if (events.ended) active_ = false;
    return events;
  }

  static uint32_t targetScaleQ16(uint32_t nativeSpeedQ16) {
    if (nativeSpeedQ16 <= 380u * One) return One / 2;
    return uint32_t((uint64_t(380) * One * (One / 2)) / nativeSpeedQ16);
  }

  uint32_t timeScaleQ16(uint32_t nativeSpeedQ16) const {
    if (!active_) return One;
    const uint32_t target = targetScaleQ16(nativeSpeedQ16);
    if (phase() == Phase::Hold) return target;
    const uint16_t easePosition = phase() == Phase::Enter ? uint16_t(EaseTicks - elapsed_)
        : uint16_t(elapsed_ - releaseTick());
    return target + uint32_t((uint64_t(One - target) * quarterSine(easePosition) + One / 2) / One);
  }

 private:
  uint16_t elapsed_ = 0;
  uint8_t level_ = 0;
  bool active_ = false;

  static uint8_t boundLevel(uint8_t level) { return level < LevelCount ? level : LevelCount - 1; }

  // Integer interpolation follows the native quarter-sine/cosine ease shape.
  static uint32_t quarterSine(uint16_t tick) {
    static const uint32_t values[] = {
      0,6424,12785,19024,25080,30893,36410,41576,46341,
      50660,54491,57798,60547,62714,64277,65220,65536
    };
    if (tick >= EaseTicks) return One;
    const uint32_t position = uint32_t(tick) * 16;
    const unsigned index = position / EaseTicks, remainder = position % EaseTicks;
    return values[index] + (values[index + 1] - values[index]) * remainder / EaseTicks;
  }
};

}  // namespace flappy
