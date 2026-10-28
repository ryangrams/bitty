#pragma once

#include "clock_scene.h"
#include "geometry_collision.h"

#include <stdint.h>

namespace flappy {

// A separate, score-free attract course. Motion always integrates at 60 Hz.
class GeometryClock {
 public:
  void reset(uint32_t seed) {
    *this = GeometryClock{};
    random_ = seed ? seed : 1;
    material_ = uint8_t(ClockScene::random(random_) % 4u);
    for (unsigned i = 0; i < HurdleCount; ++i) {
      hurdles_[i].world = uint32_t(164 + i * 176) * 256u;
      randomize(hurdles_[i]);
    }
    nextForm_ = uint8_t(1u + ClockScene::random(random_) % 4u);
  }

  void step(const ClockTime& time) {
    ++frame_;
    for (unsigned i = 0; i < 4; ++i) digits_[i] = time.digit(i);
    if (cycle_ >= CycleTicks) {
      cycle_ = 0; clockWorld_ = world_ + 294u * 256u; portalWorld_ = world_ + 432u * 256u;
      material_ = uint8_t(ClockScene::random(random_) % 4u);
      const uint8_t advance = uint8_t(1u + ClockScene::random(random_) % 4u);
      nextForm_ = uint8_t((form_ + advance) % 5u);
      portalPassed_ = false;
    }
    rate_ = rateAt(cycle_);
    const uint32_t movement = 384u * rate_ + remainder_;
    const uint32_t advance = movement >> 12;
    remainder_ = uint16_t(movement & 4095u);
    world_ += advance;
    motion_ += rate_;
    for (Hurdle& hurdle : hurdles_) if (relative(hurdle.world, world_) < -80 * 256) {
      uint32_t furthest = world_;
      for (const Hurdle& other : hurdles_)
        if (relative(other.world, furthest) > 0) furthest = other.world;
      hurdle.world = furthest + (146u + ClockScene::random(random_) % 63u) * 256u;
      randomize(hurdle);
    }
    if (!portalPassed_ && relative(portalWorld_, world_) <= (PlayerX + 3) * 256) {
      form_ = nextForm_; portalPassed_ = true;
    }
    updatePlayer(advance);
    for (unsigned i = TrailCount - 1; i > 0; --i) trail_[i] = trail_[i - 1];
    trail_[0].x = world_ + PlayerX * 256u;
    trail_[0].y = int16_t(playerY_ / 16 + 16 * 16);
    ++cycle_;
  }

  void draw(uint16_t* pixels, const ClockTime&) const {
    if (!pixels) return;
    Canvas c(pixels);
    background(c);
    drawMonument(c);
    drawCourse(c);
    drawPortal(c);
    drawTrail(c);
    drawPlayer(c);
  }

 private:
  friend struct GeometryClockTestAccess;
  enum { PlayerX = 22, Floor = 212, CycleTicks = 600, HurdleCount = 6, TrailCount = 12 };
  enum { FullRate = 4096, QuarterRate = 1024 };
  struct Point { int x, y; };
  struct Hurdle { uint32_t world = 0; uint8_t type = 0, height = 0; };
  struct Trail { uint32_t x = 0; int16_t y = 0; };

  class Canvas {
   public:
    explicit Canvas(uint16_t* pixels) : pixels_(pixels) {}
    void pixel(int x, int y, uint16_t color) {
      if (unsigned(x) < ClockScene::Width && unsigned(y) < ClockScene::Height)
        pixels_[y * ClockScene::Width + x] = color;
    }
    void rect(int x, int y, int w, int h, uint16_t color) { ClockScene::rect(pixels_, x, y, w, h, color); }
    void line(int x, int y, int x1, int y1, uint16_t color, int thickness = 1) {
      const int dx = abs(x1 - x), sx = x < x1 ? 1 : -1;
      const int dy = -abs(y1 - y), sy = y < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        rect(x, y, thickness, thickness, color);
        if (x == x1 && y == y1) return;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
      }
    }
    void box(int x, int y, int w, int h, uint16_t fill, uint16_t edge, int thickness = 1) {
      rect(x, y, w, h, fill);
      rect(x, y, w, thickness, edge); rect(x, y + h - thickness, w, thickness, edge);
      rect(x, y, thickness, h, edge); rect(x + w - thickness, y, thickness, h, edge);
    }
    void polygon(const Point* points, unsigned count, uint16_t color) {
      if (count < 3) return;
      int top = ClockScene::Height, bottom = -1;
      for (unsigned i = 0; i < count; ++i) {
        top = minimum(top, points[i].y); bottom = maximum(bottom, points[i].y);
      }
      for (int y = maximum(0, top); y <= bottom && y < ClockScene::Height; ++y) {
        int left = ClockScene::Width, right = -1;
        for (unsigned i = 0; i < count; ++i) {
          const Point a = points[i], b = points[(i + 1) % count];
          if (y < minimum(a.y, b.y) || y > maximum(a.y, b.y)) continue;
          if (a.y == b.y) { left = minimum(left, minimum(a.x, b.x)); right = maximum(right, maximum(a.x, b.x)); }
          else { const int x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y); left = minimum(left, x); right = maximum(right, x); }
        }
        if (right >= left) rect(left, y, right - left + 1, 1, color);
      }
    }
    void ellipse(int x, int y, int w, int h, uint16_t color) {
      if (w <= 0 || h <= 0) return;
      for (int row = 0; row < h; ++row) {
        const int dy = row * 2 - h + 1;
        for (int column = 0; column < w; ++column) {
          const int dx = column * 2 - w + 1;
          if (dx * dx * h * h + dy * dy * w * w <= w * w * h * h)
            pixel(x + column, y + row, color);
        }
      }
    }
   private:
    uint16_t* pixels_;
  };

  static int abs(int value) { return value < 0 ? -value : value; }
  static int minimum(int a, int b) { return a < b ? a : b; }
  static int maximum(int a, int b) { return a > b ? a : b; }
  static int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
  static int32_t relative(uint32_t a, uint32_t b) { return int32_t(a - b); }
  static uint16_t rgb(unsigned r, unsigned g, unsigned b) { return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3); }
  static uint16_t dark() { return rgb(0, 15, 40); }
  static uint16_t cyan() { return rgb(8, 192, 247); }
  static uint16_t yellow() { return rgb(255, 229, 0); }
  static uint16_t pink() { return rgb(255, 88, 225); }
  static uint16_t white() { return 0xffff; }
  static uint16_t rateAt(unsigned phase) {
    if (phase < 120 || phase >= 480) return FullRate;
    if (phase >= 180 && phase < 420) return QuarterRate;
    const unsigned elapsed = phase < 180 ? phase - 120 : phase - 420;
    const unsigned t = elapsed * 4096u / 60u;
    const unsigned smooth = ((t * t >> 12) * (12288u - 2u * t)) >> 12;
    const unsigned amount = (3072u * smooth + 2048u) >> 12;
    return uint16_t(phase < 180 ? FullRate - amount : QuarterRate + amount);
  }
  static int sine(uint8_t angle) {
    static const int16_t values[] = {0,25,50,74,98,121,142,162,181,198,213,226,237,245,251,255,256};
    const unsigned quadrant = (angle & 63u) / 16u, offset = angle & 15u;
    const int value = values[quadrant & 1u ? 16u - offset : offset];
    return quadrant >= 2 ? -value : value;
  }
  static int smoothSine(uint32_t phase) {
    const unsigned angle = phase >> 8;
    const int a = sine(uint8_t(angle)), b = sine(uint8_t(angle + 1u));
    return a + (b - a) * int(phase & 255u) / 256;
  }

  void randomize(Hurdle& hurdle) {
    hurdle.type = uint8_t(ClockScene::random(random_) % 4u);
    hurdle.height = uint8_t(24u + ClockScene::random(random_) % 3u * 8u);
  }
  void updatePlayer(uint32_t advance) {
    int target = (Floor - 32) * 256;
    if (form_ == 1 || form_ == 3 || form_ == 4) {
      const int wave = smoothSine((motion_ / 160u) & 16383u);
      target = 107 * 256 + wave * 24;
    } else {
      for (const Hurdle& hurdle : hurdles_) {
        const int distance = relative(hurdle.world, world_ + PlayerX * 256u) / 256;
        if (distance < 104 && distance > -80) {
          const int progress = 104 - distance;
          const int lift = (hurdle.height + 60) * 4 * progress * (184 - progress) * 256 / (184 * 184);
          target = minimum(target, (Floor - 32) * 256 - lift);
        }
      }
    }
    const int maximumStep = maximum(1, int(advance) * 2);
    const int delta = clamp(target - playerY_, -maximumStep, maximumStep);
    playerY_ += delta;
    playerY_ = clamp(playerY_, 64 * 256, (Floor - 32) * 256);
    velocity_ = int16_t(delta);
    if (form_ == 0 || form_ == 2) {
      if (playerY_ < (Floor - 33) * 256) angle_ = uint16_t(angle_ + advance * 2u);
      else angle_ = uint16_t((angle_ + 2048u) & 0xf000u);
    }
  }

  void background(Canvas& c) const {
    // Saturated source-style blue, purple and teal keep white edges distinct.
    static const uint8_t colors[4][3] = {{0,116,246},{108,42,225},{0,158,166},{34,83,236}};
    const unsigned palette = unsigned(motion_ / (4096u * 1800u)) % 4u;
    const unsigned blend = unsigned(motion_ / 4096u % 1800u);
    const unsigned mix = blend < 1620 ? 0 : (blend - 1620u) * 256u / 180u;
    unsigned r = (colors[palette][0] * (256u - mix) + colors[(palette + 1u) % 4u][0] * mix) >> 8;
    unsigned g = (colors[palette][1] * (256u - mix) + colors[(palette + 1u) % 4u][1] * mix) >> 8;
    unsigned b = (colors[palette][2] * (256u - mix) + colors[(palette + 1u) % 4u][2] * mix) >> 8;
    for (int y = 0; y < ClockScene::Height; ++y) {
      const unsigned light = 64u + unsigned(y) * 32u / 239u;
      c.rect(0, y, ClockScene::Width, 1, rgb(r * light / 100u, g * light / 100u, b * (82u + unsigned(y) * 16u / 239u) / 100u));
    }
    const uint16_t fill = rgb(r * 74u / 100u, g * 74u / 100u, b * 88u / 100u);
    const uint16_t edge = rgb(r * 80u / 100u, g * 80u / 100u, b * 94u / 100u);
    const int far = int(world_ / (256u * 7u) % 64u);
    const int near = int(world_ / (256u * 4u) % 78u);
    for (int x = -64 - far; x < ClockScene::Width; x += 64) c.box(x, 18, 45, 65, fill, edge, 2);
    for (int x = -78 - near; x < ClockScene::Width; x += 78) c.box(x + 16, 115, 56, 74, fill, edge, 2);
    for (unsigned i = 0; i < 7; ++i) {
      const int x = int((i * 51u + 305u - world_ / (256u * (9u + i)) % 170u) % 170u) - 16;
      const int y = 12 + int(i * 29u % 173u);
      c.rect(x, y, 2, 2, edge);
    }
  }
  void cell(Canvas& c, int x, int y, unsigned style, unsigned index) const {
    if (style == 0) {
      c.box(x, y, 10, 10, dark(), white(), 2);
      c.rect(x + 3, y + 3, 4, 2, cyan());
    } else if (style == 1 || style == 2) {
      const uint16_t light = style == 1 ? yellow() : pink();
      c.ellipse(x, y, 10, 10, white()); c.ellipse(x + 1, y + 1, 8, 8, light);
      c.ellipse(x + 3, y + 3, 4, 4, dark());
      if ((frame_ / 24u + index) % 11u == 0) c.rect(x + 4, y + 3, 2, 3, white());
    } else {
      const Point outside[] = {{x + 5,y},{x + 9,y + 5},{x + 5,y + 9},{x,y + 5}};
      const Point inside[] = {{x + 5,y + 2},{x + 7,y + 5},{x + 5,y + 7},{x + 2,y + 5}};
      c.polygon(outside,4,white()); c.polygon(inside,4,cyan());
    }
  }
  void drawMonument(Canvas& c) const {
    const int left = relative(clockWorld_, world_) / 256;
    if (left > ClockScene::Width + 2 || left + 78 < -2) return;
    for (unsigned digit = 0; digit < 4; ++digit) {
      const int x = left + (digit & 1u) * 44, y = digit < 2 ? 38 : 120;
      for (unsigned row = 0; row < 5; ++row)
        for (unsigned column = 0; column < 3; ++column)
          if (ClockScene::digitPixel(digits_[digit],column,row))
            cell(c,x + int(column) * 12,y + int(row) * 12,material_,row * 3u + column + digit * 17u);
    }
    c.ellipse(left + 36,101,6,6,white()); c.ellipse(left + 37,102,4,4,pink());
    c.ellipse(left + 36,111,6,6,white()); c.ellipse(left + 37,112,4,4,pink());
  }
  void drawCourse(Canvas& c) const {
    const int floorOffset = int(world_ / 256u % 32u);
    for (int x = -32 - floorOffset; x < ClockScene::Width; x += 32) {
      c.box(x,Floor,32,32,dark(),white(),2);
      c.rect(x + 4,Floor + 4,2,26,cyan()); c.rect(x + 4,Floor + 4,26,2,cyan());
    }
    for (const Hurdle& hurdle : hurdles_) {
      const int x = relative(hurdle.world,world_) / 256;
      if (x < -36 || x > ClockScene::Width + 2) continue;
      const int top = Floor - hurdle.height;
      if (hurdle.type == 0 || hurdle.type == 1) {
        // Use the same native spike mask as the playing renderer, at exact 2x.
        for (int row = 0; row < 16; ++row) {
          const uint16_t mask = triangleRow(row,hurdle.type == 1);
          const uint16_t inside = uint16_t(mask & (mask << 1) & (mask >> 1) &
              triangleRow(row - 1,hurdle.type == 1) & triangleRow(row + 1,hurdle.type == 1));
          for (int column = 0; column < 16; ++column) if (mask & (1u << column))
            c.rect(x + column * 2,Floor - 32 + row * 2,2,2,
                   inside & (1u << column) ? dark() : white());
        }
      } else if (hurdle.type == 2) {
        c.box(x,top,32,hurdle.height,dark(),white(),2);
        c.rect(x + 4,top + 4,2,hurdle.height - 6,cyan());
        c.rect(x + 4,top + 4,26,2,cyan());
        c.ellipse(x + 4,top - 8,24,12,pink());
        c.rect(x + 6,top + 1,20,3,dark());
      } else {
        const int cy = Floor - 22;
        for (unsigned tooth = 0; tooth < 8; ++tooth) {
          const uint8_t a = uint8_t(tooth * 8u + motion_ / 8192u);
          c.line(x + 16 + sine(uint8_t(a + 16u)) * 10 / 256,cy + sine(a) * 10 / 256,
                 x + 16 + sine(uint8_t(a + 16u)) * 19 / 256,cy + sine(a) * 19 / 256,white(),2);
        }
        c.ellipse(x + 3,cy - 13,26,26,white()); c.ellipse(x + 7,cy - 9,18,18,dark());
        c.ellipse(x + 12,cy - 4,8,8,cyan());
      }
    }
  }
  static uint16_t triangleRow(int row, bool pair) {
    if (row < 0 || row > 15) return 0;
    const int inset = (15 - row) / 2;
    if (pair) {
      const int smallInset = 7 * (15 - row) / 30;
      return uint16_t(GeometryCollisionMask::span(smallInset,7 - smallInset) |
                      GeometryCollisionMask::span(8 + smallInset,15 - smallInset));
    }
    return GeometryCollisionMask::span(inset,15 - inset);
  }
  void drawPortal(Canvas& c) const {
    const int x = relative(portalWorld_,world_) / 256 - 12;
    if (x > ClockScene::Width || x < -48) return;
    const uint16_t accent = nextForm_ == 1 ? cyan() : nextForm_ == 2 ? pink() : nextForm_ == 3 ? yellow() : rgb(82,255,98);
    c.ellipse(x,115,48,96,white()); c.ellipse(x + 2,117,44,92,accent);
    c.ellipse(x + 8,123,32,80,dark());
    c.line(x + 17,154,x + 30,164,accent,2); c.line(x + 30,164,x + 17,174,accent,2);
  }
  void drawTrail(Canvas& c) const {
    for (unsigned i = TrailCount - 1; i > 0; --i) {
      if (!trail_[i].x) continue;
      const int x = relative(trail_[i].x,world_) / 256;
      const int y = trail_[i].y / 16;
      const unsigned light = 130u - i * 7u;
      c.rect(x - 2,y - 2,i < 5 ? 4 : 2,i < 5 ? 4 : 2,rgb(0,light,light + 65u));
    }
  }

  Point transform(Point point, uint8_t angle) const {
    const int x = point.x * 2 - 15, y = point.y * 2 - 15;
    const int s = sine(angle), cosine = sine(uint8_t(angle + 16));
    return {PlayerX + (15 * 256 + x * cosine - y * s + 256) / 256,
            playerY_ / 256 + (15 * 256 + x * s + y * cosine + 256) / 256};
  }
  void iconPolygon(Canvas& c, const Point* points, unsigned count, uint8_t angle, uint16_t color) const {
    if (count > 12) return;
    Point transformed[12];
    for (unsigned i = 0; i < count; ++i) transformed[i] = transform(points[i],angle);
    c.polygon(transformed,count,color);
  }
  void iconBox(Canvas& c, int x0, int y0, int x1, int y1, uint8_t angle, uint16_t color) const {
    const Point points[] = {{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
    iconPolygon(c,points,4,angle,color);
  }
  void iconLine(Canvas& c, int x0, int y0, int x1, int y1, uint8_t angle, uint16_t color) const {
    const Point a = transform({x0,y0},angle), b = transform({x1,y1},angle);
    c.line(a.x,a.y,b.x,b.y,color,2);
  }
  void drawPlayer(Canvas& c) const {
    const uint8_t rotation = uint8_t(angle_ >> 8 & 63u);
    const uint8_t pitch = uint8_t(clamp(velocity_ / 96,-5,5) & 63);
    const uint8_t angle = form_ == 0 || form_ == 2 ? rotation : form_ == 1 || form_ == 4 ? pitch : 0;
    if (form_ == 1) {
      const Point hull[] = {{-4,10},{20,7},{10,16},{-1,16}}, inset[] = {{-1,11},{16,9},{9,14},{1,14}};
      iconPolygon(c,hull,4,angle,dark()); iconPolygon(c,inset,4,angle,cyan());
      iconBox(c,3,1,12,10,angle,dark()); iconBox(c,4,2,11,8,angle,yellow());
      iconBox(c,7,4,9,5,angle,dark());
    } else if (form_ == 2) {
      const Point outer[] = {{5,0},{10,0},{15,5},{15,10},{10,15},{5,15},{0,10},{0,5}};
      const Point inner[] = {{5,2},{10,2},{13,5},{13,10},{10,13},{5,13},{2,10},{2,5}};
      iconPolygon(c,outer,8,angle,dark()); iconPolygon(c,inner,8,angle,yellow());
      iconLine(c,3,3,12,12,angle,dark()); iconLine(c,12,3,3,12,angle,cyan());
    } else if (form_ == 3) {
      const Point dome[] = {{3,7},{4,2},{11,2},{12,7}}, saucer[] = {{-2,8},{17,8},{12,13},{3,13}};
      iconPolygon(c,dome,4,0,dark()); iconBox(c,5,3,10,6,0,yellow());
      iconPolygon(c,saucer,4,0,cyan()); iconLine(c,0,8,15,8,0,white());
      iconLine(c,3,14,1,16,0,dark()); iconLine(c,12,14,14,16,0,dark());
    } else if (form_ == 4) {
      const Point outer[] = {{0,0},{17,8},{0,15},{4,8}}, inner[] = {{3,3},{13,8},{3,12},{6,8}};
      iconPolygon(c,outer,4,angle,dark()); iconPolygon(c,inner,4,angle,yellow());
      iconLine(c,-7,8,-1,8,angle,cyan());
    } else {
      iconBox(c,0,0,15,15,angle,dark()); iconBox(c,1,1,14,14,angle,yellow());
      iconBox(c,4,4,6,6,angle,dark()); iconBox(c,9,4,11,6,angle,dark());
      iconBox(c,4,9,11,11,angle,dark());
    }
  }

  Hurdle hurdles_[HurdleCount] = {};
  Trail trail_[TrailCount] = {};
  uint32_t random_ = 1, frame_ = 0, world_ = 0, motion_ = 0;
  uint32_t clockWorld_ = 294u * 256u, portalWorld_ = 432u * 256u;
  int32_t playerY_ = (Floor - 32) * 256;
  int16_t velocity_ = 0;
  uint16_t rate_ = FullRate, remainder_ = 0, cycle_ = 0, angle_ = 0;
  uint8_t digits_[4] = {1,2,0,0};
  uint8_t material_ = 0, form_ = 0, nextForm_ = 1;
  bool portalPassed_ = false;
};

static_assert(sizeof(GeometryClock) <= 2048, "Geometry Clock must fit the portrait attract-scene budget");

}  // namespace flappy
