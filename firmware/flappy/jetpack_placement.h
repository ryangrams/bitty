#pragma once
#include <stdint.h>

namespace flappy {
struct JetpackPlacement {
  struct Weights { uint8_t top, bottom, topHalf, bottomHalf; };
  static int32_t screenQ8(int32_t nativeQ8) { return int32_t(int64_t(nativeQ8) * 23 / 60); }
  static int32_t verticalOffset(unsigned nativeExtent, Weights weights, uint32_t choice, uint32_t position) {
    const int32_t free = nativeExtent < 256 ? int32_t(256 - nativeExtent) : 0;
    if (!free) return 0;
    const unsigned roll = choice % 100;
    unsigned bound = weights.top;
    if (weights.top && roll <= bound) return free * 128;
    bound += weights.bottom;
    if (weights.bottom && roll <= bound) return -free * 128;
    bound += weights.topHalf;
    if (weights.topHalf && roll <= bound) return int32_t(position % unsigned(free / 2 + 1)) * 256;
    bound += weights.bottomHalf;
    if (weights.bottomHalf && roll <= bound) return -int32_t(position % unsigned(free / 2 + 1)) * 256;
    return (int32_t(position % unsigned(free)) - free / 2) * 256;
  }
  static int32_t coinTopQ8(unsigned rows, Weights weights, uint32_t choice, uint32_t position) {
    const int32_t nativeTop = int32_t(rows * 8 * 256) + verticalOffset(rows * 16, weights, choice, position);
    return 68 * 256 - screenQ8(nativeTop);
  }
  static uint32_t coinSpan(unsigned columns, int32_t speedQ8) {
    const uint32_t margin = speedQ8 > 0 ? uint32_t(uint64_t(speedQ8) * 60 * 60 / (4 * 23 * 256)) : 0;
    return columns * 16 + margin;
  }
  static int sine(unsigned phase) {
    static const int16_t quarter[] = {0,100,200,297,392,483,569,650,724,792,851,903,946,980,1004,1019,1024};
    phase &= 255;
    const bool negative = phase >= 128;
    unsigned x = phase & 127; if (x > 64) x = 128 - x;
    const unsigned cell = x / 4, fraction = x % 4;
    const int value = quarter[cell] + (fraction ? (quarter[cell + 1] - quarter[cell]) * int(fraction) / 4 : 0);
    return negative ? -value : value;
  }
};
} // namespace flappy
