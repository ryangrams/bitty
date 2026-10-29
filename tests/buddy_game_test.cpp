#include "../firmware/flappy/buddy_game.h"

#include <assert.h>
#include <stdint.h>
#include <iostream>

using Game = flappy::BuddyGame;
using Input = flappy::BuddyInput;
using Kind = Game::ObjectKind;
using Phase = Game::Phase;

static Input lane(int target) { Input input; input.lane = int8_t(target); return input; }
static void advance(Game& game, unsigned count, const Input& input = {}) {
  while (count--) game.tick(input);
}

static Input safeInput(const Game& game) {
  int nearest = -1;
  for (unsigned i = 0; i < Game::ObjectCount; ++i) {
    const auto& object = game.objects()[i];
    if (!object.active || object.kind != Kind::Coin || !(object.variant & Game::RouteCoin)) continue;
    if (nearest < 0 || object.progress > game.objects()[nearest].progress) nearest = int(i);
  }
  int target = nearest < 0 ? game.lane() : game.objects()[nearest].lane;
  int front = -1;
  for (unsigned i = 0; i < Game::ObjectCount; ++i)
    if (game.objects()[i].active && Game::obstacle(game.objects()[i].kind) &&
        (front < 0 || game.objects()[i].progress > game.objects()[front].progress)) front = int(i);
  if (front >= 0) {
    unsigned blocked = 0;
    for (unsigned i = 0; i < Game::ObjectCount; ++i)
      if (game.objects()[i].active && Game::obstacle(game.objects()[i].kind) &&
          game.objects()[i].progress == game.objects()[front].progress) blocked |= 1u << game.objects()[i].lane;
    if (target == 2 && game.lanePosition() < .5f && (blocked & 2u)) target = 0;
    else if (target == 0 && game.lanePosition() > 1.5f && (blocked & 2u)) target = 2;
    if (blocked & (1u << target)) {
      int bestDistance = 999;
      for (int candidate = 0; candidate < 3; ++candidate) if (!(blocked & (1u << candidate))) {
        const int offset = int(game.lanePosition() * Game::LaneUnit) - candidate * Game::LaneUnit;
        const int distance = offset < 0 ? -offset : offset;
        if (distance < bestDistance) { bestDistance = distance; target = candidate; }
      }
    }
  }
  return lane(target);
}

static uint32_t hashState(const Game& game) {
  uint32_t hash = 2166136261u;
  const auto add = [&hash](uint32_t value) { hash = (hash ^ value) * 16777619u; };
  add(game.ticks()); add(game.levelTicks()); add(game.phaseTicks()); add(unsigned(game.phase()));
  add(game.level()); add(game.score()); add(game.coins()); add(game.parts()); add(game.distance());
  add(unsigned(game.lane())); add(uint32_t(game.lanePosition() * 256)); add(game.jumpTick());
  add(game.rollTicks()); add(game.boardTicks()); add(game.snakeCharges()); add(game.snakeTicks());
  add(game.clearedObstacles()); add(game.passedObstacles()); add(game.shirt()); add(game.mask());
  for (unsigned i = 0; i < Game::ObjectCount; ++i) {
    const auto& object = game.objects()[i];
    add(object.active); add(unsigned(object.kind)); add(unsigned(object.lane));
    add(uint32_t(object.progress)); add(object.variant);
  }
  return hash;
}

static void boundedState(const Game& game) {
  assert(game.lane() >= 0 && game.lane() <= 2);
  assert(game.lanePosition() >= 0 && game.lanePosition() <= 2);
  assert(game.jumpHeight() >= 0 && game.jumpHeight() <= 1);
  assert(game.level() < Game::LevelCount && game.levelTicks() <= Game::LevelTicks);
  assert(game.objectCount() <= Game::ObjectCount);
  assert(game.boardTicks() <= Game::BoardTicks && game.snakeCharges() <= 10);
  assert(game.speed() >= Game::DepthUnit / 190 && game.speed() <= Game::DepthUnit / 110);
  for (unsigned i = 0; i < Game::ObjectCount; ++i) {
    const auto& object = game.objects()[i];
    assert(object.lane >= 0 && object.lane < 3);
    assert(object.depth >= -.6f && object.depth < 1.03f);
    if (object.active) assert(object.progress >= -Game::DepthUnit && object.progress < Game::DepthUnit);
  }
}

static void controlsAndConfiguration() {
  Game game;
  assert(game.phase() == Phase::Playing && game.lane() == 1 && game.mask() == 1);
  assert(game.levelTicks() == 0 && game.objectCount() == 0);
  game.tick(lane(0));
  assert(game.lane() == 0 && game.lanePosition() < 1 && game.lanePosition() > 0);
  advance(game, 15);
  assert(game.lanePosition() == 0);
  Input step; step.step = 1;
  game.tick(step); advance(game, 15);
  assert(game.lane() == 1 && game.lanePosition() == 1);
  step.step = 127;
  advance(game, 3, step);
  assert(game.lane() == 2);
  Input invalid; invalid.lane = 3;
  advance(game, 15, invalid);
  assert(game.lanePosition() == 2);
  invalid.lane = -128; invalid.step = -128;
  advance(game, 20, invalid);
  assert(game.lane() == 0 && game.lanePosition() == 0);
  game.setStartingLevel(255); game.setLook(7, 6); game.reset(0, 255);
  assert(game.level() == 4 && game.theme() == Game::Theme::Noodles && game.difficulty() == 2);
  assert(game.shirt() == 3 && game.mask() == 2 && game.lane() == 1);
  game.setStartingLevel(0); game.reset(1, 0);
  assert(game.level() == 0 && game.shirt() == 3 && game.mask() == 2);
}

static void jumpRollAndRearming() {
  Game game;
  Input jump; jump.jump = true;
  game.tick(jump);
  assert(game.jumpTick() == 1 && game.jumpHeight() > 0);
  assert(game.takeCue() == flappy::ArcadeCue::Fire);
  assert(game.takeCue() == flappy::ArcadeCue::None);
  advance(game, 26, jump);
  assert(game.jumpHeight() == 1 && !game.rolling());
  advance(game, 35, jump);
  assert(game.jumpHeight() == 0);
  advance(game, 10, jump);
  assert(game.jumpHeight() == 0);
  game.tick({}); game.tick(jump);
  assert(game.jumpTick() == 1);

  game.reset();
  Input roll; roll.roll = true;
  game.tick(roll);
  assert(game.rolling() && game.rollTicks() == Game::RollTicks && game.jumpHeight() == 0);
  advance(game, Game::RollTicks + 8, roll);
  assert(!game.rolling());
  game.tick({}); game.tick(roll);
  assert(game.rolling());
  game.reset();
  Input both; both.jump = both.roll = true;
  game.tick(both);
  assert(game.jumpTick() == 1 && !game.rolling());
  advance(game, 60, both);
  assert(game.jumpTick() == 0 && !game.rolling());
}

static int approach(Game& game, Kind kind) {
  for (unsigned tick = 0; tick < 5000; ++tick) {
    for (unsigned i = 0; i < Game::ObjectCount; ++i) {
      const auto& object = game.objects()[i];
      if (object.active && object.kind == kind &&
          Game::DepthUnit - object.progress <= game.speed() * 30) return int(i);
    }
    game.tick(safeInput(game));
    assert(game.phase() == Phase::Playing);
  }
  assert(false && "Target obstacle did not appear");
  return -1;
}

static void encounter(Kind kind, unsigned action, bool survives) {
  Game game; game.reset(27, 2);
  const int target = approach(game, kind);
  const int targetLane = game.objects()[target].lane;
  const uint32_t passed = game.passedObstacles();
  for (unsigned tick = 0; tick < 50 && game.objects()[target].active; ++tick) {
    Input input = lane(targetLane);
    const int32_t remaining = Game::DepthUnit - game.objects()[target].progress;
    if (remaining <= game.speed() * 20) {
      input.jump = action == 1;
      input.roll = action == 2;
    }
    game.tick(input);
  }
  assert(!game.objects()[target].active);
  assert((game.phase() == Phase::Playing) == survives);
  if (survives) assert(game.passedObstacles() == passed + 1);
  else {
    assert(game.phase() == Phase::Crashed && game.gameOver());
    assert(game.takeCue() == flappy::ArcadeCue::Death);
    const uint32_t score = game.score(), distance = game.distance(), coins = game.coins();
    Input retry; retry.pressed = true;
    game.tick(retry);
    assert(game.phase() == Phase::Crashed);
    advance(game, Game::RetryTicks);
    assert(game.score() == score && game.distance() == distance && game.coins() == coins);
    game.tick(retry);
    assert(game.phase() == Phase::Playing && game.score() == 0 && game.levelTicks() == 0);
  }
}

static void collisionRules() {
  encounter(Kind::LowObstacle, 0, false);
  encounter(Kind::LowObstacle, 1, true);
  encounter(Kind::LowObstacle, 2, false);
  encounter(Kind::Straddle, 0, false);
  encounter(Kind::Straddle, 1, false);
  encounter(Kind::Straddle, 2, true);
  encounter(Kind::Person, 1, false);
  encounter(Kind::Person, 2, false);
  Game evade; evade.reset(27, 2);
  const int target = approach(evade, Kind::Person);
  const int otherLane = (evade.objects()[target].lane + 1) % 3;
  while (evade.objects()[target].active) evade.tick(lane(otherLane));
  assert(evade.phase() == Phase::Playing);
}

static void powersAndCustomization() {
  Game game; game.reset(734, 1);
  for (unsigned tick = 0; tick < 3000 && !game.boardTicks(); ++tick) game.tick(safeInput(game));
  assert(game.boardTicks() == Game::BoardTicks && game.coins() > 0);
  game.takeCue();
  const int target = approach(game, Kind::Person);
  const int targetLane = game.objects()[target].lane;
  while (game.objects()[target].active) game.tick(lane(targetLane));
  assert(game.phase() == Phase::Playing && !game.boardTicks());
  assert(game.invulnerableTicks() == 90 && game.takeCue() == flappy::ArcadeCue::Power);
  const uint32_t before = game.clearedObstacles();
  bool collectedSnake = false;
  for (unsigned tick = 0; tick < 5000; ++tick) {
    game.tick(safeInput(game));
    assert(game.phase() == Phase::Playing);
    if (game.clearedObstacles() > before) collectedSnake = true;
    if (collectedSnake && !game.snakeCharges()) break;
  }
  assert(collectedSnake && game.clearedObstacles() == before + 10 && !game.snakeCharges());

  Game expired; expired.reset(734, 1);
  while (!expired.boardTicks()) expired.tick(safeInput(expired));
  for (unsigned tick = 0; tick < Game::BoardTicks; ++tick) expired.tick(safeInput(expired));
  assert(!expired.boardTicks());
  Game wardrobe; wardrobe.reset(734, 1);
  const uint8_t firstMask = wardrobe.mask(), firstShirt = wardrobe.shirt();
  while (wardrobe.parts() < 3 || wardrobe.shirt() == firstShirt) {
    wardrobe.tick(safeInput(wardrobe));
    assert(wardrobe.phase() == Phase::Playing && wardrobe.levelTicks() < 10000);
  }
  assert(wardrobe.mask() == uint8_t((firstMask + wardrobe.parts() / 3) % 4));
  assert(wardrobe.shirt() != firstShirt);
}

static void fairRowsAndFiniteProgression() {
  for (uint8_t difficulty = 0; difficulty < 3; ++difficulty) {
    Game game; game.reset(1122 + difficulty, difficulty);
    unsigned themes = 0;
    for (unsigned level = 0; level < Game::LevelCount; ++level) {
      assert(game.level() == level && game.theme() == static_cast<Game::Theme>(level));
      themes |= 1u << unsigned(game.theme());
      while (game.phase() == Phase::Playing) {
        for (unsigned i = 0; i < Game::ObjectCount; ++i) {
          const auto& coin = game.objects()[i];
          if (!coin.active || coin.kind != Kind::Coin || !(coin.variant & Game::RouteCoin)) continue;
          unsigned blocked = 0;
          for (unsigned j = 0; j < Game::ObjectCount; ++j) {
            const auto& object = game.objects()[j];
            if (object.active && Game::obstacle(object.kind) && object.progress == coin.progress) {
              blocked |= 1u << unsigned(object.lane);
              assert(object.lane != coin.lane);
            }
          }
          assert(blocked != 7);
        }
        game.tick(safeInput(game));
        boundedState(game);
        if (game.phase() == Phase::Crashed) { std::cerr << "COURSE FAIL difficulty=" << unsigned(difficulty) << " level=" << level << " tick=" << game.levelTicks() << " lane=" << game.lane() << " lift=" << unsigned(game.flightHeight()) << "\n"; }
        assert(game.phase() != Phase::Crashed);
        if (game.levelTicks() < Game::LevelTicks) assert(game.phase() == Phase::Playing);
      }
      assert(game.phase() == Phase::Celebrate && game.levelTicks() == Game::LevelTicks);
      assert(game.takeCue() == flappy::ArcadeCue::Win);
      const uint32_t coins = game.coins(), score = game.score();
      Input next; next.pressed = true;
      game.tick(next);
      assert(game.phase() == Phase::Celebrate);
      advance(game, Game::CelebrateTicks);
      assert(game.coins() == coins && game.score() == score);
      game.tick(next);
      if (level + 1 < Game::LevelCount) assert(game.phase() == Phase::Playing && game.levelTicks() == 0);
      else assert(game.phase() == Phase::Complete && game.gameOver());
    }
    assert(themes == 31 && game.coins() > 1000 && game.score() > 10000);
  }
}

static void deterministicResetsAndRandomInput() {
  Game a, b; a.reset(0, 1); b.reset(1, 1);
  uint32_t random = 9781;
  unsigned crashes = 0;
  for (unsigned tick = 0; tick < 150000; ++tick) {
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    Input input;
    input.lane = int8_t(int(random % 5) - 1);
    input.step = int8_t(int(random % 3) - 1);
    input.jump = (random & 31) == 1;
    input.roll = (random & 31) == 2;
    input.pressed = a.phase() == Phase::Crashed && a.phaseTicks() >= Game::RetryTicks;
    const Phase before = a.phase();
    a.tick(input); b.tick(input);
    boundedState(a); boundedState(b);
    assert(hashState(a) == hashState(b));
    assert(a.takeCue() == b.takeCue());
    crashes += before == Phase::Playing && a.phase() == Phase::Crashed;
  }
  assert(crashes > 50);
  a.setStartingLevel(2); b.setStartingLevel(2);
  a.setLook(3, 2); b.setLook(3, 2);
  a.reset(712, 0); b.reset(712, 0);
  assert(hashState(a) == hashState(b));
  std::cout << "Bitty Buddy state: " << sizeof(Game) << " bytes\n";
}

int main() {
  controlsAndConfiguration();
  jumpRollAndRearming();
  collisionRules();
  powersAndCustomization();
  fairRowsAndFiniteProgression();
  deterministicResetsAndRandomInput();
  std::cout << "Bitty Buddy collision, actions, powers, five ten-minute levels, fairness and determinism passed\n";
}
