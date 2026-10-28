"""Exercise imported coin groups with synthetic data and no original assets."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('jetpack_import_fixture', ROOT / 'tools/import_jetpack.py')
importer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(importer)


def fixture_header(runner=False, selectable=True):
    definitions = [
        (32, 16, [(y << 8) | x for x in range(32) for y in range(16)], 100, 1),
        (8, 8, [(y << 8) | x for x in range(8) for y in range(8) if x in (0, 7) or y in (0, 7)], 300, 1),
        (1, 1, [], 9000, 1),
        (6, 1, list(range(6)), 0, 2),
        (1, 1, [0], 50000, 0),
        (1, 41, [0], 50000, 1),
        (257, 1, [0], 50000, 1),
    ]
    patterns, points = [], []
    for index, (width, height, coords, weight, flags) in enumerate(definitions):
        patterns.append({'name': 'synthetic-%d' % index, 'offset': len(points), 'count': len(coords),
                         'width': width, 'height': height, 'weight': weight,
                         'flags': flags if selectable else 0})
        points.extend(coords)
    frames = []
    if runner:
        for frame in range(6):
            frames.append([0xffff - frame if 4 <= x < 16 and 2 <= y < 13 else 0xf81f
                           for y in range(15) for x in range(20)])
    return importer.header(patterns, points, frames)


PROBE = r'''
#include "jetpack_game.h"
#include <array>
#include <assert.h>
#include <stdint.h>
#include <iostream>

namespace flappy {
struct JetpackTestAccess {
  static bool start(JetpackGame& game) { return game.startImported(); }
  static int pattern(const JetpackGame& game, unsigned slot=0) { return game.importedGroups_[slot].pattern; }
  static void cash(JetpackGame& game, bool on) { game.powerTicks_[unsigned(JetpackGame::Power::CashDash)] = on ? 100 : 0; }
  static void doubleCoins(JetpackGame& game) { game.powerTicks_[unsigned(JetpackGame::Power::DoubleCoins)] = 100; }
  static void group(JetpackGame& game, unsigned slot, int pattern, int x=JetpackGame::PlayerX, int step=5, int top=28) {
    game.importedGroups_[slot] = JetpackGame::ImportedGroup{};
    auto& group = game.importedGroups_[slot];
    group.pattern = int16_t(pattern); group.x = x * JetpackGame::Unit;
    group.step = uint8_t(step); group.top = uint8_t(top);
  }
  static void player(JetpackGame& game, int y) { game.y_ = y * JetpackGame::FlightUnit; }
  static void origin(JetpackGame& game, unsigned slot, int x) { game.importedGroups_[slot].x = x * JetpackGame::Unit; }
  static void collect(JetpackGame& game) { game.collectImported(); }
  static void update(JetpackGame& game, int amount) { game.updateImported(amount * JetpackGame::Unit); }
  static unsigned mask(const JetpackGame& game, unsigned slot, unsigned byte) { return game.importedGroups_[slot].collected[byte]; }
  static void ordinary(JetpackGame& game) {
    for (auto& coin : game.coinField_) { coin.active=true; coin.x=10000*JetpackGame::Unit; coin.y=70; }
  }
  static unsigned ordinaryCount(const JetpackGame& game) { unsigned n=0; for (const auto& c:game.coinField_) n+=c.active; return n; }
  static void spawn(JetpackGame& game) { game.spawnCoins(); }
  static bool groupsReset(const JetpackGame& game) {
    for (const auto& group : game.importedGroups_) {
      if (group.pattern!=-1) return false;
      for (auto byte : group.collected) if (byte) return false;
    }
    return true;
  }
  static void pose(JetpackGame& game, unsigned frame, bool gravity) {
    game.frame_=frame; game.phase_=JetpackGame::Phase::Running;
    game.vehicle_=gravity ? JetpackGame::Vehicle::Gravity : JetpackGame::Vehicle::None;
    game.gravityUp_=gravity; game.y_=70*JetpackGame::FlightUnit;
  }
};
}
using Game=flappy::JetpackGame;
using Access=flappy::JetpackTestAccess;

#if defined(M5FLAP_JETPACK_IMPORTED) && !defined(TEST_UNMATCHED)
using namespace flappy::JetpackImported;
static void collectEveryPoint(Game& game, unsigned slots, unsigned pattern) {
  const Pattern& shape=CoinPatterns[pattern];
  for (unsigned i=0; i<shape.count; ++i) {
    const uint16_t point=CoinPoints[shape.offset+i];
    Access::player(game,28+int(point>>8)*5);
    for (unsigned slot=0; slot<slots; ++slot) Access::origin(game,slot,Game::PlayerX-int(point&255)*5);
    Access::collect(game);
  }
}

static void selectionAndFit() {
  for (uint32_t seed=1; seed<300; ++seed) {
    Game game; game.reset(seed);
    assert(Access::start(game));
    uint32_t random=seed; random^=random<<13; random^=random>>17; random^=random<<5;
    const int expected=random%400<100 ? 0 : 1;
    assert(Access::pattern(game)==expected);
    const Pattern& p=CoinPatterns[unsigned(expected)];
    const unsigned step=p.height ? 80/p.height : 5;
    const unsigned pitch=step>5 ? 5 : step<2 ? 2 : step;
    const unsigned extent=p.height*pitch;
    const unsigned top=extent<90 ? 23+(90-extent)/2 : 23;
    for (unsigned i=0; i<p.count; ++i) {
      const int y=int(top+(CoinPoints[p.offset+i]>>8)*pitch);
      assert(y>=Game::Ceiling && y<=Game::Floor);
    }
    Game cash; cash.reset(seed); Access::cash(cash,true);
    assert(Access::start(cash) && Access::pattern(cash)==3);
  }
}

static void fullMasksWithoutPoolTruncation() {
  static_assert(MaxPatternPoints==512,"synthetic capacity fixture");
  Game game; Access::ordinary(game); Access::group(game,0,0);
  collectEveryPoint(game,1,0);
  assert(game.coins()==512 && Access::ordinaryCount(game)==Game::CoinCount);
  for (unsigned byte=0; byte<64; ++byte) assert(Access::mask(game,0,byte)==255);
  collectEveryPoint(game,1,0); assert(game.coins()==512);

  game.reset(); Access::group(game,0,0); Access::group(game,1,0);
  collectEveryPoint(game,2,0); assert(game.coins()==1024);
  for (unsigned slot=0; slot<2; ++slot)
    for (unsigned byte=0; byte<64; ++byte) assert(Access::mask(game,slot,byte)==255);

  game.reset(); Access::doubleCoins(game); Access::group(game,0,0);
  collectEveryPoint(game,1,0); assert(game.coins()==1024);
  game.reset(); assert(game.coins()==0 && Access::groupsReset(game));
}

static void ringEmptyAndSlots() {
  Game game; Access::group(game,0,1);
  collectEveryPoint(game,1,1); assert(game.coins()==28);
  game.reset(); Access::group(game,0,2); Access::collect(game); assert(game.coins()==0);
  Access::update(game,300); assert(Access::pattern(game)==-1);
  Access::group(game,0,0,40); Access::group(game,1,1,200);
  assert(Access::start(game));
  assert(Access::pattern(game,0)==0 && Access::pattern(game,1)==1);
  Access::update(game,300);
  assert(Access::pattern(game,0)==-1 && Access::pattern(game,1)==-1);
  assert(Access::start(game) && Access::pattern(game)>=0);
}
#endif

static void drawingGuards() {
  struct Guarded { uint64_t before=0x123456789abcdef0ull;
    std::array<uint16_t,Game::Width*Game::Height> pixels{};
    uint64_t after=0x0fedcba987654321ull; } frame;
  Game game;
  for (unsigned pose=0; pose<20; ++pose) {
    Access::pose(game,pose*7,pose&1);
#if defined(M5FLAP_JETPACK_IMPORTED) && !defined(TEST_UNMATCHED)
    Access::group(game,0,0,int(pose)*20-100);
    Access::group(game,1,1,280-int(pose)*23);
#endif
    game.draw(frame.pixels.data(),1234);
    assert(frame.before==0x123456789abcdef0ull && frame.after==0x0fedcba987654321ull);
  }
  game.draw(nullptr);
}

int main() {
#if defined(M5FLAP_JETPACK_IMPORTED) && !defined(TEST_UNMATCHED)
  selectionAndFit(); fullMasksWithoutPoolTruncation(); ringEmptyAndSlots();
#else
  Game game; assert(!Access::start(game)); Access::spawn(game);
  assert(Access::ordinaryCount(game)>0 && Access::groupsReset(game));
#endif
  drawingGuards();
  std::cout<<"Synthetic imported runtime checks passed\n";
}
'''


@unittest.skipUnless(shutil.which('c++'), 'C++ compiler unavailable')
class JetpackRuntimeTests(unittest.TestCase):
    def test_synthetic_imports_fallback_and_optional_runner(self):
        with tempfile.TemporaryDirectory(prefix='jetpack-runtime-') as temporary:
            root = Path(temporary)
            source = root / 'probe.cpp'
            source.write_text(PROBE)
            for mode in ('patterns', 'runner', 'unmatched', 'fallback'):
                with self.subTest(mode=mode):
                    include = root / mode
                    include.mkdir()
                    (include / 'jetpack_imported.h').write_text(fixture_header(mode == 'runner', mode != 'unmatched'))
                    target = include / 'probe'
                    command = [shutil.which('c++'), '-std=c++11', '-O1', '-Wall', '-Wextra', '-Werror',
                               '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                               '-I' + str(include), '-I' + str(ROOT / 'firmware/flappy')]
                    if mode != 'fallback':
                        command += ['-DM5FLAP_JETPACK_IMPORTED=1']
                    if mode == 'unmatched':
                        command += ['-DTEST_UNMATCHED=1']
                    subprocess.run(command + [str(source), '-o', str(target)], check=True)
                    subprocess.run([str(target)], check=True)


if __name__ == '__main__':
    unittest.main()
