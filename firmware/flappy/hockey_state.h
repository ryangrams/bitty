#pragma once
#include <stdint.h>

namespace flappy { namespace hockey {

enum class Phase : uint8_t { Serve, Rally, Goal, Finished, LinkWait };
enum class Power : uint8_t { None, Lasers, Beam, Mega, Shrink, Wall, Party, Slime, Backwards, Butterflies, Rocket, TinyGoal, Freeze };
enum class Powers : uint8_t { Queen, Both, Off };
constexpr uint8_t MapCount = 8;
constexpr uint8_t PowerCount = 13;
constexpr uint8_t PlayTo = 7;

inline const char* mapName(uint8_t map) {
  static const char* const names[] = {"Rainbow Court", "Unicorn Road", "Narwhal Ice", "Jellyfish Reef", "Butterfly Meadow", "Candy Castle", "Potato Patch", "Throne Room"};
  return map < MapCount ? names[map] : "Random";
}
inline const char* powerName(Power power) {
  static const char* const names[] = {"", "PINK LASERS", "RAINBOW BEAM", "MEGA PADDLE", "SHRINK RAY", "RAINBOW WALL", "TRIPLE PUCK", "SLIME TIME", "BACKWARDS DAY", "BUTTERFLIES", "ROCKET PUCK", "TINY GOAL", "FREEZE RAY"};
  return uint8_t(power) < PowerCount ? names[uint8_t(power)] : "";
}
struct Options {
  uint8_t map = 0;  // Zero chooses a random map; fixed maps use one through eight.
  uint8_t palette = 0;
  Powers powers = Powers::Queen;
  uint16_t encode() const { return uint16_t(map | (palette << 4) | (uint8_t(powers) << 6)); }
  static bool decode(uint16_t value, Options& result) {
    if (value >= 256 || (value & 15) > MapCount || (value >> 6) > 2) return false;
    result.map = uint8_t(value & 15); result.palette = uint8_t((value >> 4) & 3);
    result.powers = Powers(value >> 6); return true;
  }
};

// Coordinates stay in host world space. The Queen's display rotates the rink.
struct Frame {
  uint32_t tick = 0;
  uint8_t x[3] = {}, y[3] = {}, paddles[2] = {67,67}, scores[2] = {};
  Power powers[2] = {Power::None,Power::None};
  uint8_t remaining[2] = {};
  uint8_t puckMask = 1, stunMask = 0, event = 0;
  Phase phase = Phase::Serve;
  uint8_t map = 0, palette = 0;
  Powers rules = Powers::Queen;
  Power pickup = Power::None;
  uint8_t pickupX = 67, pickupOwner = 0, pickupLife = 0;
};
inline bool newer(uint16_t a, uint16_t b) { const uint16_t d = uint16_t(a-b); return d && d < 0x8000; }
inline bool valid(const Frame& f) {
  if (f.map >= MapCount || f.palette > 3 || uint8_t(f.rules) > 2 || uint8_t(f.phase) > 4 ||
      f.puckMask > 7 || f.stunMask > 3 || (f.event&7)>5 || f.pickupOwner > 1 || uint8_t(f.pickup) >= PowerCount || f.pickupX > 134 || f.pickupLife > 48) return false;
  if (f.phase == Phase::Rally && !f.puckMask) return false;
  for (unsigned i=0;i<2;++i) if (f.paddles[i] > 134 || f.scores[i] > PlayTo || uint8_t(f.powers[i]) >= PowerCount || f.remaining[i] > 40) return false;
  for (unsigned i=0;i<3;++i) if (f.x[i] > 134 || f.y[i] > 239) return false;
  return true;
}
} }
