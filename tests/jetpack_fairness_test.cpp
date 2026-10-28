#include "../firmware/flappy/jetpack_game.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace flappy {
struct JetpackTestAccess {
  static JetpackGame::Zap sample(JetpackGame& game, int& spacing) {
    for (auto& zap : game.zaps_) zap = JetpackGame::Zap{};
    for (auto& missile : game.missiles_) missile = JetpackGame::Missile{};
    game.distanceToCoins_ = game.distanceToVehicle_ = 100000000;
    for (unsigned attempt = 0; attempt < 64 && !game.zaps_[0].active; ++attempt) {
      for (auto& rotator : game.rotators_) rotator = JetpackGame::Rotator{};
      game.distanceToHazard_ = 0;
      game.missileGap_ = false;
      game.spawnObjects(0);
    }
    spacing = game.distanceToHazard_ / JetpackGame::Unit;
    assert(game.zaps_[0].active);
    return game.zaps_[0];
  }
  static void fixture(JetpackGame& game, uint32_t seed, uint8_t difficulty, unsigned meters,
                      const std::vector<JetpackGame::Zap>& zaps) {
    game.reset(seed, difficulty);
    game.world_ = uint32_t(uint64_t(meters) * JetpackGame::Unit * 69 / 10);
    game.phase_ = JetpackGame::Phase::Running;
    game.distanceToHazard_ = game.distanceToCoins_ = game.distanceToVehicle_ = 100000000;
    game.distanceToBonus_ = game.distanceToToken_ = game.distanceToScientist_ = game.distanceToLaser_ = 100000000;
    for (unsigned i = 0; i < zaps.size(); ++i) game.zaps_[i] = zaps[i];
  }
  static void stage(JetpackGame& game, unsigned meters) {
    game.world_ = uint32_t(uint64_t(meters) * JetpackGame::Unit * 69 / 10);
  }
  static uint64_t bucket(const JetpackGame& game) {
    const int height = game.y_ / (4 * JetpackGame::FlightUnit);
    const int velocity = (game.velocity_ + 10 * JetpackGame::FlightUnit) / (JetpackGame::FlightUnit / 2);
    return uint64_t(uint32_t(height)) << 32 | uint32_t(velocity);
  }
  static bool fartherInDirection(const JetpackGame& a, const JetpackGame& b) {
    if (a.velocity_ < 0) return a.y_ < b.y_ || (a.y_ == b.y_ && a.velocity_ < b.velocity_);
    return a.y_ > b.y_ || (a.y_ == b.y_ && a.velocity_ > b.velocity_);
  }
  static bool unprotected(const JetpackGame& game) {
    for (uint16_t ticks : game.powerTicks_) if (ticks) return false;
    return !game.immune_ && game.vehicle_ == JetpackGame::Vehicle::None;
  }
  static bool near(int px, int py, int x, int y, int dx, int dy, int radius) {
    return JetpackGame::nearSegment(px, py, x, y, dx, dy, radius);
  }
};
}

using Game = flappy::JetpackGame;
using Access = flappy::JetpackTestAccess;

static long double distanceSquared(int px, int py, int x, int y, int dx, int dy) {
  const long double length = static_cast<long double>(dx) * dx +
      static_cast<long double>(dy) * dy;
  long double t = length ? ((static_cast<long double>(px) - x) * dx +
      (static_cast<long double>(py) - y) * dy) / length : 0;
  t = std::max(0.0L, std::min(1.0L, t));
  const long double ox = px - (x + t * dx), oy = py - (y + t * dy);
  return ox * ox + oy * oy;
}

static bool oracleHit(int px, int py, int x, int y, int dx, int dy, int radius = 6) {
  return distanceSquared(px, py, x, y, dx, dy) <= radius * radius + 1e-12L;
}

static void independentCapsuleOracle() {
  const int directions[][2] = {{0, 0}, {32, 0}, {0, 47}, {32, 39}, {32, -39},
      {-32, 39}, {-32, -39}, {13, 0}, {27, 42}};
  unsigned comparisons = 0;
  for (const auto& direction : directions)
    for (int px = -45; px <= 60; ++px)
      for (int py = -50; py <= 65; ++py) {
        const bool expected = oracleHit(px, py, 0, 0, direction[0], direction[1]);
        assert(Access::near(px, py, 0, 0, direction[0], direction[1], 6) == expected);
        ++comparisons;
      }
  assert(comparisons > 100000);
}

static void everyScreenEdgeNeedsInput() {
  unsigned ceilingThreats = 0, floorThreats = 0;
  for (uint32_t seed = 1; seed <= 128; ++seed) {
    Game game;
    game.reset(seed);
    for (unsigned sample = 0; sample < 8; ++sample) {
      int spacing = 0;
      const auto zap = Access::sample(game, spacing);
      assert(spacing >= 108);
      bool ceiling = false, floor = false;
      for (int x = -40; x <= 80; ++x) {
        ceiling |= oracleHit(Game::PlayerX, Game::Ceiling, x, zap.y, zap.dx, zap.dy);
        floor |= oracleHit(Game::PlayerX, Game::Floor, x, zap.y, zap.dx, zap.dy);
      }
      ceilingThreats += ceiling;
      floorThreats += floor;
    }
  }
  assert(ceilingThreats > 0 && floorThreats > 0);
}

// Search keeps one actual Q16.16 state per coarse height/velocity bucket.
// A found control witness is replayed through the real game and checked against
// independent geometry. The search covers sampled obstacle sequences.
struct Witness { size_t parent; bool held; };
struct SearchNode { Game game; size_t witness; bool held; };

static bool allPassed(const Game& game) {
  for (unsigned i = 0; i < Game::ZapCount; ++i) if (game.zap(i).active) return false;
  return true;
}

static bool oracleHitAfterAdvance(const Game& before, const Game& after) {
  for (unsigned i = 0; i < Game::ZapCount; ++i) {
    const auto& zap = before.zap(i);
    if (!zap.active) continue;
    const int x = int((zap.x - before.speed()) / Game::Unit);
    if (oracleHit(Game::PlayerX, after.playerY(), x, zap.y, zap.dx, zap.dy)) return true;
  }
  return false;
}

static bool searchAndReplay(const Game& initial) {
  std::vector<Witness> witnesses(1, Witness{0, false});
  std::vector<SearchNode> current(1, SearchNode{initial, 0, false});
  for (unsigned tick = 0; tick < 4000; ++tick) {
    std::vector<SearchNode> next;
    next.reserve(768);
    std::unordered_map<uint64_t, size_t> buckets;
    for (const auto& node : current) for (unsigned action = 0; action < 2; ++action) {
      Game game = node.game;
      flappy::ArcadeInput input;
      input.action = action != 0;
      game.tick(input);
      assert(Access::unprotected(game));
      const bool oracleHit = oracleHitAfterAdvance(node.game, game);
      assert((game.phase() == Game::Phase::Running) == !oracleHit);
      if (game.phase() != Game::Phase::Running) continue;
      const uint64_t key = Access::bucket(game);
      const auto existing = buckets.find(key);
      if (existing == buckets.end()) {
        if (next.size() >= 768) continue;
        buckets[key] = next.size();
        next.push_back(SearchNode{game, node.witness, action != 0});
      } else if (Access::fartherInDirection(game, next[existing->second].game)) {
        next[existing->second] = SearchNode{game, node.witness, action != 0};
      }
      if (allPassed(game)) {
        std::vector<bool> controls(1, action != 0);
        for (size_t id = node.witness; id; id = witnesses[id].parent)
          controls.push_back(witnesses[id].held);
        std::reverse(controls.begin(), controls.end());
        Game replay = initial;
        for (bool held : controls) {
          const Game before = replay;
          flappy::ArcadeInput replayInput;
          replayInput.action = held;
          replay.tick(replayInput);
          assert(replay.phase() == Game::Phase::Running && Access::unprotected(replay));
          assert(!oracleHitAfterAdvance(before, replay));
        }
        assert(allPassed(replay));
        return true;
      }
    }
    if (next.empty()) return false;
    for (auto& node : next) {
      witnesses.push_back(Witness{node.witness, node.held});
      node.witness = witnesses.size() - 1;
    }
    current.swap(next);
  }
  return false;
}

static void generatedSequencesHaveAPath() {
  const unsigned starts[] = {0, 1125, 2250};
  unsigned sequences = 0;
  for (uint32_t seed = 1; seed <= 6; ++seed) {
    for (unsigned condition = 0; condition < 3; ++condition) {
      const uint8_t difficulty = condition == 2 ? 2 : 1;
      Game generator;
      generator.reset(seed * 7919, difficulty);
      Access::stage(generator, starts[condition]);
      std::vector<Game::Zap> zaps;
      int32_t initialX = 258 * Game::Unit;
      for (unsigned sample = 0; sample < 4; ++sample) {
        int spacing = 0;
        auto zap = Access::sample(generator, spacing);
        zap.x = initialX;
        zaps.push_back(zap);
        initialX += spacing * Game::Unit;
      }
      Game initial;
      Access::fixture(initial, seed * 7919, difficulty, starts[condition], zaps);
      if (!searchAndReplay(initial)) {
        std::cerr << "No sampled route for seed " << seed * 7919 << " at distance " << starts[condition] << '\n';
        assert(false);
      }
      ++sequences;
    }
  }
  std::cout << "Jetpack sampled safe paths replayed: " << sequences << " sequences\n";
}

int main() {
  independentCapsuleOracle();
  everyScreenEdgeNeedsInput();
  generatedSequencesHaveAPath();
  std::cout << "Jetpack fairness checks passed\n";
}
