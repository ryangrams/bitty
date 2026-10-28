#include "../firmware/flappy/jetpack_game.h"

#include <array>
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <cmath>
#include <iostream>

namespace flappy {
struct JetpackTestAccess {
  static void clear(JetpackGame& game) {
    for (auto& zap : game.zaps_) zap = JetpackGame::Zap{};
    for (auto& coin : game.coinField_) coin = JetpackGame::Coin{};
    for (auto& missile : game.missiles_) missile = JetpackGame::Missile{};
    game.pickup_ = JetpackGame::Pickup{};
    game.distanceToHazard_ = game.distanceToCoins_ = game.distanceToVehicle_ = 100000000;
    game.missileGap_ = false;
  }
  static void player(JetpackGame& game, int y, int velocity = 0) {
    game.y_ = y * JetpackGame::FlightUnit; game.velocity_ = velocity * 256;
  }
  static void running(JetpackGame& game) {
    game.phase_ = JetpackGame::Phase::Running; game.armed_ = true;
  }
  static void zap(JetpackGame& game, int x, int y, int dx, int dy) {
    game.zaps_[0].active = true; game.zaps_[0].x = x * JetpackGame::Unit;
    game.zaps_[0].y = int16_t(y); game.zaps_[0].dx = int16_t(dx); game.zaps_[0].dy = int16_t(dy);
  }
  static void trail(JetpackGame& game) {
    for (unsigned i = 0; i < 9; ++i) {
      game.coinField_[i].active = true; game.coinField_[i].x = int32_t(66 + i * 11) * JetpackGame::Unit;
      game.coinField_[i].y = int16_t(83 + (i < 5 ? i : 8 - i) * 4);
    }
  }
  static void coin(JetpackGame& game, int x, int y) {
    game.coinField_[0].active = true; game.coinField_[0].x = x * JetpackGame::Unit;
    game.coinField_[0].y = int16_t(y);
  }
  static void pickup(JetpackGame& game, JetpackGame::Vehicle kind) {
    game.pickup_.active = true; game.pickup_.x = (JetpackGame::PlayerX + 1) * JetpackGame::Unit;
    game.pickup_.y = int16_t(game.playerY()); game.pickup_.kind = kind;
  }
  static void immunity(JetpackGame& game, unsigned ticks) { game.immune_ = uint16_t(ticks); }
  static int velocity(const JetpackGame& game) { return game.velocity_; }
  static double preciseY(const JetpackGame& game) { return double(game.y_) / double(JetpackGame::FlightUnit); }
  static double preciseVelocity(const JetpackGame& game) { return double(game.velocity_) / double(JetpackGame::FlightUnit); }
  static void meters(JetpackGame& game, uint32_t meters) {
    game.world_ = uint32_t(uint64_t(meters) * 69 * JetpackGame::Unit / 10);
  }
  static void hit(JetpackGame& game) { game.hit(); }
  static void world(JetpackGame& game, uint32_t world) { game.world_ = world; }
  static uint32_t world(const JetpackGame& game) { return game.world_; }
  static void missile(JetpackGame& game, unsigned warning) {
    auto& missile = game.missiles_[0]; missile.active = true; missile.x = 254 * JetpackGame::Unit;
    missile.y = int16_t(game.playerY()); missile.warning = uint8_t(warning);
  }
  static bool near(int px, int py, int x, int y, int dx, int dy, int radius = 6) {
    return JetpackGame::nearSegment(px, py, x, y, dx, dy, radius);
  }
  static unsigned coinCount(const JetpackGame& game) {
    unsigned count = 0; for (const auto& coin : game.coinField_) count += coin.active; return count;
  }
};
}
using Game = flappy::JetpackGame;
using Access = flappy::JetpackTestAccess;
using Input = flappy::ArcadeInput;
using Phase = Game::Phase;
using Vehicle = Game::Vehicle;
using Cue = flappy::ArcadeCue;
constexpr int Q = Game::Unit;
static Input press() { Input in; in.action = in.pressed = true; return in; }
static Input hold() { Input in; in.action = true; return in; }
static void start(Game& game) { game.tick(Input{}); game.tick(press()); assert(game.phase() == Phase::Running); }

static void timingFlightAndRearming() {
  Game game;
  assert(sizeof(game) < 6144 && game.phase() == Phase::Ready && game.score() == 0);
  for (int i = 0; i < 60; ++i) game.tick(hold());
  game.tick(press());
  assert(game.phase() == Phase::Ready);
  start(game);
  Access::clear(game);
  const uint32_t begin = Access::world(game);
  for (int i = 0; i < 60; ++i) game.tick(hold());
  const uint32_t traversed = Access::world(game) - begin;
  assert(traversed >= 60 * 286 && traversed <= 60 * 289);
  assert(game.playerY() == Game::Ceiling && Access::velocity(game) == 0);
  for (int i = 0; i < 80; ++i) game.tick(Input{});
  assert(game.playerY() == Game::Floor && Access::velocity(game) == 0);
  const int oldSpeed = game.speed();
  game.setNextDifficulty(2);
  assert(game.speed() == oldSpeed);
  Access::hit(game);
  assert(game.phase() == Phase::Crashing && game.takeCue() == Cue::Death);
  for (int i = 0; i < 64; ++i) game.tick(hold());
  assert(game.gameOver());
  for (int i = 0; i < 100; ++i) game.tick(hold());
  game.tick(press()); assert(game.gameOver());
  game.tick(Input{}); game.tick(press());
  assert(game.phase() == Phase::Running && !game.gameOver() && game.score() == 0);
  assert(game.speed() == 329);
  assert(game.takeCue() == Cue::Fire && game.takeCue() == Cue::None);
}

static unsigned referenceFlight(bool held, bool endSpeed) {
  Game game; start(game); Access::clear(game);
  Access::player(game, held ? Game::Floor : Game::Ceiling);
  const double scale = 92.0 / 240.0;
  double y = held ? -120 : 120, velocity = 0;
  unsigned firstBoundary = 0;
  for (unsigned frame = 1; frame <= 160; ++frame) {
    Access::meters(game, endSpeed ? 2250 : 0);
    const double nativeSpeed = endSpeed ? 480 : 175;
    const double factor = 1 + 0.5 * (nativeSpeed / 175.0 - 1);
    velocity += factor * ((held ? 0.36 : 0) - 0.15);
    velocity *= 0.98;
    y += velocity;
    if (y > 120) { y = 120; velocity = 0; }
    if (y < -120) { y = -120; velocity = 0; }
    game.tick(held ? hold() : Input{});
    const double expectedY = 114 - (y + 120) * scale;
    assert(std::abs(Access::preciseY(game) - expectedY) < 0.1);
    assert(std::abs(Access::preciseVelocity(game) + velocity * scale) < 0.005);
    if (!firstBoundary && game.playerY() == (held ? Game::Ceiling : Game::Floor) &&
        Access::velocity(game) == 0) firstBoundary = frame;
  }
  return firstBoundary;
}

static void independentFlightAndDistance() {
  assert(referenceFlight(true, false) == 57);
  assert(referenceFlight(false, false) == 70);
  assert(referenceFlight(true, true) == 40);
  assert(referenceFlight(false, true) == 48);
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  double nativeY = (114 - 70) * 240.0 / 92.0 - 120, velocity = 0;
  uint32_t random = 5761;
  bool held = false;
  double maximumError = 0;
  for (unsigned frame = 0; frame < 20000; ++frame) {
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    if (random % 13 == 0) held = !held;
    const uint32_t meters = frame % 3000;
    Access::meters(game, meters);
    const double distance = game.distance();
    const double nativeSpeed = 175 + 305 * std::min(distance / 2250.0, 1.0);
    const double factor = 1 + 0.5 * (nativeSpeed / 175.0 - 1);
    velocity = (velocity + factor * ((held ? 0.36 : 0) - 0.15)) * 0.98;
    nativeY += velocity;
    if (nativeY > 120) { nativeY = 120; velocity = 0; }
    if (nativeY < -120) { nativeY = -120; velocity = 0; }
    game.tick(held ? hold() : Input{});
    const double expectedY = 114 - (nativeY + 120) * 92 / 240;
    const double error = std::abs(Access::preciseY(game) - expectedY);
    if (error > maximumError) maximumError = error;
    assert(error < 0.1);
    assert(std::abs(Access::preciseVelocity(game) + velocity * 92 / 240) < 0.005);
  }
  Game easy, hard;
  easy.reset(71, 0); hard.reset(71, 2); start(easy); start(hard);
  Access::clear(easy); Access::clear(hard);
  for (unsigned i = 0; i < 240; ++i) {
    Access::meters(easy, 1000); Access::meters(hard, 1000);
    const Input in = i % 90 < 45 ? hold() : Input{};
    easy.tick(in); hard.tick(in);
    assert(Access::preciseY(easy) == Access::preciseY(hard));
    assert(Access::velocity(easy) == Access::velocity(hard));
    assert(easy.speed() < hard.speed());
  }
  Access::world(game, 69 * Q); assert(game.distance() == 10);
  Access::meters(game, 0); assert(game.speed() == 286);
  Access::meters(game, 1125); assert(game.speed() == 536);
  Access::meters(game, 2250); assert(game.speed() == 785);
  Access::world(game, 0x6fffffffu);
  game.tick(Input{});
  const uint32_t capped = Access::world(game);
  assert(capped >= 0x70000000u && capped < 0x70000400u);
  const uint32_t expectedDistance = uint32_t(uint64_t(capped) * 10 / (69 * Q));
  for (unsigned i = 0; i < 1000; ++i) game.tick(hold());
  assert(Access::world(game) == capped && game.distance() == expectedDistance);
  assert(game.speed() == 785);
  std::cout << "Jetpack maximum flight quantization error: " << maximumError << " px\n";
}

static void collisionGeometryAndCoins() {
  assert(Access::near(43, 70, 43, 50, 0, 40));
  assert(!Access::near(50, 70, 43, 50, 0, 40));
  assert(Access::near(43, 70, 25, 70, 35, 0));
  assert(!Access::near(43, 77, 25, 70, 35, 0));
  assert(Access::near(43, 70, 30, 57, 26, 26));
  assert(!Access::near(31, 81, 30, 57, 26, 26));
  assert(Access::near(43, 70, 30, 83, 26, -26));
  assert(Access::near(43, 70, 43, 70, 0, 0));
  assert(!Access::near(50, 70, 43, 70, 0, 0));
  for (int kind = 0; kind < 4; ++kind) {
    Game game; start(game); Access::clear(game); Access::player(game, 70);
    Access::world(game, 0x60000000);
    const int dx = kind == 0 ? 0 : 30, dy = kind == 1 ? 0 : kind == 3 ? -30 : 30;
    Access::zap(game, 43 - dx / 2 + 3, 70 - dy / 2, dx, dy);
    game.tick(Input{});
    assert(game.phase() == Phase::Crashing);
  }
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  Access::coin(game, 44, 70); game.tick(Input{});
  assert(game.coins() == 1 && game.score() == game.distance());
  assert(game.takeCue() == Cue::Power);
  Access::zap(game, 44, 84, 35, 0); game.tick(Input{});
  assert(game.phase() == Phase::Running);
}

static void fatalCueWinsOverCoinCollection() {
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  game.takeCue();
  Access::coin(game, 44, 70); Access::zap(game, 44, 50, 0, 40);
  game.tick(Input{});
  assert(game.coins() == 1 && game.phase() == Phase::Crashing);
  assert(game.takeCue() == Cue::Death);

  Game pickup; start(pickup); Access::clear(pickup); Access::player(pickup, 70);
  pickup.takeCue();
  Access::pickup(pickup, Vehicle::Armor); Access::zap(pickup, 44, 50, 0, 40);
  pickup.tick(Input{});
  assert(pickup.phase() == Phase::Running && pickup.vehicle() == Vehicle::Armor);
  assert(pickup.takeCue() == Cue::Win);
}

static void vehiclesAbsorbAndChangePhysics() {
  Game armor; start(armor); Access::clear(armor); Access::player(armor, 70);
  armor.takeCue(); Access::pickup(armor, Vehicle::Armor); armor.tick(Input{});
  assert(armor.vehicle() == Vehicle::Armor && armor.takeCue() == Cue::Win);
  for (int i = 0; i < 10; ++i) armor.tick(hold());
  assert(Access::velocity(armor) < 0);
  Access::immunity(armor, 0); Access::player(armor, 70);
  Access::zap(armor, 44, 50, 0, 40); armor.tick(Input{});
  assert(armor.vehicle() == Vehicle::None && armor.phase() == Phase::Running);
  Access::zap(armor, 44, 50, 0, 40); armor.tick(Input{});
  assert(armor.phase() == Phase::Running);
  Access::immunity(armor, 0); Access::player(armor, 70);
  armor.tick(Input{}); assert(armor.phase() == Phase::Crashing);

  Game gravity; start(gravity); Access::clear(gravity); Access::player(gravity, 70);
  Access::pickup(gravity, Vehicle::Gravity); gravity.tick(hold());
  assert(gravity.vehicle() == Vehicle::Gravity && !gravity.gravityUp());
  gravity.tick(press()); assert(!gravity.gravityUp());
  gravity.tick(Input{}); gravity.tick(press()); assert(gravity.gravityUp());
  for (int i = 0; i < 40; ++i) gravity.tick(hold());
  assert(gravity.gravityUp() && gravity.playerY() == Game::Ceiling);
  gravity.tick(Input{}); gravity.tick(press()); assert(!gravity.gravityUp());
  for (int i = 0; i < 50; ++i) gravity.tick(hold());
  assert(gravity.playerY() == Game::Floor);
}

static void independentGravityFlight() {
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  Access::pickup(game, Vehicle::Gravity); game.tick(Input{});
  game.tick(Input{}); Access::player(game, 70);
  double y = (114 - Access::preciseY(game)) * 240.0 / 92.0 - 120, velocity = 0;
  bool held = false, up = false;
  for (unsigned i = 0; i < 1200; ++i) {
    const bool next = i % 71 == 0;
    if (next && !held) up = !up;
    Input in; in.action = next; in.pressed = next && !held; held = next;
    velocity = (velocity + (up ? 0.26 : -0.26)) * 0.98;
    y += velocity;
    if (y > 120) { y = 120; velocity = 0; }
    if (y < -120) { y = -120; velocity = 0; }
    game.tick(in);
    assert(game.gravityUp() == up);
    assert(std::abs(Access::preciseY(game) - (114 - (y + 120) * 92 / 240)) < 0.1);
    assert(std::abs(Access::preciseVelocity(game) + velocity * 92 / 240) < 0.005);
  }
}

static void missilesTrackThenLock() {
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  Access::missile(game, 80);
  for (int i = 0; i < 49; ++i) {
    Access::player(game, 70 + i % 3);
    game.tick(Input{});
    assert(game.missile(0).warning == 79 - i);
    assert(game.missile(0).y == game.playerY());
  }
  game.tick(Input{});
  const int locked = game.missile(0).y;
  Access::player(game, 30);
  for (int i = 0; i < 30; ++i) {
    game.tick(hold()); assert(game.missile(0).y == locked);
    assert(game.phase() == Phase::Running);
  }
  assert(game.missile(0).warning == 0 && game.missile(0).x == 254 * Q);
  unsigned approach = 0;
  while (game.missile(0).active) {
    game.tick(hold()); ++approach; assert(game.phase() == Phase::Running);
  }
  assert(approach >= 35 && approach < 60);
}

static uint32_t checksum(const std::array<uint16_t, Game::Width * Game::Height>& pixels) {
  uint32_t h = 2166136261u;
  for (uint16_t pixel : pixels) { h ^= pixel; h *= 16777619u; }
  return h;
}
static void deterministicProgressionAndRenderBounds() {
  struct Guarded { uint32_t first = 0xa5a55a5a; std::array<uint16_t, Game::Width * Game::Height> pixels{};
    uint32_t last = 0x5a5aa5a5; } buffer;
  Game a, b; a.reset(98765); b.reset(98765); start(a); start(b);
  Access::immunity(a, 65000); Access::immunity(b, 65000);
  unsigned warnings = 0, zaps = 0, pickups = 0, coinPatterns = 0;
  int lastSpeed = a.speed();
  for (unsigned i = 0; i < 60000; ++i) {
    Input in; in.action = (i / 35) % 2; in.pressed = i % 70 == 35;
    Access::immunity(a, 65000); Access::immunity(b, 65000);
    a.tick(in); b.tick(in);
    assert(a.phase() == Phase::Running && b.phase() == Phase::Running);
    assert(a.score() == b.score() && a.playerY() == b.playerY());
    assert(a.speed() >= lastSpeed && a.speed() <= 785); lastSpeed = a.speed();
    assert(a.playerY() >= Game::Ceiling && a.playerY() <= Game::Floor);
    for (unsigned j = 0; j < Game::ZapCount; ++j) if (a.zap(j).active) {
      const auto& zap = a.zap(j); ++zaps;
      assert(zap.y >= Game::Ceiling && zap.y <= Game::Floor && zap.y + zap.dy >= Game::Ceiling && zap.y + zap.dy <= Game::Floor);
      assert(zap.dx <= 32 && zap.dy >= -39 && zap.dy <= 47);
    }
    for (unsigned j = 0; j < Game::MissileCount; ++j) warnings += a.missile(j).warning > 0;
    pickups += a.pickup().active;
    coinPatterns += Access::coinCount(a) > 0;
    if (i % 127 == 0) {
      a.draw(buffer.pixels.data(), 12345);
      assert(buffer.first == 0xa5a55a5a && buffer.last == 0x5a5aa5a5);
      const uint32_t hash = checksum(buffer.pixels);
      b.draw(buffer.pixels.data(), 12345); assert(checksum(buffer.pixels) == hash);
    }
  }
  assert(warnings > 100 && zaps > 1000 && pickups > 100 && coinPatterns > 1000);
  assert(a.distance() > 15000 && a.speed() == 785);
  const uint32_t before = a.score();
  a.draw(buffer.pixels.data()); a.draw(nullptr); assert(a.score() == before);
  for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
    a.reset(5, uint8_t(difficulty)); start(a); Access::clear(a);
    const int speed = a.speed();
    assert(speed == (difficulty == 0 ? 243 : difficulty == 1 ? 286 : 329));
  }
  std::cout << "Jetpack state: " << sizeof(Game) << " bytes\n";
}

static void randomizedRuns() {
  Game game;
  std::array<uint16_t, Game::Width * Game::Height> pixels{};
  uint32_t random = 641;
  unsigned deaths = 0, restarts = 0;
  bool held = false;
  for (unsigned i = 0; i < 250000; ++i) {
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    const bool next = game.phase() == Phase::Running ? ((random & 15) ? held : !held) : bool(i & 2);
    Input in; in.action = next; in.pressed = next && !held; held = next;
    const Phase before = game.phase();
    game.tick(in);
    deaths += before == Phase::Running && game.phase() == Phase::Crashing;
    restarts += before == Phase::Over && game.phase() == Phase::Running;
    assert(game.playerY() >= Game::Ceiling && game.playerY() <= Game::Floor);
    assert(game.speed() >= 243 && game.speed() <= 903);
    if (i % 211 == 0) game.draw(pixels.data());
  }
  assert(deaths > 50 && restarts > 50);
}

static void dumpScene(const char* path, int vehicle) {
  Game game; game.reset(734); start(game); Access::clear(game);
  Access::player(game, vehicle == 2 ? 55 : 70);
  Access::zap(game, 137, 36, 26, 35);
  Access::trail(game);
  Access::missile(game, 40);
  if (vehicle) Access::pickup(game, vehicle == 2 ? Vehicle::Gravity : Vehicle::Armor);
  game.tick(Input{});
  if (vehicle == 2) { game.tick(Input{}); game.tick(press()); }
  else if (!vehicle) for (int i = 0; i < 4; ++i) game.tick(hold());
  Access::immunity(game, 0);
  auto pixels = std::array<uint16_t, Game::Width * Game::Height>{};
  game.draw(pixels.data(), 1234);
  std::ofstream file(path, std::ios::binary);
  file << "P6\n240 135\n255\n";
  for (uint16_t pixel : pixels) {
    const char rgb[3] = {char((pixel >> 11) * 255 / 31), char((pixel >> 5 & 63) * 255 / 63), char((pixel & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}

int main(int argc, char** argv) {
  timingFlightAndRearming(); independentFlightAndDistance(); collisionGeometryAndCoins(); fatalCueWinsOverCoinCollection(); vehiclesAbsorbAndChangePhysics();
  independentGravityFlight(); missilesTrackThenLock(); deterministicProgressionAndRenderBounds(); randomizedRuns();
  if (argc > 1) dumpScene(argv[1], argc > 2 ? argv[2][0] - '0' : 0);
  std::cout << "Jetpack tests passed\n";
}
