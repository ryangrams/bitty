#include "../firmware/flappy/geometry_clock.h"

#include <array>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

using flappy::ClockTime;
using flappy::GeometryClock;
namespace Scene = flappy::ClockScene;

namespace flappy {
struct GeometryClockTestAccess {
  static unsigned rate(const GeometryClock& clock) { return clock.rate_; }
  static unsigned phase(const GeometryClock& clock) { return clock.cycle_; }
  static unsigned form(const GeometryClock& clock) { return clock.form_; }
  static unsigned material(const GeometryClock& clock) { return clock.material_; }
  static uint32_t world(const GeometryClock& clock) { return clock.world_; }
  static int y(const GeometryClock& clock) { return clock.playerY_; }
  static unsigned digit(const GeometryClock& clock, unsigned index) { return clock.digits_[index]; }
  static int clockLeft(const GeometryClock& clock) { return int32_t(clock.clockWorld_ - clock.world_) / 256; }
  static void monumentFixture(GeometryClock& clock) { clock.clockWorld_ = clock.world_ + 28u * 256u; clock.material_ = 0; }
  static void wrapFixture(GeometryClock& clock) {
    clock.world_ = UINT32_MAX - 2000u;
    clock.clockWorld_ = clock.world_ + 290u * 256u;
    clock.portalWorld_ = clock.world_ + 432u * 256u;
    for (unsigned i = 0; i < 6; ++i) clock.hurdles_[i].world = clock.world_ + (164u + i * 176u) * 256u;
  }
  static void invariants(const GeometryClock& clock) {
    assert(clock.cycle_ >= 1 && clock.cycle_ <= 600);
    assert(clock.rate_ >= 1024 && clock.rate_ <= 4096);
    assert(clock.remainder_ < 4096);
    assert(clock.playerY_ >= 64 * 256 && clock.playerY_ <= 180 * 256);
    assert(clock.velocity_ >= -768 && clock.velocity_ <= 768);
    assert(clock.form_ < 5 && clock.nextForm_ < 5 && clock.material_ < 4);
    for (const auto& hurdle : clock.hurdles_) {
      assert(hurdle.type < 4 && hurdle.height >= 24 && hurdle.height <= 40);
      assert(int32_t(hurdle.world - clock.world_) >= -80 * 256);
      assert(int32_t(hurdle.world - clock.world_) < 1400 * 256);
      const int left = int32_t(hurdle.world - clock.world_) / 256;
      const int top = hurdle.type < 2 ? 180 : hurdle.type == 2 ? 212 - hurdle.height : 171;
      // A conservative rotated-actor box must clear every obstacle silhouette.
      if (left < 68 && left + 32 > 14) assert(clock.playerY_ / 256 + 37 <= top);
    }
  }
};
}
using Access = flappy::GeometryClockTestAccess;

struct Canvas {
  static constexpr uint16_t Empty = 0xf81f;
  std::array<uint16_t, Scene::Width * Scene::Height + 128> pixels;
  Canvas() { pixels.fill(Empty); }
  uint16_t* data() { return pixels.data() + 64; }
  const uint16_t* data() const { return pixels.data() + 64; }
  void guards() const {
    for (unsigned i = 0; i < 64; ++i) assert(pixels[i] == Empty && pixels[Scene::Width * Scene::Height + 64 + i] == Empty);
  }
  uint16_t at(int x, int y) const { return data()[y * Scene::Width + x]; }
  uint64_t hash() const {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) h = (h ^ data()[i]) * 1099511628211ULL;
    return h;
  }
};
constexpr uint16_t Canvas::Empty;

static ClockTime timeAt(unsigned frame) {
  ClockTime time;
  time.hour = uint8_t((12u + frame / 216000u) % 24u);
  time.minute = uint8_t((34u + frame / 3600u) % 60u);
  time.second = uint8_t(frame / 60u % 60u);
  time.ms = frame * 1000u / 60u;
  return time;
}

static void easedQuarterSpeedAndContinuousIntegration() {
  GeometryClock clock; clock.reset(1);
  const ClockTime time = timeAt(0);
  uint32_t previousWorld = Access::world(clock);
  unsigned previousRate = 4096, quarterTicks = 0, readableTicks = 0;
  uint32_t quarterDistance = 0;
  for (unsigned frame = 0; frame < 600; ++frame) {
    clock.step(time);
    const unsigned rate = Access::rate(clock);
    const uint32_t delta = Access::world(clock) - previousWorld;
    assert(delta >= 96 && delta <= 384);
    assert(rate > previousRate ? rate - previousRate <= 78 : previousRate - rate <= 78);
    if (frame < 120 || frame >= 480) assert(rate == 4096 && delta == 384);
    if (frame >= 120 && frame < 180) assert(rate <= previousRate);
    if (frame >= 180 && frame < 420) {
      assert(rate == 1024 && delta == 96);
      ++quarterTicks; quarterDistance += delta;
      const int x = Access::clockLeft(clock);
      readableTicks += x >= 0 && x + 78 <= 135;
    }
    if (frame >= 420 && frame < 480) assert(rate >= previousRate);
    previousWorld = Access::world(clock); previousRate = rate;
    Access::invariants(clock);
  }
  assert(quarterTicks == 240 && quarterDistance == 90 * 256);
  assert(readableTicks >= 145);
  const uint32_t beforeCycle = Access::world(clock);
  clock.step(time);
  assert(Access::phase(clock) == 1 && Access::world(clock) - beforeCycle == 384);
}

static void digitsRolloverWithoutMotionReset() {
  GeometryClock clock; clock.reset(22);
  ClockTime time = timeAt(0);
  for (unsigned i = 0; i < 250; ++i) clock.step(time);
  const auto phase = Access::phase(clock);
  const uint32_t before = Access::world(clock);
  time.hour = 23; time.minute = 59; time.ms = UINT32_MAX; clock.step(time);
  assert(Access::phase(clock) == phase + 1 && Access::world(clock) - before == 96);
  assert(Access::digit(clock,0) == 2 && Access::digit(clock,1) == 3);
  assert(Access::digit(clock,2) == 5 && Access::digit(clock,3) == 9);
  time.twentyFourHour = false; clock.step(time);
  assert(Access::digit(clock,0) == 1 && Access::digit(clock,1) == 1);
  time.hour = 0; time.minute = 0; time.ms = 0; clock.step(time);
  assert(Access::digit(clock,0) == 1 && Access::digit(clock,1) == 2);
  assert(Access::digit(clock,2) == 0 && Access::digit(clock,3) == 0);
}

static void nativeSizeCoverageAndConstDraw() {
  GeometryClock a,b,c;
  a.reset(33); b.reset(33); c.reset(92);
  Canvas first,second,third;
  std::set<uint64_t> views;
  unsigned different = 0;
  for (unsigned frame = 0; frame < 7200; ++frame) {
    const ClockTime time = timeAt(frame);
    a.step(time); b.step(time); c.step(time);
    Access::invariants(a); Access::invariants(c);
    if (frame % 29u == 0) {
      const GeometryClock& immutable = a;
      immutable.draw(first.data(),time); b.draw(second.data(),time); c.draw(third.data(),time);
      assert(first.hash() == second.hash()); different += first.hash() != third.hash();
      const uint64_t before = first.hash(); immutable.draw(first.data(),time); assert(first.hash() == before);
      views.insert(first.hash());
      for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) assert(first.data()[i] != Canvas::Empty);
      first.guards(); second.guards(); third.guards();
    }
  }
  assert(views.size() > 240 && different > 230);
  a.draw(nullptr,timeAt(0));
  // A block material puts a two-pixel white outline at each occupied clock cell.
  a.reset(5); a.step(timeAt(0)); Access::monumentFixture(a); a.draw(first.data(),timeAt(0));
  assert(first.at(40,38) == 0xffff && first.at(28,38) != 0xffff);
  assert(first.at(28,86) == 0xffff && first.at(52,86) == 0xffff);
  assert(first.at(72,38) == 0xffff && first.at(96,38) == 0xffff);
  first.guards();
}

static void endlessCourseAndUnsignedWrap() {
  GeometryClock clock; clock.reset(18);
  std::set<unsigned> forms,materials;
  Canvas canvas;
  for (unsigned frame = 0; frame < 60u * 60u * 60u; ++frame) {
    const ClockTime time = timeAt(frame);
    clock.step(time); Access::invariants(clock);
    forms.insert(Access::form(clock)); materials.insert(Access::material(clock));
    if (frame % 2003u == 0) { clock.draw(canvas.data(),time); canvas.guards(); }
  }
  assert(forms.size() == 5 && materials.size() == 4);
  clock.reset(1); Access::wrapFixture(clock);
  for (unsigned frame = 0; frame < 900; ++frame) {
    const uint32_t before = Access::world(clock);
    clock.step(timeAt(frame)); Access::invariants(clock);
    assert(Access::world(clock) - before >= 96 && Access::world(clock) - before <= 384);
    if (frame % 31u == 0) { clock.draw(canvas.data(),timeAt(frame)); canvas.guards(); }
  }
}

static void ppm(const Canvas& canvas, const std::string& path) {
  std::ofstream output(path,std::ios::binary); assert(output);
  output << "P6\n135 240\n255\n";
  for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) {
    const uint16_t pixel = canvas.data()[i];
    const char rgb[] = {char((pixel >> 11) * 255 / 31),char((pixel >> 5 & 63) * 255 / 63),char((pixel & 31) * 255 / 31)};
    output.write(rgb,3);
  }
}

int main(int argc, char** argv) {
  static_assert(sizeof(GeometryClock) <= 256,"Clock scene state grew beyond its current budget");
  easedQuarterSpeedAndContinuousIntegration();
  digitsRolloverWithoutMotionReset();
  nativeSizeCoverageAndConstDraw();
  endlessCourseAndUnsignedWrap();
  if (argc > 1) {
    GeometryClock clock; clock.reset(33);
    Canvas canvas;
    for (unsigned frame = 0; frame < 2100; ++frame) {
      const ClockTime time = timeAt(frame); clock.step(time);
      if (frame == 200 || frame == 270 || frame == 800 || frame == 1460 || frame == 2060) {
        clock.draw(canvas.data(),time);
        ppm(canvas,std::string(argv[1]) + "-" + std::to_string(frame) + ".ppm");
      }
    }
  }
  std::cout << "Geometry Clock: eased quarter-speed dwell, continuous scroll, 2x art, portrait guards, rollover and one-hour autoplay passed; state "
            << sizeof(GeometryClock) << " bytes\n";
}
