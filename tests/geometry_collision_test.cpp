#include "../firmware/flappy/geometry_collision.h"
#include "../firmware/flappy/geometry_renderer.h"
#include <array>
#include <assert.h>
#include <iostream>

namespace {
// Scalar point tests transcribed from upstream collision.h for cube mode.
static bool nativeFatal(unsigned c, unsigned x, unsigned y) {
  const bool left = x < 6 && y >= 6 && y <= 8;
  const bool right = x >= 10 && y >= 6 && y <= 8;
  const bool top = y < 6 && x >= 5 && x <= 7;
  const bool bottom = y > 10 && x >= 5 && x <= 7;
  switch (c) {
    case 1: return right; case 2: return left; case 3: return top;
    case 4: case 59: case 61: return bottom;
    case 8: return y >= 4 && y < 12 && x >= 4 && x <= 8;
    case 40: return y >= 8 && x >= 2 && x <= 5;
    case 41: return y >= 8 && x >= 10 && x <= 12;
    case 42: return y >= 8 && (x & 7) >= 2 && (x & 7) <= 5;
    case 43: case 45: case 48: return y < 8 && x >= 2 && x <= 5;
    case 44: case 46: case 49: return y < 8 && x >= 10 && x <= 12;
    case 47: case 50: return y < 8 && (x & 7) >= 2 && (x & 7) <= 5;
    case 60: return y < 8 && x >= 7 && x <= 10;
    case 51: return top || right; case 52: return top || left;
    case 53: return bottom || right; case 54: return bottom || left;
    default: return false;
  }
}
static bool nativeSolid(unsigned c, unsigned x, unsigned y) {
  switch (c) {
    case 5: case 59: case 61: return y < 8;
    case 6: case 45: case 46: case 47: case 60: return y >= 8;
    case 7: case 9: case 31: return true;
    case 11: return y >= 15 - x; case 12: return y >= x;
    case 13: return y >= 15 - (x >> 1); case 14: return y >= (x >> 1);
    case 15: return y >= 15 - ((x >> 1) | 8); case 16: return y >= ((x >> 1) | 8);
    case 17: return x >= 8 && y >= 15 - ((x & 7) << 1);
    case 18: return x < 8 && y >= ((x & 7) << 1);
    case 19: return x >= 8 || y >= 15 - (x << 1);
    case 20: return x < 8 || y >= ((x & 7) << 1);
    case 21: return 15 - y >= 15 - x; case 22: return 15 - y >= x;
    case 23: return 15 - y >= 15 - ((x >> 1) | 8);
    case 24: return 15 - y >= 15 - (x >> 1);
    case 25: return 15 - y >= ((x >> 1) | 8);
    case 26: return 15 - y >= (x >> 1);
    case 27: return x >= 8 && 15 - y >= 15 - ((x & 7) << 1);
    case 28: return x < 8 && 15 - y >= ((x & 7) << 1);
    case 29: return x >= 8 || 15 - y >= 15 - (x << 1);
    case 30: return x < 8 || 15 - y >= ((x & 7) << 1);
    case 32: return y < 8 && x < 8; case 33: return y < 8 && x >= 8;
    case 34: case 43: return y >= 8 && x < 8;
    case 35: case 44: return y >= 8 && x >= 8;
    case 36: return x < 8; case 37: return x >= 8;
    case 38: return (x < 8) == (y < 8); case 39: return (x < 8) != (y < 8);
    case 55: return !(y < 8 && x < 8); case 56: return !(y < 8 && x >= 8);
    case 57: return !(y >= 8 && x < 8); case 58: return !(y >= 8 && x >= 8);
    default: return false;
  }
}
// Metatile-to-class compatibility facts from the pinned source metatiles.inc.
using C = flappy::GeometryCollision;
static const C NativeClasses[256] = {
  C::None, C::FloorCeiling, C::FloorCeiling, C::Bottom, C::DeathTop, C::FloorCeiling, C::FloorCeiling, C::None,
  C::DeathBottom, C::DeathBottom, C::DeathTop, C::DeathTop, C::DeathBottom, C::DeathTop, C::DeathLeft, C::DeathRight,
  C::All, C::Death, C::DeathBottom, C::DeathBottom, C::DeathTop, C::All, C::All, C::DeathTop,
  C::DeathBottom, C::Top, C::Death, C::Death, C::Death, C::DeathLeft, C::DeathTop, C::DeathRight,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::All,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::None,
  C::All, C::All, C::All, C::All, C::None, C::None, C::None, C::TopCenterSpike,
  C::All, C::All, C::All, C::DeathBottom, C::All, C::All, C::All, C::All,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::All,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::None,
  C::All, C::All, C::All, C::All, C::Top, C::Top, C::Top, C::Top,
  C::Bottom, C::Bottom, C::Bottom, C::Death, C::DeathBottom, C::DeathTop, C::DeathRight, C::DeathLeft,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::All,
  C::All, C::All, C::All, C::All, C::All, C::All, C::All, C::None,
  C::All, C::All, C::All, C::All, C::DownRightSpike, C::DeathBottom, C::DownLeftSpike, C::DeathRight,
  C::None, C::DeathLeft, C::UpRightSpike, C::DeathTop, C::UpLeftSpike, C::Death, C::All, C::DeathBottom,
  C::None, C::None, C::Death, C::Death, C::DeathBottom, C::DeathTop, C::DeathBottom, C::DeathTop,
  C::FloorCeiling, C::FloorCeiling, C::Right, C::Left, C::Right, C::Left, C::None, C::NoSide,
  C::SlopeRightDown45, C::SlopeLeftDown45, C::SlopeRightUp45, C::SlopeLeftUp45, C::SlopeRightDown22Right, C::SlopeRightDown22Left, C::SlopeLeftDown22Right, C::SlopeLeftDown22Left,
  C::SlopeRightUp22Right, C::SlopeRightUp22Left, C::SlopeLeftUp22Right, C::SlopeLeftUp22Left, C::SlopeRightDown66Top, C::SlopeRightDown66Bottom, C::SlopeLeftDown66Bottom, C::SlopeLeftDown66Top,
  C::SlopeRightUp66Top, C::SlopeRightUp66Bottom, C::SlopeLeftUp66Bottom, C::SlopeLeftUp66Top, C::NoSide, C::NoSide, C::NoSide, C::NoSide,
  C::LeftSpikeBlock, C::RightSpikeBlock, C::BottomLeftSpike, C::BottomRightSpike, C::BottomSpikes, C::DownLeftSpike, C::DownRightSpike, C::DownBothSpikes,
  C::UpperLeft, C::UpperRight, C::LowerLeft, C::LowerRight, C::Top, C::Bottom, C::Left, C::Right,
  C::TopLeftStairs, C::TopRightStairs, C::BottomLeftStairs, C::BottomRightStairs, C::UpperLeftLowerRight, C::UpperRightLowerLeft, C::UpLeftSpike, C::UpRightSpike,
  C::UpBothSpikes, C::DeathTopRight, C::DeathTopLeft, C::DeathBottomRight, C::DeathBottomLeft, C::None, C::None, C::BottomCenterSpike,
  C::BottomCenterSpike, C::Top, C::Bottom, C::None, C::None, C::None, C::None, C::None,
  C::None, C::None, C::None, C::All, C::None, C::None, C::None, C::None,
  C::None, C::DownRightSpike, C::DownLeftSpike, C::UpLeftSpike, C::UpRightSpike, C::Left, C::Right, C::UpperRight,
  C::Right, C::Top, C::Top, C::UpperLeft, C::Top, C::Right, C::Bottom, C::Top,
  C::None, C::None, C::None, C::None, C::None, C::None, C::None, C::None,
  C::DownRightSpike, C::DownLeftSpike, C::UpRightSpike, C::UpLeftSpike, C::DownRightSpike, C::DownLeftSpike, C::Death, C::Right,
  C::Left, C::UpRightSpike, C::UpLeftSpike, C::Death, C::All, C::Left, C::LowerRight, C::LowerLeft,
};
static void everySourcePointMatchesTheIndependentOracle() {
  for (unsigned c = 0; c <= 61; ++c) for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 16; ++x) {
    const bool solid = nativeSolid(c,x,y);
    assert(bool(flappy::GeometryCollisionMask::solidRow(uint8_t(c), int(y)) & (1u << x)) == solid);
    flappy::GeometryTile tile; tile.valid = true; tile.collision = uint8_t(c);
    assert(!solid || tile.hasSolidRegion());
    assert(bool(flappy::GeometryCollisionMask::hazardRow(uint8_t(c), int(y)) & (1u << x)) == nativeFatal(c,x,y));
  }
  assert(flappy::GeometryCollisionMask::solidRow(7,-1) == 0);
  assert(flappy::GeometryCollisionMask::hazardRow(8,16) == 0);
}
static void everyMetatileShowsAllOfItsNativeDanger() {
  std::array<uint8_t,1024> ram{};
  std::array<uint16_t,240 * 135 + 128> pixels;
  pixels.fill(0xa55a);
  auto readRam = [&](uint16_t a) { assert(a >= 0x6000 && a < 0x6400); return ram[a - 0x6000]; };
  auto readRom = [&](uint32_t o) { assert(o >= flappy::GeometryScene::CollisionTableFileOffset && o < flappy::GeometryScene::CollisionTableFileOffset + 256); return uint8_t(NativeClasses[o - flappy::GeometryScene::CollisionTableFileOffset]); };
  for (int y = 912; y < 960; y += 16) for (int x = 0; x < 256; x += 16)
    ram[(y / 240) * 256 + (y % 240) / 16 * 16 + x / 16] = 1;
  flappy::GeometryScene scene;
  scene.state = 2; scene.scrollY = 0x300;
  scene.players[0].x = 32 << 8; scene.players[0].y = 176 << 8;
  flappy::GeometryRenderOptions options;
  options.hud = false;
  flappy::GeometryRenderer renderer;
  unsigned hazardous = 0;
  for (unsigned id = 0; id < 256; ++id) {
    ram[3 * 256 + 10 * 16 + 8] = uint8_t(id);
    renderer.reset(); renderer.draw(scene,readRam,readRom,pixels.data() + 64,options);
    unsigned fatalPoints = 0;
    for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 16; ++x) {
      if (!nativeFatal(unsigned(NativeClasses[id]),x,y)) continue;
      ++fatalPoints;
      const uint16_t color = pixels[64 + (83 + y) * 240 + 128 - renderer.cameraX() + x];
      assert(color == 0xffff || color == 0x0065 || color == 0x0e1e);
    }
    hazardous += fatalPoints != 0;
    for (unsigned i = 0; i < 64; ++i) assert(pixels[i] == 0xa55a && pixels[64 + 240 * 135 + i] == 0xa55a);
  }
  assert(hazardous == 75);
}
}
int main() {
  everySourcePointMatchesTheIndependentOracle();
  everyMetatileShowsAllOfItsNativeDanger();
  std::cout << "Native collision oracle, all 256 metatiles and visible danger coverage checks passed.\n";
}
