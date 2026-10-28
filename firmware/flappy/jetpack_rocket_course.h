#pragma once

#include <stdint.h>
#ifdef M5FLAP_JETPACK_IMPORTED
#include "jetpack_imported.h"
#endif

namespace flappy { namespace JetpackRocketCourse {

enum { Unit = 256, MaxShots = 8 };
enum Flag : uint8_t { RandomLane = 1, Ranged = 2, Ricochet = 4, Dud = 8 };
enum class Family : uint8_t { Sweep, Gap, Fan, Chase, Imported };

struct Shot {
  // Both times are absolute scaled 60 Hz ticks after wave creation.
  uint16_t launchTicks = 0, warningTicks = 0;
  // Lane +256 is the ceiling. Angles are degrees Q8, positive upward.
  int16_t laneQ8 = 0, angleQ8 = 0;
  uint16_t speedQ8 = Unit;
  int8_t direction = -1;
  uint8_t flags = 0;
};

struct Wave {
  Shot shots[MaxShots];
  uint8_t count = 0;
  Family family = Family::Sweep;
  uint16_t pattern = UINT16_MAX;
  uint16_t missileSpeedQ8 = 10 * Unit, chaseSpeedQ8 = 25;
  bool imported = false;
};

inline uint32_t next(uint32_t& random) {
  if (!random) random = 0x6d2b79f5u;
  random ^= random << 13; random ^= random >> 17; random ^= random << 5;
  return random;
}

inline uint32_t roll(uint32_t& random, uint32_t range) { return range ? next(random) % range : 0; }

inline int16_t lane(int value) { return int16_t(value < -Unit ? -Unit : value > Unit ? Unit : value); }

inline int screenY(const Shot& shot, int ceiling = 22, int floor = 114) {
  const int value = lane(shot.laneQ8);
  return ceiling + (Unit - value) * (floor - ceiling) / (2 * Unit);
}

inline void sort(Wave& wave) {
  if (wave.count > MaxShots) wave.count = MaxShots;
  for (unsigned i = 1; i < wave.count; ++i) {
    const Shot value = wave.shots[i];
    unsigned j = i;
    while (j && wave.shots[j - 1].launchTicks > value.launchTicks) {
      wave.shots[j] = wave.shots[j - 1]; --j;
    }
    wave.shots[j] = value;
  }
}

#ifdef M5FLAP_JETPACK_ROCKET_WAVES
inline bool validPattern(unsigned index) {
  if (index >= JetpackImported::RocketPatternCount) return false;
  const auto& pattern = JetpackImported::RocketPatterns[index];
  if (!pattern.weight || !pattern.count || pattern.count > MaxShots ||
      uint32_t(pattern.offset) + pattern.count > JetpackImported::RocketMissileCount) return false;
  for (unsigned i = 0; i < pattern.count; ++i) {
    const auto& shot = JetpackImported::RocketMissiles[unsigned(pattern.offset) + i];
    if (shot.warningTicks > shot.launchTicks || shot.laneMinQ8 > shot.laneMaxQ8 ||
        shot.laneMinQ8 < -2 * Unit || shot.laneMaxQ8 > 2 * Unit ||
        shot.angleQ8 < -90 * Unit || shot.angleQ8 > 90 * Unit || shot.speedQ8 > 100 * Unit ||
        (shot.direction != -1 && shot.direction != 1) || (shot.flags & 240)) return false;
  }
  return true;
}

inline bool chooseImported(uint32_t& random, Wave& wave) {
  if (JetpackImported::RocketMissileSpeedQ8 > 100 * Unit || JetpackImported::RocketChaseSpeedQ8 > 100 * Unit) return false;
  uint64_t sum = 0;
  for (unsigned i = 0; i < JetpackImported::RocketPatternCount; ++i)
    if (validPattern(i)) sum += JetpackImported::RocketPatterns[i].weight;
  if (!sum || sum > UINT32_MAX) return false;
  uint32_t ticket = roll(random,uint32_t(sum));
  for (unsigned index = 0; index < JetpackImported::RocketPatternCount; ++index) {
    if (!validPattern(index)) continue;
    const auto& pattern = JetpackImported::RocketPatterns[index];
    if (ticket >= pattern.weight) { ticket -= pattern.weight; continue; }
    wave = Wave{};
    wave.imported = true; wave.family = Family::Imported; wave.pattern = uint16_t(index);
    wave.missileSpeedQ8 = JetpackImported::RocketMissileSpeedQ8;
    wave.chaseSpeedQ8 = JetpackImported::RocketChaseSpeedQ8;
    wave.count = pattern.count;
    for (unsigned i = 0; i < pattern.count; ++i) {
      const auto& source = JetpackImported::RocketMissiles[unsigned(pattern.offset) + i];
      Shot& shot = wave.shots[i];
      shot.launchTicks = source.launchTicks; shot.warningTicks = source.warningTicks;
      int selected = source.laneMinQ8;
      if (source.flags & Ranged)
        selected += int(roll(random,uint32_t(int(source.laneMaxQ8) - source.laneMinQ8 + 1)));
      else if (source.flags & RandomLane) selected = int(roll(random,2 * Unit + 1)) - Unit;
      // The device fit clips source positions outside the visible flight span.
      shot.laneQ8 = lane(selected); shot.angleQ8 = source.angleQ8;
      shot.speedQ8 = source.speedQ8; shot.direction = source.direction; shot.flags = source.flags;
    }
    sort(wave);
    return true;
  }
  return false;
}
#endif

inline void fallback(uint32_t& random, Wave& wave) {
  wave = Wave{};
  wave.family = Family(roll(random,4));
  const bool reverse = roll(random,2) != 0;
  switch (wave.family) {
    case Family::Sweep:
      wave.count = 4;
      for (unsigned i = 0; i < wave.count; ++i) {
        Shot& shot = wave.shots[i];
        shot.warningTicks = uint16_t(8 + i * 12); shot.launchTicks = uint16_t(shot.warningTicks + 30);
        shot.laneQ8 = int16_t((int(reverse ? 3 - i : i) * 2 - 3) * 62);
        shot.speedQ8 = 230; shot.flags = Ranged;
      }
      break;
    case Family::Gap: {
      const unsigned gap = 1 + roll(random,4);
      for (unsigned i = 0; i < 6; ++i) {
        if (i == gap || i == gap + 1) continue;
        Shot& shot = wave.shots[wave.count++];
        shot.warningTicks = uint16_t(10 + wave.count / 2 * 12); shot.launchTicks = uint16_t(shot.warningTicks + 30);
        shot.laneQ8 = int16_t((int(i) * 2 - 5) * 40);
        shot.speedQ8 = 218; shot.flags = Ranged;
      }
      break;
    }
    case Family::Fan: {
      const int center = int(roll(random,97)) - 48;
      wave.count = 3;
      for (unsigned i = 0; i < wave.count; ++i) {
        Shot& shot = wave.shots[i];
        shot.warningTicks = uint16_t(12 + i * 8); shot.launchTicks = uint16_t(shot.warningTicks + 30);
        shot.laneQ8 = int16_t(center); shot.angleQ8 = int16_t((int(i) - 1) * (reverse ? -24 : 24) * Unit);
        shot.flags = Ranged | Ricochet;
      }
      break;
    }
    case Family::Chase:
      wave.count = 3;
      for (unsigned i = 0; i < wave.count; ++i) {
        Shot& shot = wave.shots[i];
        shot.warningTicks = uint16_t(5 + i * 18); shot.launchTicks = uint16_t(shot.warningTicks + 30);
        shot.laneQ8 = int16_t(int(roll(random,257)) - 128);
        shot.speedQ8 = uint16_t(205 + i * 20); shot.flags = RandomLane;
      }
      break;
    default: break;
  }
  sort(wave);
}

inline bool choose(uint32_t& random, Wave& wave) {
#ifdef M5FLAP_JETPACK_ROCKET_WAVES
  if (chooseImported(random,wave)) return true;
#endif
  fallback(random,wave);
  return wave.count != 0;
}

} }  // namespace flappy::JetpackRocketCourse
