#pragma once

#include <stdint.h>

namespace flappy {

// StickC Plus has a passive buzzer on GPIO2: https://docs.m5stack.com/en/core/StickC-Plus
// These short cue patterns were authored for this project.
enum class PiezoCue : uint8_t {
  Flap, Point, Crash, MenuMove, MenuSelect, MenuBack, Preview,
  Rotate, Move, Drop, Lock, LineClear, LevelUp, GameOver, Jump, Coin, Action, Start, Boot
};

struct PiezoTone {
  uint16_t frequency = 0;
  uint16_t durationMs = 0;
  uint8_t volume = 0;
};

// Poll update on the main loop. A zero-frequency command stops the buzzer.
class PiezoSound {
 public:
  void setVolumeIndex(uint8_t index) {
    if (index > 5) index = 5;
    if (volumeIndex_ == index) return;
    volumeIndex_ = index;
    changed_ = true;
    if (!index) stop();
  }

  uint8_t volumeIndex() const { return volumeIndex_; }
  static uint8_t volume(uint8_t index) {
    return uint8_t((index > 5 ? 5 : index) * 51u);
  }

  void setQuiet(bool quiet) {
    if (quiet_ == quiet) return;
    quiet_ = quiet;
    if (quiet) stop();
  }

  bool play(PiezoCue cue, uint32_t nowMs) {
    if (quiet_ || !volumeIndex_ || uint8_t(cue) > uint8_t(PiezoCue::Boot)) return false;
    const uint8_t rank = priority(cue);
    if (active_ && rank < priority_) return false;
    if (active_ && cue == cue_ && uint32_t(nowMs - startedMs_) < 35u) return false;
    cue_ = cue;
    priority_ = rank;
    count_ = 0;
    switch (cue) {
      case PiezoCue::Flap: add(1900, 24); add(2700, 30); break;
      case PiezoCue::Point: add(2600, 55); add(3400, 90); break;
      case PiezoCue::Crash: add(1700, 65); add(1100, 100); add(700, 120); break;
      case PiezoCue::MenuMove: add(2200, 18); break;
      case PiezoCue::MenuSelect: add(2200, 25); add(3000, 45); break;
      case PiezoCue::MenuBack: add(2500, 25); add(1800, 35); break;
      case PiezoCue::Preview: add(2000, 70); add(0, 25); add(3000, 100); break;
      case PiezoCue::Rotate: add(2900, 22); break;
      case PiezoCue::Move: add(1700, 14); break;
      case PiezoCue::Drop: add(2600, 18); add(1700, 18); add(1100, 24); break;
      case PiezoCue::Lock: add(1300, 28); break;
      case PiezoCue::LineClear: add(1900, 45); add(2500, 45); add(3200, 90); break;
      case PiezoCue::LevelUp: add(1800, 45); add(2400, 45); add(3000, 45); add(3800, 100); break;
      case PiezoCue::GameOver: add(2600, 100); add(1900, 100); add(1300, 180); break;
      case PiezoCue::Jump: add(1500, 22); add(2100, 22); add(2900, 35); break;
      case PiezoCue::Coin: add(2700, 55); add(3600, 100); break;
      case PiezoCue::Action: add(2300, 25); add(1800, 30); break;
      case PiezoCue::Start: add(2000, 45); add(2600, 45); add(3300, 75); break;
      case PiezoCue::Boot: add(1047, 85); add(1568, 70); add(1319, 75); add(2093, 150); break;
      default: return false;
    }
    active_ = true;
    changed_ = true;
    position_ = 0;
    startedMs_ = nowMs;
    untilMs_ = nowMs + notes_[0].duration;
    return true;
  }

  // Optional monophonic NES output. Pass the audible pulse channel's frequency
  // and envelope level, or zero when silent. Refresh after each emulated frame.
  void setNesTone(uint16_t frequency, uint8_t level, uint32_t nowMs) {
    if (quiet_ || !volumeIndex_) return;
    if (level > 15) level = 15;
    if (frequency < 80 || frequency > 8000 || !level) frequency = 0;
    const bool changed = frequency != nesFrequency_ || level != nesLevel_;
    nesFrequency_ = frequency;
    nesLevel_ = frequency ? level : 0;
    nesMs_ = nowMs;
    haveNes_ = frequency != 0;
    if (!active_ && changed) changed_ = true;
  }

  static uint16_t nesPulseFrequency(uint32_t cpuClock, uint16_t period) {
    if (period < 8 || period > 0x7ff) return 0;
    const uint32_t divisor = 16u * (uint32_t(period) + 1u);
    const uint32_t frequency = (cpuClock + divisor / 2u) / divisor;
    return frequency >= 80 && frequency <= 8000 ? uint16_t(frequency) : 0;
  }

  void stop() {
    active_ = false;
    haveNes_ = false;
    changed_ = true;
  }

  bool active() const { return active_; }

  bool update(uint32_t nowMs, PiezoTone& output) {
    bool advanced = false;
    if (quiet_ || !volumeIndex_) {
      active_ = false;
      haveNes_ = false;
    }
    while (active_ && int32_t(nowMs - untilMs_) >= 0) {
      advanced = true;
      if (++position_ == count_) active_ = false;
      else untilMs_ += notes_[position_].duration;
    }
    if (active_) {
      if (!changed_ && !advanced) return false;
      const uint16_t frequency = notes_[position_].frequency;
      emit(output, frequency, uint16_t(untilMs_ - nowMs), volume(volumeIndex_));
      changed_ = false;
      return true;
    }
    if (haveNes_ && uint32_t(nowMs - nesMs_) <= 80u) {
      if (!changed_ && !advanced && uint32_t(nowMs - emittedMs_) < 30u) return false;
      emit(output, nesFrequency_, 65, uint8_t((uint16_t(volume(volumeIndex_)) * nesLevel_ + 14u) / 15u));
      emittedMs_ = nowMs;
      changed_ = false;
      return true;
    }
    haveNes_ = false;
    changed_ = false;
    if (!sounding_) return false;
    emit(output, 0, 0, 0);
    return true;
  }

 private:
  struct Note { uint16_t frequency; uint16_t duration; };
  Note notes_[4] = {};
  uint32_t untilMs_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t nesMs_ = 0;
  uint32_t emittedMs_ = 0;
  uint16_t nesFrequency_ = 0;
  PiezoCue cue_ = PiezoCue::Flap;
  uint8_t count_ = 0, position_ = 0, priority_ = 0, nesLevel_ = 0, volumeIndex_ = 2;
  bool active_ = false, quiet_ = false, changed_ = false, haveNes_ = false, sounding_ = false;

  void add(uint16_t frequency, uint16_t duration) {
    if (count_ < 4) notes_[count_++] = Note{frequency, duration};
  }

  void emit(PiezoTone& output, uint16_t frequency, uint16_t duration, uint8_t level) {
    output.frequency = frequency;
    output.durationMs = duration;
    output.volume = frequency ? level : 0;
    sounding_ = frequency != 0;
  }

  static uint8_t priority(PiezoCue cue) {
    switch (cue) {
      case PiezoCue::Crash: case PiezoCue::GameOver: return 4;
      case PiezoCue::LineClear: case PiezoCue::LevelUp: return 3;
      case PiezoCue::Point: case PiezoCue::Coin: case PiezoCue::Start: return 2;
      case PiezoCue::Move: case PiezoCue::MenuMove: return 0;
      default: return 1;
    }
  }
};

}  // namespace flappy
