#pragma once

#include "arcade_support.h"
#include "clock_scene.h"
#include <stdint.h>

namespace flappy {

// The clock formation gets a readable pause before the automatic fighter
// attacks it. New squadrons fly in after the last enemy is destroyed.
class GalagaClock {
 public:
  enum { EnemyCount = 60, PlayerShots = 12, EnemyShots = 8, Unit = 16, PlayerY = 219 };
  enum class Phase : uint8_t { Entrance, Read, Battle, Reform };
  enum class Flight : uint8_t { Pending, Entrance, Formation, Dive, Return, Dead };
  struct Enemy {
    int16_t x = 0, y = 0;
    uint16_t age = 0;
    uint8_t cell = 0, hp = 1, flash = 0;
    Flight flight = Flight::Dead;
  };
  struct Shot { int16_t x = 0, y = 0; int8_t vx = 0, vy = 0; bool active = false; };

  void reset(uint32_t seed) {
    random_ = seed ? seed : 1;
    for (unsigned i = 0; i < 4; ++i) ClockScene::random(random_);
    seed_ = random_;
    frame_ = cycles_ = kills_ = shotsFired_ = 0;
    phaseFrame_ = 0; phase_ = Phase::Entrance;
    count_ = 0; initialized_ = false; playerX_ = 67 * Unit;
    target_ = 255; shotDelay_ = attackDelay_ = 0;
    for (Enemy& enemy : enemies_) enemy = Enemy();
    clearShots();
    for (Effect& effect : effects_) effect = Effect();
  }

  void step(const ClockTime& time) {
    const uint16_t key = uint16_t(time.minuteKey() | (time.twentyFourHour ? 0x8000 : 0));
    if (!initialized_ || key != minuteKey_) begin(time,key);
    ++frame_; ++phaseFrame_;
    for (Effect& effect : effects_) if (effect.life) --effect.life;
    updateEnemies();
    if (phase_ == Phase::Entrance) {
      bool settled = true;
      for (unsigned i = 0; i < count_; ++i)
        if (enemies_[i].flight != Flight::Formation) { settled = false; break; }
      if (settled) { phase_ = Phase::Read; phaseFrame_ = 0; }
      return;
    }
    if (phase_ == Phase::Read) {
      if (phaseFrame_ >= 210) {
        phase_ = Phase::Battle; phaseFrame_ = 0;
        attackDelay_ = uint8_t(65 + ClockScene::random(random_) % 32);
      }
      return;
    }
    if (phase_ == Phase::Reform) {
      if (phaseFrame_ >= 75) begin(time,key);
      return;
    }
    moveFighter();
    if (shotDelay_) --shotDelay_;
    if (!shotDelay_) fire();
    updateShots();
    if (!remaining()) {
      phase_ = Phase::Reform; phaseFrame_ = 0; clearShots();
    }
  }

  void draw(uint16_t* pixels, const ClockTime&) const {
    if (!pixels) return;
    ArcadeCanvas c(pixels);
    c.clear(color(1,3,12));
    for (unsigned i = 0; i < 54; ++i) {
      const uint32_t h = hash(seed_ + i * 317u);
      const int x = int(h % 135), y = int(((h >> 9) + frame_ / (i % 3 + 2)) % 240);
      const unsigned light = (frame_ / 32 + i) % 5 == 0 ? 150 : 65;
      c.pixel(x,y,i % 4 == 0 ? color(light,light / 2,light) : color(light,light,light + 20));
    }
    for (unsigned i = 0; i < count_; ++i) {
      const Enemy& enemy = enemies_[i];
      if (enemy.flight == Flight::Pending || enemy.flight == Flight::Dead) continue;
      drawEnemy(c,enemy,i);
    }
    if (phase_ == Phase::Read) {
      const int sway = wave(frame_ * 2) * 2 / 256;
      c.rect(66 + sway,101,3,2,color(120,170,220));
      c.rect(66 + sway,109,3,2,color(120,170,220));
    }
    for (const Shot& shot : playerShots_) if (shot.active) {
      c.rect(shot.x / Unit,shot.y / Unit - 3,1,5,color(230,244,255));
      c.pixel(shot.x / Unit,shot.y / Unit + 2,color(72,220,255));
    }
    for (const Shot& shot : enemyShots_) if (shot.active) {
      c.rect(shot.x / Unit,shot.y / Unit - 1,2,4,color(255,70,84));
      c.pixel(shot.x / Unit,shot.y / Unit + 3,color(255,208,64));
    }
    drawShip(c,playerX_ / Unit,PlayerY,color(230,244,255),true);
    for (const Effect& effect : effects_) if (effect.life) {
      const int radius = (18 - effect.life) / 2 + 1;
      for (unsigned i = 0; i < 8; ++i) {
        const int x = effect.x + wave(i * 128) * radius / 256;
        const int y = effect.y + wave(i * 128 + 256) * radius / 256;
        c.pixel(x,y,effect.life > 9 ? color(255,208,64) : color(255,70,84));
      }
      if (effect.life > 13) c.rect(effect.x - 1,effect.y - 1,3,3,color(245,250,255));
    }
  }

  Phase phase() const { return phase_; }
  uint16_t phaseFrame() const { return phaseFrame_; }
  uint8_t enemyCount() const { return count_; }
  const Enemy& enemy(unsigned i) const { return enemies_[i < EnemyCount ? i : 0]; }
  const Shot* playerShots() const { return playerShots_; }
  const Shot* enemyShots() const { return enemyShots_; }
  int playerX() const { return playerX_ / Unit; }
  uint32_t cycles() const { return cycles_; }
  uint32_t enemiesDestroyed() const { return kills_; }
  uint32_t shotsFired() const { return shotsFired_; }
  uint8_t remaining() const {
    uint8_t result = 0;
    for (unsigned i = 0; i < count_; ++i) result += enemies_[i].flight != Flight::Dead;
    return result;
  }

  static bool clockCell(const ClockTime& time, unsigned cell) {
    return cell < 60 && ClockScene::digitPixel(time.digit(cell / 15),cell % 3,(cell % 15) / 3);
  }
  void home(unsigned cell, int& x, int& y) const {
    const unsigned digit = cell / 15;
    x = 27 + int(digit % 2) * 52 + int(cell % 3) * 13 + wave(frame_ * 2) * 2 / 256;
    y = 35 + int(digit / 2) * 84 + int((cell % 15) / 3) * 13 + wave(frame_ * 3) / 256;
  }

 private:
  struct Effect { int16_t x = 0, y = 0; uint8_t life = 0; };
  Enemy enemies_[EnemyCount];
  Shot playerShots_[PlayerShots], enemyShots_[EnemyShots];
  Effect effects_[10];
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, cycles_ = 0, kills_ = 0, shotsFired_ = 0;
  uint16_t phaseFrame_ = 0, minuteKey_ = 0;
  int16_t playerX_ = 67 * Unit;
  uint8_t count_ = 0, pattern_ = 0, target_ = 255, shotDelay_ = 0, attackDelay_ = 0;
  Phase phase_ = Phase::Entrance;
  bool initialized_ = false;

  static uint16_t color(unsigned r, unsigned g, unsigned b) { return ArcadeCanvas::rgb(r,g,b); }
  static int absolute(int value) { return value < 0 ? -value : value; }
  static int clamp(int value,int low,int high) { return value < low ? low : value > high ? high : value; }
  static int wave(unsigned phase) {
    phase &= 1023;
    const int x = int(phase & 511), value = x * (512 - x) / 256;
    return phase < 512 ? value : -value;
  }
  static int interpolate(int a,int b,unsigned t,unsigned duration) {
    return a + int(int32_t(b-a) * int(t) / int(duration));
  }
  static uint32_t hash(uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du; value ^= value >> 15;
    value *= 0x846ca68bu; return value ^ (value >> 16);
  }
  void clearShots() {
    for (Shot& shot : playerShots_) shot = Shot();
    for (Shot& shot : enemyShots_) shot = Shot();
  }
  void begin(const ClockTime& time,uint16_t key) {
    initialized_ = true; minuteKey_ = key; ++cycles_;
    phase_ = Phase::Entrance; phaseFrame_ = 0; count_ = 0; target_ = 255;
    shotDelay_ = attackDelay_ = 0; clearShots();
    pattern_ = uint8_t(ClockScene::random(random_) % 4);
    for (unsigned cell = 0; cell < 60; ++cell) {
      if (!clockCell(time,cell)) continue;
      Enemy& enemy = enemies_[count_++]; enemy = Enemy();
      enemy.cell = uint8_t(cell); enemy.flight = Flight::Pending;
      enemy.hp = (cell % 15) / 3 == 0 ? 2 : 1;
    }
    for (unsigned i = count_; i < EnemyCount; ++i) enemies_[i] = Enemy();
  }
  void updateEnemies() {
    unsigned attackers = 0;
    for (unsigned i = 0; i < count_; ++i) {
      Enemy& enemy = enemies_[i];
      if (enemy.flash) --enemy.flash;
      if (enemy.flight == Flight::Dead) continue;
      if (enemy.flight == Flight::Pending) {
        if (phaseFrame_ < i * 2) continue;
        enemy.flight = Flight::Entrance; enemy.age = 0;
      }
      int hx,hy; home(enemy.cell,hx,hy);
      if (enemy.flight == Flight::Entrance || enemy.flight == Flight::Return) {
        const unsigned t = ++enemy.age;
        const int side = (i & 1) ? 1 : -1;
        const unsigned kind = enemy.flight == Flight::Return ? 1u : pattern_;
        const int startX = kind == 1 ? 67 : kind == 3 ? hx : side < 0 ? -12 : 146;
        const int startY = kind == 2 ? 188 : -12;
        if (t < 96) {
          const unsigned angle = t * 1024 / 96;
          const int amplitude = kind == 3 ? 18 : 36;
          const int dx = wave(angle) * amplitude / 256;
          const int dy = kind == 1 ? wave(angle * 2) * 16 / 256 : wave(angle + 256) * 12 / 256;
          enemy.x = int16_t((interpolate(startX,hx,t,96) + dx * side) * Unit);
          enemy.y = int16_t((interpolate(startY,hy,t,96) + dy * (96-int(t)) / 96) * Unit);
        } else { enemy.flight = Flight::Formation; enemy.x = int16_t(hx * Unit); enemy.y = int16_t(hy * Unit); }
      } else if (enemy.flight == Flight::Formation) {
        enemy.x = int16_t(hx * Unit); enemy.y = int16_t(hy * Unit);
      } else if (enemy.flight == Flight::Dive) {
        ++attackers;
        const unsigned t = ++enemy.age;
        const int side = i & 1 ? 1 : -1;
        enemy.x = int16_t((hx + wave(t * 8) * side * 27 / 256) * Unit);
        enemy.y = int16_t((hy + int(t) * (252-hy) / 120) * Unit);
        if (t == 28) enemyFire(enemy);
        if (t >= 120) { enemy.flight = Flight::Return; enemy.age = 0; }
      }
    }
    if (phase_ != Phase::Battle) return;
    if (attackDelay_) { --attackDelay_; return; }
    attackDelay_ = uint8_t(48 + ClockScene::random(random_) % 48);
    if (attackers >= 2 || !count_) return;
    const unsigned start = ClockScene::random(random_) % count_;
    for (unsigned order = 0; order < count_; ++order) {
      Enemy& enemy = enemies_[(start + order) % count_];
      if (enemy.flight == Flight::Formation) { enemy.flight = Flight::Dive; enemy.age = 0; break; }
    }
  }
  void moveFighter() {
    if (target_ >= count_ || enemies_[target_].flight == Flight::Dead ||
        enemies_[target_].flight == Flight::Pending || enemies_[target_].flight == Flight::Entrance ||
        enemies_[target_].flight == Flight::Return) {
      target_ = 255;
      int best = 32767;
      for (unsigned i = 0; i < count_; ++i) {
        const Enemy& enemy = enemies_[i];
        if (enemy.flight != Flight::Formation && enemy.flight != Flight::Dive) continue;
        if (enemy.y > (PlayerY - 8) * Unit) continue;
        const int cost = absolute(enemy.x-playerX_) + (PlayerY * Unit - enemy.y) / 8;
        if (cost < best) { best = cost; target_ = uint8_t(i); }
      }
    }
    if (target_ >= count_) return;
    int wanted = enemies_[target_].x;
    for (const Shot& shot : enemyShots_) if (shot.active && shot.y > 185 * Unit && absolute(shot.x-playerX_) < 7 * Unit)
      wanted += shot.x < playerX_ ? 14 * Unit : -14 * Unit;
    playerX_ = int16_t(clamp(playerX_ + clamp(wanted-playerX_,-44,44),7 * Unit,127 * Unit));
  }
  void fire() {
    if (target_ >= count_ || absolute(enemies_[target_].x-playerX_) > 5 * Unit) return;
    for (Shot& shot : playerShots_) if (!shot.active) {
      shot = Shot(); shot.active = true; shot.x = playerX_; shot.y = (PlayerY - 7) * Unit;
      shot.vy = -5 * Unit; shotDelay_ = 9; ++shotsFired_; return;
    }
  }
  void enemyFire(const Enemy& enemy) {
    if (enemy.y > 185 * Unit) return;
    for (Shot& shot : enemyShots_) if (!shot.active) {
      shot = Shot(); shot.active = true; shot.x = enemy.x; shot.y = enemy.y;
      shot.vy = 28;
      const int distance = PlayerY * Unit - shot.y;
      shot.vx = int8_t(clamp(distance > 0 ? (playerX_-shot.x) * shot.vy / distance : 0,-19,19));
      return;
    }
  }
  void updateShots() {
    for (Shot& shot : playerShots_) {
      if (!shot.active) continue;
      const int oldY = shot.y;
      shot.y = int16_t(shot.y + shot.vy);
      if (shot.y < -4 * Unit) { shot.active = false; continue; }
      for (unsigned i = 0; i < count_ && shot.active; ++i) {
        Enemy& enemy = enemies_[i];
        if (enemy.flight == Flight::Dead || enemy.flight == Flight::Pending) continue;
        if (absolute(shot.x-enemy.x) <= 5 * Unit && shot.y - 3 * Unit <= enemy.y + 4 * Unit &&
            oldY + 2 * Unit >= enemy.y - 4 * Unit) {
          shot.active = false;
          if (--enemy.hp) enemy.flash = 9;
          else { enemy.flight = Flight::Dead; ++kills_; explosion(enemy.x / Unit,enemy.y / Unit); }
        }
      }
    }
    for (Shot& shot : enemyShots_) {
      if (!shot.active) continue;
      shot.x = int16_t(shot.x + shot.vx); shot.y = int16_t(shot.y + shot.vy);
      if (shot.y > 235 * Unit || shot.x < -4 * Unit || shot.x > 139 * Unit) shot.active = false;
      if (shot.y > (PlayerY - 3) * Unit && absolute(shot.x-playerX_) < 4 * Unit) {
        // Clock play has no lives or game-over state.
        shot.active = false; explosion(shot.x / Unit,PlayerY + 6);
      }
    }
  }
  void explosion(int x,int y) {
    unsigned slot = 0;
    for (unsigned i = 1; i < 10; ++i) if (effects_[i].life < effects_[slot].life) slot = i;
    effects_[slot].x = int16_t(x); effects_[slot].y = int16_t(y); effects_[slot].life = 18;
  }

  // These are the same authored native-pixel shapes used by GalagaGame.
  static void drawShip(ArcadeCanvas& c,int x,int y,uint16_t body,bool flame) {
    const uint16_t red = color(255,75,73), blue = color(63,174,244);
    c.rect(x,y-5,1,9,body); c.rect(x-1,y-2,3,6,body); c.rect(x-4,y+1,9,2,body);
    c.rect(x-4,y-1,1,5,body); c.rect(x+4,y-1,1,5,body);
    c.pixel(x,y-2,blue); c.pixel(x-2,y+2,red); c.pixel(x+2,y+2,red);
    if (flame) { c.pixel(x-1,y+5,red); c.pixel(x+1,y+5,red); }
  }
  void drawEnemy(ArcadeCanvas& c,const Enemy& enemy,unsigned index) const {
    const int x = enemy.x / Unit, y = enemy.y / Unit;
    const bool wings = ((frame_ / 9 + index) & 1) != 0;
    const uint16_t white = color(245,250,255), dark = color(2,5,16);
    const unsigned row = (enemy.cell % 15) / 3;
    if (row == 0) {
      const uint16_t body = enemy.flash ? white : enemy.hp == 1 ? color(114,113,255) : color(46,226,183);
      c.rect(x-2,y-3,5,7,body); c.rect(x-5,y-2,2,wings ? 5 : 3,body); c.rect(x+4,y-2,2,wings ? 5 : 3,body);
      c.line(x-4,y-3,x-2,y+1,body); c.line(x+4,y-3,x+2,y+1,body);
      c.pixel(x-1,y,dark); c.pixel(x+1,y,dark); c.rect(x-1,y+3,3,2,color(255,110,88));
      c.pixel(x-2,y-4,body); c.pixel(x+2,y-4,body);
    } else if (row < 3) {
      const uint16_t wing = enemy.flash ? white : color(244,74,104), body = color(100,190,255);
      c.rect(x,y-3,1,7,body); c.line(x-4,y-(wings ? 3 : 1),x-1,y+2,wing);
      c.line(x+4,y-(wings ? 3 : 1),x+1,y+2,wing); c.rect(x-4,y-1,3,2,wing); c.rect(x+2,y-1,3,2,wing);
      c.pixel(x-2,y+3,wing); c.pixel(x+2,y+3,wing); c.pixel(x-1,y-3,white); c.pixel(x+1,y-3,white);
    } else {
      const uint16_t wing = enemy.flash ? white : color(255,191,51), blue = color(48,143,255);
      c.rect(x-1,y-2,3,5,wing); c.rect(x-4,y-(wings ? 2 : 0),3,2,wing); c.rect(x+2,y-(wings ? 2 : 0),3,2,wing);
      c.rect(x-1,y,3,1,blue); c.pixel(x-2,y+3,blue); c.pixel(x+2,y+3,blue);
      c.pixel(x-1,y-3,white); c.pixel(x+1,y-3,white);
    }
  }
};

}  // namespace flappy
