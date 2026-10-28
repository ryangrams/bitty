#pragma once

#include "clock_scene.h"
#include "jetpack_art.h"

#include <stdint.h>

namespace flappy {

// This attract scene owns no game profile, rewards, audio or framebuffer.
class JetpackClock {
 public:
  void reset(uint32_t seed) {
    *this = JetpackClock{};
    random_ = seed ? seed : 1;
    playerY_ = 174 * 256;
    nextVehicle_ = uint8_t(ClockScene::random(random_) % 7u + 1u);
    for (unsigned i = 0; i < StaffCount; ++i) {
      staff_[i].x = int16_t((54 + i * 72) * 16);
      staff_[i].pose = uint8_t(ClockScene::random(random_) % 3u);
    }
  }

  void step(const ClockTime& time) {
    ++frame_;
    if (!formed_ || minute_ != time.minuteKey() || twelveHour_ != !time.twentyFourHour || cycle_ >= CycleTicks)
      form(time);
    else ++cycle_;

    world_ += 19u;
    updateTraffic();
    updateVehicle();
    updatePlayer();
    updateCoins();
    updateSparks();
  }

  void draw(uint16_t* pixels, const ClockTime&) const {
    if (!pixels) return;
    Canvas c(pixels);
    drawLab(c);
    for (unsigned i = 0; i < StaffCount; ++i) {
      ScaledCanvas person(c, staff_[i].x / 16, Floor - 12, 2);
      JetpackArt::drawScientist(person, 0, 0, frame_ + i * 13u, staff_[i].pose);
    }
    drawTraffic(c);
    drawAmbientCoins(c);
    for (unsigned i = 0; i < coinCount_; ++i) {
      const Coin& coin = coins_[i];
      if (coin.state == Collected) continue;
      const int x = coin.x / 16, y = coin.y / 16;
      if (x < -4 || x > ClockScene::Width + 4) continue;
      if (coin.state == Attracted) {
        c.line(x - coin.vx / 6, y - coin.vy / 6, x, y, ink(JetpackArt::BrassShade));
        c.pixel(x - coin.vx / 4, y - coin.vy / 4, ink(JetpackArt::Blue));
      }
      const unsigned spin = (frame_ + i * 17u) % 210u;
      JetpackArt::drawCoin(c, x, y, spin < 24u ? spin : 0u);
    }
    if (magnet()) JetpackArt::drawMagnetField(c, PlayerX, playerY_ / 256, frame_, 48);
    if (pickupX_ > PlayerX * 16) {
      ScaledCanvas pickup(c, pickupX_ / 16, 171, 2);
      JetpackArt::drawPickupIcon(pickup, 0, 0, frame_, 0);
    }
    const int py = playerY_ / 256;
    ScaledCanvas player(c, PlayerX, py, 2);
    if (vehicle_ >= 3) JetpackArt::drawVehicle(player, 0, 0, frame_, vehicle_, velocity_ < 0, true);
    else JetpackArt::drawRunner(player, 0, 0, frame_, velocity_ < 0 || (frame_ / 11u & 1u),
                               py >= Floor - 14, false, vehicle_, gravityUp());
    drawSparks(c);
    // Foreground bolts and the conveyor remain visible beneath the action.
    c.rect(0, Floor + 3, ClockScene::Width, 2, ink(JetpackArt::MetalShade));
    const int tread = int(world_ / 16u % 12u);
    for (int x = -12 - tread; x < ClockScene::Width; x += 12) {
      c.line(x, Floor + 6, x + 5, Floor + 9, ink(JetpackArt::PanelLight));
      c.pixel(x + 6, Floor + 10, ink(JetpackArt::Outline));
    }
  }

 private:
  friend struct JetpackClockTestAccess;
  enum { PlayerX = 40, Floor = 220, CycleTicks = 840, CoinCapacity = 64, StaffCount = 3, SparkCount = 12 };
  enum : uint8_t { Formation, Attracted, Collected };
  struct Coin {
    int16_t x = 0, y = 0;
    uint8_t homeX = 0, homeY = 0;
    int8_t vx = 0, vy = 0, scatter = 0;
    uint8_t state = Formation;
  };
  struct Staff { int16_t x = 0; uint8_t pose = 0; };
  struct Traffic {
    int16_t x = 0;
    uint8_t y = 0, kind = 0, length = 0;
    bool active = false;
  };
  struct Spark {
    int16_t x = 0, y = 0;
    int8_t vx = 0, vy = 0;
    uint8_t life = 0;
  };

  class Canvas {
   public:
    explicit Canvas(uint16_t* pixels) : pixels_(pixels) {}
    void pixel(int x, int y, uint16_t color) {
      if (unsigned(x) < ClockScene::Width && unsigned(y) < ClockScene::Height)
        pixels_[y * ClockScene::Width + x] = color;
    }
    void rect(int x, int y, int w, int h, uint16_t color) { ClockScene::rect(pixels_, x, y, w, h, color); }
    void line(int x, int y, int x1, int y1, uint16_t color) {
      const int dx = abs(x1 - x), sx = x < x1 ? 1 : -1;
      const int dy = -abs(y1 - y), sy = y < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x, y, color);
        if (x == x1 && y == y1) return;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
      }
    }
   private:
    uint16_t* pixels_;
  };

  // Authored actors keep their native pixel edges at an exact 2x size.
  class ScaledCanvas {
   public:
    ScaledCanvas(Canvas& canvas, int x, int y, int scale) : c_(canvas), x_(x), y_(y), scale_(scale) {}
    void pixel(int x, int y, uint16_t color) { c_.rect(x_ + x * scale_, y_ + y * scale_, scale_, scale_, color); }
    void rect(int x, int y, int w, int h, uint16_t color) {
      c_.rect(x_ + x * scale_, y_ + y * scale_, w * scale_, h * scale_, color);
    }
    void line(int x, int y, int x1, int y1, uint16_t color) {
      const int dx = abs(x1 - x), sx = x < x1 ? 1 : -1;
      const int dy = -abs(y1 - y), sy = y < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x, y, color);
        if (x == x1 && y == y1) return;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
      }
    }
   private:
    Canvas& c_;
    int x_, y_, scale_;
  };

  static int abs(int value) { return value < 0 ? -value : value; }
  static int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
  static uint16_t ink(JetpackArt::Ink color) { return JetpackArt::color(color); }
  static int wave(uint32_t tick, unsigned period, int height) {
    const unsigned phase = unsigned(tick % period);
    const int rising = phase < period / 2 ? int(phase) : int(period - phase);
    return rising * height * 4 / int(period) - height;
  }
  bool magnet() const { return cycle_ >= 552 && cycle_ < 716; }
  bool gravityUp() const { return vehicle_ == 2 && (frame_ / 180u & 1u); }

  void addCoin(int x, int y) {
    if (coinCount_ >= CoinCapacity) return;
    Coin& coin = coins_[coinCount_++];
    coin = Coin{};
    coin.homeX = uint8_t(x); coin.homeY = uint8_t(y);
    coin.scatter = int8_t(int(ClockScene::random(random_) % 49u) - 24);
    coin.x = int16_t((x + 160) * 16);
    coin.y = int16_t((y + coin.scatter) * 16);
  }
  void form(const ClockTime& time) {
    formed_ = true; minute_ = time.minuteKey(); twelveHour_ = !time.twentyFourHour;
    cycle_ = 0; coinCount_ = 0;
    const int top = 68 + int(ClockScene::random(random_) % 3u) * 8;
    static const uint8_t left[] = {14, 39, 73, 98};
    for (unsigned digit = 0; digit < 4; ++digit)
      for (unsigned y = 0; y < 5; ++y)
        for (unsigned x = 0; x < 3; ++x)
          if (ClockScene::digitPixel(time.digit(digit), x, y)) addCoin(left[digit] + int(x) * 7, top + int(y) * 10);
    addCoin(63, top + 10); addCoin(63, top + 30);
  }

  void updateTraffic() {
    for (unsigned i = 0; i < StaffCount; ++i) {
      staff_[i].x -= int16_t(18 + i * 2u);
      if (staff_[i].x < -20 * 16) {
        staff_[i].x = int16_t((153 + ClockScene::random(random_) % 30u) * 16);
        staff_[i].pose = uint8_t(ClockScene::random(random_) % 3u);
      }
    }
    if (frame_ % 142u == 30u) for (Traffic& traffic : traffic_) if (!traffic.active) {
      traffic.active = true; traffic.x = 160 * 16;
      traffic.kind = uint8_t(ClockScene::random(random_) % 3u);
      traffic.y = uint8_t(145 + ClockScene::random(random_) % 47u);
      traffic.length = uint8_t(16 + ClockScene::random(random_) % 16u);
      break;
    }
    for (Traffic& traffic : traffic_) if (traffic.active) {
      traffic.x -= traffic.kind == 2 ? 41 : 23;
      if (traffic.x < -36 * 16) traffic.active = false;
    }
  }
  void updateVehicle() {
    const unsigned beat = unsigned(frame_ % 1800u);
    if (beat == 460u) pickupX_ = 163 * 16;
    if (pickupX_ > PlayerX * 16) {
      pickupX_ = int16_t(pickupX_ - 22);
      if (pickupX_ <= PlayerX * 16) {
        pickupX_ = 0; vehicle_ = nextVehicle_;
        nextVehicle_ = uint8_t(1u + ClockScene::random(random_) % 7u);
        burst(PlayerX, playerY_ / 256);
      }
    }
    if (beat == 1260u && vehicle_) { vehicle_ = 0; burst(PlayerX, playerY_ / 256); }
  }
  void updatePlayer() {
    int target = 178 + wave(frame_ + 40u, 360u, 16);
    if (magnet() && vehicle_ != 3) target = 120 + wave(frame_, 180u, 7);
    if (vehicle_ == 3) target = Floor - 17 - (wave(frame_, 180u, 12) > 5 ? 14 : 0);
    if (vehicle_ == 2) target = gravityUp() ? 58 : Floor - 18;
    if (vehicle_ == 4) target += 10;
    for (const Traffic& traffic : traffic_) if (traffic.active && traffic.x / 16 < PlayerX + 74 && traffic.x / 16 > PlayerX - 38) {
      const int middle = traffic.y + (traffic.kind == 0 ? traffic.length / 2 : 0);
      if (abs(target - middle) < 43) target = middle > 151 ? middle - 48 : middle + 48;
    }
    target = clamp(target, 54, Floor - 16);
    const int error = target * 256 - playerY_;
    velocity_ = int16_t(clamp((int(velocity_) * 7 + error / 14) / 8, -440, 440));
    playerY_ = clamp(playerY_ + velocity_, 50 * 256, (Floor - 14) * 256);
  }
  void updateCoins() {
    const int remaining = cycle_ < 120 ? 120 - cycle_ : 0;
    const int ingress = 160 * remaining * remaining / (120 * 120);
    const int exit = cycle_ > 714 ? -int(cycle_ - 714) * 3 / 2 : 0;
    const int bob = wave(frame_, 360u, 20);
    for (unsigned i = 0; i < coinCount_; ++i) {
      Coin& coin = coins_[i];
      if (coin.state == Formation) {
        coin.x = int16_t((int(coin.homeX) + ingress + exit) * 16);
        coin.y = int16_t(int(coin.homeY) * 16 + bob + int(coin.scatter) * 16 * remaining / 120);
      } else if (coin.state == Attracted) {
        const int dx = PlayerX * 16 - coin.x, dy = playerY_ / 16 - coin.y;
        coin.vx = int8_t(clamp((int(coin.vx) * 3 + dx / 4) / 4, -76, 76));
        coin.vy = int8_t(clamp((int(coin.vy) * 3 + dy / 4) / 4, -76, 76));
        coin.x = int16_t(coin.x + coin.vx); coin.y = int16_t(coin.y + coin.vy);
        if (abs(dx) < 5 * 16 && abs(dy) < 7 * 16) {
          coin.state = Collected;
          sparkle(coin.x / 16, coin.y / 16);
        }
      }
    }
    // Only a few lower coins leave the digits before the formation scrolls away.
    if (cycle_ >= 558 && cycle_ < 686 && cycle_ % 12u == 6u) {
      int best = -1, lowest = -1;
      for (unsigned i = 0; i < coinCount_; ++i) if (coins_[i].state == Formation && coins_[i].homeY > lowest) {
        best = int(i); lowest = coins_[i].homeY;
      }
      if (best >= 0) coins_[unsigned(best)].state = Attracted;
    }
  }
  void sparkle(int x, int y) {
    for (Spark& spark : sparks_) if (!spark.life) {
      spark.x = int16_t(x * 16); spark.y = int16_t(y * 16);
      spark.vx = int8_t(int(ClockScene::random(random_) % 25u) - 12);
      spark.vy = int8_t(-int(ClockScene::random(random_) % 19u) - 8);
      spark.life = 20; return;
    }
  }
  void burst(int x, int y) { for (unsigned i = 0; i < 7; ++i) sparkle(x + int(i) - 3, y); }
  void updateSparks() {
    for (Spark& spark : sparks_) if (spark.life) {
      --spark.life; spark.x = int16_t(spark.x + spark.vx); spark.y = int16_t(spark.y + spark.vy);
      if (spark.life % 3u == 0u) ++spark.vy;
    }
  }

  void drawLab(Canvas& c) const {
    c.rect(0, 0, ClockScene::Width, ClockScene::Height, ink(JetpackArt::WallShade));
    c.rect(0, 28, ClockScene::Width, 94, ink(JetpackArt::Wall));
    c.rect(0, 123, ClockScene::Width, 84, ink(JetpackArt::PanelShade));
    const int wall = int(world_ / 80u % 78u);
    for (int x = -78 - wall; x < ClockScene::Width; x += 78) {
      c.rect(x, 25, 2, Floor - 26, ink(JetpackArt::PanelLight));
      c.rect(x + 3, 25, 1, Floor - 26, ink(JetpackArt::Outline));
      c.rect(x + 12, 37, 51, 24, ink(JetpackArt::PanelShade));
      c.rect(x + 14, 39, 47, 20, ink(JetpackArt::Panel));
      c.rect(x + 16, 41, 29, 3, ink(JetpackArt::Screen));
      for (int vent = 0; vent < 4; ++vent)
        c.rect(x + 20 + vent * 8, 49, 5, 2, ink(JetpackArt::PanelLight));
      c.pixel(x + 57, 42, ink((frame_ / 42u & 1u) ? JetpackArt::ArmorLight : JetpackArt::Armor));
      c.rect(x + 21, 141, 36, 38, ink(JetpackArt::Wall));
      c.rect(x + 24, 145, 30, 8, ink(JetpackArt::Panel));
      c.rect(x + 27, 157, 24, 1, ink(JetpackArt::PanelLight));
      c.rect(x + 27, 162, 24, 1, ink(JetpackArt::PanelLight));
    }
    c.rect(0, 14, ClockScene::Width, 6, ink(JetpackArt::MetalShade));
    c.rect(0, 20, ClockScene::Width, 2, ink(JetpackArt::Metal));
    c.rect(0, 22, ClockScene::Width, 2, ink(JetpackArt::Panel));
    const int lamps = int(world_ / 48u % 84u);
    for (int x = -84 - lamps; x < ClockScene::Width; x += 84) {
      c.rect(x + 21, 6, 47, 8, ink(JetpackArt::Outline));
      c.rect(x + 23, 7, 43, 4, ink(JetpackArt::Lamp));
      c.rect(x + 26, 8, 37, 2, ink(JetpackArt::LampLight));
    }
    c.rect(0, Floor - 3, ClockScene::Width, 3, ink(JetpackArt::MetalLight));
    c.rect(0, Floor, ClockScene::Width, ClockScene::Height - Floor, ink(JetpackArt::MetalShade));
    c.rect(0, Floor + 2, ClockScene::Width, 11, ink(JetpackArt::PanelShade));
    c.rect(0, Floor + 14, ClockScene::Width, 3, ink(JetpackArt::Metal));
    const int hazard = int(world_ / 16u % 24u);
    for (int x = -24 - hazard; x < ClockScene::Width; x += 24) {
      c.rect(x, Floor - 7, 12, 3, ink(JetpackArt::BrassShade));
      c.rect(x + 1, Floor - 7, 10, 1, ink(JetpackArt::Brass));
    }
  }
  void drawTraffic(Canvas& c) const {
    for (const Traffic& traffic : traffic_) if (traffic.active) {
      const int x = traffic.x / 16, y = traffic.y;
      if (traffic.kind == 2) JetpackArt::drawMissile(c, x, y, frame_);
      else JetpackArt::drawZap(c, x, y, x + (traffic.kind == 1 ? traffic.length : 0),
                              y + traffic.length, frame_);
    }
  }
  void drawAmbientCoins(Canvas& c) const {
    const unsigned pattern = unsigned(frame_ / 180u % 4u);
    const int root = 150 - int(frame_ % 180u) * 21 / 16;
    for (unsigned i = 0; i < 14; ++i) {
      const int x = root + int(i) * 9;
      int y = 174;
      if (pattern == 0) y += wave(i * 12u, 96u, 15);
      else if (pattern == 1) y += int(i % 5u) * 5 - 10;
      else if (pattern == 2) y += (i % 2u) ? 8 : -8;
      else y += i < 7 ? -int(i) * 3 : -int(13u - i) * 3;
      if (x < -4 || x > ClockScene::Width + 4) continue;
      if (abs(x - PlayerX) < 13 && abs(y - playerY_ / 256) < 17) continue;
      JetpackArt::drawCoin(c, x, y, frame_ + i * 9u);
    }
  }
  void drawSparks(Canvas& c) const {
    for (const Spark& spark : sparks_) if (spark.life) {
      const int x = spark.x / 16, y = spark.y / 16;
      const uint16_t color = ink(spark.life > 10 ? JetpackArt::FireLight : JetpackArt::Brass);
      c.pixel(x - 1, y, color); c.pixel(x + 1, y, color);
      c.pixel(x, y - 1, color); c.pixel(x, y + 1, color);
    }
  }

  Coin coins_[CoinCapacity] = {};
  Staff staff_[StaffCount] = {};
  Traffic traffic_[4] = {};
  Spark sparks_[SparkCount] = {};
  uint32_t random_ = 1, frame_ = 0, world_ = 0;
  int32_t playerY_ = 174 * 256;
  int16_t velocity_ = 0, pickupX_ = 0;
  uint16_t cycle_ = 0, minute_ = UINT16_MAX;
  uint8_t coinCount_ = 0, vehicle_ = 0, nextVehicle_ = 3;
  bool formed_ = false, twelveHour_ = false;
};

static_assert(sizeof(JetpackClock) <= 3072, "Jetpack Clock must fit the portrait attract-scene budget");

}  // namespace flappy
