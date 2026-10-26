#pragma once
#include <stdint.h>
#include "generated_assets.h"
#include "generated_collision.h"
#include "generated_skins.h"

namespace flappy {

inline uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}

inline uint16_t oppositeColor(uint16_t color, bool invert) {
  const unsigned r = ((color >> 11) & 31) * 255 / 31;
  const unsigned g = ((color >> 5) & 63) * 255 / 63;
  const unsigned b = (color & 31) * 255 / 31;
  if (invert) return rgb(255 - r, 255 - g, 255 - b);
  const unsigned hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
  const unsigned lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
  return rgb(hi + lo - r, hi + lo - g, hi + lo - b);
}

class Skins {
 public:
  static constexpr uint8_t BuiltinCount = 12;
  uint8_t selected = 0;
  bool monochrome = false;

  static uint8_t count() { return uint8_t(BuiltinCount + FlappySkins::count); }
  static const char* name(uint8_t index) {
    static const char* const names[] = {"Original", "Midnight", "Arctic", "Desert", "Candy", "Neon",
      "Mono", "Ocean", "Pumpkin", "Retro", "Toast on the Run", "Past Due"};
    if (index < BuiltinCount) return names[index];
    return index < count() ? FlappySkins::packs[index - BuiltinCount].name : names[0];
  }

  void select(uint8_t index) {
    selected = index < count() ? index : 0;
    unsigned gray = 0, total = 0;
    for (int f = 0; f < 3; ++f) for (int y = 0; y < 12; ++y) for (int x = 0; x < 17; ++x) {
      const uint16_t c = birdPixel(f, x, y);
      if (c == FlappyAssets::kTransparent) continue;
      const int r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
      const int hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
      const int lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
      gray += hi - lo <= 24;
      ++total;
    }
    monochrome = total && gray * 5 >= total * 4;
    for (int f = 0; f < 3; ++f) for (int y = 0; y < 24; ++y) {
      masks_[f][y] = 0;
      for (int x = 0; x < 34; ++x) if (birdPixel(f, x / 2, y / 2) != FlappyAssets::kTransparent)
        masks_[f][y] |= uint64_t(1) << x;
    }
  }

  const uint64_t (*masks() const)[24] {
    if (selected < 10) return FlappyCollision::birdMasks;
    if (selected >= BuiltinCount) return FlappySkins::packs[selected - BuiltinCount].birdMasks;
    return masks_;
  }

  const FlappyAssets::Sprite& background() const { return world(0); }
  const FlappyAssets::Sprite& ground() const { return world(1); }
  const FlappyAssets::Sprite& pipe(bool top) const { return world(top ? 3 : 2); }
  const FlappyAssets::Sprite& bird(int frame) const {
    static const FlappyAssets::Sprite* const birds[] = {&FlappyAssets::birdUp, &FlappyAssets::birdMid, &FlappyAssets::birdDown};
    frame = frame >= 0 && frame < 3 ? frame : 1;
    if (selected < BuiltinCount) return *birds[frame];
    const auto& p = FlappySkins::packs[selected - BuiltinCount];
    return *(frame == 0 ? p.birdUp : frame == 1 ? p.birdMid : p.birdDown);
  }

  uint16_t palette(uint16_t c) const {
    if (!selected || selected >= 10 || c == FlappyAssets::kTransparent) return c;
    unsigned r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
    const unsigned l = (r * 77 + g * 150 + b * 29) >> 8;
    switch (selected) {
      case 1: return rgb(r / 3, g / 3, b / 2 + 40);
      case 2: return rgb(r / 2 + 80, g / 2 + 100, b / 2 + 120);
      case 3: return rgb(r / 2 + 120, g * 3 / 4 + 30, b / 3);
      case 4: return rgb(b / 2 + 100, r / 2 + 40, g / 2 + 100);
      case 5: return rgb(r > 128 ? 255 : 20, g > 128 ? 255 : 10, b > 128 ? 255 : 40);
      case 6: return rgb(l, l, l);
      case 7: return rgb(r / 4, g * 3 / 4, b / 2 + 80);
      case 8: return rgb(g, g / 2, b / 5);
      case 9: { const unsigned q = l / 64; return rgb(20 + q * 46, 36 + q * 51, 12 + q * 31); }
      default: return c;
    }
  }

  uint16_t birdPixel(int frame, int x, int y) const {
    if (x < 0 || x >= 17 || y < 0 || y >= 12) return FlappyAssets::kTransparent;
    if (selected == 10) {
      if (x >= 3 && x <= 13 && y >= 1 && y <= 10) {
        if (y == 1 && (x == 3 || x == 13)) return FlappyAssets::kTransparent;
        if (x == 3 || x == 13 || y == 1 || y == 10) return rgb(119, 57, 20);
        if (x >= 6 && x <= 9 && y >= 3 && y <= 5) return rgb(255, 219, 64);
        if (y == 6 && (x == 9 || x == 12)) return rgb(32, 24, 20);
        if (y == 8 && x >= 9 && x <= 11) return rgb(178, 49, 54);
        return rgb(249, 203, 135);
      }
      if ((x == 1 || x == 2 || x == 14 || x == 15) && y == 4 + frame * 2) return rgb(195, 123, 51);
    } else if (selected == 11) {
      if (x >= 4 && x <= 12 && y >= 1 && y <= 10) {
        if (x >= 10 && y <= 3) return rgb(167, 206, 216);
        if (x == 4 || x == 12 || y == 1 || y == 10) return rgb(38, 54, 75);
        if (y == 4 && x >= 6 && x <= 9) return rgb(59, 87, 105);
        if ((y == 7 || y == 8) && x >= 7 && x <= 10) return rgb(222, 41, 58);
        return rgb(247, 245, 223);
      }
      if (y == 4 + frame * 2 && ((x >= 1 && x <= 3) || (x >= 13 && x <= 15))) return rgb(247, 245, 223);
    } else {
      const auto& asset = bird(frame);
      return palette(asset.pixels[y * asset.width + x]);
    }
    return FlappyAssets::kTransparent;
  }

 private:
  uint64_t masks_[3][24] = {};
  const FlappyAssets::Sprite& world(int index) const {
    static const FlappyAssets::Sprite* const base[] = {&FlappyAssets::background, &FlappyAssets::ground,
      &FlappyAssets::pipe, &FlappyAssets::pipeTop};
    if (selected < BuiltinCount) return *base[index];
    const auto& p = FlappySkins::packs[selected - BuiltinCount];
    const FlappyAssets::Sprite* custom = index == 0 ? p.background : index == 1 ? p.ground : index == 2 ? p.pipe : p.pipeTop;
    return custom ? *custom : *base[index];
  }
};
}  // namespace flappy
