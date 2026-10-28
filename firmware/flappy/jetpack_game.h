#pragma once

#include <stdint.h>
#include "arcade_support.h"
#include "jetpack_art.h"
#ifdef M5FLAP_JETPACK_IMPORTED
#include "jetpack_imported.h"
static_assert(flappy::JetpackImported::MaxPatternPoints <= 512, "Imported coin group exceeds collection mask");
#endif

namespace flappy {

// Original artwork and a fixed 60 Hz simulation on the native landscape grid.
class JetpackGame {
 public:
  enum { Width = 240, Height = 135, Unit = 256, FlightUnit = 65536, PlayerX = 43,
         Ceiling = 22, Floor = 114, ZapCount = 6, CoinCount = 160, ScientistCount = 8,
         MissileCount = 2 };
  enum class Phase : uint8_t { Ready, Running, Crashing, Over, Spin };
  enum class Vehicle : uint8_t { None, Armor, Gravity, Hog, Mech, Bird, Teleporter, Dragon, SAM, Random };
  struct ImportedGroup { int32_t x = 0; int16_t pattern = -1; uint8_t step = 0, top = 0, collected[64] = {}; };
  enum class Power : uint8_t { None, Shield, Magnet, DoubleCoins, CashDash, Boost, RocketTime, Count };
  struct Bonus { int32_t x = 0; int16_t y = 0; Power kind = Power::None; bool active = false; };
  struct Token { int32_t x = 0; int16_t y = 0; uint8_t kind = 0; bool active = false; };
  struct Scientist { int32_t x = 0; int16_t y = 0, velocity = 0; uint8_t state = 0, age = 0; bool active = false; };
  struct Rotator { int32_t x = 0; int16_t y = 0; uint8_t angle = 0; bool active = false; };
  struct Laser { int16_t y = 0; uint16_t ticks = 0; bool active = false; };
  struct Zap {
    int32_t x = 0;
    int16_t y = 0, dx = 0, dy = 0;
    bool active = false;
  };
  struct Coin { int32_t x = 0; int16_t y = 0; bool active = false; };
  struct Missile {
    int32_t x = 0;
    int16_t y = 0;
    uint8_t warning = 0;
    bool active = false;
  };
  struct Pickup {
    int32_t x = 0;
    int16_t y = 0;
    Vehicle kind = Vehicle::Armor;
    bool active = false;
  };

  JetpackGame() { reset(); }
  void reset(uint32_t seed = 1, uint8_t difficulty = 1) {
    seed_ = seed ? seed : 1;
    random_ = seed_;
    difficulty_ = difficulty > 2 ? 2 : difficulty;
    nextDifficulty_ = difficulty_;
    phase_ = Phase::Ready;
    frame_ = 0;
    world_ = 0;
    coins_ = 0;
    y_ = Floor * FlightUnit;
    velocity_ = 0;
    distanceToHazard_ = 160 * Unit;
    distanceToCoins_ = 30 * Unit;
    distanceToVehicle_ = 420 * Unit;
    missileGap_ = false;
    crashTicks_ = 0;
    immune_ = 0;
    vehicleHint_ = 0;
    vehicle_ = Vehicle::None;
    gravityUp_ = false;
    armed_ = false;
    thrust_ = false;
    pickup_ = Pickup{};
    for (Zap& zap : zaps_) zap = Zap{};
    for (Coin& coin : coinField_) coin = Coin{};
    for (Missile& missile : missiles_) missile = Missile{};
    cue_ = ArcadeCue::None;
    bonus_ = Bonus{}; token_ = Token{}; laser_ = Laser{};
    for (ImportedGroup& group : importedGroups_) group = ImportedGroup{};
    for (Rotator& rotator : rotators_) rotator = Rotator{};
    for (Scientist& scientist : scientists_) scientist = Scientist{};
    for (unsigned i = 0; i < unsigned(Power::Count); ++i) powerTicks_[i] = 0;
    distanceToBonus_ = 350 * Unit; distanceToToken_ = 290 * Unit;
    distanceToScientist_ = 50 * Unit; distanceToLaser_ = 1500 * Unit;
    spinTokens_ = samLetters_ = spinTicks_ = reward_ = vehicleJumps_ = vehicleBoost_ = samHits_ = 0;
    scientistInteractions_ = 0; crashBounces_ = 0; deathSpeed_ = 0;
    vehicleTicks_ = 0; teleportTarget_ = 70 * FlightUnit; teleportUp_ = true;
    vehicleWasHeld_ = vehicleInputBlocked_ = false; blastUsed_ = revived_ = spinResult_ = false;
    rewardCoins_ = 0; fxAge_ = 32;
    powerEndCash_ = powerEndBoost_ = 0;
    coinPattern_ = -1; patternCursor_ = 0; patternX_ = 0;
  }

  void tick(const ArcadeInput& input) {
    ++frame_;
    if (fxAge_ < 32) ++fxAge_;
    if (!input.action && !input.pressed) armed_ = true;
    const bool press = input.pressed && armed_;
    if (press) armed_ = false;
    if (phase_ == Phase::Ready) {
      thrust_ = false;
      if (press) {
        phase_ = Phase::Running;
        thrust_ = input.action || input.pressed;
        velocity_ = 0;
        applyLoadout();
        signal(ArcadeCue::Fire);
      }
      return;
    }
    if (phase_ == Phase::Over) {
      thrust_ = false;
      if (press) {
        reset(seed_ + 0x9e3779b9u, nextDifficulty_);
        phase_ = Phase::Running;
        thrust_ = input.action || input.pressed;
        velocity_ = 0;
        applyLoadout();
        signal(ArcadeCue::Fire);
      }
      return;
    }
    if (phase_ == Phase::Spin) { tickSpin(press); return; }
    if (phase_ == Phase::Crashing) { tickCrash(press); return; }

    if (immune_) --immune_;
    if (vehicleHint_) --vehicleHint_;
    updatePowers();
    thrust_ = input.action || input.pressed;
    moveFlight(thrust_, press);

    const int32_t advance = speed();
    if (world_ < 0x70000000u) world_ += uint32_t(advance);
    updateObjects(advance);
    updateExtras(advance);
    spawnObjects(advance);
    spawnExtras(advance);
    collect();
    collectExtras();
    interactScientists();
    if (!immune_ && !safePower()) collide();
  }

  void setNextLoadout(uint8_t ride, uint8_t gadget, bool headstart) {
    nextRide_ = ride <= unsigned(Vehicle::Random) ? ride : 0;
    nextGadget_ = gadget <= 2 ? gadget : 0; nextHeadstart_ = headstart;
  }
  uint16_t powerTicks(Power power) const { return unsigned(power) < unsigned(Power::Count) ? powerTicks_[unsigned(power)] : 0; }
  uint8_t spinTokens() const { return spinTokens_; }
  uint8_t samLetters() const { return samLetters_; }
  uint8_t vehicleHits() const { return samHits_; }
  uint16_t scientistInteractions() const { return scientistInteractions_; }
  uint8_t crashBounces() const { return crashBounces_; }
  int teleportTargetY() const { return int(teleportTarget_ / FlightUnit); }
  const Laser& laser() const { return laser_; }
  const Scientist& scientist(unsigned index) const { return scientists_[index < ScientistCount ? index : 0]; }
  void setNextDifficulty(uint8_t difficulty) { nextDifficulty_ = difficulty > 2 ? 2 : difficulty; }
  uint32_t score() const { return distance(); }
  uint32_t distance() const { return uint32_t(uint64_t(world_) * 10u / (Unit * 69u)); }
  uint32_t coins() const { return coins_; }
  bool gameOver() const { return phase_ == Phase::Over; }
  Phase phase() const { return phase_; }
  Vehicle vehicle() const { return vehicle_; }
  bool gravityUp() const { return gravityUp_; }
  int playerY() const { return int(y_ / FlightUnit); }
  int32_t speed() const {
    const int difficultyPercent = difficulty_ == 0 ? 85 : difficulty_ == 2 ? 115 : 100;
    if (powerTicks_[unsigned(Power::Boost)]) return 3271;
    return divideNearest(int64_t(normalNativeVelocity()) * 23 * Unit * difficultyPercent,
                         int64_t(60) * 60 * FlightUnit * 100);
  }
  const Zap& zap(unsigned index) const { return zaps_[index < ZapCount ? index : 0]; }
  const Missile& missile(unsigned index) const { return missiles_[index < MissileCount ? index : 0]; }
  const Pickup& pickup() const { return pickup_; }
  ArcadeCue takeCue() { const ArcadeCue result = cue_; cue_ = ArcadeCue::None; return result; }

  void draw(uint16_t* pixels, uint32_t best = 0) const {
    if (!pixels) return;
    Canvas c(pixels);
    drawLab(c);
    for (const Scientist& scientist : scientists_) if (scientist.active)
      JetpackArt::drawScientist(c, int(scientist.x / Unit), int(scientist.y / Unit), frame_, scientist.state > 2 ? 1 : scientist.state);
    drawExtras(c);
    for (const Coin& coin : coinField_) if (coin.active) drawCoin(c, int(coin.x / Unit), coin.y);
    if (pickup_.active) drawPickup(c);
    for (const Zap& zap : zaps_) if (zap.active) drawZap(c, zap);
    for (const Missile& missile : missiles_) if (missile.active) drawMissile(c, missile);
    drawPlayer(c);
    if (fxAge_ < 32) JetpackArt::drawExplosion(c, fxX_, fxY_, fxAge_, fxKind_);
    c.rect(0, 0, Width, 15, rgb(16, 24, 40));
    c.rect(0, 14, Width, 1, rgb(64, 88, 104));
    c.text("M", 4, 1, rgb(80, 184, 208));
    c.number(distance(), 16, 1, rgb(224, 240, 216));
    if (vehicleHint_ && phase_ == Phase::Running) {
      c.text(vehicleHint(), 91, 1, rgb(248, 216, 80));
    } else {
      drawCoin(c, 86, 7);
      c.number(coins_, 94, 1, rgb(248, 216, 80));
      c.text("BEST", 159, 1, rgb(80, 184, 208));
      c.number(best > score() ? best : score(), 189, 1, rgb(224, 240, 216));
    }
    if (phase_ == Phase::Ready) {
      panel(c, "JETPACK JOYRIDE", "HOLD TO FLY", "TAP TO START");
    } else if (phase_ == Phase::Spin) {
      panel(c, "FINAL SPIN", spinResult_ ? (reward_ == 0 ? "SECOND CHANCE!" : reward_ == 2 ? "MEGA BLAST!" : reward_ == 3 ? "500M HEADSTART!" : reward_ == 4 ? "DOUBLE COINS!" : "BONUS COINS!") : "SPINNING...",
          spinResult_ ? "NICE ONE!" : "TAP TO REVEAL");
      c.number(spinResult_ ? rewardCoins_ : spinTokens_, 108, 95, rgb(248, 216, 80));
    } else if (phase_ == Phase::Over) {
      panel(c, "RUN COMPLETE", "RELEASE, THEN TAP", "TO TRY AGAIN");
      c.text("METERS", 71, 96, rgb(248, 216, 80));
      c.number(score(), 113, 96, rgb(224, 240, 216));
    }
    if (phase_ == Phase::Running) {
      if (powerTicks_[unsigned(Power::Shield)] && (frame_ & 8)) {
        c.line(PlayerX - 12, playerY() - 9, PlayerX + 10, playerY() - 9, rgb(112, 213, 205));
        c.line(PlayerX - 12, playerY() + 9, PlayerX + 10, playerY() + 9, rgb(112, 213, 205));
      }
      char status[20];
      snprintf(status, sizeof(status), "%s%s%s T%u %c%c%c", powerTicks_[unsigned(Power::Magnet)] ? "M " : "",
          powerTicks_[unsigned(Power::DoubleCoins)] ? "2X " : "", safePower() ? "GO " : "", unsigned(spinTokens_),
          samLetters_ & 1 ? 'S' : '.', samLetters_ & 2 ? 'A' : '.', samLetters_ & 4 ? 'M' : '.');
      c.text(status, 3, 123, rgb(255, 224, 113));
    }
  }

 private:
  friend struct JetpackTestAccess;
  class Canvas {
   public:
    explicit Canvas(uint16_t* pixels) : pixels_(pixels) {}
    void pixel(int x, int y, uint16_t color) {
      if (unsigned(x) < Width && unsigned(y) < Height) pixels_[y * Width + x] = color;
    }
    void rect(int x, int y, int width, int height, uint16_t color) {
      if (width <= 0 || height <= 0) return;
      const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
      const int right = x + width > Width ? Width : x + width;
      const int bottom = y + height > Height ? Height : y + height;
      for (int row = top; row < bottom; ++row)
        for (int col = left; col < right; ++col) pixels_[row * Width + col] = color;
    }
    void line(int x0, int y0, int x1, int y1, uint16_t color) {
      const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
      const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
      const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
      }
    }
    void text(const char* text, int x, int y, uint16_t color) {
      MenuFont::draw(pixels_, Width, Height, text, x, y, color, 1, 40);
    }
    void number(uint32_t value, int x, int y, uint16_t color) {
      char text[12];
      snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
      this->text(text, x, y, color);
    }
   private:
    uint16_t* pixels_;
  };

  static uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
  }
  static int32_t bounded(int32_t value, int32_t low, int32_t high) {
    return value < low ? low : value > high ? high : value;
  }
  static int32_t divideNearest(int64_t value, int64_t divisor) {
    return int32_t(value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor);
  }
  int32_t normalNativeVelocity() const {
    const uint32_t meters = distance() < 2250 ? distance() : 2250;
    return 175 * FlightUnit + int32_t(int64_t(305) * FlightUnit * meters / 2250);
  }
  int32_t flightFactor() const {
    return FlightUnit + (normalNativeVelocity() - 175 * FlightUnit) / 350;
  }
  uint32_t random() {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return random_;
  }
  void signal(ArcadeCue cue) {
    if (unsigned(cue) > unsigned(cue_)) cue_ = cue;
  }
  bool missilePresent() const {
    for (const Missile& missile : missiles_) if (missile.active) return true;
    return false;
  }
  bool anyZap() const { for (const Zap& zap : zaps_) if (zap.active) return true; return false; }
  bool zapNearPlayer() const {
    for (const Zap& zap : zaps_)
      if (zap.active && zap.x > 20 * Unit && zap.x < 255 * Unit) return true;
    return false;
  }
  void updateObjects(int32_t advance) {
    for (Zap& zap : zaps_) if (zap.active) {
      zap.x -= advance;
      if (zap.x + int32_t(zap.dx) * Unit < -8 * Unit) zap.active = false;
    }
    for (Coin& coin : coinField_) if (coin.active) {
      coin.x -= advance;
      if (coin.x < -5 * Unit) coin.active = false;
    }
    if (pickup_.active) {
      pickup_.x -= advance;
      if (pickup_.x < -12 * Unit) pickup_.active = false;
    }
    for (Missile& missile : missiles_) if (missile.active) {
      if (missile.warning) {
        if (powerTicks_[unsigned(Power::RocketTime)] && (frame_ & 1)) continue;
        if (missile.warning > 30 && vehicle_ != Vehicle::SAM) missile.y = int16_t(playerY());
        if (!--missile.warning) missile.x = 254 * Unit;
      } else {
        if (powerTicks_[unsigned(Power::RocketTime)] && (frame_ & 1)) continue;
        missile.x -= 1540 + advance / 4;
        if (vehicle_ == Vehicle::SAM) {
          const bool armOverlap = missile.x <= (PlayerX + 29) * Unit && missile.x >= (PlayerX + 5) * Unit &&
              missile.y >= playerY() - 9 && missile.y <= playerY() + 9;
          if (armOverlap) { addCoins(3); signal(ArcadeCue::Hit); missile.active = false; }
          else if (missile.x < (PlayerX + 6) * Unit) { if (!immune_) hit(); missile.active = false; }
        }
        if (missile.x < -16 * Unit) missile.active = false;
      }
    }
  }
  void spawnObjects(int32_t advance) {
    distanceToHazard_ -= advance;
    distanceToCoins_ -= advance;
    distanceToVehicle_ -= advance;
    if (distanceToHazard_ <= 0 && !missilePresent() && !laser_.active && !safePower() && vehicle_ != Vehicle::SAM) {
      if (missileGap_) {
        if (zapNearPlayer()) return;
        missileGap_ = false;
        for (Missile& missile : missiles_) if (!missile.active) {
          missile.active = true;
          missile.warning = 80;
          missile.x = 254 * Unit;
          missile.y = int16_t(playerY());
          break;
        }
        distanceToHazard_ = 200 * Unit;
      } else {
        if (distance() > 200 && random() % 6 == 0) {
          for (Rotator& rotator : rotators_) if (!rotator.active) {
            rotator.active = true; rotator.x = 265 * Unit; rotator.y = int16_t(48 + random() % 42);
            rotator.angle = uint8_t(random() % 112); break;
          }
        } else spawnZap();
        const int reactionGap = int((speed() * 70 + Unit - 1) / Unit) + 32;
        const int minimumGap = reactionGap < 108 ? 108 : reactionGap;
        distanceToHazard_ = int32_t(minimumGap + int(random() % 73)) * Unit;
        if (distance() >= 80 && random() % 5 == 0) {
          missileGap_ = true;
          distanceToHazard_ = 260 * Unit;
        }
      }
    }
    if (distanceToCoins_ <= 0) {
      spawnCoins();
      distanceToCoins_ = int32_t(180 + random() % 110) * Unit;
    }
    if (distanceToVehicle_ <= 0 && !pickup_.active && vehicle_ == Vehicle::None) {
      pickup_.active = true;
      pickup_.x = 264 * Unit;
      pickup_.y = int16_t(43 + random() % 53);
      pickup_.kind = Vehicle(2 + random() % 6);
      distanceToVehicle_ = int32_t(950 + random() % 500) * Unit;
    }
  }
  void spawnZap() {
    for (Zap& zap : zaps_) if (!zap.active) {
      zap.active = true;
      zap.x = 258 * Unit;
      const unsigned kind = random() % 8;
      if (kind <= 2) {
        zap.y = kind == 0 ? Ceiling : kind == 1 ? Floor : int16_t(47 + random() % 42);
        zap.dx = 32; zap.dy = 0;
      } else if (kind <= 4) {
        zap.dy = int16_t(36 + random() % 12);
        zap.y = kind == 3 ? Ceiling : int16_t(Floor - zap.dy);
        zap.dx = 0;
      } else if (kind <= 6) {
        zap.y = kind == 5 ? Ceiling : Floor;
        zap.dx = 32; zap.dy = kind == 5 ? 39 : -39;
      } else {
        zap.y = int16_t(49 + random() % 19);
        zap.dx = 32; zap.dy = (random() & 1) ? 27 : -27;
      }
      return;
    }
  }
  void putCoin(int x, int y) {
    for (Coin& coin : coinField_) if (!coin.active) {
      coin.x = x * Unit; coin.y = int16_t(bounded(y, 27, 110)); coin.active = true; return;
    }
  }
  void spawnCoins() {
    if (startImported()) return;
    const int cy = int(43 + random() % 42);
    const unsigned pattern = random() % 8;
    if (pattern == 0) {
      for (int i = 0; i < 16; ++i) putCoin(250 + i * 10, cy);
    } else if (pattern == 1 || pattern == 2) {
      for (int i = 0; i < 17; ++i) putCoin(250 + i * 9, cy + (i < 9 ? i : 16 - i) * (pattern == 1 ? 4 : -4));
    } else if (pattern == 3) {
      for (int row = 0; row < 3; ++row) for (int col = 0; col < 10; ++col) putCoin(250 + col * 10, 40 + row * 14);
    } else if (pattern == 4) {
      static const int8_t wave[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
      for (int i = 0; i < 20; ++i) putCoin(250 + i * 9, cy + wave[i & 15]);
    } else if (pattern == 5) {
      for (int i = 0; i < 5; ++i) for (int side = -1; side <= 1; side += 2) {
        putCoin(250 + i * 10, cy + side * i * 5); putCoin(300 + i * 10, cy + side * (5 - i) * 5);
      }
    } else if (pattern == 6) {
      for (int i = 0; i < 18; ++i) { putCoin(250 + i * 9, 35 + i / 3 * 10); putCoin(250 + i * 9, 46 + i / 3 * 10); }
    } else {
      for (int i = 0; i < 6; ++i) { putCoin(250 + i * 10, cy); putCoin(290 + i * 8, cy - i * 5); putCoin(290 + i * 8, cy + i * 5); }
    }
  }
  void collect() {
    for (Coin& coin : coinField_) if (coin.active) {
      const int dx = int(coin.x / Unit) - PlayerX, dy = coin.y - playerY();
      if (dx * dx + dy * dy <= 10 * 10) {
        coin.active = false;
        addCoins(1);
        signal(ArcadeCue::Power);
      }
    }
    if (pickup_.active) {
      const int dx = int(pickup_.x / Unit) - PlayerX, dy = pickup_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) {
        const Vehicle kind = pickup_.kind;
        pickup_.active = false;
        equip(kind);
      }
    }
  }
  static bool nearSegment(int px, int py, int x, int y, int dx, int dy, int radius) {
    const int64_t ox = px - x, oy = py - y;
    const int64_t length = int64_t(dx) * dx + int64_t(dy) * dy;
    const int64_t projection = ox * dx + oy * dy;
    const int64_t rr = int64_t(radius) * radius;
    if (projection <= 0) return ox * ox + oy * oy <= rr;
    if (projection >= length) {
      const int64_t ex = ox - dx, ey = oy - dy;
      return ex * ex + ey * ey <= rr;
    }
    const int64_t cross = ox * dy - oy * dx;
    return cross * cross <= rr * length;
  }
  bool hitsBodySegment(int x, int y, int dx, int dy) const {
    const int py = playerY();
    switch (vehicle_) {
      case Vehicle::Hog: return nearSegment(PlayerX - 5, py + 1, x, y, dx, dy, 5) || nearSegment(PlayerX + 5, py + 1, x, y, dx, dy, 5);
      case Vehicle::Mech: return nearSegment(PlayerX, py - 5, x, y, dx, dy, 5) || nearSegment(PlayerX, py + 1, x, y, dx, dy, 5);
      case Vehicle::Bird: return nearSegment(PlayerX, py - 2, x, y, dx, dy, 7);
      case Vehicle::Teleporter: return nearSegment(PlayerX, py - 2, x, y, dx, dy, 7);
      case Vehicle::Dragon: return nearSegment(PlayerX + 9, py - 3, x, y, dx, dy, 7) || nearSegment(PlayerX, py, x, y, dx, dy, 6);
      default: return nearSegment(PlayerX, py, x, y, dx, dy, 6);
    }
  }
  void collide() {
    if (vehicle_ == Vehicle::SAM) return;
    if (laser_.active && laser_.ticks <= 40 && hitsBodySegment(0, laser_.y, Width - 1, 0)) { hit(); return; }
    for (Rotator& rotator : rotators_) if (rotator.active) {
      int x, y, dx, dy; rotatorEnds(rotator, x, y, dx, dy);
      if (hitsBodySegment(x, y, dx, dy)) { rotator.active = false; hit(); return; }
    }
    for (Zap& zap : zaps_) if (zap.active &&
        hitsBodySegment(int(zap.x / Unit), zap.y, zap.dx, zap.dy)) {
      zap.active = false;
      hit(); return;
    }
    for (Missile& missile : missiles_) if (missile.active && !missile.warning &&
        hitsBodySegment(int(missile.x / Unit) - 7, missile.y, 13, 0)) {
      missile.active = false;
      hit(); return;
    }
  }
  void hit() {
    if (powerTicks_[unsigned(Power::Shield)]) {
      powerTicks_[unsigned(Power::Shield)] = 0; immune_ = 90; burst(1); signal(ArcadeCue::Power); return;
    }
    if (vehicle_ == Vehicle::SAM && samHits_ > 1) { --samHits_; immune_ = 30; signal(ArcadeCue::Hit); return; }
    if (vehicle_ != Vehicle::None) {
      burst(2);
      vehicle_ = Vehicle::None;
      vehicleHint_ = 0;
      gravityUp_ = false;
      velocity_ = -200 * 256;
      immune_ = 120;
      signal(ArcadeCue::Power);
    } else {
      phase_ = Phase::Crashing;
      crashTicks_ = 150;
      deathSpeed_ = speed();
      crashBounces_ = 0;
      velocity_ = -450 * 256;
      armed_ = false;
      cue_ = ArcadeCue::Death;
    }
  }

  const char* vehicleHint() const {
    switch (vehicle_) {
      case Vehicle::Gravity: return "GRAVITY: TAP TO FLIP";
      case Vehicle::Hog: return "HOG: TAP + HOLD JUMP";
      case Vehicle::Mech: return "MECH: HOLD TO BOOST";
      case Vehicle::Bird: return "BIRD: TAP TO FLAP";
      case Vehicle::Teleporter: return "TELEPORTER: TAP TO GO";
      case Vehicle::Dragon: return "DRAGON: HOLD TO DIVE";
      case Vehicle::SAM: return "SAM: HOLD ARM UP";
      default: return "ARMOR: HOLD TO FLY";
    }
  }
  static int32_t scaledNative(int32_t hundredths) {
    return divideNearest(int64_t(hundredths) * 23 * FlightUnit, 6000);
  }
  bool safePower() const { return powerTicks_[unsigned(Power::CashDash)] || powerTicks_[unsigned(Power::Boost)]; }
  void clearHazards() {
    for (Zap& zap : zaps_) zap.active = false;
    for (Missile& missile : missiles_) missile.active = false;
    for (Rotator& rotator : rotators_) rotator.active = false;
    laser_.active = false;
  }
  void activatePower(Power power) {
    const unsigned index = unsigned(power);
    if (!index || index >= unsigned(Power::Count)) return;
    const uint16_t duration = power == Power::RocketTime ? 420 : power == Power::Shield ? 1800 : 1200;
    if (powerTicks_[index] != UINT16_MAX) powerTicks_[index] = duration;
    if (power == Power::CashDash) {
      powerTicks_[index] = UINT16_MAX; powerEndCash_ = distance() + 60; clearHazards();
      for (Coin& coin : coinField_) coin.active = false;
      for (ImportedGroup& group : importedGroups_) group = ImportedGroup{};
      if (startImported()) {
        for (ImportedGroup& group : importedGroups_) if (group.pattern >= 0) group.x = (PlayerX + 18) * Unit;
      } else {
        for (int row = -1; row <= 1; ++row) for (int col = 0; col < 24; ++col)
          putCoin(PlayerX + 14 + col * 8, playerY() + row * 9);
      }
      distanceToCoins_ = 70 * Unit;
    }
    if (power == Power::Boost) { powerTicks_[index] = UINT16_MAX; powerEndBoost_ = distance() + 100; clearHazards(); }
    signal(ArcadeCue::Win);
  }
  void burst(uint8_t kind) { fxAge_ = 0; fxKind_ = kind; fxX_ = PlayerX; fxY_ = int16_t(playerY()); }
  void equip(Vehicle vehicle) {
    burst(2);
    vehicle_ = vehicle;
    velocity_ = 0; gravityUp_ = false; vehicleJumps_ = 10; vehicleBoost_ = 0;
    vehicleTicks_ = 0; vehicleWasHeld_ = false; vehicleInputBlocked_ = true;
    teleportTarget_ = y_; teleportUp_ = true;
    if (vehicle == Vehicle::Hog || vehicle == Vehicle::Mech) y_ = Floor * FlightUnit;
    if (vehicle == Vehicle::SAM) { samHits_ = 3; vehicleTicks_ = 1800; y_ = 70 * FlightUnit; clearHazards(); }
    clearHazards();
    if (distanceToHazard_ < 170 * Unit) distanceToHazard_ = 170 * Unit;
    if (distanceToLaser_ < 600 * Unit) distanceToLaser_ = 600 * Unit;
    vehicleHint_ = 180; immune_ = 90; armed_ = false;
    signal(ArcadeCue::Win);
  }
  void applyLoadout() {
    if (nextRide_) {
      Vehicle ride = Vehicle(nextRide_);
      if (ride == Vehicle::Random) ride = Vehicle(2 + random() % 7);
      equip(ride);
    }
    if (nextGadget_ == 1) powerTicks_[unsigned(Power::Magnet)] = UINT16_MAX;
    if (nextGadget_ == 2) powerTicks_[unsigned(Power::DoubleCoins)] = UINT16_MAX;
    if (pendingRewards_ & 2) powerTicks_[unsigned(Power::DoubleCoins)] = UINT16_MAX;
    if (nextHeadstart_ || (pendingRewards_ & 1)) { activatePower(Power::Boost); powerEndBoost_ = distance() + 500; }
    pendingRewards_ = 0;
  }
  void updatePowers() {
    for (unsigned i = 1; i < unsigned(Power::Count); ++i)
      if (powerTicks_[i] && powerTicks_[i] != UINT16_MAX) --powerTicks_[i];
    if (powerTicks_[unsigned(Power::CashDash)] && distance() >= powerEndCash_) powerTicks_[unsigned(Power::CashDash)] = 0;
    if (powerTicks_[unsigned(Power::Boost)] && distance() >= powerEndBoost_) powerTicks_[unsigned(Power::Boost)] = 0;
    if (safePower()) clearHazards();
    if (vehicle_ == Vehicle::SAM && vehicleTicks_ && !--vehicleTicks_) {
      vehicle_ = Vehicle::None; immune_ = 120; clearHazards();
    }
  }
  void moveFlight(bool held, bool press) {
    if (vehicleInputBlocked_) {
      if (!held && !press) vehicleInputBlocked_ = false;
      held = press = false;
    }
    if (vehicle_ == Vehicle::Gravity) {
      if (press) { gravityUp_ = !gravityUp_; signal(ArcadeCue::Fire); }
      const int32_t acceleration = divideNearest(int64_t(FlightUnit) * (gravityUp_ ? -299 : 299), 3000);
      velocity_ = divideNearest(int64_t(velocity_ + acceleration) * 98, 100);
    } else if (vehicle_ == Vehicle::Hog || vehicle_ == Vehicle::Mech || vehicle_ == Vehicle::Bird) {
      const bool grounded = y_ >= Floor * FlightUnit;
      if (grounded) vehicleJumps_ = vehicle_ == Vehicle::Bird ? 10 : 1;
      if (press && (vehicleJumps_ || vehicle_ == Vehicle::Bird)) {
        velocity_ = -scaledNative(vehicle_ == Vehicle::Hog ? 650 : vehicle_ == Vehicle::Mech ? 600 : 500);
        if (vehicleJumps_) --vehicleJumps_;
        vehicleBoost_ = vehicle_ == Vehicle::Hog ? 12 : vehicle_ == Vehicle::Mech ? 20 : 0;
        signal(ArcadeCue::Fire);
      }
      const int gravity = vehicle_ == Vehicle::Hog ? 30 : vehicle_ == Vehicle::Mech ? 36 : 20;
      int boost = 0;
      if (held && vehicleBoost_) boost = vehicle_ == Vehicle::Hog ? 60 : 45;
      else if (held && vehicle_ == Vehicle::Mech && !grounded) boost = 29;
      if (vehicleBoost_) --vehicleBoost_;
      velocity_ = divideNearest(int64_t(velocity_ + scaledNative(gravity - boost)) * 98, 100);
    } else if (vehicle_ == Vehicle::Teleporter) {
      velocity_ = 0;
      teleportTarget_ += (teleportUp_ ? -2 : 2) * FlightUnit;
      if (teleportTarget_ < 28 * FlightUnit) { teleportTarget_ = 28 * FlightUnit; teleportUp_ = false; }
      if (teleportTarget_ > Floor * FlightUnit) { teleportTarget_ = Floor * FlightUnit; teleportUp_ = true; }
      if (press) { y_ = teleportTarget_; immune_ = 12; signal(ArcadeCue::Power); }
    } else if (vehicle_ == Vehicle::Dragon) {
      velocity_ += scaledNative(held ? 20 : -20);
      velocity_ = bounded(velocity_, -scaledNative(550), scaledNative(550));
    } else if (vehicle_ == Vehicle::SAM) {
      velocity_ = (held ? -2 : 2) * FlightUnit;
    } else {
      if (press) signal(ArcadeCue::Fire);
      // Net thrust and gravity retain the 240-unit center travel in 92 pixels.
      const int32_t acceleration = divideNearest(int64_t(flightFactor()) * (held ? -161 : 115), 2000);
      velocity_ = divideNearest(int64_t(velocity_ + acceleration) * 98, 100);
    }
    vehicleWasHeld_ = held;
    y_ += velocity_;
    const int top = unsigned(vehicle_) >= unsigned(Vehicle::Hog) ? 28 : Ceiling;
    if (y_ < top * FlightUnit) { y_ = top * FlightUnit; velocity_ = 0; }
    if (y_ > Floor * FlightUnit) { y_ = Floor * FlightUnit; velocity_ = 0; }
  }
  void addCoins(uint32_t count) {
    if (powerTicks_[unsigned(Power::DoubleCoins)]) count *= 2;
    coins_ = coins_ + count > 999999 ? 999999 : coins_ + count;
  }
  void updateExtras(int32_t advance) {
    if (bonus_.active) { bonus_.x -= advance; if (bonus_.x < -12 * Unit) bonus_.active = false; }
    if (token_.active) { token_.x -= advance; if (token_.x < -12 * Unit) token_.active = false; }
    for (Rotator& rotator : rotators_) if (rotator.active) {
      rotator.x -= advance; rotator.angle = uint8_t((unsigned(rotator.angle) + 1) % 112);
      if (rotator.x < -35 * Unit) rotator.active = false;
    }
    if (laser_.active) {
      if (laser_.ticks > 80) laser_.y = int16_t(playerY());
      if (laser_.ticks && !--laser_.ticks) laser_.active = false;
    }
    for (Scientist& scientist : scientists_) if (scientist.active) {
      scientist.x -= advance;
      if (scientist.age) --scientist.age;
      if (scientist.state == 2) {
        scientist.velocity = int16_t(bounded(scientist.velocity + 20, -1200, 1200));
        scientist.y = int16_t(scientist.y + scientist.velocity);
        if (scientist.y > 114 * Unit) {
          scientist.y = 114 * Unit;
          scientist.velocity = int16_t(-scientist.velocity * 7 / 10);
          if (scientist.velocity > -50) { scientist.velocity = 0; scientist.state = 1; }
        }
      } else {
        scientist.x -= scientist.state == 1 ? 100 : 28;
        if (scientist.x < 110 * Unit && scientist.state == 0) scientist.state = 1;
      }
      if (scientist.x < -15 * Unit) scientist.active = false;
    }
    if (powerTicks_[unsigned(Power::Magnet)]) {
      for (Coin& coin : coinField_) if (coin.active) {
        const int dx = int(coin.x / Unit) - PlayerX, dy = coin.y - playerY();
        if (dx * dx + dy * dy < 65 * 65) {
          coin.x += ((PlayerX * Unit) - coin.x) / 5;
          coin.y = int16_t(coin.y + (dy > 0 ? -2 : dy < 0 ? 2 : 0));
        }
      }
    }
    updateImported(advance);
  }
  void spawnExtras(int32_t advance) {
    distanceToBonus_ -= advance; distanceToToken_ -= advance;
    distanceToScientist_ -= advance; distanceToLaser_ -= advance;
    if (distanceToScientist_ <= 0) {
      for (Scientist& scientist : scientists_) if (!scientist.active) {
        scientist.active = true; scientist.x = 255 * Unit; scientist.y = 114 * Unit;
        scientist.state = scientist.age = 0; scientist.velocity = 0; break;
      }
      distanceToScientist_ = int32_t(70 + random() % 100) * Unit;
    }
    if (distanceToBonus_ <= 0 && !bonus_.active && vehicle_ == Vehicle::None) {
      bonus_.active = true; bonus_.x = 264 * Unit; bonus_.y = int16_t(38 + random() % 60);
      static const Power powers[] = {Power::Shield, Power::Boost, Power::CashDash, Power::RocketTime, Power::Magnet, Power::DoubleCoins};
      bonus_.kind = powers[random() % 6]; distanceToBonus_ = int32_t(450 + random() % 350) * Unit;
    }
    if (distanceToToken_ <= 0 && !token_.active && vehicle_ != Vehicle::SAM) {
      token_.active = true; token_.x = 265 * Unit; token_.y = int16_t(35 + random() % 65);
      token_.kind = 0;
      if ((random() % 3) == 0) {
        token_.kind = !(samLetters_ & 1) ? 1 : !(samLetters_ & 2) ? 2 : 3;
      }
      distanceToToken_ = int32_t(320 + random() % 200) * Unit;
    }
    if (vehicle_ == Vehicle::SAM && !missilePresent()) {
      Missile& missile = missiles_[0]; missile.active = true; missile.x = 254 * Unit;
      missile.y = int16_t(30 + random() % 77); missile.warning = 50;
    }
    if (distanceToLaser_ <= 0 && !laser_.active && !safePower() && vehicle_ != Vehicle::SAM && !missilePresent() && !anyZap()) {
      bool rotating = false; for (const Rotator& rotator : rotators_) rotating |= rotator.active;
      if (!rotating) {
        laser_.active = true; laser_.ticks = 140; laser_.y = int16_t(playerY());
        distanceToLaser_ = int32_t(1100 + random() % 700) * Unit;
      }
    }
  }
  void collectExtras() {
    if (bonus_.active && vehicle_ == Vehicle::None) {
      const int dx = int(bonus_.x / Unit) - PlayerX, dy = bonus_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) { activatePower(bonus_.kind); bonus_.active = false; }
    }
    if (token_.active) {
      const int dx = int(token_.x / Unit) - PlayerX, dy = token_.y - playerY();
      if (dx * dx + dy * dy < 12 * 12) {
        if (!token_.kind) { if (spinTokens_ < 9) ++spinTokens_; }
        else samLetters_ |= uint8_t(1u << (token_.kind - 1));
        token_.active = false; signal(ArcadeCue::Win);
        if (samLetters_ == 7) { samLetters_ = 0; equip(Vehicle::SAM); }
      }
    }
    collectImported();
  }
  void interactScientists() {
    for (Scientist& scientist : scientists_) if (scientist.active && !scientist.age && scientist.state != 2) {
      const int dx = int(scientist.x / Unit) - PlayerX;
      if (dx > -11 && dx < 12 && playerY() > 101) {
        if (scientistInteractions_ != UINT16_MAX) ++scientistInteractions_;
        scientist.age = 120;
        if (vehicle_ != Vehicle::None && vehicle_ != Vehicle::Armor && vehicle_ != Vehicle::Gravity) {
          scientist.state = 2; scientist.velocity = -800; addCoins(2);
        } else { scientist.state = 3; addCoins(1); }
        signal(ArcadeCue::Hit);
      } else if (thrust_ && dx > -15 && dx < 3 && playerY() > 78 && playerY() < 105) {
        scientist.state = 1; scientist.age = 30;
      }
    }
  }
  void rotatorEnds(const Rotator& rotator, int& x, int& y, int& dx, int& dy) const {
    static const int8_t cosine[16] = {16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6,0,6,11,15};
    const unsigned angle = (rotator.angle / 7u) & 15u;
    const int rx = cosine[angle], ry = cosine[(angle + 12) & 15u];
    x = int(rotator.x / Unit) - rx; y = rotator.y - ry; dx = rx * 2; dy = ry * 2;
  }
  void tickCrash(bool press) {
    thrust_ = false;
    if (press && !blastUsed_) {
      blastUsed_ = true; burst(0); velocity_ = -4 * FlightUnit;
      deathSpeed_ += 2 * Unit; crashTicks_ = uint16_t(crashTicks_ + 45);
      signal(ArcadeCue::Win);
    }
    velocity_ = divideNearest(int64_t(velocity_ + scaledNative(24)) * 98, 100);
    y_ += velocity_;
    if (y_ < Ceiling * FlightUnit) { y_ = Ceiling * FlightUnit; velocity_ = 0; }
    if (y_ >= Floor * FlightUnit) {
      y_ = Floor * FlightUnit;
      if (velocity_ > scaledNative(180) && crashBounces_ < 5) {
        velocity_ = -divideNearest(int64_t(velocity_) * 65, 100); ++crashBounces_;
      } else velocity_ = 0;
      deathSpeed_ = deathSpeed_ * 94 / 100;
    } else deathSpeed_ = deathSpeed_ * 995 / 1000;
    if (deathSpeed_ < 3) deathSpeed_ = 0;
    if (world_ < 0x70000000u) world_ += uint32_t(deathSpeed_);
    updateObjects(deathSpeed_); updateExtras(deathSpeed_);
    if (crashTicks_) --crashTicks_;
    if (!crashTicks_ && y_ == Floor * FlightUnit && velocity_ == 0) {
      if (spinTokens_) { phase_ = Phase::Spin; spinTicks_ = 90; spinResult_ = false; }
      else phase_ = Phase::Over;
      armed_ = false;
    }
  }
  void tickSpin(bool press) {
    if (spinTicks_) --spinTicks_;
    if (spinResult_) {
      if (spinTicks_) return;
      if (reward_ == 0) {
        phase_ = Phase::Running; vehicle_ = Vehicle::None;
        y_ = 70 * FlightUnit; velocity_ = 0; immune_ = 180; clearHazards();
        distanceToHazard_ = 170 * Unit; armed_ = false;
      } else if (reward_ == 2) {
        phase_ = Phase::Crashing; spinResult_ = false; crashTicks_ = 90; burst(0);
        velocity_ = -4 * FlightUnit; deathSpeed_ = speed() + 2 * Unit; blastUsed_ = true;
      } else if (spinTokens_) { spinResult_ = false; spinTicks_ = 60; }
      else { phase_ = Phase::Over; armed_ = false; }
      return;
    }
    if (spinTicks_ && !press) return;
    if (spinTokens_) --spinTokens_;
    const uint32_t value = random() % 12;
    spinResult_ = true; spinTicks_ = 60;
    if (value == 0 && !revived_) {
      revived_ = true; reward_ = 0; rewardCoins_ = 0; signal(ArcadeCue::Win);
    } else if (value == 1) {
      reward_ = 2; rewardCoins_ = 0; signal(ArcadeCue::Win);
    } else if (value == 2) {
      reward_ = 3; rewardCoins_ = 500; pendingRewards_ |= 1; signal(ArcadeCue::Win);
    } else if (value == 3) {
      reward_ = 4; rewardCoins_ = 2; pendingRewards_ |= 2; signal(ArcadeCue::Win);
    } else {
      static const uint16_t rewards[] = {50, 75, 100, 150, 250, 500};
      reward_ = 1; rewardCoins_ = rewards[value % 6]; addCoins(rewardCoins_); signal(ArcadeCue::Power);
    }
  }
  void drawExtras(Canvas& c) const {
    for (const Rotator& rotator : rotators_) if (rotator.active) {
      int x, y, dx, dy; rotatorEnds(rotator, x, y, dx, dy);
      JetpackArt::drawZap(c, x, y, x + dx, y + dy, frame_);
    }
    if (laser_.active) {
      const uint16_t color = laser_.ticks > 40 ? rgb(214, 78, 72) : rgb(255, 237, 139);
      if (laser_.ticks <= 40 || (frame_ & 8)) {
        for (int x = 3; x < Width; x += laser_.ticks > 40 ? 8 : 1) c.rect(x, laser_.y - 1, laser_.ticks > 40 ? 3 : 1, 3, color);
      }
      c.text(laser_.ticks > 40 ? "LASER LOCK" : "FIRING", 91, 17, color);
    }
    if (bonus_.active) JetpackArt::drawPickupIcon(c, int(bonus_.x / Unit), bonus_.y, frame_, powerIcon(bonus_.kind));
    if (token_.active) {
      JetpackArt::drawPickupIcon(c, int(token_.x / Unit), token_.y, frame_, 4);
      if (token_.kind) {
        const char letter[] = {token_.kind == 1 ? 'S' : token_.kind == 2 ? 'A' : 'M', 0};
        c.text(letter, int(token_.x / Unit) - 3, token_.y - 6, rgb(255, 249, 218));
      }
    }
    if (vehicle_ == Vehicle::Teleporter) {
      c.line(PlayerX + 5, teleportTargetY() - 5, PlayerX + 12, teleportTargetY() - 5, rgb(211, 180, 239));
      c.line(PlayerX + 5, teleportTargetY() + 5, PlayerX + 12, teleportTargetY() + 5, rgb(211, 180, 239));
    }
    drawImported(c);
  }
  static uint8_t powerIcon(Power power) {
    switch (power) {
      case Power::Shield: return 1; case Power::Magnet: return 2; case Power::DoubleCoins: return 3;
      case Power::CashDash: return 5; case Power::Boost: return 6; case Power::RocketTime: return 7;
      default: return 0;
    }
  }
  void updateImported(int32_t advance) {
#ifdef M5FLAP_JETPACK_IMPORTED
    for (ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      group.x -= advance;
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      if (group.x + int32_t(pattern.width) * group.step * Unit < -8 * Unit) group.pattern = -1;
    }
#else
    (void)advance;
#endif
  }
  bool startImported() {
#ifdef M5FLAP_JETPACK_IMPORTED
    ImportedGroup* slot = nullptr;
    for (ImportedGroup& group : importedGroups_) if (group.pattern < 0) { slot = &group; break; }
    if (!slot) return true;
    unsigned total = 0;
    const uint8_t desired = powerTicks_[unsigned(Power::CashDash)] ? 2 : 1;
    for (unsigned i = 0; i < JetpackImported::PatternCount; ++i) {
      const auto& pattern = JetpackImported::CoinPatterns[i];
      if ((pattern.flags & desired) && pattern.count && pattern.count <= 512 && pattern.height && pattern.height <= 40 && pattern.width && pattern.width <= 256 && unsigned(pattern.offset) + pattern.count <= JetpackImported::CoinPointCount) total += pattern.weight ? pattern.weight : 1;
    }
    if (!total) return false;
    unsigned choice = random() % total;
    for (unsigned i = 0; i < JetpackImported::PatternCount; ++i) {
      const auto& pattern = JetpackImported::CoinPatterns[i];
      if (!(pattern.flags & desired) || !pattern.count || pattern.count > 512 || !pattern.height || pattern.height > 40 || !pattern.width || pattern.width > 256 || unsigned(pattern.offset) + pattern.count > JetpackImported::CoinPointCount) continue;
      const unsigned weight = pattern.weight ? pattern.weight : 1;
      if (choice >= weight) { choice -= weight; continue; }
      *slot = ImportedGroup{}; slot->pattern = int16_t(i); slot->x = 252 * Unit;
      const unsigned fit = pattern.height ? 80u / pattern.height : 5;
      slot->step = uint8_t(fit > 5 ? 5 : fit < 2 ? 2 : fit);
      const unsigned extent = pattern.height * slot->step;
      slot->top = uint8_t(extent < 90 ? 23 + (90 - extent) / 2 : 23);
      return true;
    }
#endif
    return false;
  }
  void collectImported() {
#ifdef M5FLAP_JETPACK_IMPORTED
    const int radius = powerTicks_[unsigned(Power::Magnet)] ? 65 : 10;
    for (ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      for (unsigned i = 0; i < pattern.count; ++i) {
        if (group.collected[i / 8] & (1u << (i & 7))) continue;
        const uint16_t point = JetpackImported::CoinPoints[pattern.offset + i];
        const int x = int(group.x / Unit) + int(point & 255) * group.step;
        if (x > PlayerX + radius || x < PlayerX - radius) continue;
        const int y = group.top + int(point >> 8) * group.step;
        const int dx = x - PlayerX, dy = y - playerY();
        if (dx * dx + dy * dy <= radius * radius) {
          group.collected[i / 8] |= uint8_t(1u << (i & 7)); addCoins(1); signal(ArcadeCue::Power);
        }
      }
    }
#endif
  }
  void drawImported(Canvas& c) const {
#ifdef M5FLAP_JETPACK_IMPORTED
    for (const ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      for (unsigned i = 0; i < pattern.count; ++i) {
        if (group.collected[i / 8] & (1u << (i & 7))) continue;
        const uint16_t point = JetpackImported::CoinPoints[pattern.offset + i];
        const int x = int(group.x / Unit) + int(point & 255) * group.step;
        if (x < -3 || x > Width + 2) continue;
        const int y = group.top + int(point >> 8) * group.step;
        c.rect(x, y - 1, group.step > 2 ? 2 : 1, 3, rgb(220, 165, 53));
        c.pixel(x, y - 1, rgb(255, 237, 139));
      }
    }
#else
    (void)c;
#endif
  }


  void drawLab(Canvas& c) const {
    JetpackArt::drawLab(c, world_, frame_);
  }
  void drawCoin(Canvas& c, int x, int y) const {
    JetpackArt::drawCoin(c, x, y, frame_);
  }
  void drawZap(Canvas& c, const Zap& zap) const {
    const int x = int(zap.x / Unit);
    JetpackArt::drawZap(c, x, zap.y, x + zap.dx, zap.y + zap.dy, frame_);
  }
  void drawMissile(Canvas& c, const Missile& missile) const {
    const int y = missile.y;
    if (missile.warning) {
      const uint16_t red = rgb(232, 128, 48);
      if ((frame_ / 6 & 1) || missile.warning < 30) {
        c.rect(225, y - 6, 13, 13, rgb(16, 24, 40));
        c.text("!", 229, y - 6, red);
        c.line(220, y, 224, y - 4, red);
        c.line(220, y, 224, y + 4, red);
      }
      if (missile.warning <= 30) c.pixel(216, y, rgb(232, 176, 112));
      return;
    }
    JetpackArt::drawMissile(c, int(missile.x / Unit), y, frame_);
  }
  void drawPickup(Canvas& c) const {
    JetpackArt::drawPickupIcon(c, int(pickup_.x / Unit), pickup_.y, frame_, 0);
  }
  void drawPlayer(Canvas& c) const {
    if (immune_ && (frame_ & 4)) return;
    if (phase_ == Phase::Crashing || phase_ == Phase::Over || phase_ == Phase::Spin) {
      JetpackArt::drawCrash(c, PlayerX, playerY(), frame_, uint8_t(phase_ == Phase::Crashing ? frame_ / 5 & 3 : 0), playerY() == Floor && !velocity_);
    } else if (unsigned(vehicle_) >= unsigned(Vehicle::Hog)) {
      JetpackArt::drawVehicle(c, PlayerX, playerY(), frame_, uint8_t(vehicle_), thrust_, playerY() == Floor);
    } else JetpackArt::drawRunner(c, PlayerX, playerY(), frame_, thrust_,
        !thrust_ && playerY() == Floor, false, uint8_t(vehicle_), gravityUp_);
  }
  void panel(Canvas& c, const char* title, const char* line1, const char* line2) const {
    c.rect(28, 39, 184, 54, rgb(16, 24, 40));
    c.rect(28, 39, 184, 1, rgb(128, 176, 184));
    c.rect(28, 92, 184, 1, rgb(128, 176, 184));
    unsigned length = 0; while (title[length]) ++length;
    c.text(title, 120 - int(length) * 3, 43, rgb(248, 216, 80));
    length = 0; while (line1[length]) ++length;
    c.text(line1, 120 - int(length) * 3, 60, rgb(224, 240, 216));
    length = 0; while (line2[length]) ++length;
    c.text(line2, 120 - int(length) * 3, 75, rgb(128, 176, 184));
  }

  ImportedGroup importedGroups_[2];
  Bonus bonus_;
  Token token_;
  Scientist scientists_[ScientistCount];
  Rotator rotators_[2];
  Laser laser_;
  uint16_t powerTicks_[unsigned(Power::Count)] = {};
  uint32_t powerEndCash_ = 0, powerEndBoost_ = 0;
  int32_t distanceToBonus_ = 0, distanceToToken_ = 0, distanceToScientist_ = 0, distanceToLaser_ = 0;
  int32_t deathSpeed_ = 0, teleportTarget_ = 0, patternX_ = 0;
  int16_t coinPattern_ = -1;
  uint16_t patternCursor_ = 0, vehicleTicks_ = 0, scientistInteractions_ = 0, rewardCoins_ = 0;
  uint8_t spinTokens_ = 0, samLetters_ = 0, spinTicks_ = 0, reward_ = 0, vehicleJumps_ = 0, vehicleBoost_ = 0;
  int16_t fxX_ = PlayerX, fxY_ = Floor;
  uint8_t fxAge_ = 32, fxKind_ = 0;
  uint8_t samHits_ = 0, crashBounces_ = 0, nextRide_ = 0, nextGadget_ = 0, pendingRewards_ = 0;
  bool nextHeadstart_ = false, vehicleWasHeld_ = false, vehicleInputBlocked_ = false, teleportUp_ = true, blastUsed_ = false, revived_ = false, spinResult_ = false;
  Zap zaps_[ZapCount];
  Coin coinField_[CoinCount];
  Missile missiles_[MissileCount];
  Pickup pickup_;
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, world_ = 0, coins_ = 0;
  int32_t y_ = Floor * FlightUnit, velocity_ = 0;
  int32_t distanceToHazard_ = 0, distanceToCoins_ = 0, distanceToVehicle_ = 0;
  uint16_t immune_ = 0, vehicleHint_ = 0;
  uint8_t difficulty_ = 1, nextDifficulty_ = 1;
  uint16_t crashTicks_ = 0;
  Phase phase_ = Phase::Ready;
  Vehicle vehicle_ = Vehicle::None;
  ArcadeCue cue_ = ArcadeCue::None;
  bool gravityUp_ = false, armed_ = false, thrust_ = false, missileGap_ = false;
};

}  // namespace flappy
