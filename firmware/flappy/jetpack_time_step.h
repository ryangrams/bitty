#pragma once
#include <stdint.h>

namespace flappy {

// Native flight uses the scaled frame factor for force, drag and displacement.
struct JetpackTimeStep {
  enum { Unit = 65536 };
  static int32_t rounded(int64_t value, int64_t divisor) {
    return int32_t(value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor);
  }
  static uint32_t bounded(uint32_t scale) { return scale > Unit ? uint32_t(Unit) : scale; }
  static int32_t scale(int32_t value, uint32_t factor) {
    return rounded(int64_t(value) * bounded(factor), Unit);
  }
  static int32_t dragQ30(uint32_t factor) {
    // Fourth-order exp(log(.98) * factor), rounded in Q30.
    const int64_t x = rounded(int64_t(-21692492) * bounded(factor), Unit);
    const int64_t x2 = rounded(x * x, int64_t(1) << 30);
    const int64_t x3 = rounded(x2 * x, int64_t(1) << 30);
    const int64_t x4 = rounded(x3 * x, int64_t(1) << 30);
    return int32_t((int64_t(1) << 30) + x + rounded(x2, 2) + rounded(x3, 6) + rounded(x4, 24));
  }
  static int32_t accelerate(int32_t velocity, int32_t acceleration, uint32_t factor) {
    const int32_t next = velocity + scale(acceleration, factor);
    if (factor >= Unit) return rounded(int64_t(next) * 98, 100);
    return rounded(int64_t(next) * dragQ30(factor), int64_t(1) << 30);
  }
};

}  // namespace flappy
