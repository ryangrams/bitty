#pragma once

#include <stdint.h>
#include "arcade_support.h"

namespace flappy {

// A fixed 60 Hz simulation. Positions use four fractional bits.
class GalagaGame {
 public:
  enum { EnemyCount = 40, PlayerShotCount = 12, EnemyShotCount = 16 };
  enum class Phase : uint8_t { Intro, Playing, Clear, BonusResult, Over };
  enum class Flight : uint8_t { Pending, Entrance, Formation, Dive, Return, Tractor, Dead };
  enum class Kind : uint8_t { Boss, Butterfly, Bee, Captive };
  struct Enemy {
    int16_t x = 0, y = 0, originX = 0, originY = 0, targetX = 0;
    uint16_t age = 0;
    Flight flight = Flight::Dead;
    Kind kind = Kind::Bee;
    uint8_t hp = 0, flash = 0;
    bool carrying = false;
  };
  struct Shot { int16_t x = 0, y = 0, vx = 0, vy = 0; bool active = false; };

  GalagaGame() { reset(); }
  void reset(uint32_t seed = 1, uint8_t difficulty = 1) {
    seed_ = seed ? seed : 1;
    random_ = seed_;
    difficulty_ = difficulty > 2 ? 2 : difficulty;
    score_ = 0;
    extraLifeAt_ = 20000;
    stage_ = 1;
    lives_ = 3;
    playerX_ = 67 * Unit;
    frame_ = 0;
    playerDelay_ = 0;
    invulnerable_ = 100;
    shotDelay_ = 0;
    dual_ = false;
    rescueTimer_ = 0;
    cue_ = ArcadeCue::None;
    startStage();
  }

  void tick(const ArcadeInput& input) {
    ++frame_;
    if (phase_ == Phase::Over) {
      if (phaseTimer_) --phaseTimer_;
      else if (input.pressed) reset(seed_, difficulty_);
      return;
    }
    if (phase_ == Phase::Intro) {
      movePlayer(input.move);
      if (phaseTimer_ && !--phaseTimer_) phase_ = Phase::Playing;
      return;
    }
    if (phase_ == Phase::Clear || phase_ == Phase::BonusResult) {
      movePlayer(input.move);
      updateEffects();
      if (phaseTimer_ && !--phaseTimer_) {
        if (stage_ < 999) ++stage_;
        startStage();
      }
      return;
    }

    ++stageTick_;
    if (playerDelay_) {
      if (!--playerDelay_) {
        playerX_ = 67 * Unit;
        invulnerable_ = 120;
      }
    } else {
      movePlayer(input.move);
      if (invulnerable_) --invulnerable_;
      if (shotDelay_) --shotDelay_;
      if (input.action && !shotDelay_) fire();
    }
    updateEnemies();
    if (phase_ == Phase::Over) return;
    updateShots();
    updateEffects();
    if (phase_ == Phase::Over) return;
    if (rescueTimer_ && !playerDelay_) {
      if (!--rescueTimer_) {
        dual_ = true;
        movePlayer(0);
        invulnerable_ = 90;
        signal(ArcadeCue::Power);
      }
    }
    if (!remaining()) finishStage();
  }

  bool gameOver() const { return phase_ == Phase::Over; }
  uint32_t score() const { return score_; }
  uint16_t stage() const { return stage_; }
  ArcadeCue takeCue() { const ArcadeCue result = cue_; cue_ = ArcadeCue::None; return result; }
  Phase phase() const { return phase_; }
  uint8_t lives() const { return lives_; }
  int playerX() const { return playerX_ / Unit; }
  bool playerVisible() const { return playerDelay_ == 0 && !gameOver(); }
  bool dualFighter() const { return dual_; }
  bool challenging() const { return bonus_; }
  uint8_t bonusHits() const { return bonusHits_; }
  const Enemy* enemies() const { return enemies_; }
  const Shot* playerShots() const { return playerShots_; }
  const Shot* enemyShots() const { return enemyShots_; }
  uint8_t remaining() const {
    uint8_t result = 0;
    for (unsigned i = 0; i < EnemyCount; ++i) result += enemies_[i].flight != Flight::Dead;
    return result;
  }

  void draw(uint16_t* pixels, uint32_t best = 0) const {
    if (!pixels) return;
    ArcadeCanvas c(pixels);
    const uint16_t white = color(230, 244, 255), cyan = color(72, 220, 255);
    const uint16_t red = color(255, 70, 84), gold = color(255, 208, 64);
    c.clear(color(1, 3, 12));
    for (unsigned i = 0; i < 42; ++i) {
      const uint32_t h = hash(seed_ + i * 317u);
      const int x = int(h % 135u);
      const int y = 21 + int(((h >> 9) + frame_ / (i % 3 + 2)) % 204u);
      const unsigned light = ((frame_ / 32 + i) % 5 == 0) ? 150 : 75;
      c.pixel(x, y, i % 4 == 0 ? color(light, light / 2, light) : color(light, light, light + 20));
    }
    c.rect(0, 0, 135, 20, color(2, 5, 16));
    c.text("1UP", 3, 1, red);
    c.number(score_, 3, 10, white);
    c.text("BEST", 77, 1, red);
    c.number(best > score_ ? best : score_, 77, 10, white);
    c.line(0, 19, 134, 19, color(29, 51, 78));

    for (unsigned i = 0; i < EnemyCount; ++i) {
      const Enemy& e = enemies_[i];
      if (e.flight == Flight::Pending || e.flight == Flight::Dead) continue;
      if (e.flight == Flight::Tractor && e.age >= 48 && e.age < 180 && !e.carrying) {
        const int x = e.x / Unit, top = e.y / Unit + 6;
        for (int y = top; y < 220; y += 3) {
          const int width = 3 + (y - top) * 14 / (220 - top);
          c.line(x - width, y, x + width, y, color(12, 56 + ((y + frame_) % 24), 58));
          c.pixel(x - width, y, cyan);
          c.pixel(x + width, y, cyan);
        }
      }
      drawEnemy(c, e, i);
      if (e.carrying) {
        int y = e.y / Unit + 12;
        if (captureRise_) y += int(captureRise_) * (213 - y) / 64;
        drawShip(c, e.x / Unit, y, red, false);
      }
    }
    for (unsigned i = 0; i < PlayerShotCount; ++i) if (playerShots_[i].active) {
      const Shot& s = playerShots_[i];
      c.rect(s.x / Unit, s.y / Unit - 3, 1, 5, white);
      c.pixel(s.x / Unit, s.y / Unit + 2, cyan);
    }
    for (unsigned i = 0; i < EnemyShotCount; ++i) if (enemyShots_[i].active) {
      const Shot& s = enemyShots_[i];
      c.rect(s.x / Unit, s.y / Unit - 1, 2, 4, red);
      c.pixel(s.x / Unit, s.y / Unit + 3, gold);
    }
    if (!gameOver() && !playerDelay_ && (!invulnerable_ || (frame_ & 4))) {
      drawShip(c, playerX() - (dual_ ? 5 : 0), PlayerY, white, true);
      if (dual_) drawShip(c, playerX() + 5, PlayerY, white, true);
    }
    if (rescueTimer_) {
      const int t = 64 - rescueTimer_;
      const int x = (rescueX_ * (64 - t) + (playerX() + 5) * t) / 64;
      const int y = (rescueY_ * (64 - t) + PlayerY * t) / 64;
      drawShip(c, x, y, white, true);
    }
    for (unsigned i = 0; i < EffectCount; ++i) if (effects_[i].life) {
      const Effect& e = effects_[i];
      const int radius = (18 - e.life) / 2 + 1;
      for (unsigned k = 0; k < 8; ++k) {
        const int dx = wave(k * 128) * radius / 256;
        const int dy = wave(k * 128 + 256) * radius / 256;
        c.pixel(e.x + dx, e.y + dy, e.life > 9 ? gold : red);
      }
      if (e.life > 13) c.rect(e.x - 1, e.y - 1, 3, 3, white);
    }
    c.rect(0, 226, 135, 14, color(2, 5, 16));
    for (unsigned i = 1; i < lives_ && i < 7; ++i) drawShip(c, 5 + (i - 1) * 9, 234, cyan, true);
    c.text("STAGE", 73, 228, gold);
    c.number(stage_, 111, 228, white);

    if (phase_ == Phase::Intro) {
      panel(c, bonus_ ? "CHALLENGING" : "READY", bonus_ ? 31 : 52, 116, cyan);
      c.text(bonus_ ? "HIT ALL 40" : "HOLD TO FIRE", bonus_ ? 38 : 31, 133, white);
    } else if (phase_ == Phase::Clear) {
      panel(c, "STAGE CLEAR", 34, 116, cyan);
    } else if (phase_ == Phase::BonusResult) {
      panel(c, bonusHits_ == EnemyCount ? "PERFECT" : "RESULT", bonusHits_ == EnemyCount ? 46 : 49, 104, gold);
      c.text("HITS", 39, 122, cyan);
      c.number(bonusHits_, 78, 122, white);
      c.text("BONUS", 31, 139, cyan);
      c.number(bonusAward_, 72, 139, white);
    } else if (phase_ == Phase::Over) {
      panel(c, "GAME OVER", 40, 111, red);
      c.text(phaseTimer_ ? "GOOD FLIGHT" : "TAP TO RETRY", phaseTimer_ ? 34 : 31, 130, white);
    } else if (playerDelay_ && lives_) {
      c.text("READY", 52, 158, cyan);
    }
    if (rescueTimer_) c.text("RESCUED", 46, 179, gold);
  }

 private:
  friend struct GalagaGameTestAccess;
  enum { Unit = 16, PlayerY = 213, EffectCount = 10 };
  struct Effect { int16_t x = 0, y = 0; uint8_t life = 0; };
  Enemy enemies_[EnemyCount];
  Shot playerShots_[PlayerShotCount], enemyShots_[EnemyShotCount];
  Effect effects_[EffectCount];
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, stageTick_ = 0, score_ = 0;
  uint32_t extraLifeAt_ = 20000, bonusAward_ = 0;
  uint16_t stage_ = 1, phaseTimer_ = 0, attackDelay_ = 0, tractorDelay_ = 0;
  int16_t playerX_ = 67 * Unit, rescueX_ = 0, rescueY_ = 0;
  int8_t captured_ = -1;
  uint8_t lives_ = 3, difficulty_ = 1, playerDelay_ = 0, invulnerable_ = 0;
  uint8_t shotDelay_ = 0, bonusHits_ = 0, captureHold_ = 0, captureRise_ = 0, rescueTimer_ = 0;
  bool dual_ = false, bonus_ = false;
  Phase phase_ = Phase::Intro;
  ArcadeCue cue_ = ArcadeCue::None;

  static uint16_t color(unsigned r, unsigned g, unsigned b) { return ArcadeCanvas::rgb(r, g, b); }
  static int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
  static int absolute(int value) { return value < 0 ? -value : value; }
  static int interpolate(int a, int b, unsigned t, unsigned duration) {
    return a + int((int32_t(b - a) * int(t)) / int(duration));
  }
  static uint32_t hash(uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; return n ^ (n >> 16);
  }
  uint32_t random() {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return random_;
  }
  static int wave(unsigned phase) {
    phase &= 1023;
    const int x = int(phase & 511);
    const int value = x * (512 - x) / 256;
    return phase < 512 ? value : -value;
  }
  void signal(ArcadeCue value) { if (uint8_t(value) > uint8_t(cue_)) cue_ = value; }
  void addScore(uint32_t value) {
    score_ = score_ > 99999999u - value ? 99999999u : score_ + value;
    if (score_ >= extraLifeAt_) {
      if (lives_ < 9) ++lives_;
      extraLifeAt_ += 70000;
      signal(ArcadeCue::Power);
    }
  }
  void movePlayer(int move) {
    const int half = dual_ ? 10 : 5;
    playerX_ = int16_t(clamp(playerX_ + clamp(move, -1024, 1024) * 44 / 1024,
                             half * Unit, (134 - half) * Unit));
  }
  void startStage() {
    phase_ = Phase::Intro;
    phaseTimer_ = 100;
    stageTick_ = 0;
    bonus_ = stage_ % 4 == 3;
    bonusHits_ = 0;
    bonusAward_ = 0;
    captured_ = -1;
    captureHold_ = captureRise_ = 0;
    attackDelay_ = 100;
    tractorDelay_ = 450;
    for (unsigned i = 0; i < EnemyCount; ++i) {
      enemies_[i] = Enemy();
      Enemy& e = enemies_[i];
      e.kind = i < 4 ? Kind::Boss : i < 20 ? Kind::Butterfly : Kind::Bee;
      e.hp = e.kind == Kind::Boss && !bonus_ ? 2 : 1;
      e.flight = Flight::Pending;
    }
    for (unsigned i = 0; i < PlayerShotCount; ++i) playerShots_[i] = Shot();
    for (unsigned i = 0; i < EnemyShotCount; ++i) enemyShots_[i] = Shot();
    for (unsigned i = 0; i < EffectCount; ++i) effects_[i] = Effect();
  }
  void finishStage() {
    if (rescueTimer_) return;
    phase_ = bonus_ ? Phase::BonusResult : Phase::Clear;
    phaseTimer_ = bonus_ ? 210 : 140;
    if (bonus_) {
      bonusAward_ = bonusHits_ == EnemyCount ? 10000 : uint32_t(bonusHits_) * 100;
      addScore(bonusAward_);
    }
    for (unsigned i = 0; i < EnemyShotCount; ++i) enemyShots_[i].active = false;
    signal(ArcadeCue::Win);
  }
  void home(unsigned index, int& x, int& y) const {
    if (index < 4) { x = 50 + int(index) * 11; y = 42; }
    else if (index < 20) { x = 28 + int((index - 4) % 8) * 11; y = 57 + int((index - 4) / 8) * 15; }
    else { x = 17 + int((index - 20) % 10) * 11; y = 87 + int((index - 20) / 10) * 15; }
    const int pulse = wave(stageTick_ * 2) * 2 / 256;
    x += wave(stageTick_ * 3) * 5 / 256 + (x - 67) * pulse / 40;
    y += pulse;
    x *= Unit;
    y *= Unit;
  }
  void fire() {
    unsigned free = 0, active = 0;
    for (unsigned i = 0; i < PlayerShotCount; ++i) { free += !playerShots_[i].active; active += playerShots_[i].active; }
    if (free < (dual_ ? 2u : 1u) || active >= (dual_ ? 4u : 2u)) return;
    unsigned emitted = 0;
    for (unsigned i = 0; i < PlayerShotCount; ++i) if (!playerShots_[i].active) {
      Shot& s = playerShots_[i];
      s.active = true;
      s.x = int16_t(playerX_ + (dual_ ? (emitted ? 5 : -5) * Unit : 0));
      s.y = (PlayerY - 7) * Unit;
      s.vx = 0; s.vy = -5 * Unit;
      if (++emitted == (dual_ ? 2u : 1u)) break;
    }
    shotDelay_ = 11;
    signal(ArcadeCue::Fire);
  }
  void enemyFire(const Enemy& e) {
    if (bonus_ || playerDelay_ || e.y > 185 * Unit || e.y < 25 * Unit) return;
    for (unsigned i = 0; i < EnemyShotCount; ++i) if (!enemyShots_[i].active) {
      Shot& s = enemyShots_[i];
      s.active = true; s.x = e.x; s.y = e.y + 5 * Unit;
      s.vy = int16_t(22 + difficulty_ * 3 + (stage_ > 10 ? 10 : stage_));
      const int distance = PlayerY * Unit - s.y;
      s.vx = int16_t(clamp(distance > 0 ? (playerX_ - s.x) * s.vy / distance : 0, -19, 19));
      return;
    }
  }
  void launch(Enemy& e, bool tractor) {
    e.flight = tractor ? Flight::Tractor : Flight::Dive;
    e.age = 0;
    e.originX = e.x; e.originY = e.y;
    e.targetX = int16_t(clamp(playerX_, 15 * Unit, 119 * Unit));
  }
  void updateEnemies() {
    if (captureRise_) --captureRise_;
    unsigned attacking = 0;
    for (unsigned i = 0; i < EnemyCount; ++i) {
      Enemy& e = enemies_[i];
      if (e.flash) --e.flash;
      const uint32_t spawnAt = bonus_ ? (i / 8) * 205 + (i % 8) * 10 : i * 8 + (i / 8) * 52;
      if (e.flight == Flight::Pending && stageTick_ >= spawnAt) { e.flight = Flight::Entrance; e.age = 0; }
      if (e.flight == Flight::Dead || e.flight == Flight::Pending) continue;
      int hx, hy; home(i, hx, hy);
      if (e.flight == Flight::Entrance) {
        const unsigned t = e.age++;
        const int side = ((i / 8) & 1) ? -1 : 1;
        if (bonus_) {
          e.x = int16_t((side == 1 ? -10 : 144) * Unit + side * int(t) * 15 +
                         side * wave(t * 7) * 20 * Unit / 256);
          e.y = int16_t((42 + int(i % 8) * 2) * Unit + int(t) * 10 + wave(t * 8) * 22 * Unit / 256);
          if (t > 190) e.flight = Flight::Dead;
        } else if (t < 48) {
          e.x = int16_t(interpolate((side == 1 ? -12 : 146) * Unit, 67 * Unit, t, 48));
          e.y = int16_t(interpolate(24 * Unit, 62 * Unit, t, 48));
        } else if (t < 108) {
          const unsigned angle = (t - 48) * 1024 / 60;
          e.x = int16_t(67 * Unit + side * wave(angle) * 44 * Unit / 256);
          e.y = int16_t(82 * Unit - wave(angle + 256) * 20 * Unit / 256);
        } else if (t < 156) {
          e.x = int16_t(interpolate(67 * Unit, hx, t - 108, 48));
          e.y = int16_t(interpolate(62 * Unit, hy, t - 108, 48));
        } else { e.flight = Flight::Formation; e.x = int16_t(hx); e.y = int16_t(hy); }
      } else if (e.flight == Flight::Formation) {
        e.x = int16_t(hx); e.y = int16_t(hy);
      } else if (e.flight == Flight::Dive) {
        ++attacking;
        const unsigned t = e.age++;
        const int side = (i & 1) ? 1 : -1;
        if (t < 32) {
          e.x = int16_t(e.originX + side * wave(t * 16) * 13 * Unit / 256);
          e.y = int16_t(e.originY - wave(t * 16) * 10 * Unit / 256 + int(t) * 6);
        } else {
          const unsigned d = t - 32;
          e.y = int16_t(e.originY + 32 * 6 + int(d) * (27 + (stage_ > 8 ? 8 : stage_)));
          const unsigned toward = d < 74 ? d : 74;
          e.x = int16_t(interpolate(e.originX, e.targetX, toward, 74) +
                           side * wave(d * 9) * 27 * Unit / 256);
        }
        if (t == 42 || (t == 72 && stage_ > 1)) enemyFire(e);
        if (e.y > 251 * Unit) {
          if (e.kind == Kind::Captive) e.flight = Flight::Dead;
          else { e.flight = Flight::Return; e.age = 0; e.originX = e.x; e.originY = -10 * Unit; }
        }
      } else if (e.flight == Flight::Return) {
        const unsigned t = ++e.age;
        e.x = int16_t(interpolate(e.originX, hx, t, 70));
        e.y = int16_t(interpolate(e.originY, hy, t, 70));
        if (t >= 70) { e.flight = Flight::Formation; e.age = 0; }
      } else if (e.flight == Flight::Tractor) {
        ++attacking;
        const unsigned t = e.age++;
        if (t < 48) {
          e.x = int16_t(interpolate(e.originX, e.targetX, t, 48));
          e.y = int16_t(interpolate(e.originY, 126 * Unit, t, 48));
        }
        if (t >= 48 && t < 180 && !e.carrying && !playerDelay_ && !dual_ && !invulnerable_ && lives_ > 1) {
          if (absolute(playerX_ - e.x) < 13 * Unit) {
            if (++captureHold_ >= 28) capture(i);
          } else captureHold_ = 0;
        }
        if (t >= 210) {
          e.flight = Flight::Return; e.age = 0; e.originX = e.x; e.originY = e.y;
          captureHold_ = 0;
        }
      }
      if (!bonus_ && !playerDelay_ && !invulnerable_ && e.flight != Flight::Dead &&
          absolute(e.y - PlayerY * Unit) < 7 * Unit &&
          absolute(e.x - playerX_) < (dual_ ? 13 : 8) * Unit) {
        if (e.carrying) captured_ = -1;
        explosion(e.x / Unit, e.y / Unit);
        e.flight = Flight::Dead;
        hurtPlayer(e.x);
      }
    }
    if (bonus_ || playerDelay_) return;
    if (tractorDelay_) --tractorDelay_;
    if (attackDelay_) { --attackDelay_; return; }
    attackDelay_ = uint16_t(clamp(112 - int(stage_) * 5 - int(difficulty_) * 12, 35, 112));
    if (stageTick_ < 225 || attacking >= unsigned(clamp(2 + stage_ / 3, 2, 5))) return;
    const unsigned start = random() % EnemyCount;
    for (unsigned n = 0; n < EnemyCount; ++n) {
      const unsigned i = (start + n) % EnemyCount;
      Enemy& e = enemies_[i];
      if (e.flight != Flight::Formation) continue;
      const bool tractor = e.kind == Kind::Boss && !e.carrying && captured_ < 0 && !dual_ && !rescueTimer_ &&
                           !tractorDelay_ && lives_ > 1;
      launch(e, tractor);
      if (tractor) tractorDelay_ = 650;
      break;
    }
  }
  void capture(unsigned index) {
    Enemy& e = enemies_[index];
    e.carrying = true;
    captured_ = int8_t(index);
    captureRise_ = 64;
    captureHold_ = 0;
    --lives_;
    playerDelay_ = 120;
    shotDelay_ = 0;
    signal(ArcadeCue::Death);
  }
  void updateShots() {
    for (unsigned i = 0; i < PlayerShotCount; ++i) {
      Shot& s = playerShots_[i];
      if (!s.active) continue;
      const int oldY = s.y;
      s.y = int16_t(s.y + s.vy);
      if (s.y < 20 * Unit) { s.active = false; continue; }
      for (unsigned j = 0; j < EnemyCount && s.active; ++j) {
        Enemy& e = enemies_[j];
        if (e.flight == Flight::Pending || e.flight == Flight::Dead) continue;
        if (e.carrying && !captureRise_ && absolute(s.x - e.x) <= 3 * Unit &&
            s.y - 3 * Unit <= e.y + 16 * Unit && oldY + 2 * Unit >= e.y + 8 * Unit) {
          e.carrying = false; captured_ = -1; s.active = false;
          explosion(e.x / Unit, e.y / Unit + 12); signal(ArcadeCue::Hit);
          break;
        }
        const int halfWidth = e.kind == Kind::Boss ? 5 : 4;
        if (absolute(s.x - e.x) <= halfWidth * Unit && s.y - 3 * Unit <= e.y + 4 * Unit && oldY + 2 * Unit >= e.y - 4 * Unit) {
          s.active = false;
          if (--e.hp) { e.flash = 9; signal(ArcadeCue::Hit); }
          else destroyEnemy(j);
        }
      }
    }
    for (unsigned i = 0; i < EnemyShotCount; ++i) {
      Shot& s = enemyShots_[i];
      if (!s.active) continue;
      const int oldY = s.y;
      s.x = int16_t(s.x + s.vx); s.y = int16_t(s.y + s.vy);
      if (s.x < -4 * Unit || s.x > 139 * Unit || s.y > 225 * Unit) { s.active = false; continue; }
      if (!playerDelay_ && !invulnerable_ && s.y + 3 * Unit >= (PlayerY - 5) * Unit &&
          oldY <= (PlayerY + 5) * Unit && absolute(s.x - playerX_) < (dual_ ? 10 : 4) * Unit) {
        s.active = false; hurtPlayer(s.x);
      }
    }
  }
  void destroyEnemy(unsigned index) {
    Enemy& e = enemies_[index];
    const bool attacking = e.flight == Flight::Dive || e.flight == Flight::Tractor || e.flight == Flight::Return;
    uint32_t points = e.kind == Kind::Boss ? (attacking ? 400 : 150) :
                      e.kind == Kind::Butterfly ? (attacking ? 160 : 80) : (attacking ? 100 : 50);
    if (bonus_) { ++bonusHits_; points = 0; }
    explosion(e.x / Unit, e.y / Unit);
    e.flight = Flight::Dead;
    if (e.carrying) {
      captured_ = -1;
      e.carrying = false;
      if (attacking) {
        rescueTimer_ = 64;
        rescueX_ = e.x / Unit; rescueY_ = e.y / Unit + 12;
        points = 1000;
      } else {
        e.kind = Kind::Captive; e.hp = 1; e.y += 12 * Unit;
        launch(e, false);
      }
    }
    addScore(points);
    signal(ArcadeCue::Hit);
  }
  void hurtPlayer(int hitX) {
    if (playerDelay_ || invulnerable_) return;
    explosion((dual_ ? playerX_ + (hitX < playerX_ ? -5 : 5) * Unit : playerX_) / Unit, PlayerY);
    if (dual_) {
      dual_ = false;
      playerX_ += hitX < playerX_ ? 5 * Unit : -5 * Unit;
      invulnerable_ = 100;
    } else {
      if (lives_) --lives_;
      playerDelay_ = 100;
      if (!lives_) { phase_ = Phase::Over; phaseTimer_ = 90; }
    }
    shotDelay_ = 0;
    for (unsigned i = 0; i < EnemyShotCount; ++i) enemyShots_[i].active = false;
    signal(ArcadeCue::Death);
  }
  void explosion(int x, int y) {
    unsigned slot = 0;
    for (unsigned i = 0; i < EffectCount; ++i) if (effects_[i].life < effects_[slot].life) slot = i;
    effects_[slot].x = int16_t(x); effects_[slot].y = int16_t(y); effects_[slot].life = 18;
  }
  void updateEffects() { for (unsigned i = 0; i < EffectCount; ++i) if (effects_[i].life) --effects_[i].life; }
  static void drawShip(ArcadeCanvas& c, int x, int y, uint16_t body, bool flame) {
    const uint16_t red = color(255, 75, 73), blue = color(63, 174, 244);
    c.rect(x, y - 5, 1, 9, body);
    c.rect(x - 1, y - 2, 3, 6, body);
    c.rect(x - 4, y + 1, 9, 2, body);
    c.rect(x - 4, y - 1, 1, 5, body);
    c.rect(x + 4, y - 1, 1, 5, body);
    c.pixel(x, y - 2, blue);
    c.pixel(x - 2, y + 2, red); c.pixel(x + 2, y + 2, red);
    if (flame) { c.pixel(x - 1, y + 5, red); c.pixel(x + 1, y + 5, red); }
  }
  void drawEnemy(ArcadeCanvas& c, const Enemy& e, unsigned index) const {
    const int x = e.x / Unit, y = e.y / Unit;
    const bool wings = ((frame_ / 9 + index) & 1) != 0;
    const uint16_t white = color(245, 250, 255), dark = color(2, 5, 16);
    if (e.kind == Kind::Captive) { drawShip(c, x, y, color(255, 83, 82), false); return; }
    if (e.kind == Kind::Boss) {
      const uint16_t body = e.flash ? white : e.hp == 1 ? color(114, 113, 255) : color(46, 226, 183);
      c.rect(x - 2, y - 3, 5, 7, body);
      c.rect(x - 5, y - 2, 2, wings ? 5 : 3, body);
      c.rect(x + 4, y - 2, 2, wings ? 5 : 3, body);
      c.line(x - 4, y - 3, x - 2, y + 1, body);
      c.line(x + 4, y - 3, x + 2, y + 1, body);
      c.pixel(x - 1, y, dark); c.pixel(x + 1, y, dark);
      c.rect(x - 1, y + 3, 3, 2, color(255, 110, 88));
      c.pixel(x - 2, y - 4, body); c.pixel(x + 2, y - 4, body);
    } else if (e.kind == Kind::Butterfly) {
      const uint16_t wing = e.flash ? white : color(244, 74, 104), body = color(100, 190, 255);
      c.rect(x, y - 3, 1, 7, body);
      c.line(x - 4, y - (wings ? 3 : 1), x - 1, y + 2, wing);
      c.line(x + 4, y - (wings ? 3 : 1), x + 1, y + 2, wing);
      c.rect(x - 4, y - 1, 3, 2, wing); c.rect(x + 2, y - 1, 3, 2, wing);
      c.pixel(x - 2, y + 3, wing); c.pixel(x + 2, y + 3, wing);
      c.pixel(x - 1, y - 3, white); c.pixel(x + 1, y - 3, white);
    } else {
      const uint16_t wing = e.flash ? white : color(255, 191, 51), blue = color(48, 143, 255);
      c.rect(x - 1, y - 2, 3, 5, wing);
      c.rect(x - 4, y - (wings ? 2 : 0), 3, 2, wing);
      c.rect(x + 2, y - (wings ? 2 : 0), 3, 2, wing);
      c.rect(x - 1, y, 3, 1, blue);
      c.pixel(x - 2, y + 3, blue); c.pixel(x + 2, y + 3, blue);
      c.pixel(x - 1, y - 3, white); c.pixel(x + 1, y - 3, white);
    }
  }
  static void panel(ArcadeCanvas& c, const char* text, int x, int y, uint16_t ink) {
    c.rect(17, y - 5, 101, 18, color(3, 10, 24));
    c.line(17, y - 5, 117, y - 5, color(32, 66, 92));
    c.line(17, y + 12, 117, y + 12, color(32, 66, 92));
    c.text(text, x, y, ink);
  }
};

}  // namespace flappy
