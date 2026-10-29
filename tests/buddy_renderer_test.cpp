#include <array>
#include <assert.h>
#include <iostream>
#include <set>
#include <vector>
#include "../firmware/flappy/buddy_renderer.h"
#include "../firmware/flappy/buddy_clock.h"

using namespace flappy;
static constexpr unsigned Pixels = 135 * 240, Guard = 0x6538;
struct Canvas {
  std::array<uint16_t,Pixels + 64> data;
  Canvas() { data.fill(Guard); }
  uint16_t* pixels() { return data.data() + 32; }
  void guarded() const {
    for (unsigned i = 0; i < 32; ++i) assert(data[i] == Guard && data[Pixels + 32 + i] == Guard);
  }
  uint64_t hash() const {
    uint64_t n = 1469598103934665603ULL;
    for (unsigned i = 32; i < Pixels + 32; ++i) n = (n ^ data[i]) * 1099511628211ULL;
    return n;
  }
};

int main() {
  Canvas first, second;
  BuddyGame game; game.reset(123);
  BuddyRenderer::draw(first.pixels(),game,42);
  BuddyRenderer::draw(second.pixels(),game,42);
  assert(first.data == second.data); first.guarded();
  std::set<uint64_t> themes;
  BuddyRenderer::Object objects[BuddyGame::ObjectCount];
  for (unsigned i = 0; i < BuddyGame::ObjectCount; ++i)
    objects[i] = {BuddyGame::ObjectKind(i % 12),float(i % 3),float(i % 7) / 6,uint16_t(i)};
  for (unsigned theme = 0; theme < 5; ++theme) {
    BuddyRenderer::Scene scene;
    scene.theme = BuddyGame::Theme(theme); scene.objects = objects; scene.count = BuddyGame::ObjectCount;
    scene.frame = 100; scene.jump = .5f; scene.board = true; scene.snake = 10;
    BuddyRenderer::drawScene(first.pixels(),scene);
    BuddyRenderer::drawScene(second.pixels(),scene);
    assert(first.data == second.data); first.guarded(); themes.insert(first.hash());
    for (unsigned phase = 0; phase < 4; ++phase) {
      scene.phase = BuddyGame::Phase(phase);
      for (unsigned frame = 0; frame < 300; frame += 17) {
        scene.frame = frame; scene.lane = frame % 3; scene.rolling = frame % 2;
        BuddyRenderer::drawScene(first.pixels(),scene); first.guarded();
      }
    }
  }
  assert(themes.size() == 5);
  std::set<uint64_t> poses;
  for (unsigned i = 0; i < 6; ++i) {
    first.data.fill(Guard);
    BuddyRenderer::drawBuddy(first.pixels(),67,180,BuddyRenderer::Pose(i),14,0,1,3);
    first.guarded(); poses.insert(first.hash());
    for (int x : {-100,0,134,250}) for (int y : {-100,0,239,350}) {
      BuddyRenderer::drawBuddy(first.pixels(),x,y,BuddyRenderer::Pose(i),14,3,3,4); first.guarded();
    }
  }
  assert(poses.size() == 6);
  for (auto pose : {BuddyRenderer::Pose::Run,BuddyRenderer::Pose::Push,BuddyRenderer::Pose::Jump,BuddyRenderer::Pose::Roll}) {
    first.data.fill(Guard); second.data.fill(Guard);
    BuddyRenderer::drawBuddy(first.pixels(),67,180,pose,14,0,0,2);
    BuddyRenderer::drawBuddy(second.pixels(),67,180,pose,14,0,1,2);
    assert(first.data == second.data); // Gameplay shows the hood, independent of LED face.
  }
  first.data.fill(Guard);second.data.fill(Guard);
  BuddyRenderer::drawBuddy(first.pixels(),67,180,BuddyRenderer::Pose::Dance,14,0,0,2);
  BuddyRenderer::drawBuddy(second.pixels(),67,180,BuddyRenderer::Pose::Dance,14,0,1,2);
  assert(first.data != second.data);
  // The finish starts with a forward roll, then a taller dance pose. The title stays fixed.
  BuddyRenderer::Scene finish;
  finish.phase = BuddyGame::Phase::Celebrate; finish.frame = 14; finish.phaseTicks = 0;
  BuddyRenderer::drawScene(first.pixels(),finish);
  finish.phaseTicks = 80; BuddyRenderer::drawScene(second.pixels(),finish);
  first.guarded(); second.guarded();
  unsigned bodyDifference = 0;
  for (unsigned y = 174; y < 214; ++y) for (unsigned x = 48; x < 88; ++x)
    bodyDifference += first.pixels()[y * 135 + x] != second.pixels()[y * 135 + x];
  assert(bodyDifference > 150);
  for (unsigned y = 78; y < 144; ++y) for (unsigned x = 7; x < 129; ++x)
    assert(first.pixels()[y * 135 + x] == second.pixels()[y * 135 + x]);
  finish.phaseTicks = 180; BuddyRenderer::drawScene(second.pixels(),finish);
  assert(first.data == second.data);
  BuddyRenderer::Scene malformed;
  malformed.objects = objects; malformed.count = 999;
  for (auto& o : objects) { o.lane = -99; o.depth = 99; }
  malformed.lane = -99; malformed.jump = 999;
  BuddyRenderer::drawScene(first.pixels(),malformed); first.guarded();
  BuddyRenderer::draw(nullptr,game); BuddyRenderer::drawScene(nullptr,malformed);
  BuddyClock clock, copy; clock.reset(12); copy.reset(12);
  ClockTime time; time.hour = 23; time.minute = 59;
  std::set<uint64_t> clockFrames;
  for (unsigned frame = 0; frame < 1800; ++frame) {
    time.second = uint8_t(frame / 60 % 60); clock.step(time); copy.step(time);
    if (frame % 59 == 0) {
      clock.draw(first.pixels(),time); copy.draw(second.pixels(),time);
      assert(first.data == second.data); first.guarded(); second.guarded();
      clockFrames.insert(first.hash());
      for (unsigned y = 225; y < 240; ++y) for (unsigned x = 0; x < 135; ++x)
        assert(first.pixels()[y * 135 + x] == BuddyRenderer::rgb(37,28,45));
    }
  }
  assert(clockFrames.size() > 20);
  clock.draw(first.pixels(),time); const auto before = first.hash();
  time.minute = 0; time.hour = 0; clock.draw(first.pixels(),time); assert(before != first.hash());
  clock.draw(nullptr,time);
  std::cout << "Buddy graphics: five worlds, six poses, clipped canvas, deterministic clock and reserved footer passed.\n";
}
