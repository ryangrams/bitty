#include "../firmware/flappy/stick_input.h"
#include <assert.h>
#include <limits>
#include <iostream>
using flappy::StickInput::LaunchZero;
int main() {
  LaunchZero zero;
  zero.begin(100);
  for (unsigned i=0;i<7;++i) assert(!zero.sample(.37f + (i & 1 ? .002f : -.002f),100 + i*16));
  assert(zero.pending()); assert(zero.sample(.37f,212));
  assert(!zero.pending() && zero.neutral() > .369f && zero.neutral() < .371f);
  assert(!zero.sample(-.9f,300));
  zero.begin(400); assert(zero.pending());
  assert(!zero.sample(std::numeric_limits<float>::quiet_NaN(),416));
  assert(!zero.sample(2,432));
  for (unsigned i=0;i<7;++i) assert(!zero.sample(i & 1 ? .8f : -.8f,448+i*16));
  assert(zero.pending()); zero.cancel(); assert(!zero.pending());
  zero.begin(0xfffffff0u);
  assert(!zero.expired(0x10)); assert(zero.expired(0x310));
  zero.begin(10); assert(!zero.sample(.2f,570)); assert(!zero.sample(.2f,600));
  assert(zero.sample(.2f,620));
  zero.begin(1000); assert(!zero.sample(-.4f,1000));
  assert(zero.pending()); // A launch always requests a fresh zero.
  std::cout << "Tilt launch: stable averaging, movement rejection, new launches, invalid samples and timer wrap passed\n";
}
