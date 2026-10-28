#include "../firmware/flappy/arkanoid_game.h"

#include <assert.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>

namespace flappy {
struct ArkanoidTestAccess {
  static void field(ArkanoidGame& game) {
    game.clearObjects();
    game.clearPowers();
    for (auto& brick : game.bricks_) brick = ArkanoidGame::Brick{};
    game.remaining_ = 0;
    game.stageDelay_ = 0;
    game.deathDelay_ = 0;
    game.over_ = false;
    game.cue_ = ArcadeCue::None;
    game.hitCount_ = 0;
  }
  static void brick(ArkanoidGame& game, unsigned row, unsigned col, uint8_t hits = 1) {
    auto& brick = game.bricks_[row * ArkanoidGame::Columns + col];
    if (brick.hits && brick.hits != 255) --game.remaining_;
    brick.hits = hits;
    brick.color = uint8_t(row % 6);
    if (hits && hits != 255) ++game.remaining_;
  }
  static ArkanoidGame::Ball& ball(ArkanoidGame& game, unsigned index = 0) { return game.balls_[index]; }
  static void stationary(ArkanoidGame& game) {
    auto& ball = game.balls_[0];
    ball = ArkanoidGame::Ball{};
    ball.active = true;
    ball.x = 67 * ArkanoidGame::Unit;
    ball.y = 180 * ArkanoidGame::Unit;
  }
  static void capsule(ArkanoidGame& game, ArkanoidGame::Power power) {
    auto& capsule = game.capsules_[0];
    capsule.power = power;
    capsule.x = game.paddleX_;
    capsule.y = (ArkanoidGame::PaddleY - 2) * ArkanoidGame::Unit;
    capsule.active = true;
  }
  static void falling(ArkanoidGame& game, unsigned index, ArkanoidGame::Power power, int x, int y) {
    auto& capsule = game.capsules_[index];
    capsule.power = power; capsule.x = x * ArkanoidGame::Unit;
    capsule.y = y * ArkanoidGame::Unit; capsule.active = true;
  }
  static void power(ArkanoidGame& game, ArkanoidGame::Power power, uint16_t ticks) {
    game.powerTicks_[unsigned(power)] = ticks;
  }
  static unsigned laserCount(const ArkanoidGame& game) {
    unsigned count = 0;
    for (const auto& laser : game.lasers_) if (laser.active) ++count;
    return count;
  }
  static void lives(ArkanoidGame& game, uint8_t count) { game.lives_ = count; }
  static void score(ArkanoidGame& game, uint32_t score) { game.score_ = score; }
  static void stage(ArkanoidGame& game, uint16_t stage) {
    game.stage_ = stage;
    game.buildStage();
    game.serve();
  }
  static uint16_t stageDelay(const ArkanoidGame& game) { return game.stageDelay_; }
  static uint32_t rng(const ArkanoidGame& game) { return game.random_; }
};
}

using Game = flappy::ArkanoidGame;
using Access = flappy::ArkanoidTestAccess;
using Power = Game::Power;
using Input = flappy::ArcadeInput;
using Cue = flappy::ArcadeCue;
constexpr int Q = Game::Unit;

static Input press() { Input input; input.action = input.pressed = true; return input; }
static void fixture(Game& game) {
  game.reset(123);
  Access::field(game);
  Access::brick(game, 0, 0);
  Access::stationary(game);
}
static double magnitude(const Game::Ball& ball) { return std::hypot(double(ball.vx), double(ball.vy)); }

static void servingMovementAndRestart() {
  Game game;
  assert(game.stage() == 1 && game.lives() == 3 && game.score() == 0);
  assert(game.activeBalls() == 1 && game.ball(0).attached && !game.gameOver());
  Input held; held.action = true;
  for (int i = 0; i < 90; ++i) game.tick(held);
  assert(game.ball(0).attached);
  Input right; right.move = 1024;
  for (int i = 0; i < 200; ++i) game.tick(right);
  assert(game.paddleX() == Game::Right - game.paddleWidth() / 2);
  assert(game.ball(0).x == game.paddleX() * Q);
  Input left; left.move = -1024;
  for (int i = 0; i < 200; ++i) game.tick(left);
  assert(game.paddleX() == Game::Left + game.paddleWidth() / 2);
  game.tick(press());
  assert(!game.ball(0).attached && game.ball(0).vy < 0);
  assert(game.takeCue() == Cue::Fire && game.takeCue() == Cue::None);

  Access::field(game);
  Access::brick(game, 0, 0);
  Access::lives(game, 1);
  game.tick(Input{});
  assert(game.gameOver() && game.lives() == 0 && game.takeCue() == Cue::Death);
  game.tick(held);
  assert(game.gameOver());
  game.tick(press());
  assert(!game.gameOver() && game.lives() == 3 && game.ball(0).attached);
  game.reset(3, 0); assert(game.lives() == 4);
  game.reset(3, 2); assert(game.lives() == 3);
}

static Game::Ball reboundAt(int offset, int incomingSpeed = 4 * Q) {
  Game game;
  fixture(game);
  auto& ball = Access::ball(game);
  ball.x = (game.paddleX() + offset) * Q;
  ball.y = (Game::PaddleY - 3) * Q;
  ball.vx = 0;
  ball.vy = incomingSpeed;
  game.tick(Input{});
  assert(ball.vy < 0 && !ball.attached);
  return ball;
}

static void paddleAnglesAndFastCollision() {
  const auto center = reboundAt(0), left = reboundAt(-10), right = reboundAt(10), mid = reboundAt(5);
  assert(left.vx < 0 && right.vx > 0 && mid.vx > 0);
  assert(std::abs(center.vx) < mid.vx && mid.vx < right.vx);
  assert(std::abs(left.vx + right.vx) <= 2);
  assert(std::abs(center.vy) > std::abs(mid.vy) && std::abs(mid.vy) > std::abs(right.vy));
  assert(std::abs(magnitude(left) - magnitude(center)) < 3);
  const auto fast = reboundAt(3, 45 * Q);
  assert(fast.y < (Game::PaddleY - 2) * Q);

  Game game;
  fixture(game);
  auto& ball = Access::ball(game);
  ball.x = 8 * Q; ball.y = 155 * Q; ball.vx = -50 * Q; ball.vy = 0;
  game.tick(Input{});
  assert(ball.x >= (Game::Left + 2) * Q && ball.vx > 0);
  ball.x = 70 * Q; ball.y = 31 * Q; ball.vx = 0; ball.vy = -40 * Q;
  game.tick(Input{});
  assert(ball.y >= (Game::Ceiling + 2) * Q && ball.vy > 0);
}

static void bricksPreventTunnelingAndKeepGold() {
  Game game;
  fixture(game);
  Access::brick(game, 3, 4);
  const unsigned target = 3 * Game::Columns + 4;
  auto& ball = Access::ball(game);
  ball.x = (Game::BrickLeft + 4 * Game::BrickPitchX + 6) * Q;
  ball.y = 108 * Q; ball.vx = 0; ball.vy = -48 * Q;
  game.tick(Input{});
  assert(game.brick(target).hits == 0 && game.remainingBricks() == 1);
  assert(ball.vy > 0 && ball.y > 81 * Q);
  assert(game.score() == 90 && game.takeCue() == Cue::Hit);

  Access::brick(game, 3, 4, 3);
  const uint32_t score = game.score();
  for (int hit = 0; hit < 3; ++hit) {
    ball.x = (Game::BrickLeft + 4 * Game::BrickPitchX + 6) * Q;
    ball.y = 84 * Q; ball.vx = 0; ball.vy = -8 * Q;
    game.tick(Input{});
    assert(game.brick(target).hits == 2 - hit);
  }
  assert(game.score() == score + 110);
  Access::brick(game, 3, 4, 255);
  const uint32_t beforeGold = game.score();
  for (int hit = 0; hit < 8; ++hit) {
    ball.x = (Game::BrickLeft + 4 * Game::BrickPitchX + 6) * Q;
    ball.y = 84 * Q; ball.vx = 0; ball.vy = -8 * Q;
    game.tick(Input{});
    assert(game.brick(target).hits == 255 && ball.vy > 0);
  }
  assert(game.score() == beforeGold && game.remainingBricks() == 1);

  Access::brick(game, 6, 5);
  const unsigned horizontal = 6 * Game::Columns + 5;
  ball.x = 53 * Q; ball.y = (Game::BrickTop + 6 * Game::BrickPitchY + 3) * Q;
  ball.vx = 45 * Q; ball.vy = 0;
  game.tick(Input{});
  assert(game.brick(horizontal).hits == 0 && ball.vx < 0);
}

static void capsulePowersAndExpiry() {
  Game game;
  fixture(game);
  Access::capsule(game, Power::Expand);
  game.tick(Input{});
  assert(game.paddleWidth() == 38 && game.powerTicks(Power::Expand) == 1200);
  assert(game.score() == 250 && game.takeCue() == Cue::Power);
  for (int i = 0; i < 1199; ++i) game.tick(Input{});
  assert(game.paddleWidth() == 38 && game.powerTicks(Power::Expand) == 1);
  game.tick(Input{});
  assert(game.paddleWidth() == 24 && game.powerTicks(Power::Expand) == 0);

  auto& ball = Access::ball(game);
  ball.vx = 200; ball.vy = -584;
  Access::capsule(game, Power::Slow);
  game.tick(Input{});
  const double slow = magnitude(ball);
  assert(slow > 450 && slow < 470);
  Access::power(game, Power::Slow, 1);
  game.tick(Input{});
  assert(magnitude(ball) > slow * 1.3 && magnitude(ball) < 625);
  Access::stationary(game);
  Access::capsule(game, Power::Life);
  game.tick(Input{});
  assert(game.lives() == 4);
  Access::lives(game, 9);
  Access::capsule(game, Power::Life);
  game.tick(Input{});
  assert(game.lives() == 9);

  Access::capsule(game, Power::Catch);
  game.tick(Input{});
  ball.x = game.paddleX() * Q; ball.y = (Game::PaddleY - 3) * Q;
  ball.vx = 50; ball.vy = 4 * Q;
  game.tick(Input{});
  assert(ball.attached && ball.held > 0);
  Input move; move.move = 1024;
  game.tick(move);
  assert(std::abs(ball.x - game.paddleX() * Q) < Q);
  game.tick(press());
  assert(!ball.attached && ball.vy < 0);
  ball.x = game.paddleX() * Q; ball.y = (Game::PaddleY - 3) * Q;
  ball.vx = 50; ball.vy = 4 * Q;
  game.tick(Input{});
  assert(ball.attached);
  for (int i = 0; i < 240; ++i) game.tick(Input{});
  assert(!ball.attached && ball.vy < 0);
}

static void lasersMultiballAndLifeLoss() {
  Game game;
  fixture(game);
  Access::capsule(game, Power::Laser);
  game.tick(Input{});
  Input held; held.action = true;
  game.tick(held);
  assert(Access::laserCount(game) == 2 && game.takeCue() == Cue::Power);
  for (int i = 0; i < 11; ++i) game.tick(held);
  assert(Access::laserCount(game) == 2);
  game.tick(held);
  assert(Access::laserCount(game) == 4);
  for (int i = 0; i < 90; ++i) game.tick(held);
  assert(Access::laserCount(game) <= Game::MaxLasers);
  Access::power(game, Power::Laser, 1);
  for (int i = 0; i < 60; ++i) game.tick(held);
  assert(Access::laserCount(game) == 0 && game.powerTicks(Power::Laser) == 0);

  fixture(game);
  Access::brick(game, 5, 3, 2);
  Access::power(game, Power::Laser, 1200);
  game.tick(held);
  for (int i = 0; i < 30; ++i) game.tick(held);
  assert(game.brick(5 * Game::Columns + 3).hits == 0);
  assert(game.score() >= 80);

  fixture(game);
  Access::capsule(game, Power::Multi);
  game.tick(Input{});
  assert(game.activeBalls() == 3);
  assert(game.ball(0).vx != game.ball(1).vx && game.ball(1).vx != game.ball(2).vx);
  Access::ball(game, 0).active = false;
  game.tick(Input{});
  assert(game.activeBalls() == 2 && game.lives() == 3);
  Access::ball(game, 1).active = false;
  Access::ball(game, 2).active = false;
  game.tick(Input{});
  assert(game.lives() == 2 && game.activeBalls() == 1 && game.ball(0).attached);
  assert(game.powerTicks(Power::Laser) == 0);
  for (int i = 0; i < 44; ++i) game.tick(press());
  assert(game.ball(0).attached);
  game.tick(press());
  assert(game.ball(0).attached);
  game.tick(press());
  assert(!game.ball(0).attached);
}

static void stagesAndBonusLife() {
  Game game;
  fixture(game);
  Access::brick(game, 0, 0, 0);
  Access::brick(game, 3, 4);
  Access::brick(game, 4, 3, 255);
  auto& ball = Access::ball(game);
  ball.x = (Game::BrickLeft + 4 * Game::BrickPitchX + 6) * Q;
  ball.y = 84 * Q; ball.vx = 0; ball.vy = -8 * Q;
  game.tick(Input{});
  assert(game.remainingBricks() == 0 && Access::stageDelay(game) == 90);
  assert(game.stage() == 1 && game.score() == 590 && game.takeCue() == Cue::Win);
  for (int i = 0; i < 89; ++i) game.tick(press());
  assert(game.stage() == 1);
  game.tick(Input{});
  assert(game.stage() == 2 && game.remainingBricks() > 0 && game.ball(0).attached);
  uint32_t hashes[12] = {};
  for (int stage = 1; stage <= 12; ++stage) {
    Access::stage(game, uint16_t(stage));
    unsigned breakable = 0, gold = 0;
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < Game::BrickCount; ++i) {
      hash = (hash ^ game.brick(i).hits) * 16777619u;
      breakable += game.brick(i).hits != 0 && game.brick(i).hits != 255;
      gold += game.brick(i).hits == 255;
    }
    assert(breakable == game.remainingBricks() && breakable > 20);
    if (stage > 1) assert(gold > 0);
    hashes[stage - 1] = hash;
    if (stage > 1 && stage <= 6) assert(hashes[stage - 1] != hashes[stage - 2]);
  }
  fixture(game);
  Access::score(game, 14950);
  Access::brick(game, 3, 4);
  auto& bonusBall = Access::ball(game);
  bonusBall.x = (Game::BrickLeft + 4 * Game::BrickPitchX + 6) * Q;
  bonusBall.y = 84 * Q; bonusBall.vx = 0; bonusBall.vy = -8 * Q;
  game.tick(Input{});
  assert(game.lives() == 4 && game.score() == 15040);
}

static void deterministicLongRunAndPixelGuards() {
  Game first, second;
  first.reset(125, 1); second.reset(125, 1);
  std::array<uint16_t, 135 * 240 + 32> pixels;
  pixels.fill(0x9b7d);
  uint32_t controlRng = 91578;
  unsigned capsuleDrops = 0;
  for (unsigned tick = 0; tick < 200000; ++tick) {
    controlRng ^= controlRng << 13; controlRng ^= controlRng >> 17; controlRng ^= controlRng << 5;
    const auto& target = first.ball(0);
    const int targetX = int(target.x / Q);
    Input input;
    input.move = int16_t((targetX - first.paddleX()) * 180);
    if (input.move < -1024) input.move = -1024;
    if (input.move > 1024) input.move = 1024;
    if ((tick / 800) % 7 == 0) input.move = int16_t(controlRng % 2049) - 1024;
    input.action = tick % 60 < 50;
    input.pressed = tick % 75 == 0;
    first.tick(input); second.tick(input);
    assert(first.score() == second.score() && first.stage() == second.stage());
    assert(first.lives() == second.lives() && first.gameOver() == second.gameOver());
    assert(first.paddleX() >= Game::Left + first.paddleWidth() / 2);
    assert(first.paddleX() <= Game::Right - first.paddleWidth() / 2);
    assert(first.activeBalls() <= Game::MaxBalls && first.lives() <= 9);
    for (unsigned i = 0; i < Game::MaxBalls; ++i) {
      const auto& a = first.ball(i); const auto& b = second.ball(i);
      assert(a.x == b.x && a.y == b.y && a.vx == b.vx && a.vy == b.vy);
      assert(a.active == b.active && a.attached == b.attached);
      if (a.active) assert(a.x >= 7 * Q && a.x <= 127 * Q && a.y < 244 * Q);
    }
    for (unsigned i = 0; i < Game::MaxCapsules; ++i) capsuleDrops += first.capsule(i).active;
    assert(Access::rng(first) == Access::rng(second));
    assert(first.takeCue() == second.takeCue());
    if (tick % 97 == 0) {
      first.draw(pixels.data() + 16, 123456);
      for (unsigned guard = 0; guard < 16; ++guard) {
        assert(pixels[guard] == 0x9b7d);
        assert(pixels[pixels.size() - 1 - guard] == 0x9b7d);
      }
    }
  }
  assert(capsuleDrops > 0);
  first.draw(nullptr);
}

static void writePreview(const char* path) {
  Game game;
  game.reset(56);
  Access::stage(game, 2);
  Input input = press();
  for (unsigned tick = 0; tick < 30; ++tick) {
    input.pressed = tick == 0;
    input.move = tick % 80 < 40 ? 220 : -220;
    game.tick(input);
  }
  Access::capsule(game, Power::Expand);
  game.tick(Input{});
  Access::capsule(game, Power::Laser);
  game.tick(Input{});
  game.tick(press());
  Access::falling(game, 0, Power::Multi, 42, 143);
  Access::falling(game, 1, Power::Catch, 105, 120);
  std::array<uint16_t, 135 * 240> pixels;
  game.draw(pixels.data(), 14250);
  std::ofstream output(path, std::ios::binary);
  output << "P6\n135 240\n255\n";
  for (uint16_t color : pixels) {
    const unsigned char rgb[] = {static_cast<unsigned char>(((color >> 11) & 31) * 255 / 31),
        static_cast<unsigned char>(((color >> 5) & 63) * 255 / 63),
        static_cast<unsigned char>((color & 31) * 255 / 31)};
    output.write(reinterpret_cast<const char*>(rgb), 3);
  }
}

int main(int argc, char** argv) {
  servingMovementAndRestart();
  paddleAnglesAndFastCollision();
  bricksPreventTunnelingAndKeepGold();
  capsulePowersAndExpiry();
  lasersMultiballAndLifeLoss();
  stagesAndBonusLife();
  deterministicLongRunAndPixelGuards();
  if (argc > 1) writePreview(argv[1]);
  std::cout << "Arkanoid collision, power, stage, life, deterministic 200000-tick simulation and pixel guards passed. "
            << "Game state: " << sizeof(Game) << " bytes.\n";
}
