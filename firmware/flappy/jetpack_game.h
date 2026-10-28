#pragma once

#include <stdint.h>
#include "arcade_support.h"
#include "jetpack_art.h"

namespace flappy {

// Original artwork and a fixed 60 Hz simulation on the native landscape grid.
class JetpackGame {
 public:
  enum { Width = 240, Height = 135, Unit = 256, FlightUnit = 65536, PlayerX = 43,
         Ceiling = 22, Floor = 114, ZapCount = 6, CoinCount = 56,
         MissileCount = 2 };
  enum class Phase : uint8_t { Ready, Running, Crashing, Over };
  enum class Vehicle : uint8_t { None, Armor, Gravity };
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
    distanceToVehicle_ = 1000 * Unit;
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
  }

  void tick(const ArcadeInput& input) {
    ++frame_;
    if (!input.action && !input.pressed) armed_ = true;
    const bool press = input.pressed && armed_;
    if (press) armed_ = false;
    if (phase_ == Phase::Ready) {
      thrust_ = false;
      if (press) {
        phase_ = Phase::Running;
        thrust_ = input.action || input.pressed;
        velocity_ = 0;
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
        signal(ArcadeCue::Fire);
      }
      return;
    }
    if (phase_ == Phase::Crashing) {
      thrust_ = false;
      velocity_ = bounded(velocity_ + 35 * 256, -500 * 256, 900 * 256);
      y_ += velocity_;
      if (y_ < Ceiling * FlightUnit) { y_ = Ceiling * FlightUnit; velocity_ = 0; }
      if (y_ >= Floor * FlightUnit) { y_ = Floor * FlightUnit; velocity_ = 0; }
      if (crashTicks_ && !--crashTicks_) {
        phase_ = Phase::Over;
        armed_ = false;
      }
      return;
    }

    if (immune_) --immune_;
    if (vehicleHint_) --vehicleHint_;
    thrust_ = input.action || input.pressed;
    if (vehicle_ == Vehicle::Gravity) {
      if (press) {
        gravityUp_ = !gravityUp_;
        signal(ArcadeCue::Fire);
      }
      const int32_t acceleration = divideNearest(int64_t(FlightUnit) * (gravityUp_ ? -299 : 299), 3000);
      velocity_ = divideNearest(int64_t(velocity_ + acceleration) * 98, 100);
    } else {
      if (press) signal(ArcadeCue::Fire);
      // Net thrust and gravity retain the 240-unit center travel in 92 pixels.
      const int32_t acceleration = divideNearest(int64_t(flightFactor()) * (thrust_ ? -161 : 115), 2000);
      velocity_ = divideNearest(int64_t(velocity_ + acceleration) * 98, 100);
    }
    y_ += velocity_;
    if (y_ < Ceiling * FlightUnit) { y_ = Ceiling * FlightUnit; velocity_ = 0; }
    if (y_ > Floor * FlightUnit) { y_ = Floor * FlightUnit; velocity_ = 0; }
    const int32_t advance = speed();
    if (world_ < 0x70000000u) world_ += uint32_t(advance);
    updateObjects(advance);
    spawnObjects(advance);
    collect();
    if (!immune_) collide();
  }

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
    for (const Coin& coin : coinField_) if (coin.active) drawCoin(c, int(coin.x / Unit), coin.y);
    if (pickup_.active) drawPickup(c);
    for (const Zap& zap : zaps_) if (zap.active) drawZap(c, zap);
    for (const Missile& missile : missiles_) if (missile.active) drawMissile(c, missile);
    drawPlayer(c);
    c.rect(0, 0, Width, 15, rgb(16, 24, 40));
    c.rect(0, 14, Width, 1, rgb(64, 88, 104));
    c.text("M", 4, 1, rgb(80, 184, 208));
    c.number(distance(), 16, 1, rgb(224, 240, 216));
    if (vehicleHint_ && phase_ == Phase::Running) {
      c.text(vehicle_ == Vehicle::Gravity ? "GRAVITY: TAP TO FLIP" : "ARMOR: HOLD TO FLY",
             100, 1, rgb(248, 216, 80));
    } else {
      drawCoin(c, 86, 7);
      c.number(coins_, 94, 1, rgb(248, 216, 80));
      c.text("BEST", 159, 1, rgb(80, 184, 208));
      c.number(best > score() ? best : score(), 189, 1, rgb(224, 240, 216));
    }
    if (phase_ == Phase::Ready) {
      panel(c, "JETPACK JOYRIDE", "HOLD TO FLY", "TAP TO START");
    } else if (phase_ == Phase::Over) {
      panel(c, "RUN COMPLETE", "RELEASE, THEN TAP", "TO TRY AGAIN");
      c.text("METERS", 71, 96, rgb(248, 216, 80));
      c.number(score(), 113, 96, rgb(224, 240, 216));

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
        if (missile.warning > 30) missile.y = int16_t(playerY());
        if (!--missile.warning) missile.x = 254 * Unit;
      } else {
        missile.x -= 1540 + advance / 4;
        if (missile.x < -16 * Unit) missile.active = false;
      }
    }
  }
  void spawnObjects(int32_t advance) {
    distanceToHazard_ -= advance;
    distanceToCoins_ -= advance;
    distanceToVehicle_ -= advance;
    if (distanceToHazard_ <= 0 && !missilePresent()) {
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
        spawnZap();
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
      pickup_.kind = (random() & 1) ? Vehicle::Armor : Vehicle::Gravity;
      distanceToVehicle_ = int32_t(1400 + random() % 700) * Unit;
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
  void spawnCoins() {
    const int cy = int(35 + random() % 66);
    const unsigned pattern = random() % 3;
    for (unsigned i = 0; i < 9; ++i) {
      const int wave = i < 5 ? int(i) : int(8 - i);
      int y = cy + (pattern == 0 ? 0 : pattern == 1 ? wave * 5 : -wave * 5);
      y = int(bounded(y, 29, 108));
      for (Coin& coin : coinField_) if (!coin.active) {
        coin.x = int32_t(250 + i * 11) * Unit;
        coin.y = int16_t(y);
        coin.active = true;
        break;
      }
    }
  }
  void collect() {
    for (Coin& coin : coinField_) if (coin.active) {
      const int dx = int(coin.x / Unit) - PlayerX, dy = coin.y - playerY();
      if (dx * dx + dy * dy <= 10 * 10) {
        coin.active = false;
        if (coins_ < 999999) ++coins_;
        signal(ArcadeCue::Power);
      }
    }
    if (pickup_.active) {
      const int dx = int(pickup_.x / Unit) - PlayerX, dy = pickup_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) {
        vehicle_ = pickup_.kind;
        pickup_.active = false;
        velocity_ = 0;
        gravityUp_ = false;
        vehicleHint_ = 180;
        immune_ = 90;
        armed_ = false;
        signal(ArcadeCue::Win);
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
  void collide() {
    for (Zap& zap : zaps_) if (zap.active &&
        nearSegment(PlayerX, playerY(), int(zap.x / Unit), zap.y, zap.dx, zap.dy, 6)) {
      zap.active = false;
      hit(); return;
    }
    for (Missile& missile : missiles_) if (missile.active && !missile.warning &&
        nearSegment(PlayerX, playerY(), int(missile.x / Unit) - 7, missile.y, 13, 0, 6)) {
      missile.active = false;
      hit(); return;
    }
  }
  void hit() {
    if (vehicle_ != Vehicle::None) {
      vehicle_ = Vehicle::None;
      vehicleHint_ = 0;
      gravityUp_ = false;
      velocity_ = -200 * 256;
      immune_ = 120;
      signal(ArcadeCue::Power);
    } else {
      phase_ = Phase::Crashing;
      crashTicks_ = 64;
      velocity_ = -450 * 256;
      armed_ = false;
      cue_ = ArcadeCue::Death;
    }
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
    const int x = int(pickup_.x / Unit), y = pickup_.y;
    const uint16_t glow = pickup_.kind == Vehicle::Gravity ? rgb(152, 120, 216) : rgb(80, 184, 208);
    c.rect(x - 9, y - 9, 18, 18, rgb(16, 24, 40));
    c.rect(x - 8, y - 8, 16, 1, glow); c.rect(x - 8, y + 7, 16, 1, glow);
    c.rect(x - 8, y - 8, 1, 16, glow); c.rect(x + 7, y - 8, 1, 16, glow);
    c.text(pickup_.kind == Vehicle::Gravity ? "G" : "A", x - 3, y - 6, glow);
    if (frame_ & 16) {
      c.pixel(x - 11, y, rgb(224, 240, 216));
      c.pixel(x + 10, y, rgb(224, 240, 216));
    }
  }
  void drawPlayer(Canvas& c) const {
    if (immune_ && (frame_ & 4)) return;
    JetpackArt::drawRunner(c, PlayerX, playerY(), frame_, thrust_,
        !thrust_ && playerY() == Floor,
        phase_ == Phase::Crashing || phase_ == Phase::Over, uint8_t(vehicle_), gravityUp_);
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

  Zap zaps_[ZapCount];
  Coin coinField_[CoinCount];
  Missile missiles_[MissileCount];
  Pickup pickup_;
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, world_ = 0, coins_ = 0;
  int32_t y_ = Floor * FlightUnit, velocity_ = 0;
  int32_t distanceToHazard_ = 0, distanceToCoins_ = 0, distanceToVehicle_ = 0;
  uint16_t immune_ = 0, vehicleHint_ = 0;
  uint8_t difficulty_ = 1, nextDifficulty_ = 1, crashTicks_ = 0;
  Phase phase_ = Phase::Ready;
  Vehicle vehicle_ = Vehicle::None;
  ArcadeCue cue_ = ArcadeCue::None;
  bool gravityUp_ = false, armed_ = false, thrust_ = false, missileGap_ = false;
};

}  // namespace flappy
