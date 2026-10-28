#include "../firmware/flappy/jetpack_spin.h"
#include <array>
#include <assert.h>
#include <iostream>
#include <set>

struct Canvas {
  std::array<uint16_t, 240 * 135 + 2> pixels;
  Canvas() { pixels.fill(0); pixels.front() = 0x1234; pixels.back() = 0x5678; }
  void pixel(int x, int y, uint16_t ink) {
    assert(x >= 0 && x < 240 && y >= 0 && y < 135);
    pixels[1 + y * 240 + x] = ink;
  }
  void rect(int x, int y, int w, int h, uint16_t ink) {
    for (int row = y; row < y + h; ++row) for (int col = x; col < x + w; ++col) pixel(col,row,ink);
  }
  uint64_t hash() const {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned i = 1; i <= 240 * 135; ++i) h = (h ^ pixels[i]) * 1099511628211ULL;
    return h;
  }
  void bounds() const {
    assert(pixels.front() == 0x1234 && pixels.back() == 0x5678);
    for (int y = 0; y < 135; ++y) for (int x = 0; x < 240; ++x)
      if (pixels[1 + y * 240 + x]) assert(x >= 66 && x < 174 && y >= 55 && y < 94);
  }
};

int main() {
  using namespace flappy;
  using namespace JetpackRewards;
  JetpackSpin spin;
  assert(!spin.running() && !spin.started());
  for (unsigned prize = 0; prize < unsigned(Prize::Count); ++prize) {
    spin.begin(Outcome{true, awardFor(Prize(prize)), 3, false}, prize + 113);
    assert(spin.running() && spin.symbol(0) == spin.symbol(1) && spin.symbol(1) == spin.symbol(2));
    std::set<uint64_t> hashes;
    for (unsigned tick = 0; tick < JetpackSpin::Duration; ++tick) {
      assert(spin.age() == tick && spin.running());
      for (unsigned reel = 0; reel < 3; ++reel) assert(spin.stopped(reel) == (tick >= JetpackSpin::stopTick(reel)));
      Canvas c; spin.draw(c); c.bounds(); hashes.insert(c.hash()); spin.tick();
    }
    assert(hashes.size() > 30 && !spin.running());
    Canvas settled; spin.draw(settled); settled.bounds();
    for (unsigned i = 0; i < 300; ++i) spin.tick();
    Canvas later; spin.draw(later); assert(settled.hash() == later.hash());
  }
  for (unsigned matches = 1; matches <= 2; ++matches) for (uint32_t seed = 0; seed < 256; ++seed) {
    spin.begin(Outcome{false, Award{Kind::Coins,0,0}, uint8_t(matches), false}, seed);
    assert(spin.symbol(0) != spin.symbol(2));
    assert((spin.symbol(0) == spin.symbol(1)) == (matches == 2));
  }
  spin.reset(); assert(!spin.started() && !spin.running());
  assert(JetpackSpin::WinHold == 150 && JetpackSpin::LossHold == 8);
  std::cout << "Jetpack reel presentation and clipping checks passed\n";
}
