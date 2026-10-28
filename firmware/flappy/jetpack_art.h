#pragma once

#include <stdint.h>
#ifdef M5FLAP_JETPACK_IMPORTED
#include "jetpack_imported.h"
#endif

namespace flappy { namespace JetpackArt {

constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
}

enum Ink : uint8_t {
  Outline, HairShade, Hair, HairLight, SkinShade, Skin, SkinLight, Eye,
  ShirtShade, Shirt, ShirtLight, TrousersShade, Trousers, BootShade, Boot,
  MetalShade, Metal, MetalLight, BrassShade, Brass, BrassLight,
  FireShade, Fire, FireLight, ArmorShade, Armor, ArmorLight,
  GravityShade, Gravity, GravityLight, WallShade, Wall, WallLight,
  PanelShade, Panel, PanelLight, LampShade, Lamp, LampLight, Screen,
  RedShade, Red, RedLight, BlueShade, Blue, BlueLight, Fur, FurLight, InkCount
};

inline uint16_t color(Ink ink) {
  static const uint16_t colors[] = {
    rgb(14, 19, 32), rgb(48, 27, 30), rgb(104, 54, 34), rgb(168, 101, 50),
    rgb(173, 96, 66), rgb(240, 167, 101), rgb(255, 216, 151), rgb(250, 249, 218),
    rgb(22, 79, 97), rgb(39, 143, 157), rgb(112, 213, 205),
    rgb(29, 30, 56), rgb(68, 64, 91), rgb(41, 29, 32), rgb(110, 65, 43),
    rgb(46, 58, 73), rgb(102, 126, 140), rgb(190, 212, 212),
    rgb(122, 83, 32), rgb(220, 165, 53), rgb(255, 224, 113),
    rgb(203, 65, 33), rgb(255, 145, 36), rgb(255, 237, 139),
    rgb(37, 84, 72), rgb(69, 155, 106), rgb(158, 222, 153),
    rgb(62, 48, 105), rgb(128, 96, 185), rgb(211, 180, 239),
    rgb(16, 29, 43), rgb(23, 41, 57), rgb(30, 50, 64),
    rgb(20, 35, 49), rgb(35, 55, 69), rgb(49, 71, 82),
    rgb(76, 86, 84), rgb(121, 133, 118), rgb(183, 187, 146), rgb(57, 108, 106),
    rgb(113, 39, 52), rgb(195, 58, 65), rgb(249, 119, 97),
    rgb(29, 63, 120), rgb(48, 116, 193), rgb(135, 207, 238),
    rgb(205, 174, 97), rgb(252, 236, 178)
  };
  return colors[ink < InkCount ? ink : Outline];
}

template<class Canvas> class RunnerBrush {
 public:
  RunnerBrush(Canvas& canvas, int x, int y, bool flipped) : c_(canvas), x_(x - 11), y_(y - 7), flipped_(flipped) {}
  void pixel(int x, int y, Ink ink) { c_.pixel(x_ + x, y_ + (flipped_ ? 14 - y : y), color(ink)); }
  void rect(int x, int y, int w, int h, Ink ink) {
    for (int row = 0; row < h; ++row)
      for (int col = 0; col < w; ++col) pixel(x + col, y + row, ink);
  }
 private:
  Canvas& c_;
  int x_, y_;
  bool flipped_;
};

template<class Canvas>
void drawRunner(Canvas& canvas, int x, int y, uint32_t frame, bool thrust,
                bool onFloor, bool crash, uint8_t suit, bool gravityUp) {
#ifdef M5FLAP_JETPACK_IMPORTED
  if (JetpackImported::HasRunner && !crash && suit <= 2 && JetpackImported::FrameCount >= 6) {
    const unsigned pose = onFloor ? unsigned(frame / 6u) % 4u : 4u + unsigned(frame / 7u) % 2u;
    if (pose < JetpackImported::FrameCount) {
      for (unsigned row = 0; row < 15; ++row)
        for (unsigned col = 0; col < 20; ++col) {
          const uint16_t pixel = JetpackImported::RunnerPixels[pose][row * 20 + col];
          if (pixel != 0xf81f) canvas.pixel(x - 11 + int(col),
              y - 7 + (suit == 2 && gravityUp ? 14 - int(row) : int(row)), pixel);
        }
      return;
    }
  }
#endif
  RunnerBrush<Canvas> p(canvas, x, y, suit == 2 && gravityUp && !crash);
  const Ink clothDark = suit == 1 ? ArmorShade : suit == 2 ? GravityShade : ShirtShade;
  const Ink cloth = suit == 1 ? Armor : suit == 2 ? Gravity : Shirt;
  const Ink clothLight = suit == 1 ? ArmorLight : suit == 2 ? GravityLight : ShirtLight;
  if (crash) {
    p.rect(1, 11, 7, 3, Outline); p.rect(1, 11, 3, 2, Boot);
    p.rect(4, 10, 4, 3, Trousers); p.rect(7, 9, 7, 5, Outline);
    p.rect(7, 9, 6, 3, cloth); p.rect(8, 9, 4, 1, clothLight);
    p.rect(8, 12, 6, 2, clothDark); p.rect(6, 8, 3, 3, MetalShade);
    p.rect(6, 8, 2, 2, Brass); p.pixel(6, 8, BrassLight);
    p.rect(13, 8, 5, 6, HairShade); p.rect(14, 9, 5, 5, SkinShade);
    p.rect(14, 9, 4, 3, Skin); p.rect(15, 9, 3, 1, SkinLight);
    p.rect(13, 8, 4, 2, Hair); p.pixel(14, 8, HairLight);
    p.pixel(17, 10, Outline); p.pixel(18, 11, SkinLight);
    p.rect(13, 13, 5, 2, HairShade); p.rect(10, 13, 4, 1, Skin);
    p.pixel(19, 12, SkinShade);
    return;
  }

  const unsigned stride = unsigned(frame / 6u) & 3u;
  const unsigned flying = unsigned(frame / 7u) & 1u;
  const int bob = onFloor && (stride == 1 || stride == 3) ? 1 : 0;

  // Rear arm, harness and tank sit behind the face and chest.
  p.rect(7, 7 + bob, 4, 5, Outline);
  p.rect(7, 8 + bob, 3, 3, clothDark);
  p.rect(3, 5 + bob, 4, 7 - bob, Outline);
  p.rect(3, 6 + bob, 3, 5 - bob, Metal);
  p.rect(3, 6 + bob, 1, 4 - bob, MetalLight);
  p.rect(4, 5 + bob, 3, 2, Brass);
  p.pixel(4, 5 + bob, BrassLight); p.pixel(6, 6 + bob, BrassShade);
  p.rect(4, 8, 2, 1, MetalShade); p.pixel(5, 9, BrassLight);
  p.rect(3, 11, 4, 1, MetalShade); p.rect(4, 12, 2, 1, Outline);
  p.rect(7, 5 + bob, 1, 5, BrassShade); p.pixel(7, 5 + bob, BrassLight);

  p.rect(8, 7 + bob, 7, 5 - bob, Outline);
  p.rect(9, 7 + bob, 5, 4 - bob, cloth);
  p.rect(9, 7 + bob, 2, 3 - bob, clothLight);
  p.rect(8, 10, 5, 2, clothDark); p.rect(10, 11, 5, 1, TrousersShade);
  p.rect(11, 6 + bob, 3, 2, SkinShade);
  p.pixel(12, 7 + bob, Skin);
  p.rect(8, 7 + bob, 1, 4 - bob, BrassShade);
  p.pixel(9, 10, BrassLight);

  // A broad quiff and a lit cheek keep the face readable at native size.
  p.rect(10, bob, 6, 1, HairShade);
  p.rect(9, 1 + bob, 9, 2, HairShade);
  p.rect(10, 1 + bob, 6, 1, HairLight);
  p.rect(9, 2 + bob, 6, 3, Hair);
  p.rect(10, 2 + bob, 3, 1, HairLight);
  p.rect(11, 3 + bob, 6, 4, SkinShade);
  p.rect(12, 3 + bob, 5, 3, Skin);
  p.rect(13, 3 + bob, 4, 1, SkinLight);
  p.rect(9, 3 + bob, 2, 3, HairShade);
  p.rect(10, 4 + bob, 2, 2, Skin);
  p.pixel(10, 4 + bob, SkinLight); p.pixel(10, 5 + bob, SkinShade);
  p.rect(14, 3 + bob, 3, 1, HairShade);
  p.rect(14, 4 + bob, 3, 2, Eye);
  p.rect(16, 4 + bob, 1, 2, Outline);
  p.rect(17, 5 + bob, 2, 1, SkinLight); p.pixel(19, 5 + bob, SkinShade);
  p.rect(15, 6 + bob, 3, 1, SkinShade);
  p.pixel(16, 6 + bob, HairShade); p.pixel(17, 6 + bob, SkinLight);
  p.rect(11, 6 + bob, 2, 1, HairShade);

  const bool armForward = !onFloor || stride == 0 || stride == 3;
  if (armForward) {
    p.rect(14, 8 + bob, 2, 2, clothDark); p.pixel(14, 8 + bob, clothLight);
    p.rect(15, 9, 3, 2, SkinShade); p.rect(16, 9, 3, 1, Skin);
    p.pixel(18, 9, SkinLight);
  } else {
    p.rect(12, 8 + bob, 3, 2, clothDark); p.pixel(12, 8 + bob, clothLight);
    p.rect(11, 10, 3, 1, SkinShade); p.pixel(11, 10, Skin);
  }

  if (onFloor) {
    if (stride == 0) {
      p.rect(8, 12, 3, 2, TrousersShade); p.rect(7, 13, 4, 2, BootShade);
      p.rect(7, 13, 3, 1, Boot); p.rect(12, 12, 3, 2, Trousers);
      p.rect(13, 14, 4, 1, Outline); p.rect(13, 13, 3, 1, Boot);
    } else if (stride == 1) {
      p.rect(10, 12, 3, 2, TrousersShade); p.rect(9, 14, 4, 1, BootShade);
      p.rect(12, 12, 3, 2, Trousers); p.rect(12, 14, 5, 1, Boot);
      p.pixel(16, 14, Outline);
    } else if (stride == 2) {
      p.rect(12, 12, 3, 2, TrousersShade); p.rect(13, 13, 4, 2, BootShade);
      p.rect(13, 13, 3, 1, Boot); p.rect(8, 12, 3, 2, Trousers);
      p.rect(6, 14, 5, 1, Outline); p.rect(6, 13, 4, 1, Boot);
    } else {
      p.rect(9, 12, 3, 2, TrousersShade); p.rect(8, 14, 4, 1, BootShade);
      p.rect(11, 12, 3, 2, Trousers); p.rect(10, 14, 5, 1, Boot);
      p.pixel(14, 14, Outline);
    }
  } else {
    p.rect(8, 12, 3, 2, TrousersShade);
    p.rect(7 - int(flying), 13, 4, 2, BootShade);
    p.rect(7 - int(flying), 13, 3, 1, Boot);
    p.rect(12, 12, 3, 1 + int(flying), Trousers);
    p.rect(13, 12 + int(flying), 4, 2, BootShade);
    p.rect(14, 12 + int(flying), 3, 1, Boot);
  }

  if (suit) {
    p.pixel(10, 8 + bob, MetalLight); p.rect(10, 9, 3, 1, clothLight);
    if (suit == 2) {
      p.pixel(6, 7, GravityLight); p.pixel(6, 9, Gravity);
      p.rect(11, 10, 2, 1, Eye);
    } else p.rect(5, 7, 1, 2, ArmorLight);
  }
  if (thrust && suit != 2 && !onFloor) {
    p.rect(3, 12, 4, 2, FireShade); p.rect(4, 12, 2, 2, Fire);
    p.pixel(4, 12, FireLight); p.pixel(5, 13, FireLight);
    p.pixel(flying ? 3 : 6, 14, Fire); p.pixel(flying ? 5 : 4, 14, FireLight);
    p.pixel(flying ? 1 : 8, 13, FireShade); p.pixel(flying ? 0 : 7, 14, BrassLight);
  }
}

template<class Canvas>
void drawCoin(Canvas& c, int x, int y, uint32_t frame) {
  const unsigned pose = unsigned(frame / 6u) & 3u;
  const int half = pose == 0 ? 2 : pose == 2 ? 0 : 1;
  c.rect(x - half, y - 4, half * 2 + 1, 9, color(BrassShade));
  c.rect(x - half - 1, y - 2, half * 2 + 3, 5, color(BrassShade));
  c.rect(x - half, y - 3, half * 2 + 1, 7, color(Brass));
  c.rect(x - half, y - 2, half + 1, 4, color(BrassLight));
  c.pixel(x, y - 3, color(FireLight));
  if (half) {
    c.rect(x + half, y - 1, 1, 3, color(BrassShade));
    c.pixel(x + half - 1, y + 2, color(BrassLight));
  }
}

template<class Canvas>
void drawGem(Canvas& c, int x, int y, uint32_t frame) {
  c.line(x - 3, y - 2, x, y - 5, color(Outline));
  c.line(x, y - 5, x + 3, y - 2, color(Outline));
  c.line(x - 3, y - 2, x, y + 4, color(Outline));
  c.line(x + 3, y - 2, x, y + 4, color(Outline));
  c.rect(x - 2, y - 2, 5, 3, color(Blue));
  c.line(x - 2, y - 2, x, y + 3, color(BlueLight));
  c.line(x + 2, y - 2, x, y + 3, color(Shirt));
  c.line(x - 1, y - 3, x + 1, y - 3, color(SkinLight));
  c.pixel(x - 1, y - 2, color((frame / 5u & 1u) ? Eye : BlueLight));
}

template<class Canvas>
void drawMagnetField(Canvas& c, int x, int y, uint32_t frame, int radius) {
  if (radius <= 0) return;
  static const int8_t sine[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
  const int near = radius < 31 ? radius : 31;
  const int phase = int(frame / 2u % 18u);
  const int ring = near - phase * (near - 11) / 18;
  for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
    const unsigned angle = (unsigned(frame / 4u) + quadrant * 4u) & 15u;
    const unsigned next = (angle + 1) & 15u;
    const int ax = x + sine[(angle + 4) & 15u] * ring / 16;
    const int ay = y + sine[angle] * ring / 16;
    const int bx = x + sine[(next + 4) & 15u] * ring / 16;
    const int by = y + sine[next] * ring / 16;
    c.line(ax, ay, bx, by, color(Blue));
    c.pixel(bx, by, color(BlueLight));
  }
  for (unsigned i = 0; i < 8; ++i) {
    const unsigned angle = (i * 2u + unsigned(frame / 8u)) & 15u;
    const int px = x + sine[(angle + 4) & 15u] * radius / 16;
    const int py = y + sine[angle] * radius / 16;
    c.pixel(px, py, color(Screen));
  }
  c.rect(x - 15, y - 3, 3, 7, color(RedShade));
  c.rect(x - 14, y - 3, 2, 2, color(RedLight));
  c.rect(x - 14, y + 2, 2, 2, color(BlueLight));
  c.pixel(x - 13, y, color(MetalLight));
}

template<class Canvas>
void drawAttractedCoin(Canvas& c, int x, int y, uint32_t frame, uint8_t value, int vx, int vy) {
  const int tx = vx > 0 ? -1 : vx < 0 ? 1 : 0;
  const int ty = vy > 0 ? -1 : vy < 0 ? 1 : 0;
  c.pixel(x + tx * 4, y + ty * 4, color(value >= 5 ? BlueShade : BrassShade));
  c.pixel(x + tx * 2, y + ty * 2, color(value >= 5 ? Blue : Brass));
  if (value >= 5) {
    c.line(x, y - 2, x + 2, y, color(BlueLight));
    c.line(x + 2, y, x, y + 2, color(Blue));
    c.line(x, y + 2, x - 2, y, color(Shirt));
    c.line(x - 2, y, x, y - 2, color(BlueLight));
    c.pixel(x, y - 1, color(Eye));
  } else {
    const int half = frame / 4u & 1u ? 1 : 0;
    c.rect(x - half, y - 2, half * 2 + 1, 5, color(Brass));
    c.pixel(x - half, y - 1, color(BrassLight));
    c.pixel(x, y - 2, color(FireLight));
  }
}

template<class Canvas>
void drawMissile(Canvas& c, int x, int y, uint32_t frame) {
  c.rect(x - 4, y - 3, 12, 7, color(Outline));
  c.rect(x - 6, y - 2, 3, 5, color(MetalShade));
  c.rect(x - 8, y - 1, 3, 3, color(Outline));
  c.rect(x - 6, y - 1, 3, 3, color(MetalLight));
  c.pixel(x - 7, y, color(Metal));
  c.rect(x - 3, y - 2, 9, 5, color(FireShade));
  c.rect(x - 3, y - 2, 8, 1, color(Fire));
  c.rect(x - 2, y - 1, 6, 1, color(BrassLight));
  c.rect(x - 3, y + 2, 9, 1, color(HairShade));
  c.rect(x, y - 3, 2, 7, color(MetalShade));
  c.rect(x, y - 2, 1, 5, color(MetalLight));
  c.rect(x + 4, y - 5, 3, 2, color(Outline));
  c.rect(x + 5, y - 4, 3, 2, color(Metal));
  c.pixel(x + 6, y - 4, color(MetalLight));
  c.rect(x + 4, y + 4, 3, 2, color(Outline));
  c.rect(x + 5, y + 3, 3, 2, color(MetalShade));
  c.pixel(x + 5, y + 3, color(Metal));
  const int flame = 3 + int(frame / 3u & 1u);
  c.rect(x + 8, y - 1, flame, 3, color(FireShade));
  c.rect(x + 8, y, flame + 2, 1, color(Fire));
  c.rect(x + 8, y, 2, 1, color(FireLight));
}

template<class Canvas>
void drawZap(Canvas& c, int x0, int y0, int x1, int y1, uint32_t frame) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  const int px = dx >= dy ? 0 : 1, py = dx >= dy ? 1 : 0;
  c.line(x0 - px * 2, y0 - py * 2, x1 - px * 2, y1 - py * 2, color(Outline));
  c.line(x0 + px * 2, y0 + py * 2, x1 + px * 2, y1 + py * 2, color(Outline));
  c.line(x0 - px, y0 - py, x1 - px, y1 - py, color(FireShade));
  c.line(x0 + px, y0 + py, x1 + px, y1 + py, color(Brass));
  c.line(x0, y0, x1, y1, color(frame & 4u ? FireLight : BrassLight));
  for (unsigned end = 0; end < 2; ++end) {
    const int x = end ? x1 : x0, y = end ? y1 : y0;
    c.rect(x - 3, y - 4, 7, 9, color(Outline));
    c.rect(x - 4, y - 2, 9, 5, color(Outline));
    c.rect(x - 2, y - 3, 5, 7, color(MetalShade));
    c.rect(x - 3, y - 1, 7, 3, color(Metal));
    c.rect(x - 1, y - 2, 3, 5, color(Brass));
    c.rect(x - 2, y - 1, 5, 3, color(Brass));
    c.rect(x - 1, y - 1, 3, 3, color(FireLight));
    c.pixel(x - 2, y - 3, color(MetalLight));
    c.pixel(x + 2, y + 3, color(BrassShade));
  }
}

template<class Canvas>
void disc(Canvas& c, int x, int y, int radius, Ink ink) {
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dx = -radius; dx <= radius; ++dx)
      if (dx * dx + dy * dy <= radius * radius) c.pixel(x + dx, y + dy, color(ink));
}

template<class Canvas>
void wheel(Canvas& c, int x, int y, uint32_t frame) {
  disc(c, x, y, 3, Outline); disc(c, x, y, 2, MetalShade);
  c.line(x - 1, y - 1, x + 1, y + 1, color(MetalLight));
  c.pixel(x, y, color(Brass));
  c.pixel(x + int(frame & 1u) * 2 - 1, y - 2, color(Metal));
}

template<class Canvas>
void riderFace(Canvas& c, int x, int y, bool goggles = false) {
  c.rect(x - 3, y - 3, 6, 2, color(HairShade));
  c.rect(x - 2, y - 3, 4, 1, color(HairLight));
  c.rect(x - 3, y - 1, 2, 3, color(Hair));
  c.rect(x - 1, y - 1, 4, 4, color(SkinShade));
  c.rect(x - 1, y - 1, 4, 2, color(Skin));
  c.pixel(x, y - 1, color(SkinLight));
  c.rect(x + 1, y, 2, 1, color(goggles ? BlueLight : Eye));
  c.pixel(x + 2, y, color(Outline)); c.pixel(x + 3, y + 1, color(SkinLight));
}

template<class Canvas>
void drawVehicle(Canvas& c, int x, int y, uint32_t frame, uint8_t vehicle,
                 bool activeInput, bool onFloor) {
  const int stride = int(frame / 5u & 1u);
  if (vehicle <= 2) {
    drawRunner(c, x, y, frame, activeInput, onFloor, false, vehicle, false);
    return;
  }
  if (vehicle == 3) {
    // Low chrome chassis, round tires and forward handlebars distinguish the bike.
    wheel(c, x - 10, y + 4, frame); wheel(c, x + 10, y + 4, frame);
    c.line(x - 10, y + 3, x - 4, y - 2, color(Metal));
    c.line(x - 4, y - 2, x + 4, y + 3, color(MetalLight));
    c.line(x + 4, y + 3, x + 10, y + 3, color(Metal));
    c.line(x + 10, y + 3, x + 8, y - 4, color(MetalLight));
    c.rect(x - 11, y - 3, 13, 4, color(Outline));
    c.rect(x - 10, y - 3, 11, 2, color(Red));
    c.rect(x - 9, y - 3, 8, 1, color(RedLight));
    c.rect(x - 4, y, 7, 3, color(MetalShade));
    c.line(x - 3, y + 1, x + 1, y + 1, color(MetalLight));
    c.rect(x - 12, y - 5, 7, 2, color(TrousersShade));
    c.line(x + 7, y - 4, x + 12, y - 4, color(MetalLight));
    c.rect(x + 11, y - 3, 3, 2, color(BrassLight));
    c.rect(x - 4, y - 6, 5, 4, color(Shirt));
    c.pixel(x - 3, y - 6, color(ShirtLight));
    c.line(x, y - 4, x + 7, y - 4, color(Skin));
    riderFace(c, x - 1, y - 9, true);
    c.rect(x - 3, y - 1, 4, 3, color(Trousers));
    c.rect(x - 1, y + 1, 4, 1, color(Boot));
    if (activeInput && !onFloor) {
      c.line(x - 13, y + 1, x - 17, y + 2, color(Fire));
      c.pixel(x - 18 + stride, y + 3, color(BrassLight));
    }
  } else if (vehicle == 4) {
    // The stomper has independent piston legs and heavy asymmetric fists.
    const int left = onFloor ? stride : 0, right = onFloor ? 1 - stride : 1;
    c.rect(x - 9, y, 5, 5 + left, color(Outline));
    c.rect(x - 8, y, 3, 4 + left, color(Metal));
    c.rect(x - 7, y + 1, 1, 3, color(MetalLight));
    c.rect(x + 4, y, 5, 5 + right, color(Outline));
    c.rect(x + 5, y, 3, 4 + right, color(MetalShade));
    c.rect(x - 12, y + 5 + left, 9, 2, color(MetalShade));
    c.rect(x + 3, y + 5 + right, 9, 2, color(MetalShade));
    c.rect(x - 11, y + 5 + left, 7, 1, color(MetalLight));
    c.rect(x + 4, y + 5 + right, 7, 1, color(Metal));
    c.rect(x - 10, y - 10, 20, 12, color(Outline));
    c.rect(x - 9, y - 9, 18, 10, color(BrassShade));
    c.rect(x - 8, y - 9, 16, 3, color(Brass));
    c.rect(x - 7, y - 9, 13, 1, color(BrassLight));
    c.rect(x - 5, y - 8, 10, 7, color(MetalShade));
    c.rect(x - 4, y - 7, 8, 5, color(Screen));
    riderFace(c, x, y - 7, true);
    c.rect(x - 8, y - 3, 3, 3, color(Metal));
    c.rect(x + 5, y - 3, 3, 3, color(Metal));
    c.rect(x - 15, y - 6, 5, 5, color(Outline));
    c.rect(x - 14, y - 5, 4, 3, color(Brass));
    c.rect(x - 17, y - 1 + stride, 7, 5, color(MetalShade));
    c.rect(x - 16, y - 1 + stride, 5, 2, color(MetalLight));
    c.rect(x + 10, y - 7, 5, 6, color(Outline));
    c.rect(x + 10, y - 6, 4, 4, color(Brass));
    c.rect(x + 11, y - 1 - stride, 7, 5, color(MetalShade));
    c.rect(x + 12, y - 1 - stride, 5, 2, color(Metal));
    if (activeInput) c.pixel(x + 16, y - 1 - stride, color(BrassLight));
  } else if (vehicle == 5) {
    // Broad feather clusters and an oversized beak make the flapper read as a bird.
    disc(c, x, y + 1, 6, Outline); disc(c, x, y, 5, Brass);
    c.rect(x - 3, y - 3, 6, 7, color(FurLight));
    c.rect(x - 2, y + 4, 4, 1, color(Fur));
    c.line(x - 6, y - 1, x - 13, y - 4, color(BrassShade));
    c.line(x - 6, y, x - 14, y - 1, color(Brass));
    c.line(x - 7, y + 1, x - 12, y + 3, color(BrassLight));
    disc(c, x + 7, y - 4, 4, Outline); disc(c, x + 7, y - 4, 3, Brass);
    c.rect(x + 7, y - 6, 2, 3, color(Eye)); c.pixel(x + 8, y - 5, color(Outline));
    c.rect(x + 10, y - 3, 6, 2, color(Fire));
    c.rect(x + 10, y - 1, 4, 1, color(FireShade));
    const int wing = activeInput ? (stride ? -4 : -8) : 1;
    c.line(x - 3, y - 1, x - 10, y + wing, color(BrassShade));
    c.line(x - 2, y - 1, x - 9, y + wing, color(BrassLight));
    c.line(x - 2, y, x - 7, y + wing + 2, color(FurLight));
    c.line(x - 2, y + 5, x - 4 - stride, y + 7, color(Fire));
    c.line(x + 2, y + 5, x + 5 - stride, y + 7, color(Fire));
    riderFace(c, x - 1, y - 9);
    c.rect(x - 4, y - 6, 5, 2, color(Shirt));
  } else if (vehicle == 6) {
    // A glass capsule and four energized brackets surround the seated pilot.
    disc(c, x, y - 1, 8, Outline); disc(c, x, y - 1, 7, GravityShade);
    disc(c, x, y - 2, 5, BlueShade);
    c.line(x - 3, y - 6, x + 2, y - 6, color(BlueLight));
    c.rect(x - 3, y - 2, 6, 4, color(Gravity));
    riderFace(c, x, y - 4, true);
    c.rect(x - 6, y + 3, 12, 3, color(MetalShade));
    c.rect(x - 5, y + 3, 10, 1, color(MetalLight));
    for (int side = -1; side <= 1; side += 2) {
      c.rect(x + side * 9 - 1, y - 6, 3, 11, color(Gravity));
      c.rect(x + side * 9, y - 5, 1, 9, color(GravityLight));
      c.pixel(x + side * 12, y - 3 + stride * 3, color(BlueLight));
      if (activeInput) {
        c.line(x + side * 12, y - 8, x + side * 16, y - 8, color(GravityLight));
        c.line(x + side * 12, y + 6, x + side * 15, y + 6, color(Blue));
      }
    }
  } else if (vehicle == 7) {
    // Long snout, curled tail and contrasting membrane identify the dragon.
    c.line(x - 5, y + 1, x - 14, y + 4, color(ArmorShade));
    c.line(x - 14, y + 4, x - 17, y, color(Armor));
    c.pixel(x - 18, y - 2, color(ArmorLight));
    disc(c, x - 1, y, 5, Outline); disc(c, x - 1, y - 1, 4, Armor);
    c.rect(x - 3, y + 1, 6, 3, color(ArmorLight));
    c.rect(x + 2, y - 4, 4, 6, color(ArmorShade));
    c.rect(x + 5, y - 7, 7, 6, color(Outline));
    c.rect(x + 6, y - 6, 6, 5, color(Armor));
    c.rect(x + 10, y - 4, 7, 3, color(Armor));
    c.line(x + 10, y - 1, x + 16, y - 1, color(ArmorShade));
    c.rect(x + 8, y - 6, 2, 2, color(Eye)); c.pixel(x + 9, y - 5, color(Outline));
    c.pixel(x + 15, y - 3, color(Outline));
    c.line(x + 6, y - 7, x + 5, y - 10, color(FurLight));
    c.line(x + 10, y - 7, x + 11, y - 9, color(Fur));
    const int wingY = activeInput ? y - 2 : y - 11 + stride;
    c.line(x - 1, y - 2, x - 10, wingY, color(GravityShade));
    c.line(x - 2, y - 1, x - 12, wingY + 3, color(Gravity));
    c.line(x - 3, y, x - 7, wingY + 4, color(GravityLight));
    c.line(x - 10, wingY, x - 12, wingY + 3, color(ArmorLight));
    c.line(x - 2, y + 4, x - 3 - stride, y + 7, color(ArmorShade));
    c.line(x + 2, y + 3, x + 4 + stride, y + 6, color(Armor));
    riderFace(c, x - 2, y - 7);
    c.rect(x - 5, y - 4, 5, 2, color(Shirt));
    if (activeInput && frame & 4u) c.pixel(x + 17, y, color(Fire));
  } else if (vehicle == 8) {
    const int body = 105;
    // This floor robot keeps its silhouette planted while the blocking fist moves.
    c.rect(x - 10, body + 6, 7, 9, color(Outline));
    c.rect(x + 3, body + 6, 7, 9, color(Outline));
    c.rect(x - 9, body + 7, 5, 6, color(Blue));
    c.rect(x + 4, body + 7, 5, 6, color(BlueShade));
    c.rect(x - 13, body + 13, 10, 4, color(MetalShade));
    c.rect(x + 3, body + 13, 11, 4, color(MetalShade));
    c.rect(x - 12, body + 13, 8, 1, color(MetalLight));
    c.rect(x + 4, body + 13, 9, 1, color(Metal));
    c.rect(x - 12, body - 12, 24, 20, color(Outline));
    c.rect(x - 11, body - 11, 22, 17, color(BlueShade));
    c.rect(x - 10, body - 11, 20, 5, color(Blue));
    c.rect(x - 9, body - 11, 16, 1, color(BlueLight));
    c.rect(x - 6, body - 8, 12, 10, color(MetalShade));
    c.rect(x - 5, body - 7, 10, 8, color(Screen));
    riderFace(c, x, body - 5, true);
    c.rect(x - 5, body - 2, 9, 4, color(Shirt));
    c.rect(x - 9, body + 3, 18, 2, color(Brass));
    c.rect(x - 16, body - 7, 5, 13, color(MetalShade));
    c.rect(x - 16, body - 7, 4, 3, color(MetalLight));
    c.rect(x - 17, body + 1, 7, 6, color(BrassShade));
    const int elbow = (102 + y) / 2;
    for (int t = -2; t <= 2; ++t) {
      c.line(x + 9 + t, 102, x + 14 + t, elbow, color(t == -2 ? BlueLight : MetalShade));
      c.line(x + 14 + t, elbow, x + 16 + t, y, color(t == -2 ? MetalLight : Metal));
    }
    disc(c, x + 14, elbow, 3, BrassShade); disc(c, x + 14, elbow, 1, BrassLight);
    c.rect(x + 12, y - 4, 10, 9, color(Outline));
    c.rect(x + 13, y - 3, 8, 7, color(Brass));
    c.rect(x + 13, y - 3, 7, 2, color(BrassLight));
    c.rect(x + 14, y + 2, 6, 2, color(BrassShade));
    for (int knuckle = 0; knuckle < 3; ++knuckle) c.pixel(x + 15 + knuckle * 2, y - 1, color(activeInput ? Eye : BrassShade));
  }
}

template<class Canvas>
void drawScientist(Canvas& c, int x, int y, uint32_t frame, uint8_t state) {
  const int stride = int(frame / (state ? 4u : 8u) & 3u);
  if (state == 2) {
    const int flip = stride & 1;
    c.line(x - 5, y + 2, x - 7 + stride, y - 2, color(Trousers));
    c.rect(x - 4, y - 1, 7, 5, color(MetalShade));
    c.rect(x - 4, y - 1, 6, 3, color(Eye));
    c.rect(x - 3, y - 1, 4, 1, color(MetalLight));
    riderFace(c, x + 4, y - 2 - flip, true);
    c.line(x - 1, y + 3, x + 1 + stride, y + 6, color(Skin));
    c.pixel(x - 7, y - 3, color(Outline));
    return;
  }
  const int bob = stride & 1;
  c.rect(x - 3, y - 7 + bob, 5, 4, color(LampShade));
  c.rect(x - 2, y - 6 + bob, 5, 3, color(Skin));
  c.rect(x - 2, y - 7 + bob, 4, 1, color(MetalLight));
  c.rect(x, y - 5 + bob, 3, 1, color(BlueLight));
  c.pixel(x + 2, y - 5 + bob, color(Outline));
  c.rect(x - 3, y - 3 + bob, 6, 7 - bob, color(MetalShade));
  c.rect(x - 3, y - 3 + bob, 5, 6 - bob, color(Eye));
  c.rect(x - 2, y - 2 + bob, 1, 5 - bob, color(MetalLight));
  c.rect(x, y - 2 + bob, 1, 3, color(BlueShade));
  if (state == 1) {
    c.line(x - 3, y - 2, x - 7, y - 5 + bob, color(Eye));
    c.pixel(x - 8, y - 6 + bob, color(SkinLight));
    c.line(x + 2, y - 2, x + 6, y - 6 + bob, color(Eye));
    c.pixel(x + 7, y - 7 + bob, color(Skin));
  } else {
    c.rect(x + 2, y - 1 + bob, 3, 2, color(MetalLight));
    c.pixel(x + 5, y - 1 + bob, color(Skin));
  }
  const int spread = stride == 0 ? 3 : stride == 2 ? -3 : 0;
  c.line(x - 2, y + 3, x - 2 - spread, y + 6, color(Trousers));
  c.line(x + 1, y + 3, x + 1 + spread, y + 6, color(TrousersShade));
  c.rect(x - 3 - spread, y + 5, 3, 2, color(Outline));
  c.rect(x + spread, y + 5, 3, 2, color(BootShade));
}

template<class Canvas>
void drawCrash(Canvas& c, int x, int y, uint32_t frame, uint8_t roll, bool sliding) {
  if (sliding) {
    drawRunner(c, x, y, frame, false, false, true, 0, false);
    c.pixel(x - 11, y + 6, color(Metal));
    c.pixel(x - 8 - int(frame & 1u), y + 7, color(Brass));
    return;
  }
  const unsigned pose = roll & 3u;
  const int headX[] = {4, -3, -5, 3}, headY[] = {-3, -3, 2, 3};
  c.rect(x - 4, y - 3, 8, 7, color(Outline));
  c.rect(x - 3, y - 2, 6, 5, color(ShirtShade));
  c.rect(x - 2, y - 2, 4, 3, color(Shirt));
  c.pixel(x - 1, y - 2, color(ShirtLight));
  riderFace(c, x + headX[pose], y + headY[pose]);
  const int ax[] = {-7, 3, 7, -2}, ay[] = {-4, -6, 3, 6};
  c.line(x, y, x + ax[pose], y + ay[pose], color(SkinShade));
  c.pixel(x + ax[pose], y + ay[pose], color(SkinLight));
  const int footX[] = {-6, 5, 5, -6}, footY[] = {5, 4, -5, -4};
  c.line(x - 1, y + 2, x + footX[pose], y + footY[pose], color(Trousers));
  c.rect(x + footX[pose] - 1, y + footY[pose], 3, 2, color(Boot));
  c.line(x + 2, y + 1, x - footX[pose], y + footY[pose] - 1, color(TrousersShade));
  c.pixel(x - footX[pose], y + footY[pose], color(Outline));
  c.rect(x - 7, y - 1, 3, 4, color(MetalShade));
  c.pixel(x - 7, y - 1, color(BrassLight));
}

template<class Canvas>
void drawPickupIcon(Canvas& c, int x, int y, uint32_t frame, uint8_t kind) {
  const Ink rim = kind == 0 ? Brass : kind == 1 ? Blue : kind == 2 ? Red : kind == 4 ? Gravity : Armor;
  c.rect(x - 6, y - 8, 13, 17, color(Outline));
  c.rect(x - 8, y - 6, 17, 13, color(Outline));
  c.rect(x - 6, y - 7, 13, 15, color(MetalShade));
  c.rect(x - 7, y - 5, 15, 11, color(MetalShade));
  c.rect(x - 5, y - 6, 11, 13, color(PanelShade));
  c.line(x - 5, y - 7, x + 5, y - 7, color(rim));
  c.pixel(x - 7, y - 5, color(MetalLight));
  c.pixel(x + 6, y + 6, color(frame & 8u ? Eye : rim));
  if (kind == 0) {
    c.rect(x - 4, y - 4, 9, 8, color(BrassShade));
    c.rect(x - 3, y - 4, 7, 6, color(Brass));
    c.rect(x - 2, y - 4, 2, 5, color(BrassLight));
    c.rect(x - 1, y - 2, 2, 4, color(Outline)); c.pixel(x, y + 3, color(Eye));
  } else if (kind == 1) {
    c.rect(x - 4, y - 4, 9, 6, color(BlueShade));
    c.rect(x - 3, y - 4, 7, 6, color(Blue));
    c.line(x - 3, y + 1, x, y + 5, color(Blue));
    c.line(x + 3, y + 1, x, y + 5, color(Blue));
    c.rect(x - 2, y - 3, 2, 5, color(BlueLight));
    c.pixel(x - 1, y + 3, color(Eye));
  } else if (kind == 2) {
    c.rect(x - 4, y - 4, 3, 8, color(Red)); c.rect(x + 2, y - 4, 3, 8, color(RedShade));
    c.rect(x - 3, y + 2, 7, 3, color(Red));
    c.rect(x - 4, y - 4, 3, 3, color(Eye)); c.rect(x + 2, y - 4, 3, 3, color(MetalLight));
    c.pixel(x - 2, y + 3, color(RedLight));
  } else if (kind == 3) {
    drawCoin(c, x - 2, y + 1, 0); drawCoin(c, x + 2, y - 2, 6);
    c.line(x - 4, y - 5, x + 4, y + 4, color(Eye));
    c.line(x + 4, y - 5, x - 4, y + 4, color(Eye));
  } else if (kind == 4) {
    disc(c, x, y, 5, GravityShade); disc(c, x, y, 4, Gravity);
    c.rect(x - 2, y - 2, 5, 5, color(GravityLight));
    c.pixel(x, y - 3, color(Eye)); c.pixel(x, y + 3, color(GravityShade));
    c.rect(x - 1, y - 1, 3, 3, color(GravityShade));
  } else if (kind == 5) {
    for (int i = -1; i <= 1; ++i) {
      c.line(x - 4 + i * 2, y + 3, x + 2 + i * 2, y - 3, color(Brass));
      c.pixel(x + 2 + i * 2, y - 3, color(BrassLight));
    }
    c.line(x - 4, y + 5, x + 4, y + 5, color(ArmorLight));
  } else if (kind == 6) {
    c.line(x - 4, y - 4, x, y, color(Fire)); c.line(x, y, x - 4, y + 4, color(Fire));
    c.line(x, y - 4, x + 4, y, color(FireLight)); c.line(x + 4, y, x, y + 4, color(FireLight));
    c.pixel(x - 5, y, color(Red));
  } else {
    disc(c, x, y, 5, Blue); disc(c, x, y, 4, Eye);
    c.line(x, y, x, y - 3, color(BlueShade)); c.line(x, y, x + 3, y, color(Red));
    c.rect(x - 1, y - 6, 3, 2, color(MetalLight));
    c.pixel(x + 4, y - 4, color(BrassLight));
  }
}

template<class Canvas>
void drawExplosion(Canvas& c, int x, int y, uint32_t age, uint8_t kind = 0) {
  if (age >= 32) return;
  const int radius = 2 + int(age < 12 ? age / 2 : (31 - age) / 3);
  const Ink outer = kind == 1 ? Blue : kind == 2 ? MetalShade : FireShade;
  disc(c, x, y, radius, outer);
  if (radius > 2) disc(c, x - 1, y - 1, radius - 2, kind == 1 ? BlueLight : Fire);
  if (age < 18) disc(c, x, y - 1, radius > 3 ? radius - 3 : 1, kind == 1 ? Eye : FireLight);
  const int8_t dx[] = {-8, -5, 2, 8, 6, -1, -7, 5};
  const int8_t dy[] = {-3, -8, -9, -4, 5, 8, 6, 9};
  for (unsigned i = 0; i < 8; ++i) {
    const int px = x + dx[i] * int(age + 4) / 16;
    const int py = y + dy[i] * int(age + 4) / 16 + int(age * age / 200);
    c.pixel(px, py, color(kind == 2 ? (i & 1u ? Metal : Brass) : (i & 1u ? Fire : BrassLight)));
    if (age < 12 && !(i & 1u)) c.pixel(px + 1, py, color(MetalLight));
  }
}

template<class Canvas>
void drawLab(Canvas& c, uint32_t worldQ8, uint32_t frame) {
  c.rect(0, 0, 240, 135, color(Wall));
  c.rect(0, 15, 240, 26, color(WallLight));
  c.rect(0, 96, 240, 25, color(WallShade));
  const int far = int(worldQ8 / (256u * 6u) % 96u);
  for (int x = -far - 96; x < 240; x += 96) {
    c.rect(x + 1, 20, 92, 98, color(PanelShade));
    c.rect(x + 3, 22, 88, 94, color(Wall));
    c.line(x + 3, 22, x + 90, 22, color(PanelLight));
    c.line(x + 3, 22, x + 3, 114, color(Panel));
    c.rect(x + 91, 23, 2, 94, color(WallShade));
    for (int dy = 27; dy < 115; dy += 24) {
      c.rect(x + 7, dy, 2, 2, color(PanelLight));
      c.pixel(x + 8, dy + 1, color(WallShade));
      c.rect(x + 85, dy, 2, 2, color(Panel));
    }
    c.rect(x + 13, 42, 39, 29, color(WallShade));
    c.rect(x + 15, 44, 35, 25, color(Panel));
    c.rect(x + 17, 46, 31, 21, color(PanelShade));
    c.line(x + 19, 48, x + 45, 48, color(PanelLight));
    c.rect(x + 20, 52, 10, 11, color(WallShade));
    c.rect(x + 22, 54, 6, 7, color(Screen));
    c.rect(x + 23, 55, 3, 1, color(Lamp));
    c.rect(x + 33, 53, 11, 1, color(PanelLight));
    c.rect(x + 33, 57, 7, 1, color(Screen));
    c.rect(x + 33, 61, 10, 1, color(PanelLight));
    if ((frame / 45u + unsigned(x + far + 96) / 96u) & 1u) c.pixel(x + 44, 63, color(Lamp));
    c.rect(x + 61, 37, 3, 63, color(WallShade));
    c.rect(x + 62, 38, 1, 60, color(PanelLight));
    c.rect(x + 59, 43, 7, 2, color(Panel));
    c.rect(x + 59, 87, 7, 2, color(Panel));
    c.rect(x + 62, 97, 15, 3, color(Panel));
    c.rect(x + 75, 83, 3, 16, color(PanelShade));
    c.rect(x + 74, 82, 5, 2, color(PanelLight));
    c.rect(x + 17, 86, 30, 2, color(PanelLight));
    c.rect(x + 20, 88, 24, 18, color(PanelShade));
    for (int slot = 0; slot < 4; ++slot) c.rect(x + 24 + slot * 5, 92, 2, 9, color(Panel));
  }

  const int overhead = int(worldQ8 / (256u * 3u) % 64u);
  c.rect(0, 15, 240, 3, color(WallShade));
  for (int x = -overhead - 64; x < 240; x += 64) {
    c.rect(x + 12, 17, 34, 4, color(LampShade));
    c.rect(x + 14, 18, 30, 2, color(Lamp));
    c.rect(x + 17, 18, 24, 1, color(LampLight));
    c.pixel(x + 12, 18, color(PanelLight)); c.pixel(x + 45, 18, color(WallShade));
  }

  const int staff = int(worldQ8 / (256u * 4u) % 160u);
  for (int x = 74 - staff; x < 240; x += 160) {
    const int walk = int(frame / 12u) & 1;
    c.rect(x, 105, 4, 4, color(LampShade));
    c.rect(x + 1, 106, 3, 3, color(Lamp));
    c.pixel(x + 3, 107, color(WallShade));
    c.rect(x - 1, 109, 6, 7, color(PanelLight));
    c.rect(x, 109, 3, 6, color(LampShade));
    c.pixel(x + 2, 110, color(WallShade));
    c.rect(x + 3, 111, 3, 2, color(Panel));
    c.pixel(x + 5, 111, color(Lamp));
    c.rect(x + walk, 116, 2, 4, color(WallShade));
    c.rect(x + 3 - walk, 116, 2, 4, color(WallShade));
  }

  c.rect(0, 121, 240, 2, color(MetalShade));
  c.rect(0, 123, 240, 5, color(BrassShade));
  const int floor = int(worldQ8 / 256u % 16u);
  for (int x = -floor - 16; x < 240; x += 16) {
    for (int dy = 0; dy < 5; ++dy) c.rect(x + dy, 123 + dy, 7, 1, color(LampShade));
    c.pixel(x + 12, 124, color(Brass));
  }
  c.rect(0, 128, 240, 7, color(WallShade));
  c.line(0, 128, 239, 128, color(PanelLight));
  for (int x = -floor - 16; x < 240; x += 16) {
    c.rect(x + 2, 130, 12, 1, color(Panel));
    c.rect(x + 1, 131, 1, 3, color(PanelShade));
    c.pixel(x + 14, 133, color(PanelLight));
  }
}

} }  // namespace flappy::JetpackArt
