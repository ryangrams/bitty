#include "../firmware/flappy/galaga_game.h"

#include <array>
#include <assert.h>
#include <iostream>

namespace flappy {
struct GalagaGameTestAccess {
  static void field(GalagaGame& g) {
    g.phase_ = GalagaGame::Phase::Playing;
    g.phaseTimer_ = 0;
    g.stageTick_ = 0;
    g.invulnerable_ = g.playerDelay_ = 0;
    g.attackDelay_ = 500;
    for (unsigned i = 0; i < GalagaGame::EnemyCount; ++i) g.enemies_[i] = GalagaGame::Enemy();
    for (unsigned i = 0; i < GalagaGame::PlayerShotCount; ++i) g.playerShots_[i] = GalagaGame::Shot();
    for (unsigned i = 0; i < GalagaGame::EnemyShotCount; ++i) g.enemyShots_[i] = GalagaGame::Shot();
    enemy(g, 39, GalagaGame::Kind::Bee, GalagaGame::Flight::Formation, 122, 100);
  }
  static GalagaGame::Enemy& enemy(GalagaGame& g, unsigned index, GalagaGame::Kind kind,
                                  GalagaGame::Flight flight, int x, int y, unsigned hp = 1) {
    auto& e = g.enemies_[index];
    e = GalagaGame::Enemy(); e.flight = flight; e.kind = kind; e.hp = uint8_t(hp);
    e.x = e.originX = int16_t(x * 16); e.y = e.originY = int16_t(y * 16); e.targetX = g.playerX_;
    return e;
  }
  static void shot(GalagaGame& g, int x, int y, bool hostile = false) {
    auto& s = hostile ? g.enemyShots_[0] : g.playerShots_[0];
    s = GalagaGame::Shot(); s.active = true; s.x = int16_t(x * 16); s.y = int16_t(y * 16);
    s.vy = hostile ? 32 : -80;
  }
  static void vulnerable(GalagaGame& g) { g.invulnerable_ = g.playerDelay_ = 0; }
  static void dual(GalagaGame& g) { g.dual_ = true; }
  static void stage(GalagaGame& g, unsigned stage) { g.stage_ = uint16_t(stage); g.startStage(); }
  static void setScore(GalagaGame& g, unsigned score) { g.score_ = score; }
  static void kill(GalagaGame& g, unsigned i) { g.destroyEnemy(i); }
  static void formationPosition(GalagaGame& g, unsigned i, int& x, int& y) { g.home(i, x, y); x /= 16; y /= 16; }
  static void finish(GalagaGame& g) { for (auto& e : g.enemies_) e.flight = GalagaGame::Flight::Dead; }
  static unsigned delay(const GalagaGame& g) { return g.playerDelay_; }
  static int captured(const GalagaGame& g) { return g.captured_; }
  static void beam(GalagaGame& g, unsigned age = 48) {
    auto& e = enemy(g, 0, GalagaGame::Kind::Boss, GalagaGame::Flight::Tractor, g.playerX(), 126, 2);
    e.age = uint16_t(age);
  }
  static void frame(GalagaGame& g, uint32_t frame) { g.frame_ = frame; }
};
}

using G = flappy::GalagaGame;
using A = flappy::GalagaGameTestAccess;
using flappy::ArcadeInput;

static ArcadeInput control(int16_t move = 0, bool action = false, bool pressed = false) {
  ArcadeInput input; input.move = move; input.action = action; input.pressed = pressed; return input;
}

static void advance(G& g, unsigned frames, const ArcadeInput& input = {}) {
  while (frames--) g.tick(input);
}

static void fixedTickAndMovement() {
  G g;
  assert(g.stage() == 1 && g.lives() == 3 && g.remaining() == 40);
  assert(g.phase() == G::Phase::Intro);
  const int start = g.playerX();
  g.tick(control(1024, false, false));
  assert(g.playerX() > start);
  advance(g, 200, control(-32768, false, false));
  assert(g.playerX() == 5);
  advance(g, 200, control(32767, false, false));
  assert(g.playerX() == 129);
  assert(!g.gameOver());
  assert(g.takeCue() == flappy::ArcadeCue::None);
  advance(g, 800);
  unsigned formed = 0;
  for (unsigned i = 0; i < G::EnemyCount; ++i) {
    const auto& e = g.enemies()[i];
    assert(e.flight != G::Flight::Pending && e.flight != G::Flight::Entrance);
    formed += e.flight == G::Flight::Formation;
  }
  assert(formed > 25);
}

static void holdingFireAndSweptHits() {
  G g;
  A::field(g);
  advance(g, 100, control(0, true, false));
  unsigned shots = 0;
  for (unsigned i = 0; i < G::PlayerShotCount; ++i) shots += g.playerShots()[i].active;
  assert(shots > 0 && shots <= 2);
  assert(g.takeCue() == flappy::ArcadeCue::Fire);
  assert(g.takeCue() == flappy::ArcadeCue::None);

  A::field(g);
  A::enemy(g, 20, G::Kind::Bee, G::Flight::Entrance, 50, 82);
  auto& e = const_cast<G::Enemy&>(g.enemies()[20]);
  e.age = 48;
  A::shot(g, 67, 67);
  g.tick({});
  assert(g.enemies()[20].flight == G::Flight::Dead);
  assert(g.score() == 50);
  advance(g, 5);
  assert(g.score() == 50);
}

static void bossesNeedTwoHitsAndDiveScores() {
  G g;
  A::field(g);
  A::enemy(g, 0, G::Kind::Boss, G::Flight::Dive, 67, 90, 2);
  A::shot(g, 67, 98);
  g.tick({});
  assert(g.enemies()[0].hp == 1 && g.score() == 0);
  A::shot(g, g.enemies()[0].x / 16, g.enemies()[0].y / 16 + 6);
  g.tick({});
  assert(g.enemies()[0].flight == G::Flight::Dead);
  assert(g.score() == 400);
  A::enemy(g, 1, G::Kind::Butterfly, G::Flight::Dive, 67, 90);
  A::shot(g, 67, 97);
  g.tick({});
  assert(g.score() == 560);
}

static void enemyFireDeathAndRestart() {
  G g;
  A::field(g);
  for (unsigned life = 3; life > 0; --life) {
    A::vulnerable(g);
    A::shot(g, g.playerX(), 207, true);
    g.tick({});
    assert(g.lives() == life - 1);
    assert(g.takeCue() == flappy::ArcadeCue::Death);
    assert(g.gameOver() == (life == 1));
    if (life > 1) {
      advance(g, 100);
      assert(g.playerVisible());
      assert(g.lives() == life - 1);
      A::shot(g, g.playerX(), 207, true);
      g.tick({});
      assert(g.lives() == life - 1);
    }
  }
  advance(g, 89, control(0, true, false));
  assert(g.gameOver());
  g.tick(control(0, true, true));
  assert(g.gameOver());
  g.tick(control(0, false, false));
  g.tick(control(0, true, true));
  assert(!g.gameOver() && g.lives() == 3 && g.score() == 0 && g.stage() == 1);
}

static void dualShotAndSingleWingLoss() {
  G g;
  A::field(g); A::dual(g);
  g.tick(control(0, true, false));
  unsigned shots = 0;
  int low = 999, high = -999;
  for (unsigned i = 0; i < G::PlayerShotCount; ++i) if (g.playerShots()[i].active) {
    ++shots;
    const int x = g.playerShots()[i].x / 16;
    if (x < low) low = x;
    if (x > high) high = x;
  }
  assert(shots == 2 && high - low == 10);
  A::shot(g, g.playerX() - 5, 207, true);
  g.tick({});
  assert(!g.dualFighter() && g.lives() == 3 && g.playerVisible());
}

static void twoVolleysStayOnScreen() {
  for (unsigned dual = 0; dual < 2; ++dual) {
    G g;
    A::field(g);
    if (dual) A::dual(g);
    unsigned maximum = 0;
    for (unsigned frame = 0; frame < 180; ++frame) {
      g.tick(control(0, true));
      unsigned active = 0;
      for (unsigned i = 0; i < G::PlayerShotCount; ++i) active += g.playerShots()[i].active;
      assert(active <= (dual ? 4u : 2u));
      if (active > maximum) maximum = active;
    }
    assert(maximum == (dual ? 4u : 2u));
  }
}

static void tractorCaptureRescueAndFriendlyFire() {
  G g;
  A::field(g); A::beam(g);
  advance(g, 27);
  assert(A::captured(g) < 0 && g.lives() == 3);
  g.tick({});
  assert(A::captured(g) == 0 && g.enemies()[0].carrying && g.lives() == 2);
  assert(!g.playerVisible());
  advance(g, 120);
  assert(g.playerVisible());
  auto& boss = const_cast<G::Enemy&>(g.enemies()[0]);
  boss.flight = G::Flight::Dive; boss.hp = 1; boss.age = 0;
  boss.x = boss.originX = 67 * 16; boss.y = boss.originY = 90 * 16;
  A::shot(g, 71, 97);
  g.tick({});
  assert(g.enemies()[0].flight == G::Flight::Dead && A::captured(g) < 0);
  assert(g.score() == 1000);
  advance(g, 64);
  assert(g.dualFighter() && g.lives() == 2);

  G other;
  A::field(other);
  auto& carrier = A::enemy(other, 0, G::Kind::Boss, G::Flight::Dive, 67, 90, 2);
  carrier.carrying = true;
  A::shot(other, 67, 109);
  other.tick({});
  assert(!other.enemies()[0].carrying && other.enemies()[0].hp == 2 && other.score() == 0);
  advance(other, 70);
  assert(!other.dualFighter());

  G formed;
  A::field(formed);
  auto& docked = A::enemy(formed, 0, G::Kind::Boss, G::Flight::Formation, 50, 42, 1);
  docked.carrying = true;
  A::kill(formed, 0);
  assert(formed.enemies()[0].kind == G::Kind::Captive);
  assert(formed.enemies()[0].flight == G::Flight::Dive);
  advance(formed, 250);
  assert(!formed.dualFighter() && formed.enemies()[0].flight == G::Flight::Dead);
}

static void tractorEscapeAndNoCaptureOnLastLife() {
  G g;
  A::field(g); A::beam(g);
  advance(g, 35, control(1024, false, false));
  assert(g.lives() == 3 && A::captured(g) < 0);
}

static void stagesBonusAndExtraLife() {
  G g;
  A::field(g);
  A::finish(g);
  g.tick({});
  assert(g.phase() == G::Phase::Clear);
  advance(g, 140);
  assert(g.stage() == 2 && g.phase() == G::Phase::Intro);
  A::stage(g, 3);
  assert(g.challenging());
  advance(g, 100);
  for (unsigned i = 0; i < G::EnemyCount; ++i) A::kill(g, i);
  g.tick({});
  assert(g.bonusHits() == 40 && g.score() == 10000 && g.phase() == G::Phase::BonusResult);
  advance(g, 210);
  assert(g.stage() == 4 && !g.challenging());

  G missed;
  A::stage(missed, 3);
  advance(missed, 1500);
  assert(missed.stage() == 4);
  assert(missed.score() == 0 && missed.lives() == 3);

  G life;
  A::field(life); A::setScore(life, 19960);
  A::enemy(life, 0, G::Kind::Bee, G::Flight::Formation, 67, 90);
  A::kill(life, 0);
  assert(life.score() == 20010 && life.lives() == 4);
  A::kill(life, 39);
  assert(life.lives() == 4);
}

static uint32_t imageHash(const std::array<uint16_t, 135 * 240 + 16>& frame) {
  uint32_t result = 2166136261u;
  for (unsigned i = 8; i < frame.size() - 8; ++i) { result ^= frame[i]; result *= 16777619u; }
  return result;
}

static void ordinaryShotsCanClearStages() {
  for (unsigned seed = 1; seed <= 3; seed += 2) {
    G g; g.reset(seed);
    for (unsigned frame = 0; frame < 20000 && !g.gameOver() && g.stage() < 4; ++frame) {
      int target = g.playerX(), priority = -9999;
      for (unsigned i = 0; i < G::EnemyCount; ++i) {
        const auto& e = g.enemies()[i];
        const int x = e.x / 16, y = e.y / 16;
        if (e.flight == G::Flight::Dead || e.flight == G::Flight::Pending ||
            x < 8 || x > 126 || y < 25 || y > 190) continue;
        const int distance = x > g.playerX() ? x - g.playerX() : g.playerX() - x;
        const int value = y * 2 - distance;
        if (value > priority) { priority = value; target = x; }
      }
      const int dx = target - g.playerX();
      g.tick(control(int16_t(dx > 3 ? 1024 : dx < -3 ? -1024 : dx * 250), true));
    }
    assert(!g.gameOver() && g.stage() == 4 && g.score() > 6000);
  }
}

static void longSeededSimulationAndPixelGuards() {
  std::array<uint16_t, 135 * 240 + 16> pixels;
  G first, second;
  for (unsigned seed = 1; seed <= 12; ++seed) {
    first.reset(seed, uint8_t(seed % 3)); second.reset(seed, uint8_t(seed % 3));
    uint32_t random = seed;
    for (unsigned frame = 0; frame < 30000; ++frame) {
      random = random * 1664525u + 1013904223u;
      const ArcadeInput input = control(int16_t(int(random >> 21) - 1024), (random & 7) < 6, (random & 63) == 0);
      first.tick(input); second.tick(input);
      assert(first.score() == second.score() && first.stage() == second.stage() && first.lives() == second.lives());
      assert(first.playerX() >= (first.dualFighter() ? 10 : 5));
      assert(first.playerX() <= (first.dualFighter() ? 124 : 129));
      assert(first.lives() <= 9);
      for (unsigned i = 0; i < G::EnemyCount; ++i) {
        const auto& e = first.enemies()[i]; const auto& other = second.enemies()[i];
        assert(e.x == other.x && e.y == other.y && e.flight == other.flight && e.hp == other.hp);
        if (e.flight != G::Flight::Dead && e.flight != G::Flight::Pending) {
          assert(e.x > -128 * 16 && e.x < 256 * 16);
          assert(e.y > -32 * 16 && e.y < 256 * 16);
        }
      }
      if (frame % 601 == 0) {
        pixels.fill(0xa55a);
        first.draw(pixels.data() + 8, 55555);
        const uint32_t snapshot = imageHash(pixels);
        for (unsigned i = 0; i < 8; ++i) assert(pixels[i] == 0xa55a && pixels[pixels.size() - 1 - i] == 0xa55a);
        second.draw(pixels.data() + 8, 55555);
        assert(imageHash(pixels) == snapshot);
      }
    }
  }
  assert(sizeof(G) < 2048);
  std::cout << "Galaga game size: " << sizeof(G) << " bytes.\n";
}

int main() {
  fixedTickAndMovement();
  holdingFireAndSweptHits();
  bossesNeedTwoHitsAndDiveScores();
  enemyFireDeathAndRestart();
  dualShotAndSingleWingLoss();
  twoVolleysStayOnScreen();
  tractorCaptureRescueAndFriendlyFire();
  tractorEscapeAndNoCaptureOnLastLife();
  stagesBonusAndExtraLife();
  ordinaryShotsCanClearStages();
  longSeededSimulationAndPixelGuards();
  std::cout << "Galaga gameplay checks passed.\n";
  return 0;
}
