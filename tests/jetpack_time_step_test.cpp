#include "../firmware/flappy/jetpack_time_step.h"
#include <assert.h>
#include <cmath>
#include <iostream>

int main() {
  using Time = flappy::JetpackTimeStep;
  const double q = Time::Unit;
  double worstDrag = 0, worstTrajectory = 0;
  for (uint32_t factor = 0; factor <= Time::Unit; ++factor) {
    const double expected = std::pow(.98, factor / q);
    const double error = std::abs(Time::dragQ30(factor) / double(int64_t(1) << 30) - expected);
    if (error > worstDrag) worstDrag = error;
  }
  assert(worstDrag < 5e-9);
  for (uint32_t factor : {0u,20000u,25941u,32768u,49152u,65536u}) {
    for (int sign : {-1,1}) {
      double y = 0, v = 0;
      int32_t fixedY = 0, fixedV = 0;
      const int32_t acceleration = sign * 3800;
      for (unsigned tick = 0; tick < 1200; ++tick) {
        const double force = (tick / 200) & 1 ? -acceleration / q : acceleration / q;
        v = (v + force * factor / q) * std::pow(.98, factor / q);
        y += v * factor / q;
        fixedV = Time::accelerate(fixedV, (tick / 200) & 1 ? -acceleration : acceleration, factor);
        fixedY += Time::scale(fixedV, factor);
        const double error = std::abs(fixedY / q - y);
        if (error > worstTrajectory) worstTrajectory = error;
      }
    }
  }
  assert(worstTrajectory < .04);
  assert(Time::accelerate(123456,-3210,65536) == Time::rounded(int64_t(123456-3210)*98,100));
  assert(Time::accelerate(123456,-3210,0) == 123456);
  assert(Time::scale(-123456,32768) == -61728);
  assert(Time::dragQ30(UINT32_MAX) == Time::dragQ30(65536));
  std::cout << "Jetpack scaled flight checks passed. Drag error " << worstDrag << ", trajectory error " << worstTrajectory << "px\n";
}
