#include "../firmware/flappy/jetpack_art.h"
#include "../firmware/flappy/menu_font.h"

#include <array>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace Art = flappy::JetpackArt;
constexpr uint16_t Empty = 0xf81f;

struct Canvas {
  static constexpr int Width = 240, Height = 135;
  std::array<uint16_t, Width * Height + 2> storage;
  Canvas() { storage.fill(Empty); storage.front() = 0x1234; storage.back() = 0x5678; }
  uint16_t* data() { return storage.data() + 1; }
  const uint16_t* data() const { return storage.data() + 1; }
  uint16_t at(int x, int y) const { return data()[y * Width + x]; }
  void pixel(int x, int y, uint16_t ink) {
    if (x >= 0 && x < Width && y >= 0 && y < Height) data()[y * Width + x] = ink;
  }
  void rect(int x, int y, int width, int height, uint16_t ink) {
    for (int row = 0; row < height; ++row)
      for (int col = 0; col < width; ++col) pixel(x + col, y + row, ink);
  }
  void line(int x0, int y0, int x1, int y1, uint16_t ink) {
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      pixel(x0, y0, ink);
      if (x0 == x1 && y0 == y1) break;
      const int twice = error * 2;
      if (twice >= dy) { error += dy; x0 += sx; }
      if (twice <= dx) { error += dx; y0 += sy; }
    }
  }
  void guards() const { assert(storage.front() == 0x1234 && storage.back() == 0x5678); }
};

static uint64_t hash(const Canvas& c) {
  uint64_t value = 1469598103934665603ULL;
  for (unsigned i = 0; i < Canvas::Width * Canvas::Height; ++i) value = (value ^ c.data()[i]) * 1099511628211ULL;
  return value;
}

static bool contains(const Canvas& c, Art::Ink ink) {
  for (unsigned i = 0; i < Canvas::Width * Canvas::Height; ++i) if (c.data()[i] == Art::color(ink)) return true;
  return false;
}

static void runnerPosesAndBounds() {
  std::set<uint64_t> runs, flights;
  for (unsigned suit = 0; suit < 3; ++suit) {
    for (unsigned frame : {0u, 6u, 12u, 18u}) {
      Canvas c;
      Art::drawRunner(c, 100, 70, frame, false, true, false, uint8_t(suit), false);
      unsigned painted = 0;
      for (int y = 0; y < Canvas::Height; ++y)
        for (int x = 0; x < Canvas::Width; ++x) if (c.at(x, y) != Empty) {
          assert(x >= 89 && x <= 108 && y >= 63 && y <= 77);
          ++painted;
        }
      assert(painted >= 130 && painted <= 240);
      assert(contains(c, Art::SkinLight) && contains(c, Art::Eye));
      assert(contains(c, Art::Hair) && contains(c, Art::HairLight));
      assert(contains(c, Art::MetalLight) && contains(c, Art::BrassLight));
      assert(!contains(c, Art::Fire));
      if (suit == 0) runs.insert(hash(c));
      c.guards();
    }
    for (unsigned frame : {0u, 7u}) {
      Canvas c;
      Art::drawRunner(c, 100, 70, frame, true, false, false, uint8_t(suit), false);
      assert(contains(c, Art::Fire) == (suit != 2));
      if (suit == 0) flights.insert(hash(c));
      for (int y = 0; y < Canvas::Height; ++y)
        for (int x = 0; x < Canvas::Width; ++x)
          if (c.at(x, y) != Empty) assert(x >= 89 && x <= 108 && y >= 63 && y <= 77);
      c.guards();
    }
    Canvas fallen;
    Art::drawRunner(fallen, 100, 70, 0, true, false, true, uint8_t(suit), true);
    assert(contains(fallen, Art::Skin) && !contains(fallen, Art::Fire));
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x)
        if (fallen.at(x, y) != Empty) assert(x >= 89 && x <= 108 && y >= 63 && y <= 77);
    fallen.guards();
  }
  assert(runs.size() == 4 && flights.size() == 2);
  Canvas normal, reverse;
  Art::drawRunner(normal, 100, 70, 7, false, false, false, 2, false);
  Art::drawRunner(reverse, 100, 70, 7, false, false, false, 2, true);
  for (int dy = -7; dy <= 7; ++dy)
    for (int dx = -11; dx <= 8; ++dx) assert(normal.at(100 + dx, 70 + dy) == reverse.at(100 + dx, 70 - dy));
  Canvas edge;
  for (int x : {0, 239}) for (int y : {0, 134})
    Art::drawRunner(edge, x, y, 7, true, false, false, 0, false);
  edge.guards();
}

static unsigned brightness(uint16_t c) {
  return unsigned(c >> 11) * 299u * 8u + unsigned(c >> 5 & 63) * 587u * 4u + unsigned(c & 31) * 114u * 8u;
}

static void labCoverageMotionAndPalette() {
  std::set<uint16_t> palette;
  for (unsigned i = 0; i < Art::InkCount; ++i) {
    palette.insert(Art::color(Art::Ink(i)));
    assert(Art::color(Art::Ink(i)) != Empty);
  }
  assert(palette.size() >= 44 && palette.size() <= 48);
  std::set<uint64_t> views;
  for (uint32_t world : {0u, 256u, 123456u, UINT32_MAX}) {
    Canvas c;
    Art::drawLab(c, world, 24);
    unsigned backgroundBright = 0;
    std::set<uint16_t> used;
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x) {
        assert(c.at(x, y) != Empty && palette.count(c.at(x, y)));
        used.insert(c.at(x, y));
        if (y >= 22 && y <= 114 && brightness(c.at(x, y)) >= brightness(Art::color(Art::Skin))) ++backgroundBright;
      }
    assert(used.size() >= 12 && backgroundBright == 0);
    views.insert(hash(c));
    c.guards();
  }
  assert(views.size() == 4);
  Canvas still, animated;
  Art::drawLab(still, 0, 0); Art::drawLab(animated, 0, 45);
  assert(hash(still) != hash(animated));
}

static void foregroundShapesAndClipping() {
  std::set<uint64_t> coins;
  for (unsigned frame : {0u, 6u, 12u}) {
    Canvas c;
    Art::drawCoin(c, 100, 70, frame);
    coins.insert(hash(c));
    assert(contains(c, Art::BrassLight) && contains(c, Art::BrassShade));
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x)
        if (c.at(x, y) != Empty) assert(x >= 97 && x <= 103 && y >= 66 && y <= 74);
    c.guards();
  }
  assert(coins.size() == 3);
  Canvas first, second;
  Art::drawMissile(first, 100, 70, 0); Art::drawMissile(second, 100, 70, 3);
  assert(hash(first) != hash(second));
  for (int y = 0; y < Canvas::Height; ++y)
    for (int x = 0; x < Canvas::Width; ++x)
      if (second.at(x, y) != Empty) assert(x >= 92 && x <= 113 && y >= 65 && y <= 75);
  assert(contains(first, Art::MetalLight) && contains(first, Art::FireLight));
  Canvas edges;
  for (int x : {0, 239}) for (int y : {0, 134}) {
    Art::drawCoin(edges, x, y, 0);
    Art::drawMissile(edges, x, y, 3);
  }
  Art::drawZap(edges, -4, -4, 244, 139, 4);
  edges.guards(); first.guards(); second.guards();
  Canvas zap;
  Art::drawZap(zap, 90, 50, 130, 90, 0);
  assert(contains(zap, Art::FireLight) && contains(zap, Art::MetalLight));
  for (int t = 0; t <= 40; ++t) assert(zap.at(90 + t, 50 + t) != Empty);
  zap.guards();
}

static void expandedVehicleSilhouettesAndAnimation() {
  std::set<uint64_t> shapes;
  for (uint8_t vehicle = 3; vehicle <= 7; ++vehicle) {
    Canvas still, moving;
    Art::drawVehicle(still, 100, 70, 0, vehicle, false, true);
    Art::drawVehicle(moving, 100, 70, 5, vehicle, true, true);
    shapes.insert(hash(still));
    assert(hash(still) != hash(moving));
    unsigned occupied = 0;
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x) if (moving.at(x, y) != Empty) {
        assert(x >= 82 && x <= 118 && y >= 58 && y <= 77);
        ++occupied;
      }
    assert(occupied >= 140 && occupied <= 600);
    assert(contains(still, Art::Skin));
    still.guards(); moving.guards();
  }
  assert(shapes.size() == 5);
  std::set<uint64_t> samArms;
  for (int height : {27, 70, 114}) {
    Canvas c;
    Art::drawVehicle(c, 100, height, 5, 8, true, true);
    assert(c.at(90, 121) != Empty && c.at(105, 121) != Empty);
    assert(c.at(116, height) != Empty);
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x)
        if (c.at(x, y) != Empty) assert(x >= 82 && x <= 121 && y >= 23 && y <= 121);
    samArms.insert(hash(c)); c.guards();
  }
  assert(samArms.size() == 3);
  Canvas edges;
  for (uint8_t vehicle = 3; vehicle <= 8; ++vehicle)
    for (int x : {0, 239}) for (int y : {0, 134}) Art::drawVehicle(edges, x, y, UINT32_MAX, vehicle, true, true);
  edges.guards();
}

static void scientistsCrashesPickupsAndEffects() {
  std::set<uint64_t> states;
  for (uint8_t state = 0; state < 3; ++state) {
    std::set<uint64_t> poses;
    for (unsigned frame : {0u, 8u, 12u}) {
      Canvas c;
      Art::drawScientist(c, 100, 70, frame, state);
      poses.insert(hash(c));
      for (int y = 0; y < Canvas::Height; ++y)
        for (int x = 0; x < Canvas::Width; ++x)
          if (c.at(x, y) != Empty) assert(x >= 92 && x <= 108 && y >= 63 && y <= 76);
      assert(contains(c, Art::Skin)); c.guards();
    }
    assert(poses.size() >= 2);
    states.insert(*poses.begin());
  }
  assert(states.size() == 3);
  std::set<uint64_t> crashes;
  for (uint8_t roll = 0; roll < 4; ++roll) {
    Canvas c;
    Art::drawCrash(c, 100, 70, 8, roll, false);
    crashes.insert(hash(c));
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x)
        if (c.at(x, y) != Empty) assert(x >= 89 && x <= 108 && y >= 63 && y <= 77);
    c.guards();
  }
  Canvas slide;
  Art::drawCrash(slide, 100, 70, 8, 0, true);
  crashes.insert(hash(slide)); assert(crashes.size() == 5);
  std::set<uint64_t> pickups;
  for (uint8_t kind = 0; kind < 8; ++kind) {
    Canvas c;
    Art::drawPickupIcon(c, 100, 70, 0, kind);
    pickups.insert(hash(c));
    for (int y = 0; y < Canvas::Height; ++y)
      for (int x = 0; x < Canvas::Width; ++x)
        if (c.at(x, y) != Empty) assert(x >= 92 && x <= 108 && y >= 62 && y <= 78);
    c.guards();
  }
  assert(pickups.size() == 8);
  for (uint8_t kind = 0; kind < 3; ++kind) {
    std::set<uint64_t> phases;
    for (unsigned age : {0u, 8u, 16u, 31u}) {
      Canvas c;
      Art::drawExplosion(c, 100, 70, age, kind);
      phases.insert(hash(c));
      for (int y = 0; y < Canvas::Height; ++y)
        for (int x = 0; x < Canvas::Width; ++x)
          if (c.at(x, y) != Empty) assert(x >= 81 && x <= 119 && y >= 50 && y <= 94);
      c.guards();
    }
    assert(phases.size() == 4);
    Canvas expired;
    Art::drawExplosion(expired, 100, 70, 32, kind);
    const Canvas blank; assert(hash(expired) == hash(blank));
  }
  Canvas edge;
  for (int x : {0, 239}) for (int y : {0, 134}) {
    for (uint8_t kind = 0; kind < 8; ++kind) Art::drawPickupIcon(edge, x, y, 8, kind);
    for (uint8_t state = 0; state < 3; ++state) Art::drawScientist(edge, x, y, 12, state);
    for (uint8_t roll = 0; roll < 4; ++roll) Art::drawCrash(edge, x, y, 8, roll, false);
    Art::drawExplosion(edge, x, y, 31, 2);
  }
  edge.guards();
}

static void label(Canvas& c, const char* text, int x, int y) {
  flappy::MenuFont::draw(c.data(), Canvas::Width, Canvas::Height, text, x, y, Art::color(Art::MetalLight));
}

static void writePpm(const Canvas& c, const std::string& path) {
  std::ofstream file(path.c_str(), std::ios::binary);
  assert(file);
  file << "P6\n240 135\n255\n";
  for (unsigned i = 0; i < Canvas::Width * Canvas::Height; ++i) {
    const uint16_t value = c.data()[i];
    const char rgb[] = {char((value >> 11) * 255 / 31), char((value >> 5 & 63) * 255 / 63), char((value & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}

int main(int argc, char** argv) {
  runnerPosesAndBounds();
  labCoverageMotionAndPalette();
  foregroundShapesAndClipping();
  expandedVehicleSilhouettesAndAnimation();
  scientistsCrashesPickupsAndEffects();
  if (argc > 1) {
    const std::string base = argv[1];
    Canvas scene;
    Art::drawLab(scene, 123456, 24);
    Art::drawRunner(scene, 43, 68, 7, true, false, false, 0, false);
    for (int i = 0; i < 7; ++i) Art::drawCoin(scene, 82 + i * 12, 78 - i * 3, unsigned(i * 6));
    Art::drawZap(scene, 184, 36, 204, 66, 0);
    Art::drawMissile(scene, 216, 94, 3);
    writePpm(scene, base + "-scene.ppm");
    Canvas poses;
    poses.rect(0, 0, 240, 135, Art::color(Art::WallShade));
    for (unsigned suit = 0; suit < 3; ++suit) {
      for (unsigned pose = 0; pose < 4; ++pose)
        Art::drawRunner(poses, 24 + int(pose * 48), 20 + int(suit * 44), pose * 6, false, true, false, uint8_t(suit), false);
      Art::drawRunner(poses, 216, 20 + int(suit * 44), 7, true, false, false, uint8_t(suit), suit == 2);
    }
    writePpm(poses, base + "-poses.ppm");
    Canvas vehicles;
    vehicles.rect(0, 0, 240, 135, Art::color(Art::WallShade));
    const char* vehicleNames[] = {"RUNNER", "ARMOR", "GRAVITY", "HOG", "MECH", "BIRD", "TELEPORT", "DRAGON", "SAM"};
    for (uint8_t vehicle = 0; vehicle <= 8; ++vehicle) {
      const int x = 40 + (vehicle % 3) * 80, y = 20 + (vehicle / 3) * 45;
      Art::drawVehicle(vehicles, x, y, 5, vehicle, true, vehicle < 6);
      label(vehicles, vehicleNames[vehicle], x - 24, 33 + (vehicle / 3) * 45);
    }
    writePpm(vehicles, base + "-vehicles.ppm");
    Canvas extras;
    extras.rect(0, 0, 240, 135, Art::color(Art::WallShade));
    const char* iconNames[] = {"VEHICLE", "SHIELD", "MAGNET", "X2 COINS", "TOKEN", "CASH DASH", "BOOST", "ROCKET"};
    for (uint8_t icon = 0; icon < 8; ++icon) {
      const int x = 30 + (icon % 4) * 60, y = 18 + (icon / 4) * 40;
      Art::drawPickupIcon(extras, x, y, 8, icon);
      label(extras, iconNames[icon], x - 24, y + 11);
    }
    for (uint8_t state = 0; state < 3; ++state) Art::drawScientist(extras, 15 + state * 25, 102, 12, state);
    for (uint8_t roll = 0; roll < 4; ++roll) Art::drawCrash(extras, 105 + roll * 35, 102, 8, roll, false);
    label(extras, "STAFF", 8, 117); label(extras, "TUMBLE / BOUNCE", 98, 117);
    writePpm(extras, base + "-extras.ppm");
  }
  std::cout << "Jetpack artwork bounds, poses, palette, contrast and lab coverage passed\n";
}
