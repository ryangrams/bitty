#pragma once

#include <stdint.h>

namespace flappy {

// StickC Plus has a passive buzzer on GPIO2: https://docs.m5stack.com/en/core/StickC-Plus
// These short cue patterns were authored for this project.
enum class PiezoCue : uint8_t {
  Flap, Point, Crash, MenuMove, MenuSelect, MenuBack, Preview,
  Rotate, Move, Drop, Lock, LineClear, LevelUp, GameOver, Jump, Coin, Action, Start, Boot
};

enum class CueVoice : uint8_t { Square, Pulse, Triangle, Noise };
inline const uint8_t* cueWaveform(CueVoice voice) {
  static const uint8_t square[16]={208,208,208,208,208,208,208,208,48,48,48,48,48,48,48,48};
  static const uint8_t pulse[16]={224,224,224,224,96,96,96,96,96,96,96,96,96,96,96,96};
  static const uint8_t triangle[16]={128,160,192,224,255,224,192,160,128,96,64,32,1,32,64,96};
  static const uint8_t noise[16]={204,52,28,228,116,140,68,188,244,12,156,100,20,236,172,84};
  return voice==CueVoice::Pulse ? pulse : voice==CueVoice::Triangle ? triangle : voice==CueVoice::Noise ? noise : square;
}
struct PiezoTone {
  uint16_t frequency = 0;
  uint16_t durationMs = 0;
  uint8_t volume = 0;
  CueVoice voice = CueVoice::Square;
};
enum class SoundTheme : uint8_t { Classic, Space, Breaker, Jet, Buddy, Hockey };

// Poll update on the main loop. A zero-frequency command stops the buzzer.
class PiezoSound {
 public:
  void setTheme(SoundTheme theme) { theme_ = theme; }
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
    if (!gamePattern(cue)) switch (cue) {
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
  SoundTheme theme_ = SoundTheme::Classic;
  uint8_t count_ = 0, position_ = 0, priority_ = 0, nesLevel_ = 0, volumeIndex_ = 2;
  bool active_ = false, quiet_ = false, changed_ = false, haveNes_ = false, sounding_ = false;

  void add(uint16_t frequency, uint16_t duration) {
    if (count_ < 4) notes_[count_++] = Note{frequency, duration};
  }

  bool gamePattern(PiezoCue cue) {
    if (theme_ == SoundTheme::Classic) return false;
    const bool fire = cue == PiezoCue::Move || cue == PiezoCue::Action || cue == PiezoCue::Jump;
    const bool hit = cue == PiezoCue::Lock;
    const bool power = cue == PiezoCue::Coin;
    const bool death = cue == PiezoCue::Crash;
    const bool win = cue == PiezoCue::LevelUp || cue == PiezoCue::Start;
    if (!fire && !hit && !power && !death && !win) return false;
    if (theme_ == SoundTheme::Space) {
      if (fire) { add(3200,10); add(2400,10); add(1700,12); add(1100,14); }
      else if (hit) { add(880,20); add(440,24); add(220,32); }
      else if (power) { add(659,45); add(988,45); add(1319,70); }
      else if (death) { add(196,60); add(147,80); add(98,130); }
      else { add(523,60); add(659,60); add(784,60); add(1047,150); }
    } else if (theme_ == SoundTheme::Breaker) {
      if (fire || hit) { add(hit ? 1760 : 880,18); add(hit ? 1320 : 660,14); }
      else if (power) { add(784,40); add(1175,40); add(1568,80); }
      else if (death) { add(784,50); add(392,60); add(196,120); }
      else { add(587,45); add(784,45); add(1175,90); }
    } else if (theme_ == SoundTheme::Jet) {
      if (fire) { add(180,12); add(110,12); }
      else if (hit) { add(330,18); add(165,35); }
      else if (power) { add(1568,30); add(2093,45); add(2637,45); }
      else if (death) { add(220,50); add(110,75); add(82,140); }
      else { add(392,50); add(523,50); add(659,50); add(1047,100); }
    } else if (theme_ == SoundTheme::Buddy) {
      if (fire) { add(440,25); add(660,25); add(880,30); }
      else if (hit) { add(330,18); add(220,25); }
      else if (power) { add(1319,25); add(1760,35); add(2093,55); }
      else if (death) { add(523,40); add(392,50); add(262,80); }
      else { add(659,45); add(784,45); add(988,45); add(1319,100); }
    } else {
      if (fire || hit) { add(hit ? 740 : 494,14); add(hit ? 370 : 247,18); }
      else if (power) { add(880,40); add(1175,40); add(1760,80); }
      else if (death) { add(370,70); add(247,110); }
      else { add(587,55); add(740,55); add(880,55); add(1175,130); }
    }
    return true;
  }

  void emit(PiezoTone& output, uint16_t frequency, uint16_t duration, uint8_t level) {
    output.frequency = frequency;
    output.durationMs = duration;
    output.volume = frequency ? level : 0;
    output.voice = cue_==PiezoCue::Crash || cue_==PiezoCue::GameOver ? CueVoice::Noise :
        cue_==PiezoCue::Boot || theme_==SoundTheme::Space ? CueVoice::Pulse :
        theme_==SoundTheme::Hockey && cue_==PiezoCue::Lock ? CueVoice::Triangle : CueVoice::Square;
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
