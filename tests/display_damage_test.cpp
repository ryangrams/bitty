#include "../firmware/flappy/display_damage.h"

#include <assert.h>
#include <stdint.h>
#include <iostream>
#include <vector>

using flappy::DamageRows;

struct Changes {
  unsigned rows = 0;
  unsigned runs = 0;
};

static Changes frame(DamageRows& damage, const std::vector<uint16_t>& pixels,
                     int width, int height, uint32_t now) {
  assert(damage.beginFrame(width, height, now));
  Changes changed;
  bool previous = false;
  for (int row = 0; row < height; ++row) {
    const bool current = damage.changed(row, pixels.data() + row * width);
    if (current) ++changed.rows;
    if (current && !previous) ++changed.runs;
    previous = current;
  }
  damage.endFrame();
  return changed;
}

static void unchangedAndChangedRuns() {
  DamageRows damage;
  std::vector<uint16_t> pixels(12 * 16, 0x1234);
  Changes changed = frame(damage, pixels, 12, 16, 0);
  assert(changed.rows == 16 && changed.runs == 1);
  changed = frame(damage, pixels, 12, 16, 1);
  assert(changed.rows == 0 && changed.runs == 0);
  for (unsigned row = 0; row < 16; ++row) {
    pixels[row * 12 + row % 12] ^= 1;
    changed = frame(damage, pixels, 12, 16, row + 2);
    assert(changed.rows == 1 && changed.runs == 1);
    changed = frame(damage, pixels, 12, 16, row + 20);
    assert(changed.rows == 0 && changed.runs == 0);
  }
  pixels[2 * 12] ^= 0x100;
  pixels[3 * 12] ^= 0x100;
  pixels[8 * 12] ^= 0x100;
  changed = frame(damage, pixels, 12, 16, 60);
  assert(changed.rows == 3 && changed.runs == 2);
}

static void invalidationResizeAndRefresh() {
  DamageRows damage;
  std::vector<uint16_t> pixels(24, 0x4321);
  assert(frame(damage, pixels, 3, 8, 100).rows == 8);
  assert(frame(damage, pixels, 3, 8, 1099).rows == 0);
  assert(frame(damage, pixels, 3, 8, 1100).rows == 8);
  assert(frame(damage, pixels, 3, 8, 1101).rows == 0);
  damage.invalidate();
  assert(frame(damage, pixels, 3, 8, 1102).rows == 8);
  assert(frame(damage, pixels, 6, 4, 1103).rows == 4);
  assert(frame(damage, pixels, 6, 4, 1104).rows == 0);
  assert(frame(damage, pixels, 3, 8, 1105).rows == 8);
  assert(damage.beginFrame(3, 8, 1106));
  assert(!damage.changed(0, pixels.data()));
  assert(frame(damage, pixels, 3, 8, 1107).rows == 8);
  damage.invalidate();
  assert(frame(damage, pixels, 3, 8, UINT32_MAX - 500).rows == 8);
  assert(frame(damage, pixels, 3, 8, 0).rows == 0);
  assert(frame(damage, pixels, 3, 8, 499).rows == 8);
}

static void boundariesAndIncompleteFrames() {
  DamageRows damage;
  std::vector<uint16_t> pixels(DamageRows::MaxRows * 2, 0);
  assert(!damage.changed(0, pixels.data()));
  assert(!damage.beginFrame(0, 1, 0));
  assert(!damage.beginFrame(-1, 1, 0));
  assert(!damage.beginFrame(65536, 1, 0));
  assert(!damage.beginFrame(1, 0, 0));
  assert(!damage.beginFrame(1, DamageRows::MaxRows + 1, 0));
  assert(damage.beginFrame(2, DamageRows::MaxRows, 0));
  assert(!damage.changed(-1, pixels.data()));
  assert(!damage.changed(DamageRows::MaxRows, pixels.data()));
  assert(!damage.changed(0, nullptr));
  assert(damage.changed(255, pixels.data()));
  damage.endFrame();
  assert(!damage.changed(255, pixels.data()));
  assert(frame(damage, pixels, 2, DamageRows::MaxRows, 1).rows == DamageRows::MaxRows);
  assert(frame(damage, pixels, 2, DamageRows::MaxRows, 2).rows == 0);
  pixels[511] = 1;
  assert(frame(damage, pixels, 2, DamageRows::MaxRows, 3).rows == 1);
}

int main() {
  static_assert(sizeof(DamageRows) <= 1120, "Display damage storage must remain bounded");
  unchangedAndChangedRuns();
  invalidationResizeAndRefresh();
  boundariesAndIncompleteFrames();
  std::cout << "Display row damage checks passed. State bytes: " << sizeof(DamageRows) << "\n";
}
