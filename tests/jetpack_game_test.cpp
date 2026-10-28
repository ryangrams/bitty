#include "../firmware/flappy/jetpack_game.h"

#include <array>
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <cmath>
#include <iostream>

namespace flappy {
struct JetpackProgressionTestAccess {
  static void due(JetpackProgression& policy, uint32_t power, uint32_t vehicle) {
    policy.powerDue_ = power; policy.vehicleDue_ = vehicle; policy.nextContent_ = 0;
  }
};
struct JetpackTestAccess {
  static void clear(JetpackGame& game) {
    for (auto& zap : game.zaps_) zap = JetpackGame::Zap{};
    for (auto& coin : game.coinField_) coin = JetpackGame::Coin{};
    for (auto& coin : game.attractedCoins_) coin = JetpackGame::AttractedCoin{};
    game.cashShowerRemaining_ = 0;
    for (auto& missile : game.missiles_) missile = JetpackGame::Missile{};
    game.pickup_ = JetpackGame::Pickup{};
    game.bonus_ = game.cashPayload_ = JetpackGame::Bonus{}; game.token_ = game.samToken_ = JetpackGame::Token{}; game.laser_ = JetpackGame::Laser{};
    for (auto& npc : game.scientists_) npc = JetpackGame::Scientist{};
    for (auto& rotator : game.rotators_) rotator = JetpackGame::Rotator{};
    for (auto& group : game.importedGroups_) group = JetpackGame::ImportedGroup{};
    for (auto& power : game.powerTicks_) power = 0;
    game.spinTokens_ = 0;
    game.progression_.setEnabled(false);
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
    game.coinField_[0].y = int16_t(y); game.coinField_[0].value = 1;
  }
  static void gem(JetpackGame& game, int x, int y) { coin(game,x,y); game.coinField_[0].value = 5; }
  static bool ordinaryCoin(const JetpackGame& game) { return game.coinField_[0].active; }
  static bool attract(JetpackGame& game, int x, int y, uint8_t value) { return game.attractCoin(x * JetpackGame::Unit,y,value); }
  static int attractedX(const JetpackGame& game) { return int(game.attractedCoins_[0].x); }
  static void shower(JetpackGame& game, uint16_t count, uint8_t gems) {
    game.cashShowerRemaining_ = count; game.cashShowerGemPercent_ = gems;
    game.cashShowerX_ = 43 * JetpackGame::Unit; game.cashShowerY_ = 70 * JetpackGame::Unit;
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
    game.world_ = uint32_t((uint64_t(meters) * 69 * JetpackGame::Unit + 9) / 10);
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
  static void equip(JetpackGame& game, JetpackGame::Vehicle vehicle) { game.equip(vehicle); }
  static uint8_t teleportStage(const JetpackGame& game) { return game.vehiclePhysics_.teleportStage; }
  static uint16_t immunity(const JetpackGame& game) { return game.immune_; }
  static int32_t nativeRideSpeed(const JetpackGame& game) { return game.vehiclePhysics_.nativeSpeed; }
  static void rocketHold(JetpackGame& game) { game.rocketTime_.advance(90); game.movementScale_ = game.rocketTime_.timeScaleQ16(uint32_t(game.normalNativeVelocity())); }
  static void power(JetpackGame& game, JetpackGame::Power power) { game.activatePower(power); }
  static void laser(JetpackGame& game, unsigned ticks, int y) { game.laser_.active = true; game.laser_.ticks = uint16_t(ticks); game.laser_.y = int16_t(y); }
  static void npc(JetpackGame& game) { game.scientists_[0].active = true; game.scientists_[0].x = 45 * JetpackGame::Unit; game.scientists_[0].y = 114 * JetpackGame::Unit; }
  static void missileAt(JetpackGame& game, int x, int y) { missile(game, 0); game.missiles_[0].x = x * JetpackGame::Unit; game.missiles_[0].y = int16_t(y); }
  static void token(JetpackGame& game, unsigned kind) { game.token_.active = true; game.token_.x = 44 * JetpackGame::Unit; game.token_.y = int16_t(game.playerY()); game.token_.kind = uint8_t(kind); }
  static void spin(JetpackGame& game, unsigned value) {
    game.phase_ = JetpackGame::Phase::Spin; game.spinTokens_ = 1; game.spinTicks_ = 0; game.spinResult_ = false; game.armed_ = true;
    game.finalSpin_.reset();
    using Kind = JetpackRewards::Kind;
    const Kind target = value == 0 ? Kind::Revive : value == 1 ? Kind::Blast : value == 2 ? Kind::Headstart : value == 3 ? Kind::DoubleCoins : value == 4 ? Kind::Tokens : Kind::Coins;
    for (uint32_t seed = 1; ; ++seed) {
      uint32_t r = seed; r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      const uint32_t prize = r;
      r ^= r << 13; r ^= r >> 17; r ^= r << 5; const uint32_t stop = r;
      r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      JetpackRewards::FinalSpin reference;
      const auto outcome = reference.spin(prize, stop, r);
      if (outcome.hasPrize && outcome.award.kind == target) { game.random_ = seed; break; }
    }
  }
  static uint8_t reward(const JetpackGame& game) { return game.reward_; }
  static uint8_t pendingRewards(const JetpackGame& game) { return game.pendingRewards_; }
  static void spawn(JetpackGame& game) { game.spawnProgression(); }
  static JetpackProgression& progression(JetpackGame& game) { return game.progression_; }
  static const JetpackGame::Bonus& bonus(const JetpackGame& game) { return game.bonus_; }
  static bool payload(const JetpackGame& game) { return game.cashPayload_.active; }
  static void catchPayload(JetpackGame& game) { game.cashPayload_.x = 44 * JetpackGame::Unit; game.cashPayload_.y = int16_t(game.playerY()); }
  static int32_t boostExtra(const JetpackGame& game) { return game.boostExtraVelocity_; }
  static void tokens(JetpackGame& game, uint16_t count) { game.spinTokens_ = count; }
  static bool body(const JetpackGame& game, int x, int y, int dx, int dy) { return game.hitsBodySegment(x,y,dx,dy); }
  static void moveObjects(JetpackGame& game, int advance) { game.updateObjects(advance); }
  static void rotator(JetpackGame& game) { game.rotators_[0].active = true; game.rotators_[0].x = 44 * JetpackGame::Unit; game.rotators_[0].y = 70; game.rotators_[0].angle = 0; }
#ifdef M5FLAP_JETPACK_IMPORTED
  static void pattern(JetpackGame& game, unsigned index) {
    for (auto& group : game.importedGroups_) group = JetpackGame::ImportedGroup{};
    auto& group = game.importedGroups_[0]; const auto& pattern = JetpackImported::CoinPatterns[index];
    group.pattern = int16_t(index); group.step = uint8_t(std::max(2u, std::min(5u, 80u / pattern.height)));
    group.top = uint8_t(23 + (90 - pattern.height * group.step) / 2); group.x = 40 * JetpackGame::Unit;
  }
  static void collectPattern(JetpackGame& game, unsigned index) {
    auto& group = game.importedGroups_[0]; const auto& pattern = JetpackImported::CoinPatterns[index];
    for (unsigned i = 0; i < pattern.count; ++i) {
      const uint16_t point = JetpackImported::CoinPoints[pattern.offset + i];
      group.x = (JetpackGame::PlayerX - int(point & 255) * group.step) * JetpackGame::Unit;
      game.y_ = (group.top + int(point >> 8) * group.step) * JetpackGame::FlightUnit;
      game.collectImported();
    }
  }
  static void singleImportedCoin(JetpackGame& game, unsigned index) {
    pattern(game,index); auto& group = game.importedGroups_[0];
    const uint16_t point = JetpackImported::CoinPoints[JetpackImported::CoinPatterns[index].offset];
    for (uint8_t& bits : group.collected) bits = 255;
    group.collected[0] = 254;
    group.x = (JetpackGame::PlayerX + 35 - int(point & 255) * group.step) * JetpackGame::Unit;
    game.y_ = (group.top + int(point >> 8) * group.step) * JetpackGame::FlightUnit;
  }
  static void singleImportedAtBody(JetpackGame& game, unsigned index) {
    singleImportedCoin(game,index); auto& group = game.importedGroups_[0];
    const uint16_t point = JetpackImported::CoinPoints[JetpackImported::CoinPatterns[index].offset];
    group.x = (JetpackGame::PlayerX - int(point & 255) * group.step) * JetpackGame::Unit;
    group.top = uint8_t(105 - int(point >> 8) * group.step);
    player(game,28);
  }
  static void collectImported(JetpackGame& game) { game.collectImported(); }
  static bool firstImportedDetached(const JetpackGame& game) { return game.importedGroups_[0].collected[0] & 1; }
  static void freeAttractionSlot(JetpackGame& game) { game.attractedCoins_[0] = JetpackGame::AttractedCoin{}; }
#endif
  static unsigned coinCount(const JetpackGame& game) {
    unsigned count = 0; for (const auto& coin : game.coinField_) count += coin.active; for (const auto& group : game.importedGroups_) count += group.pattern >= 0; return count;
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
  assert(sizeof(game) < 4096 && game.phase() == Phase::Ready && game.score() == 0);
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
  for (int i = 0; i < 150; ++i) game.tick(hold());
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
  armor.tick(Input{});
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

static void expandedRidesAndFreshControls() {
  for (uint8_t ride = 2; ride <= 8; ++ride) {
    Game game; game.setNextLoadout(ride, 0, false); start(game); Access::clear(game);
    assert(uint8_t(game.vehicle()) == ride);
    for (unsigned i = 0; i < 8; ++i) game.tick(hold());
    if (ride == 2) assert(!game.gravityUp());
    if (ride == 3 || ride == 4) assert(game.playerY() == Game::Floor);
    if (ride == 6) assert(game.playerY() == Game::Floor);
    game.tick(Input{}); Access::player(game, ride == 3 || ride == 4 ? Game::Floor : 70);
    game.tick(press());
    if (ride == 2) assert(game.gravityUp());
    if (ride == 3 || ride == 4 || ride == 5 || ride == 8) assert(Access::velocity(game) < 0);
    if (ride == 6) {
      assert(game.playerY() == 70 && Access::teleportStage(game) == 1);
      for (unsigned i = 0; i < 11; ++i) { game.tick(Input{}); assert(game.playerY() == 70); }
      game.tick(Input{});
      assert(game.playerY() == game.teleportTargetY() && Access::teleportStage(game) == 2);
    }
    if (ride == 7) assert(Access::velocity(game) > 0);
    game.setNextLoadout(0, 2, true);
    assert(uint8_t(game.vehicle()) == ride);
  }
  Game bird; bird.setNextLoadout(5, 0, false); start(bird); Access::clear(bird); bird.tick(Input{});
  for (unsigned i = 0; i < 15; ++i) {
    Access::player(bird, 70); bird.tick(press()); assert(Access::velocity(bird) < 0);
    for (unsigned release = 0; release < 10; ++release) bird.tick(Input{});
  }
  Game pickup; start(pickup); Access::clear(pickup); Access::zap(pickup, 44, 40, 0, 50);
  Access::missileAt(pickup, 70, 70); Access::laser(pickup, 30, 70);
  Access::equip(pickup, Vehicle::Mech);
  assert(!pickup.zap(0).active && !pickup.missile(0).active && !pickup.laser().active);
  Access::player(pickup, 70);
  assert(Access::body(pickup, 15, 61, 60, 0));
  Access::equip(pickup, Vehicle::None); Access::player(pickup, 70);
  assert(!Access::body(pickup, 15, 61, 60, 0));
}

static void verifiedVehicleIntegration() {
  Game sam; sam.setNextLoadout(uint8_t(Vehicle::SAM),0,false); start(sam); Access::clear(sam);
  sam.tick(Input{}); Access::player(sam,70);
  double y = 70, velocity = 0;
  for (unsigned tick = 0; tick < 30; ++tick) {
    sam.tick(hold());
    const double native = double(Access::nativeRideSpeed(sam)) / Game::FlightUnit;
    const double factor = 1 + (native - 175) / 350;
    velocity = (velocity - .21 * (92.0 / 240) * factor) * .98;
    y += velocity;
    if (y < 27) { y = 27; velocity = 0; }
    assert(std::abs(Access::preciseY(sam) - y) < .01);
  }
  for (unsigned tick = 0; tick < 120; ++tick) sam.tick(hold());
  assert(sam.playerY() == 27);
  sam.tick(Input{});
  assert(Access::velocity(sam) > 0);

  Game tele; tele.setNextLoadout(uint8_t(Vehicle::Teleporter),0,false); start(tele); Access::clear(tele);
  tele.tick(Input{}); Access::player(tele,70); Access::immunity(tele,0);
  const int target = tele.teleportTargetY();
  tele.tick(press());
  for (unsigned tick = 0; tick < 11; ++tick) tele.tick(Input{});
  assert(tele.playerY() == 70 && Access::immunity(tele) == 0);
  tele.tick(Input{});
  assert(tele.playerY() == target && Access::immunity(tele) == 0);

  Game hog; hog.setNextLoadout(uint8_t(Vehicle::Hog),0,false); start(hog); Access::clear(hog);
  for (unsigned tick = 0; tick < 600; ++tick) hog.tick(Input{});
  assert(std::abs(double(Access::nativeRideSpeed(hog)) / Game::FlightUnit - 635.437691859) < .004);
  assert(hog.speed() > 4 * Game::Unit);
  for (unsigned tick = 0; tick < 5000; ++tick) hog.tick(Input{});
  assert(hog.speed() <= 1636);
}

static void allPowerupsAndNpcInteractions() {
  using Power = Game::Power;
  Game game; start(game); Access::clear(game); Access::player(game, 70);
  Access::power(game, Power::Shield); Access::hit(game);
  assert(game.phase() == Phase::Running && game.powerTicks(Power::Shield) == 0);
  Access::immunity(game, 0); Access::hit(game); assert(game.phase() == Phase::Crashing);
  for (Power power : {Power::Boost, Power::CashDash}) {
    Game protectedGame; start(protectedGame); Access::clear(protectedGame);
    Access::zap(protectedGame, 45, 50, 0, 50); Access::power(protectedGame, power);
    assert(!protectedGame.zap(0).active && protectedGame.powerTicks(power));
    if (power == Power::Boost) {
      const int initial = protectedGame.speed();
      protectedGame.tick(Input{}); assert(protectedGame.speed() > initial);
      assert(protectedGame.speed() < 500);
    } else {
      protectedGame.tick(Input{}); assert(Access::coinCount(protectedGame) == 1);
    }
    const unsigned duration = power == Power::Boost ? 100 : 60;
    Access::meters(protectedGame, duration); protectedGame.tick(Input{});
    assert(protectedGame.powerTicks(power) == 0);
  }
  Game coins; start(coins); Access::clear(coins); Access::player(coins, 70);
  Access::power(coins, Power::DoubleCoins); Access::coin(coins, 44, 70); coins.tick(Input{});
  assert(coins.coins() == 2);
  Access::power(coins, Power::Magnet); Access::coin(coins, 90, 70);
  coins.tick(Input{}); assert(coins.coins() == 2 && coins.attractedCoinCount() == 1);
  const int before = Access::attractedX(coins);
  coins.tick(Input{}); assert(coins.coins() == 2 && Access::attractedX(coins) < before);
  for (unsigned i = 0; i < 80; ++i) { Access::player(coins, 70); coins.tick(Input{}); }
  assert(coins.coins() == 4);
  Game slow, fast; start(slow); start(fast); Access::clear(slow); Access::clear(fast);
  Access::power(slow, Power::RocketTime); Access::rocketHold(slow); Access::missileAt(slow, 220, 40); Access::missileAt(fast, 220, 40);
  for (unsigned i = 0; i < 10; ++i) { slow.tick(Input{}); fast.tick(Input{}); }
  assert(slow.missile(0).x > fast.missile(0).x + 20 * Q);
  Game npc; start(npc); Access::clear(npc); Access::npc(npc); npc.tick(Input{});
  assert(npc.phase() == Phase::Running && npc.scientistInteractions() == 1 && npc.coins() == 1);
  Game stomp; stomp.setNextLoadout(3, 0, false); start(stomp); Access::clear(stomp); Access::npc(stomp); stomp.tick(Input{});
  assert(stomp.scientist(0).state == 2 && stomp.coins() == 2 && stomp.phase() == Phase::Running);
}

static void samVisibleBlocksAndLaserSeparation() {
  Game sam; sam.setNextLoadout(8, 0, false); start(sam); Access::clear(sam); Access::player(sam, 70); Access::immunity(sam, 0);
  Access::missileAt(sam, Game::PlayerX + 34, 79); Access::moveObjects(sam, 0);
  assert(!sam.missile(0).active && sam.vehicleHits() == 3 && sam.coins() == 3);
  Access::missileAt(sam, Game::PlayerX + 34, 80); Access::moveObjects(sam, 0);
  assert(sam.missile(0).active && sam.coins() == 3);
  for (unsigned i = 0; i < 5; ++i) Access::moveObjects(sam, 0);
  assert(!sam.missile(0).active && sam.vehicleHits() == 2);
  Game laser; start(laser); Access::clear(laser); Access::player(laser, 70);
  Access::laser(laser, 140, 70);
  for (unsigned i = 0; i < 99; ++i) { Access::player(laser, 70); laser.tick(Input{}); assert(laser.phase() == Phase::Running); }
  Access::player(laser, 70); laser.tick(Input{}); assert(laser.phase() == Phase::Crashing);
  Game rotating; start(rotating); Access::clear(rotating); Access::player(rotating, 70); Access::rotator(rotating);
  rotating.tick(Input{}); assert(rotating.phase() == Phase::Crashing);
  Game letters; start(letters); Access::clear(letters); Access::player(letters, 70);
  for (unsigned kind = 1; kind <= 3; ++kind) { Access::token(letters, kind); letters.tick(Input{}); }
  assert(letters.vehicle() == Vehicle::SAM && letters.samLetters() == 0 && letters.vehicleHits() == 3);
}

static void finalSpinAndCrashRewards() {
  for (unsigned reward : {0u,1u,2u,3u,7u}) {
    Game game; start(game); Access::clear(game); Access::spin(game, reward); game.tick(press());
    assert(game.phase() == Phase::Spin);
    const unsigned expected = reward == 0 ? 0 : reward == 1 ? 2 : reward == 2 ? 3 : reward == 3 ? 4 : 1;
    assert(Access::reward(game) == expected);
    for (unsigned i = 0; i < flappy::JetpackSpin::Duration + flappy::JetpackSpin::WinHold - 1; ++i) game.tick(Input{});
    assert(game.phase() == Phase::Spin);
    game.tick(Input{});
    if (reward == 0) assert(game.phase() == Phase::Running);
    if (reward == 1) {
      assert(game.phase() == Phase::Crashing); const uint32_t before = game.distance();
      for(unsigned i=0;i<50;++i)game.tick(Input{}); assert(game.distance()>before);
      for(unsigned i=0;i<400 && !game.gameOver();++i)game.tick(Input{});
      assert(game.gameOver() && game.playerY() == Game::Floor && Access::velocity(game) == 0);
    }
    if (reward == 2 || reward == 3) {
      assert(game.gameOver() && Access::pendingRewards(game)); game.tick(Input{}); game.tick(press());
      assert(game.phase() == Phase::Running);
      if (reward == 2) assert(game.powerTicks(Game::Power::Boost));
      else assert(game.powerTicks(Game::Power::DoubleCoins));
    }
    if (reward == 7) assert(game.gameOver() && game.coins() > 0);
  }
  Game crash; start(crash); Access::clear(crash); Access::player(crash, 60); Access::hit(crash);
  const uint32_t before = crash.distance();
  for (unsigned i = 0; i < 400 && !crash.gameOver(); ++i) crash.tick(Input{});
  assert(crash.gameOver() && crash.crashBounces() >= 1 && crash.distance() > before);
}

static void bonusChoicesAndProfileIsolation() {
  flappy::JetpackProfile profile;
  auto saved = profile.state(); saved.coins = 1000; saved.sam = 1; profile.restore(saved);
  Game game; game.attachProfile(&profile); start(game); Access::clear(game); Access::player(game, 70);
  assert(game.rankedRun() && game.samLetters() == 1);
  Access::power(game, Game::Power::DoubleCoins); Access::coin(game, 44, 70); game.tick(Input{});
  assert(game.coins() == 2 && game.runStats().coinsCollected == 2);
  Access::spin(game, 7); Access::tokens(game, 3);
  for (unsigned i = 0; i < 500; ++i) game.tick(Input{});
  assert(game.phase() == Phase::Spin && game.spinTokens() == 3 && game.runStats().prizesWon == 0);
  assert(game.cashInSpinTokens() == 3 && game.gameOver());
  assert(profile.coins() >= 1302 && profile.state().runs == 1 && profile.samLetters() == 1);
  const uint32_t bank = profile.coins(); game.settleRun(); game.settleRun();
  assert(profile.coins() == bank && profile.state().runs == 1);

  for (unsigned reward : {2u,3u}) {
    Game practice; practice.attachProfile(&profile); practice.setProfileEnabled(false); start(practice);
    Access::clear(practice); Access::spin(practice, reward); practice.tick(press());
    assert(!practice.rankedRun() && practice.lastSpinWon());
    assert(Access::pendingRewards(practice) == 0);
    for (unsigned i = 0; i < flappy::JetpackSpin::Duration + flappy::JetpackSpin::WinHold; ++i) practice.tick(Input{});
    assert(practice.gameOver() && profile.coins() == bank);
    practice.setProfileEnabled(true); practice.tick(Input{}); practice.tick(press());
    assert(practice.rankedRun() && !practice.powerTicks(Game::Power::Boost) && !practice.powerTicks(Game::Power::DoubleCoins));
    assert(profile.state().headstarts == 0 && profile.state().doubleRuns == 0);
  }

  Game tokens; start(tokens); Access::clear(tokens); Access::spin(tokens, 4); tokens.tick(press());
  assert(tokens.lastSpinWon() && tokens.lastAward().kind == flappy::JetpackRewards::Kind::Tokens && tokens.spinTokens() == 3);
  assert(tokens.runStats().tokensCollected == 0 && tokens.runStats().prizesWon == 1);
  for (unsigned i = 0; i < 200; ++i) tokens.tick(Input{});
  assert(tokens.phase() == Phase::Spin && tokens.spinTokens() == 3);
}

static void nativePowerTimingAndPayload() {
  Game boost; start(boost); Access::clear(boost); Access::power(boost, Game::Power::Boost);
  for (unsigned i = 1; i <= 60; ++i) {
    boost.tick(Input{});
    assert(Access::boostExtra(boost) == int(std::min(2000u, i * 35)) * Game::FlightUnit);
  }
  Access::meters(boost, 70); boost.tick(Input{});
  assert(!boost.powerTicks(Game::Power::Boost) && Access::boostExtra(boost) == 1800 * Game::FlightUnit);
  for (unsigned i = 0; i < 100; ++i) boost.tick(Input{});
  assert(Access::boostExtra(boost) == 0);

  Game cash; start(cash); Access::clear(cash); Access::power(cash, Game::Power::CashDash);
  for (uint32_t meter = 0; meter < 60; ++meter) { Access::meters(cash, meter); cash.tick(Input{}); }
  assert(!Access::payload(cash) && cash.coins() == 0 && Access::coinCount(cash) >= 50);
  Access::meters(cash, 60); cash.tick(Input{});
  assert(Access::payload(cash) && cash.coins() == 0 && !cash.powerTicks(Game::Power::CashDash));
  Access::player(cash, 70); Access::catchPayload(cash); cash.tick(Input{});
  assert(!Access::payload(cash) && cash.coins() == 0 && cash.cashShowerPending() == 300);
  for (unsigned i = 0; i < 400; ++i) { Access::player(cash, 70); cash.tick(Input{}); }
  assert(cash.cashShowerPending() == 0 && cash.attractedCoinCount() == 0 && cash.coins() < 1800);
  assert(cash.runStats().coinsCollected == cash.coins());

  flappy::JetpackProfile profile; auto saved = profile.state();
  saved.levels[flappy::JetpackProfile::Shield] = 4;
  saved.levels[flappy::JetpackProfile::Boost] = 4;
  profile.restore(saved);
  Game upgraded; upgraded.attachProfile(&profile); start(upgraded); Access::clear(upgraded);
  Access::power(upgraded, Game::Power::Shield); assert(upgraded.shieldHits() == 5);
  for (unsigned i = 0; i < 5; ++i) {
    Access::hit(upgraded); assert(upgraded.phase() == Phase::Running && upgraded.shieldHits() == 4 - i);
  }
  assert(!upgraded.powerTicks(Game::Power::Shield));
  Access::power(upgraded, Game::Power::Boost);
  for (unsigned i = 0; i < 100; ++i) upgraded.tick(Input{});
  assert(Access::boostExtra(upgraded) == 3500 * Game::FlightUnit);
}

static void boundedMagnetAndGemAnimation() {
  Game full; start(full); Access::clear(full); Access::player(full,70); Access::power(full,Game::Power::Magnet);
  for (unsigned i = 0; i < Game::AttractedCoinCount; ++i) assert(Access::attract(full,85,70,1));
  assert(!Access::attract(full,85,70,1));
  Access::gem(full,90,70); full.tick(Input{});
  assert(full.coins() == 0 && Access::ordinaryCoin(full) && full.attractedCoinCount() == Game::AttractedCoinCount);
  for (unsigned i = 0; i < 150; ++i) { Access::player(full,70); full.tick(Input{}); }
  assert(full.coins() == Game::AttractedCoinCount + 5 && full.attractedCoinCount() == 0);
  const uint32_t collected = full.coins();
  for (unsigned i = 0; i < 100; ++i) { Access::player(full,70); full.tick(Input{}); }
  assert(full.coins() == collected);

  Game shower; start(shower); Access::clear(shower); Access::player(shower,70);
  for (unsigned i = 0; i < Game::AttractedCoinCount; ++i) assert(Access::attract(shower,85,70,1));
  Access::power(shower,Game::Power::Magnet); Access::shower(shower,100,100); shower.tick(Input{});
  assert(shower.cashShowerPending() == 100 && shower.coins() == 0);
  for (unsigned i = 0; i < 300; ++i) { Access::player(shower,70); shower.tick(Input{}); }
  assert(shower.cashShowerPending() == 0 && shower.attractedCoinCount() == 0 && shower.coins() > Game::AttractedCoinCount && shower.coins() <= Game::AttractedCoinCount + 500);
  Game doubleGem; start(doubleGem); Access::clear(doubleGem); Access::player(doubleGem,70);
  Access::power(doubleGem,Game::Power::DoubleCoins); Access::power(doubleGem,Game::Power::Magnet); Access::gem(doubleGem,85,70);
  doubleGem.tick(Input{}); assert(doubleGem.coins() == 0 && doubleGem.attractedCoinCount() == 1);
  for (unsigned i = 0; i < 100; ++i) { Access::player(doubleGem,70); doubleGem.tick(Input{}); }
  assert(doubleGem.coins() == 10 && doubleGem.attractedCoinCount() == 0);

  Game sam; sam.setNextLoadout(8,0,false); start(sam); Access::clear(sam); Access::player(sam,28);
  Access::coin(sam,44,105); sam.tick(Input{}); assert(sam.coins() == 1);
  Access::coin(sam,44,28); sam.tick(Input{}); assert(sam.coins() == 1);

#ifdef M5FLAP_JETPACK_IMPORTED
  unsigned pattern = 0;
  while (pattern < flappy::JetpackImported::PatternCount && !flappy::JetpackImported::CoinPatterns[pattern].count) ++pattern;
  assert(pattern < flappy::JetpackImported::PatternCount);
  Game imported; start(imported); Access::clear(imported); Access::power(imported,Game::Power::Magnet);
  Access::singleImportedCoin(imported,pattern);
  const int playerY = imported.playerY();
  for (unsigned i = 0; i < Game::AttractedCoinCount; ++i) assert(Access::attract(imported,85,playerY,1));
  Access::collectImported(imported);
  assert(!Access::firstImportedDetached(imported) && imported.coins() == 0);
  Access::freeAttractionSlot(imported); Access::collectImported(imported);
  assert(Access::firstImportedDetached(imported) && imported.coins() == 0 && imported.attractedCoinCount() == Game::AttractedCoinCount);
  Access::collectImported(imported); assert(imported.attractedCoinCount() == Game::AttractedCoinCount);
  for (unsigned i = 0; i < 150; ++i) { Access::player(imported,playerY); imported.tick(Input{}); }
  assert(imported.coins() == Game::AttractedCoinCount && imported.attractedCoinCount() == 0);
  Game importedSam; importedSam.setNextLoadout(8,0,false); start(importedSam); Access::clear(importedSam);
  Access::singleImportedAtBody(importedSam,pattern); Access::collectImported(importedSam);
  assert(importedSam.coins() == 1);
#endif
}

static void poweredCoinsRemainPlayable() {
  Game boost; start(boost); Access::clear(boost); Access::meters(boost,100); Access::power(boost,Game::Power::Boost);
  auto& policy = Access::progression(boost); policy.setEnabled(true);
  flappy::JetpackProgressionTestAccess::due(policy,100000,100000); Access::spawn(boost);
  assert(Access::coinCount(boost) > 0);
  Game cash; start(cash); Access::clear(cash); Access::meters(cash,100); Access::power(cash,Game::Power::CashDash);
  auto& cashPolicy = Access::progression(cash); cashPolicy.setEnabled(true);
  flappy::JetpackProgressionTestAccess::due(cashPolicy,100000,100000); Access::spawn(cash);
  assert(Access::coinCount(cash) == 0);
  Game rocket; start(rocket); Access::clear(rocket); Access::player(rocket,70); Access::coin(rocket,44,70);
  Access::power(rocket,Game::Power::RocketTime);
  assert(Access::ordinaryCoin(rocket)); rocket.tick(Input{}); assert(rocket.coins() == 1);
}

static void realSpawnAnchorEncounterTiming() {
  using Policy = flappy::JetpackProgression;
  for (bool vehicle : {false,true}) {
    Game game; start(game); Access::clear(game); Access::meters(game, 101);
    auto& policy = Access::progression(game); policy.setEnabled(true);
    flappy::JetpackProgressionTestAccess::due(policy, vehicle ? 100000 : 200, vehicle ? 200 : 100000);
    Access::spawn(game); policy.setEnabled(false);
    Policy::Event target; bool found = false;
    for (uint32_t serial = 1; serial <= game.eventSerial(); ++serial) {
      Policy::Event event; if (!game.event(serial, event)) continue;
      if (event.kind == (vehicle ? Policy::Kind::Vehicle : Policy::Kind::Power)) { target = event; found = true; }
    }
    assert(found && target.dueMeters == 200);
    assert(target.encounterDistance == target.nativeDistance + 577);
    while ((vehicle ? game.pickup().x : Access::bonus(game).x) > Game::PlayerX * Q) game.tick(Input{});
    const uint32_t actual = game.nativeDistance();
    assert(actual >= target.encounterDistance - 1 && actual <= target.encounterDistance + 6);
  }
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
  for (unsigned i = 0; i < 60000; ++i) {
    Input in; in.action = (i / 35) % 2; in.pressed = i % 70 == 35;
    Access::immunity(a, 65000); Access::immunity(b, 65000);
    a.tick(in); b.tick(in);
    assert(a.phase() == Phase::Running && b.phase() == Phase::Running);
    assert(a.score() == b.score() && a.playerY() == b.playerY());
    assert(a.speed() >= 286 && a.speed() <= 4300);
    assert(a.playerY() >= Game::Ceiling && a.playerY() <= Game::Floor);
    for (unsigned j = 0; j < Game::ZapCount; ++j) if (a.zap(j).active) {
      const auto& zap = a.zap(j); ++zaps;
#ifdef M5FLAP_JETPACK_OBSTACLES
      assert(zap.y >= 18 && zap.y <= 118 && zap.y + zap.dy >= 18 && zap.y + zap.dy <= 118);
      assert(int32_t(zap.dx) * zap.dx + int32_t(zap.dy) * zap.dy <= 100 * 100);
#else
      assert(zap.y >= Game::Ceiling && zap.y <= Game::Floor && zap.y + zap.dy >= Game::Ceiling && zap.y + zap.dy <= Game::Floor);
      assert(zap.dx <= 32 && zap.dy >= -39 && zap.dy <= 47);
#endif
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
  assert(a.distance() > 15000 && a.speed() >= 785);
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
    assert(game.speed() >= 243 && game.speed() <= 4300);
    if (i % 211 == 0) game.draw(pixels.data());
  }
  assert(deaths > 50 && restarts > 50);
}

static void expandedPoolAndRenderStress() {
  struct Guarded { uint32_t before = 0x18273645; std::array<uint16_t, Game::Width * Game::Height> pixels{}; uint32_t after = 0x56677889; } buffer;
  for (uint8_t ride = 0; ride <= 8; ++ride) {
    Game game; game.setNextLoadout(ride, uint8_t(ride % 3), false); start(game);
    bool held = false;
    for (unsigned i = 0; i < 6000; ++i) {
      Access::immunity(game, 65000);
      const bool next = (i / (13 + ride)) % 2;
      Input in; in.action = next; in.pressed = next && !held; held = next;
      game.tick(in);
      assert(game.phase() == Phase::Running && game.playerY() >= Game::Ceiling && game.playerY() <= Game::Floor);
      if (i % 91 == 0) {
        game.draw(buffer.pixels.data(), 9000);
        assert(buffer.before == 0x18273645 && buffer.after == 0x56677889);
      }
    }
  }
}

static void importedGroupsKeepEveryCell() {
#ifdef M5FLAP_JETPACK_IMPORTED
  struct Guarded { uint32_t before = 0x143278ab; std::array<uint16_t, Game::Width * Game::Height> pixels{}; uint32_t after = 0x6789abcd; } buffer;
  uint32_t cells = 0;
  for (unsigned index = 0; index < flappy::JetpackImported::PatternCount; ++index) {
    const auto& pattern = flappy::JetpackImported::CoinPatterns[index];
    assert(pattern.count <= 512 && pattern.height <= 40);
    Game game; start(game); Access::clear(game); Access::pattern(game, index);
    game.draw(buffer.pixels.data());
    assert(buffer.before == 0x143278ab && buffer.after == 0x6789abcd);
    Access::collectPattern(game, index);
    assert(game.coins() == pattern.count);
    cells += pattern.count;
  }
  assert(cells == flappy::JetpackImported::CoinPointCount);
  std::cout << "Imported cells retained and collected: " << cells << "\n";
#endif
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
  independentGravityFlight(); missilesTrackThenLock(); expandedRidesAndFreshControls(); verifiedVehicleIntegration(); allPowerupsAndNpcInteractions(); samVisibleBlocksAndLaserSeparation(); finalSpinAndCrashRewards(); bonusChoicesAndProfileIsolation(); nativePowerTimingAndPayload(); boundedMagnetAndGemAnimation(); poweredCoinsRemainPlayable(); realSpawnAnchorEncounterTiming(); deterministicProgressionAndRenderBounds(); randomizedRuns(); expandedPoolAndRenderStress(); importedGroupsKeepEveryCell();
  if (argc > 1) dumpScene(argv[1], argc > 2 ? argv[2][0] - '0' : 0);
  std::cout << "Jetpack tests passed\n";
}
