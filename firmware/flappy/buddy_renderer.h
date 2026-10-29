#pragma once

#include <stdint.h>
#include <stdio.h>
#include "buddy_game.h"

namespace flappy {

constexpr uint16_t buddyRgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
}

// All shapes and LED patterns are authored for Bitty's native portrait canvas.
class BuddyRenderer {
 public:
  enum { Width = 135, Height = 240 };
  enum class Pose : uint8_t { Run, Push, Jump, Roll, Dance, Crash };
  struct Object {
    BuddyGame::ObjectKind kind;
    float lane, depth;
    uint16_t variant;
    uint8_t lift;
    Object(BuddyGame::ObjectKind k = BuddyGame::ObjectKind::Coin, float l = 1,
           float d = .5f, uint16_t v = 0, uint8_t h = 0) : kind(k), lane(l), depth(d), variant(v), lift(h) {}
  };
  struct Scene {
    BuddyGame::Theme theme = BuddyGame::Theme::Mall;
    BuddyGame::Phase phase = BuddyGame::Phase::Playing;
    uint32_t frame = 0, phaseTicks = 0, score = 0, coins = 0, best = 0, levelTicks = 0;
    unsigned level = 0, shirt = 0, mask = 1, snake = 0;
    BuddyGame::Section section = BuddyGame::Section::Welcome;
    unsigned multiplier = 1, combo = 0, goalIndex = 0, goalValue = 0, goalTarget = 25, sectionProgress = 0;
    bool magnet = false, reward = false;
    unsigned lift = 0;
    float lane = 1, jump = 0;
    bool rolling = false, board = false, hud = true;
    const Object* objects = nullptr;
    unsigned count = 0;
    int bottom = Height;
  };

  static constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
  }
  static void draw(uint16_t* pixels, const BuddyGame& game, uint32_t best = 0) {
    Object objects[BuddyGame::ObjectCount];
    Scene s;
    s.theme = game.theme(); s.phase = game.phase(); s.frame = game.ticks();
    s.phaseTicks = game.phaseTicks();
    s.score = game.score(); s.coins = game.coins(); s.best = best;
    s.level = game.level(); s.levelTicks = game.levelTicks();
    s.section = game.section(); s.sectionProgress = game.sectionProgress(); s.multiplier = game.scoreMultiplier(); s.combo = game.combo();
    s.lift = game.flightHeight(); s.magnet = game.magnetTicks() != 0; s.reward = game.rewardTicks() != 0;
    s.goalIndex = unsigned(game.ticks() / 180 % 3);
    s.goalValue = game.goal(s.goalIndex); s.goalTarget = game.goalTarget(s.goalIndex);
    s.lane = game.lanePosition(); s.jump = game.jumpHeight();
    s.rolling = game.rolling(); s.board = game.boardTicks() != 0;
    s.snake = game.snakeCharges(); s.shirt = unsigned(game.shirt()); s.mask = unsigned(game.mask());
    for (unsigned i = 0; i < BuddyGame::ObjectCount; ++i) {
      const auto& o = game.objects()[i];
      if (o.active && o.progress >= 0 && s.count < BuddyGame::ObjectCount)
        objects[s.count++] = {o.kind, float(o.lane) + float(o.pull) / BuddyGame::LaneUnit, o.depth, o.variant, o.lift};
    }
    s.objects = objects;
    drawScene(pixels, s);
  }

  static const char* themeName(BuddyGame::Theme theme) {
    switch (theme) {
      case BuddyGame::Theme::School: return "AFTER SCHOOL";
      case BuddyGame::Theme::Park: return "POCKET PARK";
      case BuddyGame::Theme::Volcano: return "LAVA LAUNDRY";
      case BuddyGame::Theme::Noodles: return "NOODLE NIGHT";
      default: return "POCKET MALL";
    }
  }

  static void drawScene(uint16_t* pixels, const Scene& s) {
    if (!pixels) return;
    Canvas c(pixels, s.bottom);
    world(c, s.theme, s.frame);
    // Depth sorting keeps nearby people and pocket change in front of distant art.
    unsigned order[BuddyGame::ObjectCount];
    const unsigned count = s.objects ? (s.count < BuddyGame::ObjectCount ? s.count : BuddyGame::ObjectCount) : 0;
    for (unsigned i = 0; i < count; ++i) {
      unsigned j = i;
      while (j && s.objects[order[j - 1]].depth > s.objects[i].depth) {
        order[j] = order[j - 1]; --j;
      }
      order[j] = i;
    }
    for (unsigned i = 0; i < count; ++i) obstacle(c, s.objects[order[i]], s.theme, s.frame);
    const float lane = bounded(s.lane, 0, 2);
    const int px = 67 + int((lane - 1) * 43);
    const int jump = int(bounded(s.jump, 0, 1) * 39) + int((s.lift > 100 ? 100 : s.lift) * 92 / 100);
    c.ellipse(px - 13 + jump / 8, 210, 26 - jump / 4, 5, rgb(104,78,116));
    if (s.board && !s.lift) hoverboard(c, px, 213, s.frame, 256);
    if (s.lift) {
      const int by = 170 - jump;
      c.line(px + 13,by + 2,px + 15,206 - jump,Cream);
      c.ellipse(px + 4,by - 28,20,27,Ink); c.ellipse(px + 6,by - 26,16,23,Coral);
      c.rect(px + 8,by - 22,3,8,Cream); c.rect(px + 12,by - 2,4,3,Gold);
    }
    Pose pose = s.rolling ? Pose::Roll : jump > 3 ? Pose::Jump : Pose::Run;
    if (!s.rolling && !jump && s.frame % 28 < 4) pose = Pose::Push;
    if (s.phase == BuddyGame::Phase::Crashed) pose = Pose::Crash;
    if (s.phase == BuddyGame::Phase::Celebrate || s.phase == BuddyGame::Phase::Complete)
      pose = s.phaseTicks % 180 < 55 ? Pose::Roll : Pose::Dance;
    buddy(c, px, 210 - jump - (s.board ? 5 : 0), pose, s.frame, s.shirt, s.mask, 1,
          s.phase == BuddyGame::Phase::Playing);
    if (s.magnet) {
      const int spread = 16 + int(s.frame % 24);
      for (int side = -1; side <= 1; side += 2) {
        c.line(px + side * spread,184 - jump,px + side * (spread + 5),180 - jump,Mint);
        c.line(px + side * spread,195 - jump,px + side * (spread + 5),197 - jump,Mint);
      }
    }
    if (s.snake) {
      snake(c, px + 17, 188 - jump, s.frame, 240);
      c.text("WHACK", 6, 215, Mint);
      c.number(s.snake, 31, 215, Cream);
    }
    if (s.hud) hud(c, s);
    if (s.phase != BuddyGame::Phase::Playing) panel(c, s);
  }

  // x is the middle of the sprite, feetY is its floor. Integer scaling preserves every pixel.
  static void drawBuddy(uint16_t* pixels, int x, int feetY, Pose pose, uint32_t frame = 0,
                        unsigned shirt = 0, unsigned mask = 1, int scale = 1) {
    if (!pixels || scale < 1 || scale > 4) return;
    Canvas c(pixels);
    buddy(c, x, feetY, pose, frame, shirt, mask, scale, pose != Pose::Dance && pose != Pose::Crash);
  }
  static void drawText(uint16_t* pixels, const char* text, int x, int y, uint16_t color, int scale = 1) {
    if (!pixels || scale < 1 || scale > 4) return;
    Canvas c(pixels); c.text(text, x, y, color, scale);
  }
  static void drawCoin(uint16_t* pixels, int x, int y, uint32_t frame = 0, int scale = 1) {
    if (!pixels || scale < 1 || scale > 4) return;
    Canvas c(pixels); coin(c, x, y, frame, scale * 256);
  }

 private:
  static constexpr uint16_t Ink = buddyRgb(37,28,45), Cream = buddyRgb(255,239,201),
    Gold = buddyRgb(255,191,71), Coral = buddyRgb(246,114,108), Mint = buddyRgb(106,242,204),
    Violet = buddyRgb(143,101,187), White = buddyRgb(255,253,234), Shadow = buddyRgb(68,47,78);
  static float bounded(float v, float low, float high) {
    return !(v >= low) ? low : v > high ? high : v;
  }
  static int abs(int n) { return n < 0 ? -n : n; }
  static int tri(uint32_t n, unsigned span) {
    const unsigned v = n % (span * 2); return int(v < span ? v : span * 2 - v);
  }
  class Canvas {
   public:
    explicit Canvas(uint16_t* p, int bottom = Height) : p_(p), bottom_(bottom < 0 ? 0 : bottom > Height ? Height : bottom) {}
    void pixel(int x, int y, uint16_t color) { if (x >= 0 && x < Width && y >= 0 && y < bottom_) p_[y * Width + x] = color; }
    void rect(int x, int y, int w, int h, uint16_t color) {
      if (w < 1 || h < 1) return;
      int x1 = x + w, y1 = y + h;
      x = x < 0 ? 0 : x; y = y < 0 ? 0 : y;
      x1 = x1 > Width ? Width : x1; y1 = y1 > bottom_ ? bottom_ : y1;
      for (int py = y; py < y1; ++py) for (int px = x; px < x1; ++px) p_[py * Width + px] = color;
    }
    void line(int x, int y, int x1, int y1, uint16_t color) {
      const int dx = abs(x1 - x), dy = -abs(y1 - y), sx = x < x1 ? 1 : -1, sy = y < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x,y,color); if (x == x1 && y == y1) break;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
      }
    }
    void ellipse(int x, int y, int w, int h, uint16_t color) {
      if (w <= 0 || h <= 0 || w > 400 || h > 400) return;
      for (int row = 0; row < h; ++row) for (int col = 0; col < w; ++col) {
        const int nx = 2 * col + 1 - w, ny = 2 * row + 1 - h;
        if (nx * nx * h * h + ny * ny * w * w <= w * w * h * h) pixel(x + col,y + row,color);
      }
    }
    void text(const char* label, int x, int y, uint16_t color, int scale = 1) {
      for (unsigned i = 0; label && label[i] && i < 40; ++i) {
        const uint16_t bits = glyph(label[i]);
        for (int row = 0; row < 5; ++row) for (int col = 0; col < 3; ++col)
          if (bits & (1u << (14 - row * 3 - col))) rect(x + col * scale,y + row * scale,scale,scale,color);
        x += 4 * scale;
      }
    }
    void label(const char* text, int x, int y, uint16_t color) {
      MenuFont::draw(p_,Width,bottom_,text,x,y,color);
    }
    void number(uint32_t n, int x, int y, uint16_t color) {
      char value[12]; snprintf(value,sizeof(value),"%lu",static_cast<unsigned long>(n)); text(value,x,y,color);
    }
   private:
    uint16_t* p_; int bottom_;
    static uint16_t glyph(char ch) {
      static const uint16_t letters[] = {
        0x2BED,0x6BAE,0x3923,0x6B6E,0x79A7,0x79A4,0x396B,0x5BED,0x7497,
        0x126A,0x5BAD,0x4927,0x5FED,0x5F6D,0x2B6A,0x6BA4,0x2B7B,0x6BAD,
        0x388E,0x7492,0x5B6F,0x5B6A,0x5BFD,0x5AAD,0x5A92,0x72A7
      };
      static const uint16_t digits[] = {0x7B6F,0x2C97,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
      if (ch >= 'a' && ch <= 'z') ch -= 32;
      if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
      if (ch >= '0' && ch <= '9') return digits[ch - '0'];
      if (ch == ':') return 0x0410;
      if (ch == '+') return 0x05D0;
      if (ch == '-') return 0x01C0;
      if (ch == '/') return 0x12A4;
      if (ch == '!') return 0x2492;
      return 0;
    }
  };
  struct Sprite {
    Canvas& c; int x, y, scale;
    void rect(int px,int py,int w,int h,uint16_t col) {
      const int x0 = x + px * scale / 256, y0 = y + py * scale / 256;
      c.rect(x0,y0,(px + w) * scale / 256 - px * scale / 256,
             (py + h) * scale / 256 - py * scale / 256,col);
    }
    void line(int x0,int y0,int x1,int y1,uint16_t col) {
      c.line(x + x0 * scale / 256,y + y0 * scale / 256,x + x1 * scale / 256,y + y1 * scale / 256,col);
    }
  };

  static void buddy(Canvas& c,int x,int feetY,Pose pose,uint32_t frame,unsigned shirt,unsigned mask,int scale,bool rear) {
    const uint16_t shirts[] = {rgb(44,38,55),rgb(107,63,139),rgb(47,109,136),rgb(175,67,83)};
    const uint16_t cloth = shirts[shirt % 4], seam = shirt % 4 ? rgb(227,151,172) : rgb(91,76,104);
    const bool beat = (frame / 7) % 2;
    int bob = pose == Pose::Run ? int(beat) : pose == Pose::Dance ? tri(frame / 4,3) - 2 : 0;
    Sprite b{c,x - 17 * scale,feetY - (36 + bob) * scale,256 * scale};
    if (rear) {
      // Back of the hood during play. LED eyes appear only on intermissions.
      if (pose == Pose::Roll) {
        b.rect(8,14,18,3,Ink); b.rect(4,18,26,13,Ink); b.rect(7,31,20,3,Ink);
        b.rect(6,19,22,11,cloth); b.rect(9,16,15,17,cloth);
        b.rect(14,18,1,13,seam); b.rect(10,30,12,1,seam);
        const int turn = int(frame / 5 % 4);
        b.rect(turn < 2 ? 2 : 27,turn % 2 ? 22 : 17,5,4,Cream);
        b.rect(turn < 2 ? 25 : 4,turn % 2 ? 30 : 28,6,3,White);
        return;
      }
      b.rect(8,19,19,13,Ink); b.rect(6,22,23,7,Ink);
      b.rect(8,21,19,11,cloth); b.rect(10,30,15,1,seam);
      b.rect(5,4,24,17,Ink); b.rect(8,1,18,22,Ink); b.rect(11,0,12,2,Ink);
      b.rect(7,5,20,14,cloth); b.rect(10,3,14,18,cloth);
      b.rect(16,4,1,14,seam); b.rect(11,20,12,1,seam);
      b.rect(10,16,1,3,seam); b.rect(23,16,1,3,seam);
      // A small stitched lightning badge distinguishes the back from the mask.
      b.rect(15,24,4,1,Gold); b.rect(14,25,4,1,Gold); b.rect(16,26,3,1,Gold);
      const int ly = pose == Pose::Jump ? 18 : pose == Pose::Push ? 30 : beat ? 24 : 27;
      const int ry = pose == Pose::Jump ? 16 : pose == Pose::Push ? 30 : beat ? 27 : 24;
      b.rect(3,ly - 3,5,5,Ink); b.rect(1,ly,6,4,Cream);
      b.rect(27,ry - 3,5,5,Ink); b.rect(28,ry,6,4,Cream);
      const int lf = pose == Pose::Jump ? 29 : beat ? 32 : 33;
      const int rf = pose == Pose::Jump ? 28 : beat ? 33 : 32;
      b.rect(7,lf,8,3,Ink); b.rect(8,lf,7,2,White);
      b.rect(19,rf,8,3,Ink); b.rect(19,rf,7,2,White);
      return;
    }
    if (pose == Pose::Roll) {
      b.rect(8,14,18,3,Ink); b.rect(4,18,26,13,Ink); b.rect(7,31,20,3,Ink);
      b.rect(6,19,22,11,cloth); b.rect(9,16,15,17,cloth);
      b.rect(10,19,14,10,rgb(19,25,33));
      if (mask % 4 == 1) {
        b.rect(12,21,3,6,Coral); b.rect(13,20,2,6,Gold); b.rect(13,23,1,3,White);
        b.rect(20,21,3,6,Coral); b.rect(21,19,2,7,Gold); b.rect(21,23,1,3,White);
      } else {
        const uint16_t led = mask % 4 == 2 ? Coral : Mint;
        b.rect(12,21,3,2,led); b.rect(20,21,3,2,led); b.rect(15,26,5,1,Gold);
      }
      const int turn = (frame / 5) % 4;
      b.rect(turn < 2 ? 2 : 27,turn % 2 ? 22 : 17,5,4,Cream);
      b.rect(turn < 2 ? 25 : 4,turn % 2 ? 30 : 28,6,3,White);
      b.rect(9,31,8,1,seam);
      return;
    }
    if (pose == Pose::Crash) {
      b.rect(4,22,25,10,Ink); b.rect(7,19,20,15,cloth);
      b.rect(12,21,15,10,rgb(19,25,33)); b.rect(14,23,2,2,Coral); b.rect(22,27,2,2,Coral);
      b.rect(14,27,2,2,Coral); b.rect(22,23,2,2,Coral);
      b.rect(0,29,5,4,Cream); b.rect(26,32,7,3,White);
      return;
    }
    // The sweatshirt touches the shoes. Little Buddies have no legs.
    b.rect(8,19,19,13,Ink); b.rect(6,22,23,7,Ink); b.rect(8,21,19,10,cloth);
    b.rect(9,30,17,3,cloth); b.rect(10,30,15,1,seam);
    b.rect(13,24,9,5,seam); b.rect(14,24,7,4,cloth);
    b.rect(5,4,24,17,Ink); b.rect(8,1,18,22,Ink); b.rect(11,0,12,2,Ink);
    b.rect(7,5,20,14,cloth); b.rect(10,3,14,18,cloth);
    b.rect(9,6,16,13,rgb(15,24,31)); b.rect(8,8,18,9,rgb(15,24,31));
    b.rect(11,4,12,1,seam); b.rect(8,8,1,7,seam);
    if (mask % 4 == 1) {
      for (unsigned eye = 0; eye < 2; ++eye) {
        const int ex = 11 + eye * 8;
        const int flicker = (frame / 5 + eye) % 3;
        b.rect(ex,10,5,6,rgb(255,96,69)); b.rect(ex + 1,8 - flicker,3,8 + flicker,Gold);
        b.rect(ex + 2,11,1,4,White); b.rect(ex + 4,12,1,4,Coral);
      }
      b.rect(14,18,7,1,Gold);
    } else if (mask % 4 == 0) {
      b.rect(11,10,4,2,Mint); b.rect(20,10,4,2,Mint);
      b.rect(12,14,2,2,Mint); b.rect(21,14,2,2,Mint); b.rect(14,16,7,1,Mint);
    } else if (mask % 4 == 2) {
      b.rect(11,8,2,5,Coral); b.rect(21,8,2,5,Coral);
      b.rect(12,11,3,2,Coral); b.rect(19,11,3,2,Coral);
      b.rect(15,14,5,1,Gold); b.rect(17,15,1,2,Gold);
      b.rect(10,15,3,1,Mint); b.rect(22,15,3,1,Mint);
    } else {
      for (unsigned i = 0; i < 9; ++i) {
        const unsigned h = i * 31 + frame / 3;
        b.rect(10 + (h % 13),8 + (h / 3 % 9),2,1,i % 2 ? Mint : Coral);
      }
    }
    const bool push = pose == Pose::Push;
    const int ly = push ? 30 : pose == Pose::Jump ? 19 : pose == Pose::Dance ? (beat ? 10 : 25) : (beat ? 23 : 26);
    const int ry = push ? 30 : pose == Pose::Jump ? 16 : pose == Pose::Dance ? (beat ? 25 : 10) : (beat ? 26 : 23);
    b.rect(3,ly - 3,5,5,Ink); b.rect(1,ly,6,4,Cream); b.rect(2,ly + 3,4,1,rgb(222,163,141));
    b.rect(27,ry - 3,5,5,Ink); b.rect(28,ry,6,4,Cream); b.rect(29,ry + 3,4,1,rgb(222,163,141));
    const int leftFoot = pose == Pose::Jump ? 29 : (beat && pose == Pose::Run ? 32 : 33);
    const int rightFoot = pose == Pose::Jump ? 28 : (!beat && pose == Pose::Run ? 32 : 33);
    b.rect(7,leftFoot,8,3,Ink); b.rect(8,leftFoot,7,2,White); b.rect(19,rightFoot,8,3,Ink); b.rect(19,rightFoot,7,2,White);
  }

  static void coin(Canvas& c,int x,int y,uint32_t frame,int scale) {
    const int width = (frame / 7) % 4 == 1 ? 3 : (frame / 7) % 4 == 3 ? 5 : 7;
    Sprite b{c,x - width * scale / 512,y - 4 * scale / 256,scale};
    b.rect(1,0,width - 2,1,rgb(158,99,43)); b.rect(0,1,width,6,rgb(158,99,43));
    b.rect(1,1,width - 2,5,Gold); b.rect(1,1,width - 2,1,Cream);
    if (width > 3) b.rect(3,2,1,3,rgb(211,123,37));
    b.rect(1,6,width - 2,1,rgb(242,155,44));
  }
  static void hoverboard(Canvas& c,int x,int y,uint32_t frame,int scale) {
    Sprite b{c,x - 17 * scale / 256,y - 3 * scale / 256,scale};
    b.rect(3,7,28,1,Mint); b.rect(0,0,34,4,Ink); b.rect(2,0,30,2,Mint);
    b.rect(6,0,21,1,White); b.rect(3,4,7,2,Violet); b.rect(23,4,7,2,Violet);
    for (int i = 0; i < 3; ++i) b.rect(6 + i * 10,7 + (frame / 4 + i) % 3,3,1,Mint);
  }
  static void snake(Canvas& c,int x,int y,uint32_t frame,int scale) {
    Sprite b{c,x - 6 * scale / 256,y - 18 * scale / 256,scale};
    b.rect(3,8,4,14,Ink); b.rect(4,9,2,12,Mint);
    b.rect(0,5,9,5,Ink); b.rect(1,5,7,3,Mint); b.rect(5,1,9,5,Ink);
    b.rect(6,2,7,3,Mint); b.rect(11,2,1,1,Ink); b.rect(13,4,3,1,Coral);
    if ((frame / 6) % 2) b.rect(1,13,4,2,Mint);
  }

  static void world(Canvas& c,BuddyGame::Theme theme,uint32_t frame) {
    uint16_t sky = rgb(237,190,174), wall = rgb(182,121,142), floor = rgb(184,157,182), tile = rgb(204,180,199);
    if (theme == BuddyGame::Theme::School) { sky = rgb(245,212,151); wall = rgb(124,151,140); floor = rgb(177,175,142); tile = rgb(201,197,163); }
    if (theme == BuddyGame::Theme::Park) { sky = rgb(242,207,166); wall = rgb(88,133,109); floor = rgb(174,166,112); tile = rgb(207,188,135); }
    if (theme == BuddyGame::Theme::Volcano) { sky = rgb(103,68,94); wall = rgb(89,60,75); floor = rgb(102,81,96); tile = rgb(136,103,107); }
    if (theme == BuddyGame::Theme::Noodles) { sky = rgb(139,77,104); wall = rgb(90,59,83); floor = rgb(165,112,113); tile = rgb(190,133,126); }
    c.rect(0,0,Width,Height,Ink); c.rect(0,0,Width,91,sky);
    c.rect(0,77,Width,163,wall);
    if (theme == BuddyGame::Theme::Mall || theme == BuddyGame::Theme::School) {
      c.rect(43,27,49,42,Cream); c.rect(46,30,43,36,rgb(203,161,163));
      for (int i = 0; i < 4; ++i) c.rect(47 + i * 11,30,2,36,Cream);
      c.rect(46,46,43,2,Cream); c.rect(42,69,51,5,wall);
      for (int side = 0; side < 2; ++side) {
        const int x = side ? 105 : 0;
        c.rect(x,29,30,83,wall); c.rect(x + 3,34,24,27,rgb(108,83,110));
        c.rect(x + 5,36,20,23,rgb(243,202,161));
        c.rect(x + 7,39,7,17,Coral); c.rect(x + 17,39,6,17,Violet);
        c.rect(x,65,30,9,Cream); c.rect(x,65,30,2,Ink);
        if (theme == BuddyGame::Theme::Mall) {
          for (int awning = 0; awning < 6; ++awning) c.rect(x + awning * 5,66,4,7,awning % 2 ? Cream : Coral);
          c.rect(x + 3,78,24,30,rgb(113,80,117)); c.rect(x + 6,81,18,24,rgb(225,166,162));
          c.rect(x + 10,83,1,22,Cream); c.rect(x + 19,83,1,22,Cream);
        } else {
          for (int locker = 0; locker < 3; ++locker) {
            c.rect(x + locker * 10,67,9,41,rgb(99,132,125));
            c.rect(x + 3 + locker * 10,71,4,1,Cream); c.rect(x + 6 + locker * 10,85,1,3,Gold);
          }
        }
      }
      c.text(theme == BuddyGame::Theme::Mall ? "TEES" : "101",4,20,Ink);
      c.text(theme == BuddyGame::Theme::Mall ? "SNAX" : "102",105,20,Ink);
    } else if (theme == BuddyGame::Theme::Park) {
      c.ellipse(89,32,22,22,Cream);
      for (int i = 0; i < 6; ++i) {
        const int x = i < 3 ? i * 12 - 8 : 106 + (i - 3) * 12;
        c.rect(x + 8,61,4,53,rgb(104,84,74));
        c.ellipse(x - 6,33,32,44,i % 2 ? rgb(88,139,110) : rgb(118,158,114));
        c.ellipse(x,35,20,21,rgb(154,183,127));
      }
      c.line(0,113,44,74,Cream); c.line(134,113,89,74,Cream);
      c.rect(2,111,29,3,rgb(87,109,88)); c.rect(105,111,28,3,rgb(87,109,88));
    } else if (theme == BuddyGame::Theme::Volcano) {
      for (int y = 0; y < 65; ++y) {
        const int width = 5 + y * 3 / 2;
        c.rect(67 - width / 2,31 + y,width,1,y < 8 ? Gold : wall);
      }
      c.rect(61,37,12,5,Coral); c.rect(64,42,7,11,Gold); c.rect(67,50,6,14,Coral);
      for (int i = 0; i < 18; ++i) {
        const int x = (i * 41 + frame / 7) % 135, y = 30 + (i * 19 + frame / 3) % 75;
        c.rect(x,y,1,2,i % 3 ? Coral : Gold);
      }
    } else {
      for (int side = 0; side < 2; ++side) {
        const int x = side ? 106 : 0;
        c.rect(x,33,29,80,Shadow); c.rect(x + 3,40,23,28,rgb(167,90,105));
        c.rect(x + 5,44,19,19,rgb(235,160,118)); c.rect(x + 9,47,11,1,Gold);
        c.rect(x + 9,51,11,1,Gold); c.rect(x + 9,55,11,1,Gold);
        c.rect(x,72,29,7,Coral); c.rect(x + 3,81,24,25,rgb(110,71,96));
      }
      c.line(0,29,134,45,Ink);
      for (int i = 0; i < 5; ++i) {
        const int x = 12 + i * 27, y = 31 + i * 3;
        c.rect(x - 1,y,9,2,Gold); c.ellipse(x - 4,y + 2,15,18,Coral);
        c.rect(x + 2,y + 3,2,15,Gold); c.rect(x - 1,y + 20,9,2,Gold);
      }
    }
    // Each scanline is a section of a three-lane path with a single vanishing point.
    for (int y = 73; y < Height; ++y) {
      const int d = y - 73, half = 20 + d * 87 / 167;
      c.rect(67 - half,y,half * 2 + 1,1,floor);
      c.pixel(67 - half,y,Cream); c.pixel(67 + half,y,Cream);
      for (int lane = -1; lane <= 1; lane += 2) {
        const int x = 67 + lane * half / 3;
        c.pixel(x,y,tile);
      }
    }
    const unsigned scroll = frame % 84;
    for (int row = 0; row < 8; ++row) {
      const int d = (row * 21 + int(scroll / 4)) % 168;
      const int y = 74 + d * d / 168;
      const int half = 20 + (y - 73) * 87 / 167;
      c.line(67 - half,y,67 + half,y,tile);
    }
    if (theme == BuddyGame::Theme::Volcano) {
      for (int y = 110; y < Height; ++y) {
        const int edge = 27 - (y - 110) / 4;
        c.rect(0,y,edge > 0 ? edge : 3,1,(y + frame / 3) % 13 < 6 ? Coral : Gold);
        c.rect(135 - (edge > 0 ? edge : 3),y,edge > 0 ? edge : 3,1,(y + frame / 3) % 17 < 8 ? Coral : Gold);
      }
    }
  }

  static void obstacle(Canvas& c,const Object& object,BuddyGame::Theme theme,uint32_t frame) {
    const float d = bounded(object.depth,0,1.22f);
    const int floorY = 73 + int(d * d * 141);
    const int x = 67 + int((bounded(object.lane,0,2) - 1) * (12 + d * 31));
    const int scale = 70 + int(d * 186);
    if (object.kind == BuddyGame::ObjectKind::Coin) { coin(c,x,floorY - ((object.variant & BuddyGame::AirCoin) ? 14 + int((object.variant >> 10) & 3) * 12 + object.lift : 8 + object.lift) * scale / 256,frame + (object.variant & 3),scale); return; }
    if (object.kind == BuddyGame::ObjectKind::Hoverboard) { hoverboard(c,x,floorY - 4,frame,scale); return; }
    if (object.kind == BuddyGame::ObjectKind::Snake) { snake(c,x,floorY - 3,frame,scale); return; }
    Sprite b{c,x - 18 * scale / 256,floorY - 70 * scale / 256,scale};
    if (object.kind == BuddyGame::ObjectKind::Person || object.kind == BuddyGame::ObjectKind::Straddle) {
      const uint16_t skin = object.variant % 3 == 0 ? rgb(228,161,124) : object.variant % 3 == 1 ? rgb(154,101,83) : rgb(244,195,151);
      const uint16_t top = object.variant % 2 ? rgb(234,169,98) : rgb(110,138,153);
      const uint16_t pants = rgb(75,63,88);
      b.rect(13,0,12,13,Ink); b.rect(13,3,12,11,skin); b.rect(12,0,13,5,rgb(84,60,61));
      b.rect(13,5,2,3,rgb(84,60,61)); b.rect(22,6,1,1,Ink);
      b.rect(9,14,20,24,Ink); b.rect(10,14,18,23,top); b.rect(11,16,4,18,rgb(195,179,149));
      b.rect(5,17,5,23,Ink); b.rect(6,18,4,20,top); b.rect(6,37,4,5,skin);
      b.rect(28,17,5,23,Ink); b.rect(28,18,4,20,top); b.rect(28,37,4,5,skin);
      b.rect(13,33,6,4,pants); b.rect(14,33,4,1,Cream);
      if (object.kind == BuddyGame::ObjectKind::Straddle) {
        for (int y = 36; y < 66; ++y) {
          const int spread = (y - 36) * 8 / 30;
          b.rect(10 - spread,y,8,1,pants); b.rect(21 + spread,y,8,1,pants);
          b.rect(11 - spread,y,2,1,rgb(110,91,119));
        }
        b.rect(0,65,12,5,Ink); b.rect(27,65,12,5,Ink);
        b.rect(0,68,12,1,Cream); b.rect(27,68,12,1,Cream);
        b.rect(17,44,4,9,Gold); b.rect(14,50,10,3,Gold); b.rect(16,53,6,2,Gold); b.rect(18,55,2,2,Gold);
      } else {
        b.rect(10,36,9,29,pants); b.rect(20,36,9,29,pants);
        b.rect(12,37,2,26,rgb(110,91,119)); b.rect(22,37,2,26,rgb(110,91,119));
        b.rect(6,65,13,5,Ink); b.rect(20,65,13,5,Ink); b.rect(6,68,13,1,Cream); b.rect(20,68,13,1,Cream);
      }
      return;
    }
    Sprite item{c,x - 12 * scale / 256,floorY - 23 * scale / 256,scale};
    if (object.kind == BuddyGame::ObjectKind::Balloon) {
      item.rect(11,15,1,10,Cream); item.rect(9,15,5,2,Gold);
      item.rect(3,0,17,14,Ink); item.rect(5,1,13,12,Coral); item.rect(6,2,3,6,Cream);
    } else if (object.kind == BuddyGame::ObjectKind::Magnet) {
      item.rect(3,2,5,16,Ink); item.rect(17,2,5,16,Ink); item.rect(5,16,15,5,Ink);
      item.rect(4,3,3,13,Coral); item.rect(18,3,3,13,Mint); item.rect(6,16,13,3,Coral);
      item.rect(4,3,3,4,Cream); item.rect(18,3,3,4,Cream);
    } else if (object.kind == BuddyGame::ObjectKind::Multiplier) {
      item.rect(2,2,21,20,Ink); item.rect(4,4,17,16,Gold);
      item.line(7,8,17,17,Ink); item.line(17,8,7,17,Ink);
      item.rect(9,1,7,2,Cream);
    } else if (object.kind == BuddyGame::ObjectKind::Mystery) {
      item.rect(1,3,23,19,Ink); item.rect(3,5,19,15,Violet);
      item.rect(10,4,4,17,Gold); item.rect(2,10,21,3,Gold);
      item.rect(7,1,5,4,Coral); item.rect(14,1,5,4,Coral);
    } else if (object.kind == BuddyGame::ObjectKind::Electronics) {
      item.rect(4,3,17,17,Ink); item.rect(6,5,13,13,Mint); item.rect(10,8,6,7,Shadow);
      for (int i = 0; i < 4; ++i) { item.rect(3 + i * 6,0,2,4,Gold); item.rect(3 + i * 6,19,2,4,Gold); }
    } else if (object.kind == BuddyGame::ObjectKind::Shirt) {
      item.rect(7,2,11,19,Ink); item.rect(1,4,23,8,Ink); item.rect(8,3,9,17,Violet);
      item.rect(2,5,21,6,Violet); item.rect(10,3,5,3,Cream); item.rect(11,10,4,4,Gold);
    } else if (theme == BuddyGame::Theme::School) {
      item.rect(0,8,25,14,Ink); item.rect(1,9,23,5,Coral); item.rect(3,14,20,3,Cream);
      item.rect(2,18,22,3,Violet); item.rect(3,5,17,4,Gold); item.rect(5,5,13,1,Cream);
    } else if (theme == BuddyGame::Theme::Park) {
      item.rect(1,5,23,14,Ink); item.rect(2,6,21,4,rgb(171,113,83)); item.rect(2,12,21,5,rgb(171,113,83));
      item.rect(4,18,3,5,Ink); item.rect(19,18,3,5,Ink); item.rect(1,19,23,2,rgb(171,113,83));
    } else if (theme == BuddyGame::Theme::Volcano) {
      item.rect(4,3,15,20,Ink); item.rect(1,9,23,12,Ink); item.rect(4,6,16,14,rgb(131,85,87));
      item.rect(8,4,9,4,Coral); item.rect(6,11,5,2,Gold); item.rect(11,13,4,5,Coral); item.rect(18,15,3,5,Gold);
    } else if (theme == BuddyGame::Theme::Noodles) {
      item.rect(0,7,25,4,Cream); item.rect(2,11,21,6,Coral); item.rect(5,17,15,4,Coral); item.rect(8,21,9,2,Ink);
      for (int i = 0; i < 4; ++i) item.rect(4 + i * 5,5 - i % 2,3,4,Gold);
      item.rect(5,1,1,6,Cream); item.rect(18,-2,1,8,Cream); item.line(14,3,23,-6,Ink);
    } else if (object.variant % 3 == 1) {
      item.rect(3,4,19,14,Ink); item.rect(4,5,17,12,Violet); item.rect(6,2,13,2,Ink);
      item.rect(5,17,19,3,Ink); item.rect(2,19,5,4,Ink); item.rect(20,19,5,4,Ink);
      item.line(21,6,27,-4,Ink); item.rect(7,5,8,5,Coral);
    } else {
      item.rect(3,4,19,18,Ink); item.rect(4,5,17,15,Coral); item.rect(8,0,11,5,Ink);
      item.rect(10,1,7,4,Cream); item.rect(6,8,13,1,rgb(255,168,136)); item.rect(8,11,9,6,Gold);
    }
    item.rect(8,0,9,1,Cream);
  }

  static const char* sectionName(BuddyGame::Section section) {
    switch (section) {
      case BuddyGame::Section::Slalom: return "SIDEWALK SLALOM";
      case BuddyGame::Section::Hurdles: return "HOP TO IT";
      case BuddyGame::Section::Underpass: return "MIND YOUR HEAD";
      case BuddyGame::Section::Crowd: return "RUSH HOUR";
      case BuddyGame::Section::Rhythm: return "HOP ROLL REPEAT";
      case BuddyGame::Section::CoinRush: return "POCKET MONEY";
      case BuddyGame::Section::Breather: return "CATCH YOUR BREATH";
      default: return "LITTLE FEET BIG PLANS";
    }
  }
  static void hud(Canvas& c,const Scene& s) {
    c.rect(0,0,Width,31,Ink);
    char label[24];
    snprintf(label,sizeof(label),"%lu  x%u",static_cast<unsigned long>(s.score),s.multiplier);
    c.label(label,4,1,Gold);
    if (s.coins < 1000) snprintf(label,sizeof(label),"%lu",static_cast<unsigned long>(s.coins));
    else snprintf(label,sizeof(label),"%luK",static_cast<unsigned long>(s.coins / 1000));
    coin(c,102,7,s.frame,180); c.label(label,108,1,Cream);
    c.label(sectionName(s.section),5,14,Mint);
    c.rect(5,26,125,2,Shadow);
    c.rect(5,26,int(s.sectionProgress > 125 ? 125 : s.sectionProgress),2,Mint);
    c.rect(0,216,Width,24,Ink);
    const char* goal = s.goalIndex == 0 ? "COINS" : s.goalIndex == 1 ? "JUMP" : "ROLL";
    snprintf(label,sizeof(label),"%s %u/%u",goal,s.goalValue,s.goalTarget);
    c.label(s.reward ? "POCKET PRIZE!" : label,5,216,s.reward ? Gold : Cream);
    c.label("M5 JUMP R ROLL",10,228,rgb(194,167,189));
  }
  static void panel(Canvas& c,const Scene& s) {
    const bool win = s.phase == BuddyGame::Phase::Celebrate || s.phase == BuddyGame::Phase::Complete;
    if (win) {
      for (unsigned i = 0; i < 30; ++i) {
        const int x = (i * 43 + s.frame / 4) % Width, y = 30 + (i * 29 + s.frame * (1 + i % 2)) % 150;
        c.rect(x,y,2,3,i % 3 == 0 ? Mint : i % 3 == 1 ? Gold : Coral);
      }
    }
    c.rect(10,81,119,66,Shadow); c.rect(7,78,119,66,Ink); c.rect(9,80,115,62,Cream);
    const char* title = win ? "BUDDY BOOGIE" : "POCKET CHECK";
    c.text(title,win ? 20 : 20,88,Ink,2);
    c.text(win ? "YOU MADE IT. DANCE." : "WHO PUT THAT THERE?",win ? 30 : 28,106,Shadow);
    c.text("SCORE",22,119,Shadow); c.number(s.score,54,119,Ink);
    c.text(win ? "A  KEEP ROLLING" : "A  TRY AGAIN",win ? 37 : 43,132,rgb(153,73,105));
  }
};

}  // namespace flappy
