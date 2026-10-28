#pragma once

#include <stdint.h>

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
  PanelShade, Panel, PanelLight, LampShade, Lamp, LampLight, Screen, InkCount
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
    rgb(76, 86, 84), rgb(121, 133, 118), rgb(183, 187, 146), rgb(57, 108, 106)
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
