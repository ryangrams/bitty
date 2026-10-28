#include "../firmware/flappy/jetpack_clock.h"

#include <array>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>

using flappy::ClockTime;
using flappy::JetpackClock;
namespace Scene = flappy::ClockScene;

namespace flappy {
struct JetpackClockTestAccess {
  static unsigned cycle(const JetpackClock& clock) { return clock.cycle_; }
  static unsigned count(const JetpackClock& clock) { return clock.coinCount_; }
  static unsigned minute(const JetpackClock& clock) { return clock.minute_; }
  static unsigned vehicle(const JetpackClock& clock) { return clock.vehicle_; }
  static unsigned collected(const JetpackClock& clock) {
    unsigned total = 0;
    for (unsigned i = 0; i < clock.coinCount_; ++i) total += clock.coins_[i].state == JetpackClock::Collected;
    return total;
  }
  static unsigned attracted(const JetpackClock& clock) {
    unsigned total = 0;
    for (unsigned i = 0; i < clock.coinCount_; ++i) total += clock.coins_[i].state == JetpackClock::Attracted;
    return total;
  }
  static std::set<std::pair<int,int> > cells(const JetpackClock& clock) {
    std::set<std::pair<int,int> > result;
    int top = 240;
    for (unsigned i = 0; i < clock.coinCount_; ++i) if (clock.coins_[i].homeY < top) top = clock.coins_[i].homeY;
    for (unsigned i = 0; i < clock.coinCount_; ++i)
      result.insert({clock.coins_[i].homeX, clock.coins_[i].homeY - top});
    return result;
  }
  static void invariants(const JetpackClock& clock) {
    assert(clock.coinCount_ > 0 && clock.coinCount_ <= 64);
    assert(clock.cycle_ <= 840);
    assert(clock.playerY_ >= 50 * 256 && clock.playerY_ <= 206 * 256);
    assert(clock.velocity_ >= -440 && clock.velocity_ <= 440);
    assert(clock.vehicle_ <= 7 && clock.nextVehicle_ >= 1 && clock.nextVehicle_ <= 7);
    for (unsigned i = 0; i < clock.coinCount_; ++i) {
      const auto& coin = clock.coins_[i];
      assert(coin.homeX >= 28 && coin.homeX <= 96);
      assert(coin.homeY >= 28 && coin.homeY <= 152);
      assert(coin.state <= JetpackClock::Collected);
      assert(coin.x > -4096 && coin.x < 5000);
      assert(coin.y >= 0 && coin.y < 240 * 16);
    }
    for (const auto& staff : clock.staff_) {
      assert(staff.x >= -20 * 16 && staff.x <= 198 * 16);
      assert(staff.pose < 3);
    }
    for (const auto& hazard : clock.traffic_) if (hazard.active) {
      assert(hazard.x > -36 * 16 && hazard.x <= 160 * 16);
      assert(hazard.y >= 145 && hazard.y <= 191);
      assert(hazard.kind <= 2);
    }
  }
};
}
using Access = flappy::JetpackClockTestAccess;

struct Canvas {
  static constexpr uint16_t Empty = 0xf81f;
  std::array<uint16_t, Scene::Width * Scene::Height + 2> pixels;
  Canvas() { pixels.fill(Empty); pixels.front() = 0x1234; pixels.back() = 0x5678; }
  uint16_t* data() { return pixels.data() + 1; }
  const uint16_t* data() const { return pixels.data() + 1; }
  void guards() const { assert(pixels.front() == 0x1234 && pixels.back() == 0x5678); }
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
  time.ms = uint32_t(frame) * 1000u / 60u;
  return time;
}

static void boundsAndDeterminism() {
  JetpackClock a, b, c;
  a.reset(1742); b.reset(1742); c.reset(491);
  Canvas first, second, other;
  a.draw(first.data(), timeAt(0)); first.guards();
  unsigned changedSeed = 0;
  for (unsigned frame = 0; frame < 5400; ++frame) {
    const ClockTime time = timeAt(frame);
    a.step(time); b.step(time); c.step(time);
    Access::invariants(a); Access::invariants(b); Access::invariants(c);
    if (frame % 23u == 0) {
      const JetpackClock& immutable = a;
      immutable.draw(first.data(), time);
      b.draw(second.data(), time); c.draw(other.data(), time);
      assert(first.hash() == second.hash());
      changedSeed += first.hash() != other.hash();
      const uint64_t before = first.hash();
      immutable.draw(first.data(), time); assert(first.hash() == before);
      for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) assert(first.data()[i] != Canvas::Empty);
      first.guards(); second.guards(); other.guards();
    }
  }
  assert(changedSeed > 180);
  a.draw(nullptr, timeAt(0));
}

static void coinDigitsAndFormatChanges() {
  JetpackClock clock;
  clock.reset(5);
  ClockTime time; time.hour = 12; time.minute = 34;
  clock.step(time);
  assert(Access::count(clock) == 39);
  static const char* digits[] = {
    "010110010010111", "111001111100111", "111001111001111", "101101111001001"
  };
  std::set<std::pair<int,int> > expected;
  for (unsigned i = 0; i < 4; ++i)
    for (int y = 0; y < 5; ++y) for (int x = 0; x < 3; ++x)
      if (digits[i][y * 3 + x] == '1') expected.insert({28 + int(i % 2) * 44 + x * 12, int(i / 2) * 68 + y * 12});
  assert(Access::cells(clock) == expected);
  for (unsigned i = 0; i < 240; ++i) clock.step(time);
  assert(Access::cycle(clock) == 240);
  time.hour = 23; time.minute = 59; clock.step(time);
  assert(Access::cycle(clock) == 0 && Access::minute(clock) == 1439);
  const auto twentyFour = Access::cells(clock);
  time.twentyFourHour = false; clock.step(time);
  assert(Access::cycle(clock) == 0 && Access::cells(clock) != twentyFour);
  time.hour = 0; time.minute = 0; time.ms = UINT32_MAX; clock.step(time);
  assert(Access::cycle(clock) == 0 && Access::minute(clock) == 0);
  time.ms = 0; clock.step(time);
  assert(Access::cycle(clock) == 1);
}

static void collectAndReform() {
  JetpackClock clock;
  clock.reset(9);
  const ClockTime time = timeAt(0);
  clock.step(time);
  const unsigned count = Access::count(clock);
  unsigned collected = 0, attracted = 0;
  for (unsigned frame = 1; frame <= 840; ++frame) {
    clock.step(time);
    if (frame < 558) assert(Access::collected(clock) == 0 && Access::attracted(clock) == 0);
    if (Access::collected(clock) > collected) collected = Access::collected(clock);
    if (Access::attracted(clock) > attracted) attracted = Access::attracted(clock);
  }
  assert(attracted >= 2 && collected >= 7 && collected < count / 2);
  clock.step(time);
  assert(Access::cycle(clock) == 0 && Access::count(clock) == count);
  assert(Access::collected(clock) == 0 && Access::attracted(clock) == 0);
}

static void longRunKeepsMoving() {
  JetpackClock clock;
  clock.reset(10);
  std::set<unsigned> vehicles;
  std::set<uint64_t> views;
  Canvas canvas;
  for (unsigned frame = 0; frame < 24u * 60u * 60u; ++frame) {
    const ClockTime time = timeAt(frame);
    clock.step(time);
    Access::invariants(clock);
    vehicles.insert(Access::vehicle(clock));
    if (frame % 777u == 0) {
      clock.draw(canvas.data(), time);
      canvas.guards(); views.insert(canvas.hash());
    }
  }
  assert(vehicles.size() == 8 && views.size() > 100);
}

static void ppm(const Canvas& canvas, const std::string& path) {
  std::ofstream output(path, std::ios::binary); assert(output);
  output << "P6\n135 240\n255\n";
  for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) {
    const uint16_t pixel = canvas.data()[i];
    const char rgb[] = {char((pixel >> 11) * 255 / 31), char((pixel >> 5 & 63) * 255 / 63), char((pixel & 31) * 255 / 31)};
    output.write(rgb, 3);
  }
}

int main(int argc, char** argv) {
  static_assert(sizeof(JetpackClock) <= 1200, "Scene state grew beyond its current budget");
  boundsAndDeterminism();
  coinDigitsAndFormatChanges();
  collectAndReform();
  longRunKeepsMoving();
  if (argc > 1) {
    JetpackClock clock; clock.reset(9);
    Canvas canvas;
    for (unsigned frame = 0; frame < 1800; ++frame) {
      const ClockTime time = timeAt(frame);
      clock.step(time);
      if (frame == 180 || frame == 620 || frame == 1040 || frame == 1380) {
        clock.draw(canvas.data(), time);
        ppm(canvas, std::string(argv[1]) + "-" + std::to_string(frame) + ".ppm");
      }
    }
  }
  std::cout << "Jetpack Clock: portrait guards, deterministic autoplay, coin digits, collection, reformation and 24-minute run passed; state "
            << sizeof(JetpackClock) << " bytes\n";
}
