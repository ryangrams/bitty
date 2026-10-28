#include "../firmware/flappy/jetpack_placement.h"
#include <assert.h>
#include <math.h>
#include <iostream>
using Placement = flappy::JetpackPlacement;
int main() {
  const Placement::Weights random = {0,0,0,0}, top = {100,0,0,0}, bottom = {0,100,0,0};
  assert(Placement::screenQ8(16 * 256) == 1570);
  for (unsigned height = 1; height <= 40; ++height) {
    int32_t minimum = 1000000, maximum = -1000000;
    for (unsigned seed = 0; seed < 1000; ++seed) {
      const int32_t y = Placement::coinTopQ8(height, random, seed, seed);
      if (y < minimum) minimum = y;
      if (y > maximum) maximum = y;
    }
    if (height < 16) assert(maximum - minimum > 100);
    else assert(minimum == maximum); // Tall groups retain their pitch and crop.
    const int32_t upper = Placement::coinTopQ8(height, top, 50, 77);
    const int32_t lower = Placement::coinTopQ8(height, bottom, 50, 77);
    assert(upper <= lower);
  }
  // Width and speed determine only this formation's span, including blanks.
  assert(Placement::coinSpan(10, 286) == 203);
  assert(Placement::coinSpan(20, 286) == 363);
  assert(Placement::coinSpan(10, 903) > Placement::coinSpan(10, 286));
  for (unsigned phase = 0; phase < 256; ++phase) {
    const double expected = sin(phase * 6.283185307179586 / 256) * 1024;
    assert(fabs(Placement::sine(phase) - expected) < 3);
    assert(Placement::sine(phase) == -Placement::sine(phase + 128));
  }
  assert(Placement::sine(0) == 0 && Placement::sine(64) == 1024);
  std::cout << "Jetpack fixed pitch, weighted vertical bounds, isolated formation spans and smooth rotation passed\n";
}
