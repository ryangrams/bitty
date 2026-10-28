#include <stdint.h>

#ifdef JETPACK_ROCKET_TEST_FIXTURE
#define M5FLAP_JETPACK_ROCKET_WAVES 1
namespace flappy { namespace JetpackImported {
struct RocketMissile { uint16_t launchTicks,warningTicks; int16_t laneMinQ8,laneMaxQ8,angleQ8; uint16_t speedQ8; int8_t direction; uint8_t flags; };
struct RocketPattern { uint16_t offset,weight; uint8_t count; };
static const RocketMissile RocketMissiles[] = {
  {90,60,-256,256,0,256,-1,2}, {40,10,-128,128,0,768,-1,3},
  {60,30,512,512,7680,512,-1,6}, {10,30,0,0,0,256,-1,0}
};
#if JETPACK_ROCKET_TEST_FIXTURE == 1
static const RocketPattern RocketPatterns[] = {{0,100,2},{2,300,1},{0,65500,9},{30,65500,1},{0,0,1},{3,65500,1}};
#else
static const RocketPattern RocketPatterns[] = {{0,0,2},{0,65500,9},{30,65500,1},{3,65500,1}};
#endif
static const unsigned RocketMissileCount = sizeof(RocketMissiles) / sizeof(RocketMissiles[0]);
static const unsigned RocketPatternCount = sizeof(RocketPatterns) / sizeof(RocketPatterns[0]);
static const uint16_t RocketMissileSpeedQ8 = 2560, RocketChaseSpeedQ8 = 25;
} }
#endif

#include "../firmware/flappy/jetpack_rocket_course.h"

#include <array>
#include <assert.h>
#include <cstdlib>
#include <iostream>
#include <new>

static unsigned allocations = 0;
void* operator new(std::size_t size) {
  ++allocations;
  if (void* result = std::malloc(size)) return result;
  throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }

namespace Course = flappy::JetpackRocketCourse;

static void normalized(const Course::Wave& wave) {
  assert(wave.count > 0 && wave.count <= Course::MaxShots);
  for (unsigned i = 0; i < wave.count; ++i) {
    const Course::Shot& shot = wave.shots[i];
    assert(shot.warningTicks <= shot.launchTicks);
    assert(shot.laneQ8 >= -256 && shot.laneQ8 <= 256);
    assert(shot.angleQ8 >= -90 * 256 && shot.angleQ8 <= 90 * 256);
    assert(shot.direction == -1 || shot.direction == 1);
    assert(shot.speedQ8 <= 100 * 256 && (shot.flags & 240) == 0);
    assert(Course::screenY(shot) >= 22 && Course::screenY(shot) <= 114);
    assert(Course::screenY(shot,0,134) >= 0 && Course::screenY(shot,0,134) <= 134);
    if (i) assert(wave.shots[i - 1].launchTicks <= shot.launchTicks);
  }
}

static void equal(const Course::Wave& a,const Course::Wave& b) {
  assert(a.count == b.count && a.family == b.family && a.pattern == b.pattern && a.imported == b.imported);
  assert(a.missileSpeedQ8 == b.missileSpeedQ8 && a.chaseSpeedQ8 == b.chaseSpeedQ8);
  for (unsigned i = 0; i < a.count; ++i) {
    const auto& x = a.shots[i]; const auto& y = b.shots[i];
    assert(x.launchTicks == y.launchTicks && x.warningTicks == y.warningTicks && x.laneQ8 == y.laneQ8);
    assert(x.angleQ8 == y.angleQ8 && x.speedQ8 == y.speedQ8 && x.direction == y.direction && x.flags == y.flags);
  }
}

static void deterministicNoAllocationAndGuards() {
  struct Guarded { uint32_t before = 0x12345678; Course::Wave wave; uint32_t after = 0x87654321; } a,b;
  uint32_t x = 0, y = 0;
  const unsigned begin = allocations;
  for (unsigned i = 0; i < 20000; ++i) {
    assert(Course::choose(x,a.wave) && Course::choose(y,b.wave));
    assert(x == y && x != 0); equal(a.wave,b.wave); normalized(a.wave);
    assert(a.before == 0x12345678 && a.after == 0x87654321);
    assert(b.before == 0x12345678 && b.after == 0x87654321);
  }
  assert(allocations == begin);
  Course::Shot shot; shot.laneQ8 = 256; assert(Course::screenY(shot) == 22);
  shot.laneQ8 = -256; assert(Course::screenY(shot) == 114);
  shot.laneQ8 = 0; assert(Course::screenY(shot) == 68);
  shot.laneQ8 = 512; assert(Course::screenY(shot) == 22);
  shot.laneQ8 = -512; assert(Course::screenY(shot) == 114);
  static_assert(sizeof(Course::Wave) <= 112,"Rocket wave must stay bounded");
}

static void authoredFamiliesAndStableOrder() {
  std::array<unsigned,4> families{};
  uint32_t random = 718393;
  for (unsigned i = 0; i < 20000; ++i) {
    Course::Wave wave; Course::fallback(random,wave); normalized(wave);
    assert(!wave.imported && wave.pattern == UINT16_MAX);
    ++families[unsigned(wave.family)];
    for (unsigned j = 0; j < wave.count; ++j) assert(wave.shots[j].launchTicks - wave.shots[j].warningTicks == 30);
    if (wave.family == Course::Family::Fan) {
      assert(wave.count == 3 && wave.shots[0].laneQ8 == wave.shots[2].laneQ8);
      assert(wave.shots[0].angleQ8 == -wave.shots[2].angleQ8);
    }
    if (wave.family == Course::Family::Gap) assert(wave.count == 4);
  }
  for (unsigned count : families) assert(count > 4700 && count < 5300);
  Course::Wave wave; wave.count = 3;
  wave.shots[0].launchTicks = 90; wave.shots[0].speedQ8 = 100;
  wave.shots[1].launchTicks = 30; wave.shots[1].speedQ8 = 200;
  wave.shots[2].launchTicks = 30; wave.shots[2].speedQ8 = 300;
  Course::sort(wave);
  assert(wave.shots[0].speedQ8 == 200 && wave.shots[1].speedQ8 == 300 && wave.shots[2].speedQ8 == 100);
}

static void optionalImportedCourses() {
#ifdef M5FLAP_JETPACK_ROCKET_WAVES
  uint32_t random = 6217;
  unsigned imported = 0;
  std::array<unsigned,256> selected{};
  unsigned lowest = 0, highest = 0;
  for (unsigned i = 0; i < 30000; ++i) {
    Course::Wave wave; assert(Course::choose(random,wave)); normalized(wave);
    imported += wave.imported;
    if (wave.imported) {
      assert(wave.pattern < flappy::JetpackImported::RocketPatternCount);
      if (wave.pattern < selected.size()) ++selected[wave.pattern];
      for (unsigned j = 0; j < wave.count; ++j) {
        lowest += wave.shots[j].laneQ8 == -256; highest += wave.shots[j].laneQ8 == 256;
      }
    }
  }
  (void)lowest; (void)highest;
#if defined(JETPACK_ROCKET_TEST_FIXTURE) && JETPACK_ROCKET_TEST_FIXTURE == 2
  assert(imported == 0);
#else
  assert(imported == 30000);
  for (unsigned i = 0; i < flappy::JetpackImported::RocketPatternCount && i < selected.size(); ++i)
    if (Course::validPattern(i)) assert(selected[i]); else assert(!selected[i]);
#if defined(JETPACK_ROCKET_TEST_FIXTURE) && JETPACK_ROCKET_TEST_FIXTURE == 1
  assert(selected[0] > 7000 && selected[0] < 8000 && selected[1] > 22000 && selected[1] < 23000);
  assert(lowest > 10 && highest > 22000);
  uint32_t seed = 1;
  for (unsigned i = 0; i < 100; ++i) {
    Course::Wave wave; Course::choose(seed,wave);
    if (!wave.pattern) assert(wave.shots[0].launchTicks == 40 && wave.shots[0].speedQ8 == 768);
  }
#endif
#endif
#else
  uint32_t random = 71; Course::Wave wave;
  assert(Course::choose(random,wave) && !wave.imported);
#endif
}

int main() {
  deterministicNoAllocationAndGuards(); authoredFamiliesAndStableOrder(); optionalImportedCourses();
  std::cout << "Rocket course deterministic, bounded, normalized and allocation-free tests passed\n";
}
