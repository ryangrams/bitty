#include "../firmware/flappy/geometry_renderer.h"

#include <array>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

namespace {
using flappy::GeometryScene;
using flappy::GeometryRenderer;
using flappy::GeometryRenderOptions;
constexpr unsigned Pixels = GeometryRenderer::Width * GeometryRenderer::Height;
constexpr uint16_t Guard = 0xa55a;
constexpr uint16_t Yellow = uint16_t((255 >> 3) << 11 | (229 >> 2) << 5);
constexpr uint16_t Pink = uint16_t((255 >> 3) << 11 | (91 >> 2) << 5 | (211 >> 3));
constexpr uint16_t White = 0xffff;

struct Fixture {
  GeometryScene scene;
  GeometryRenderer renderer;
  GeometryRenderOptions options;
  std::array<uint8_t, 1024> ram{};
  std::array<uint8_t, 256> collisions{};
  std::array<uint16_t, Pixels + 128> storage;

  Fixture() {
    storage.fill(Guard);
    scene.state = 2;
    scene.scrollY = 0x300;
    scene.players[0].x = 80 << 8;
    scene.players[0].y = 100 << 8;
    options.hud = false;
    collisions[0x10] = 7;
    collisions[0x11] = 8;
    collisions[0x19] = 5;
    collisions[1] = 9;
  }
  uint16_t* pixels() { return storage.data() + 64; }
  uint8_t readRam(uint16_t address) const {
    assert(address >= 0x6000 && address < 0x6400);
    return ram[address - 0x6000];
  }
  uint8_t readRom(uint32_t offset) const {
    assert(offset >= GeometryScene::CollisionTableFileOffset && offset < GeometryScene::CollisionTableFileOffset + 256);
    return collisions[offset - GeometryScene::CollisionTableFileOffset];
  }
  void draw() {
    renderer.draw(scene, [this](uint16_t address) { return readRam(address); },
      [this](uint32_t offset) { return readRom(offset); }, pixels(), options);
    for (unsigned i = 0; i < 64; ++i) assert(storage[i] == Guard && storage[Pixels + 64 + i] == Guard);
  }
  void tile(int worldX, int worldY, uint8_t id) {
    assert(worldX >= 0 && worldY >= 0 && worldY < 960);
    const unsigned index = unsigned(worldY / 240) * 256 + unsigned(worldY % 240) / 16 * 16 + ((unsigned(worldX) >> 4) & 15);
    ram[index] = id;
  }
  std::vector<uint16_t> image() { return {pixels(), pixels() + Pixels}; }
  uint16_t at(int x, int y) { assert(x >= 0 && x < 240 && y >= 0 && y < 135); return pixels()[y * 240 + x]; }
  void floor() { for (int y = 912; y < 960; y += 16) for (int x = 0; x < 256; x += 16) tile(x, y, 1); }
};

static uint64_t hash(const std::vector<uint16_t>& image) {
  uint64_t result = 1469598103934665603ULL;
  for (uint16_t pixel : image) result = (result ^ pixel) * 1099511628211ULL;
  return result;
}

static void fullWidthStatesAndVerifiedText() {
  Fixture f;
  f.options.hud = true;
  std::set<uint64_t> names;
  for (uint8_t level = 0; level < 22; ++level) {
    assert(std::strlen(GeometryRenderer::levelName(level)) > 0);
    assert(flappy::MenuFont::textWidth(GeometryRenderer::levelName(level)) <= 208);
    f.scene.level = level;
    for (uint8_t state : {uint8_t(0), uint8_t(1), uint8_t(3), uint8_t(5), uint8_t(6)}) {
      f.scene.state = state;
      f.renderer.draw(f.scene, [](uint16_t) -> uint8_t { assert(false); return 0; },
        [](uint32_t) -> uint8_t { assert(false); return 0; }, f.pixels(), f.options);
      assert(f.at(0, 0) != 0 && f.at(239, 0) != 0 && f.at(0, 134) != 0 && f.at(239, 134) != 0);
      for (unsigned i = 0; i < 64; ++i) assert(f.storage[i] == Guard && f.storage[Pixels + 64 + i] == Guard);
    }
    names.insert(hash(f.image()));
  }
  assert(names.size() == 22);
  f.scene.state = 2;
  f.draw();
  const auto unknownProgress = f.image();
  f.options.progress = 45; f.options.attempt = 7;
  f.draw();
  assert(unknownProgress != f.image());
  f.renderer.reset();
  f.renderer.draw(f.scene, [](uint16_t) -> uint8_t { assert(false); return 0; },
    [](uint32_t) -> uint8_t { assert(false); return 0; }, nullptr);
}

static void nativeBlocksAndGapsKeepTheirCoordinates() {
  Fixture f;
  f.floor();
  f.scene.players[0].y = 176 << 8;
  f.tile(128, 880, 0x10);
  f.draw();
  assert(f.renderer.cameraY() == 77);
  const int top = 880 - f.scene.linearScrollY() - f.renderer.cameraY();
  const int left = 128 - f.renderer.cameraX();
  assert(f.at(left, top) == White && f.at(left + 15, top) == White);
  assert(f.at(left, top + 15) == White && f.at(left + 15, top + 15) == White);
  for (unsigned step = 1; step < 16; ++step) {
    f.scene.scrollX = step;
    f.draw();
    assert(f.at(left - int(step), top) == White && f.at(left + 15 - int(step), top) == White);
  }
  f.scene.scrollX = 0;
  f.draw();
  assert(f.at(8, 123) != White);
  for (int y = 912; y < 960; y += 16) { f.tile(96, y, 0); f.tile(112, y, 0); }
  f.draw();
  assert(f.at(128 - f.renderer.cameraX(), 123) == White);
  const uint16_t inGap = f.at(106 - f.renderer.cameraX(), 124), onFloor = f.at(138 - f.renderer.cameraX(), 124);
  Fixture empty;
  empty.scene.players[0].y = f.scene.players[0].y;
  empty.draw();
  assert(inGap == empty.at(106 - empty.renderer.cameraX(), 124));
  assert(onFloor != inGap);
}

static void startupKeepsTheCubeVisibleThenAnchorsAtForty() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8;
  for (int playerX = 0; playerX <= 80; ++playerX) {
    f.scene.players[0].x = uint16_t(playerX << 8); f.draw();
    const int displayX = playerX - f.renderer.cameraX();
    assert(displayX == (playerX < 40 ? playerX : 40));
    assert(f.renderer.cameraX() >= 0 && f.renderer.cameraX() <= 40);
    unsigned yellow = 0;
    for (int y = 0; y < 135; ++y) for (int x = displayX; x < displayX + 16; ++x)
      yellow += f.at(x,y) == Yellow;
    assert(yellow > 0);
  }
  flappy::GeometryWorld world;
  f.scene.attempt = 1;
  assert(world.capture(f.scene, [&](uint16_t address) { return f.readRam(address); }));
  std::array<uint8_t,57> forward{};
  forward[55] = 0x10;
  world.storeLevelColumn(17, forward.data());
  f.scene.bindWorld(&world); f.draw();
  assert(f.at(232, 83) == White && f.at(239, 83) == White);
  assert(f.at(8, 83) != White);
}

static void sourcePaletteControlsSaturatedBackgrounds() {
  Fixture f;
  f.options.useSourceColor = true;
  f.options.backgroundColor = 0xf81f;
  f.draw();
  const auto pink = f.image();
  const uint16_t pinkEdge = f.at(0, 134);
  assert(((pinkEdge >> 11) & 31) > ((pinkEdge >> 5) & 63) / 2);
  assert((pinkEdge & 31) > ((pinkEdge >> 5) & 63) / 2);
  f.options.backgroundColor = 0x07e0;
  f.draw();
  const uint16_t greenEdge = f.at(0, 134);
  assert(pink != f.image());
  assert(((greenEdge >> 5) & 63) > ((greenEdge >> 11) & 31) * 2);
  assert(((greenEdge >> 5) & 63) > (greenEdge & 31) * 2);
  for (int y = 0; y < 135; ++y) assert(f.at(0, y) != 0 && f.at(239, y) != 0);
  f.options.backgroundColor = 0;
  f.draw();
  const auto blackFallback = f.image();
  f.options.backgroundColor = 0xffff;
  f.draw();
  assert(blackFallback == f.image());
  f.options.useSourceColor = false;
  f.draw();
  assert(blackFallback == f.image());
}

static void partialPlatformsHaveEmptyHalves() {
  Fixture f;
  f.scene.players[0].x = 32 << 8;
  f.scene.players[0].y = 176 << 8;
  f.floor(); f.draw();
  const auto baseline = f.image();
  f.tile(128, 880, 0x19);
  f.draw();
  const int x = 128 - f.renderer.cameraX(), y = 880 - 720 - f.renderer.cameraY();
  assert(f.at(x + 7, y) == White);
  assert(f.at(x + 7, y + 7) == White);
  for (int dy = 8; dy < 16; ++dy)
    for (int dx = 0; dx < 16; ++dx) assert(f.at(x + dx, y + dy) == baseline[(y + dy) * 240 + x + dx]);
}

static void allTerrainClassesStayWithinTheirCells() {
  Fixture f;
  f.scene.players[0].x = 16 << 8;
  f.scene.players[0].y = 176 << 8;
  f.floor(); f.draw();
  const auto baseline = f.image();
  const int left = 128 - f.renderer.cameraX(), top = 880 - 720 - f.renderer.cameraY();
  for (unsigned collision = 1; collision <= 0x3d; ++collision) {
    if (collision == 10) continue;
    f.collisions[128] = uint8_t(collision);
    f.tile(128, 880, 128);
    f.draw();
    unsigned changed = 0;
    for (int y = 0; y < 135; ++y) for (int x = 0; x < 240; ++x) {
      const bool differs = f.at(x, y) != baseline[y * 240 + x];
      if (differs) { assert(x >= left && x < left + 16 && y >= top && y < top + 16); ++changed; }
    }
    assert(changed > 0);
  }
}

static void nineFormsMiniGravityAndRotation() {
  Fixture f;
  std::set<uint64_t> forms;
  for (uint8_t mode = 0; mode < 9; ++mode) {
    f.renderer.reset(); f.scene.mode = mode; f.draw();
    std::vector<uint16_t> local;
    const int x = f.scene.players[0].pixelX() - f.renderer.cameraX(), y = f.scene.players[0].pixelY() - f.renderer.cameraY();
    for (int dy = -5; dy < 21; ++dy) for (int dx = -9; dx < 25; ++dx) {
      const uint16_t color = f.at(x + dx, y + dy);
      local.push_back(color == Yellow || color == uint16_t((3 >> 3) << 11 | (241 >> 2) << 5 | (255 >> 3)) || color == uint16_t((14 >> 2) << 5 | (35 >> 3)) ? color : 0);
    }
    forms.insert(hash(local));
  }
  assert(forms.size() == 9);
  f.scene.mode = 0; f.renderer.reset(); f.draw();
  const auto normal = f.image();
  const int x = f.scene.players[0].pixelX() - f.renderer.cameraX(), y = f.scene.players[0].pixelY() - f.renderer.cameraY();
  f.scene.players[0].gravity = true; f.draw();
  for (int dy = 0; dy < 16; ++dy) for (int dx = 0; dx < 16; ++dx)
    assert(f.at(x + dx, y + dy) == normal[(y + 15 - dy) * 240 + x + dx]);
  f.scene.players[0].gravity = false; f.scene.players[0].rotation = 6 * 256; f.draw();
  for (int dy = 0; dy < 16; ++dy) for (int dx = 0; dx < 16; ++dx)
    assert(f.at(x + dx, y + dy) == normal[(y + 15 - dx) * 240 + x + dy]);
  f.scene.players[0].rotation = 0; f.scene.players[0].mini = true; f.draw();
  unsigned yellow = 0;
  for (int dy = 0; dy < 16; ++dy) for (int dx = 0; dx < 16; ++dx) if (f.at(x + dx, y + dy) == Yellow) {
    assert(dx >= 0 && dx < 8 && dy >= 4 && dy < 12); ++yellow;
  }
  assert(yellow > 0 && yellow < 64);
  f.scene.players[0].mini = false;
  f.scene.dual = true; f.scene.players[1] = f.scene.players[0]; f.scene.players[1].x = 112 << 8;
  f.draw();
  unsigned pink = 0;
  for (unsigned i = 0; i < Pixels; ++i) pink += f.pixels()[i] == Pink;
  assert(pink > 0);
}

static void objectAnchorsAndInvisibleTriggers() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8;
  f.draw();
  const auto baseline = f.image();
  auto& object = f.scene.objects[0];
  object.active = true; object.x = 128; object.y = 128;
  for (uint8_t type : {uint8_t(0x0f), uint8_t(0x40), uint8_t(0x47), uint8_t(0x70), uint8_t(0x7e), uint8_t(0x80), uint8_t(0xff)}) {
    object.type = type; f.draw(); assert(f.image() == baseline);
  }
  for (uint8_t type : {uint8_t(0), uint8_t(5), uint8_t(0x0a), uint8_t(7)}) {
    object.type = type; f.draw(); assert(f.image() != baseline);
  }
  object.type = 0; f.draw();
  int minX = 240, maxX = -1, minY = 135, maxY = -1;
  for (int y = 0; y < 135; ++y) for (int x = 0; x < 240; ++x) if (f.at(x, y) != baseline[y * 240 + x]) {
    minX = x < minX ? x : minX; maxX = x > maxX ? x : maxX;
    minY = y < minY ? y : minY; maxY = y > maxY ? y : maxY;
  }
  assert(minX == 128 - f.renderer.cameraX() && maxX == 151 - f.renderer.cameraX() && minY == 129 - f.renderer.cameraY() && maxY == 176 - f.renderer.cameraY());
}

static void forwardObjectPreviewsKeepAnchorsAndNativeState() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8; f.draw();
  const auto baseline = f.image();
  auto& native = f.scene.objects[0];
  native.type = 0; native.active = true; native.x = 248; native.y = 128;
  f.draw(); assert(f.image() == baseline);
  auto& preview = f.scene.previews[0];
  preview = native; preview.preview = true; f.scene.previewCount = 1;
  f.draw();
  int minX = 240, maxX = -1, minY = 135, maxY = -1;
  for (int y = 0; y < 135; ++y) for (int x = 0; x < 240; ++x) if (f.at(x,y) != baseline[y * 240 + x]) {
    minX = x < minX ? x : minX; maxX = x > maxX ? x : maxX;
    minY = y < minY ? y : minY; maxY = y > maxY ? y : maxY;
  }
  assert(minX == 208 && maxX == 231 && minY == 52 && maxY == 99);
  assert(native.x == 248 && native.y == 128 && native.active && !native.preview && !native.activated);
  preview.type = 0x18; preview.x = 280; f.draw();
  assert(f.image() != baseline);
  for (int y = 0; y < 135; ++y) for (int x = 0; x < 232; ++x) assert(f.at(x,y) == baseline[y * 240 + x]);
  f.scene.previewCount = 255;
  for (auto& object : f.scene.previews) { object = preview; object.x = 320; object.y = -1024; }
  f.draw(); assert(f.image() == baseline);
}

static uint16_t rawScroll(int linear) { return uint16_t((linear / 240) * 256 + linear % 240); }

static void cameraKeepsJumpAndLandingVisible() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8; f.draw();
  assert(f.renderer.cameraY() == 77);
  assert(f.renderer.worldCameraY() == 797);
  for (int py = 174; py >= 132; py -= 2) {
    f.scene.players[0].y = uint16_t(py << 8); f.draw();
    assert(f.renderer.worldCameraY() == 797);
  }
  for (int py = 134; py <= 176; py += 2) {
    f.scene.players[0].y = uint16_t(py << 8); f.draw();
    assert(f.renderer.worldCameraY() == 797);
  }
  for (int py = 174; py >= 40; py -= 2) {
    const int before = f.renderer.worldCameraY();
    f.scene.players[0].y = uint16_t(py << 8); f.draw();
    assert(std::abs(f.renderer.worldCameraY() - before) <= 8);
    assert(py - f.renderer.cameraY() >= 8);
  }
  for (int py = 42; py <= 176; py += 2) {
    const int before = f.renderer.worldCameraY();
    f.scene.players[0].y = uint16_t(py << 8); f.draw();
    assert(std::abs(f.renderer.worldCameraY() - before) <= 8);
    assert(py - f.renderer.cameraY() < 135);
  }
}

static void nativeCameraScrollCannotMoveTheWorldView() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8;
  f.draw();
  const auto baseline = f.image();
  for (int scroll = 657; scroll <= 896; ++scroll) {
    f.scene.scrollY = rawScroll(scroll);
    f.scene.players[0].y = uint16_t((896 - scroll) * 256);
    f.draw();
    assert(f.renderer.worldCameraY() == 797 && f.image() == baseline);
  }
  for (int boundary : {240, 480, 720}) {
    Fixture page;
    page.scene.scrollY = rawScroll(boundary - 15);
    const int playerWorldY = boundary + 112, floorWorldY = playerWorldY + 16;
    page.scene.players[0].y = uint16_t((playerWorldY - (boundary - 15)) * 256);
    for (int x = 0; x < 256; x += 16) page.tile(x, floorWorldY, 1);
    page.draw();
    const auto expected = page.image();
    for (int scroll = boundary - 15; scroll <= boundary + 15; ++scroll) {
      page.scene.scrollY = rawScroll(scroll);
      page.scene.players[0].y = uint16_t((playerWorldY - scroll) * 256);
      page.draw();
      assert(page.image() == expected);
    }
  }
}

static void gravityChangeAndDualFollowStayContinuous() {
  Fixture f;
  f.floor(); f.scene.players[0].y = 176 << 8; f.draw();
  for (int x = 0; x < 256; x += 16) f.tile(x,800,0x10);
  f.scene.players[0].gravity = true;
  for (int py = 174; py >= 96; py -= 2) {
    const int before = f.renderer.worldCameraY();
    f.scene.players[0].y = uint16_t(py << 8); f.draw();
    assert(std::abs(f.renderer.worldCameraY() - before) <= 8);
    assert(py - f.renderer.cameraY() >= 0 && py - f.renderer.cameraY() < 135);
  }
  Fixture dual;
  dual.floor(); dual.scene.players[0].y = 176 << 8;
  dual.scene.dual = true; dual.scene.players[1] = dual.scene.players[0];
  dual.scene.players[1].x = 160 << 8; dual.scene.players[1].y = 96 << 8;
  for (unsigned frame = 0; frame < 120; ++frame) {
    const int before = dual.renderer.worldCameraY(); dual.draw();
    if (frame) assert(std::abs(dual.renderer.worldCameraY() - before) <= 8);
    for (const auto& player : dual.scene.players)
      assert(player.pixelY() - dual.renderer.cameraY() >= 0 && player.pixelY() - dual.renderer.cameraY() + 16 <= 135);
  }
}

static void malformedAndBoundaryCoordinatesAreClipped() {
  Fixture f;
  for (uint32_t scrollX : {uint32_t(0), uint32_t(255), uint32_t(UINT32_MAX)})
    for (uint16_t scrollY : {uint16_t(0), uint16_t(0x3ef), uint16_t(0xffff)})
      for (uint8_t mode = 0; mode < 9; ++mode)
        for (uint16_t position : {uint16_t(0), uint16_t(0xffff)}) {
          f.scene.scrollX = scrollX; f.scene.scrollY = scrollY; f.scene.mode = mode;
          f.scene.dual = true;
          for (auto& player : f.scene.players) { player.x = position; player.y = position; player.mini = mode & 1; player.gravity = mode & 2; player.rotation = 65535; }
          for (auto& object : f.scene.objects) { object.active = true; object.type = mode; object.x = uint8_t(position); object.y = uint8_t(position); }
          f.draw();
        }
}

static void wireOrderMatchesEveryNativePixel() {
  Fixture native, wire;
  native.floor(); wire.floor();
  native.options.hud = wire.options.hud = true;
  native.options.progress = wire.options.progress = 43;
  native.options.attempt = wire.options.attempt = 12;
  native.options.useSourceColor = wire.options.useSourceColor = true;
  wire.options.wireOrder = true;
  for (uint8_t state : {uint8_t(0), uint8_t(1), uint8_t(2), uint8_t(3), uint8_t(6)})
    for (uint8_t mode = 0; mode < 9; ++mode)
      for (uint16_t color : {uint16_t(0xf81f), uint16_t(0x07e0), uint16_t(0x001f), uint16_t(0xffff)}) {
        native.scene.state = wire.scene.state = state;
        native.scene.mode = wire.scene.mode = mode;
        native.options.backgroundColor = wire.options.backgroundColor = color;
        native.scene.dual = wire.scene.dual = true;
        native.scene.players[1] = wire.scene.players[1] = native.scene.players[0];
        native.scene.players[1].x = wire.scene.players[1].x = 112 << 8;
        for (unsigned i = 0; i < 16; ++i) {
          native.scene.objects[i].active = wire.scene.objects[i].active = true;
          native.scene.objects[i].type = wire.scene.objects[i].type = uint8_t(i);
          native.scene.objects[i].x = wire.scene.objects[i].x = uint8_t(32 + i * 12);
          native.scene.objects[i].y = wire.scene.objects[i].y = uint8_t(96 + i % 4 * 16);
        }
        native.draw(); wire.draw();
        for (unsigned i = 0; i < Pixels; ++i) {
          const uint16_t encoded = wire.pixels()[i];
          assert(native.pixels()[i] == uint16_t((encoded >> 8) | (encoded << 8)));
        }
      }
}

static void fullSolidFillMatchesGenericRasterForAllNeighborClasses() {
  Fixture fast, reference;
  reference.options.fullSolidFastPath = false;
  fast.scene.players[0].x = reference.scene.players[0].x = 32 << 8;
  fast.scene.players[0].y = reference.scene.players[0].y = 100 << 8;
  const auto compare = [&]() {
    fast.draw(); reference.draw();
    assert(fast.image() == reference.image());
  };
  const auto setTile = [&](int x, int y, uint8_t id, uint8_t collision) {
    fast.collisions[id] = reference.collisions[id] = collision;
    fast.tile(x, y, id); reference.tile(x, y, id);
  };
  for (uint8_t full : {uint8_t(7), uint8_t(9), uint8_t(0x1f)}) {
    for (unsigned neighbor = 0; neighbor <= 0x3d; ++neighbor) {
      for (unsigned direction = 0; direction < 5; ++direction) {
        fast.ram.fill(0); reference.ram.fill(0);
        fast.renderer.reset(); reference.renderer.reset();
        const int x = 128, y = 880;
        setTile(x, y, 128, full);
        for (unsigned edge = 0; edge < 4; ++edge) {
          const int dx[] = {0,0,-16,16}, dy[] = {-16,16,0,0};
          const uint8_t collision = direction == 4 ? uint8_t((neighbor + edge * 13) % 62) :
            direction == edge ? uint8_t(neighbor) : uint8_t(0);
          setTile(x + dx[edge], y + dy[edge], uint8_t(129 + edge), collision);
        }
        fast.options.wireOrder = reference.options.wireOrder = (neighbor & 1) != 0;
        compare();
      }
    }
  }
  for (int x : {0, 240}) for (int y : {800, 928}) {
    fast.scene.scrollY = reference.scene.scrollY = 0x303;
    for (uint8_t full : {uint8_t(7), uint8_t(9), uint8_t(0x1f)})
      for (uint8_t neighbor : {uint8_t(0), uint8_t(7), uint8_t(5), uint8_t(0x0b), uint8_t(0x3d)}) {
        fast.ram.fill(0); reference.ram.fill(0);
        fast.renderer.reset(); reference.renderer.reset();
        setTile(x, y, 128, full);
        if (x >= 16) setTile(x - 16, y, 129, neighbor);
        if (x < 240) setTile(x + 16, y, 130, neighbor);
        setTile(x, y - 16, 131, neighbor);
        if (y < 944) setTile(x, y + 16, 132, neighbor);
        compare();
      }
  }
}
}

int main() {
  static_assert(sizeof(GeometryRenderer) <= 48, "Renderer state must stay small");
  fullWidthStatesAndVerifiedText();
  nativeBlocksAndGapsKeepTheirCoordinates();
  startupKeepsTheCubeVisibleThenAnchorsAtForty();
  sourcePaletteControlsSaturatedBackgrounds();
  partialPlatformsHaveEmptyHalves();
  allTerrainClassesStayWithinTheirCells();
  nineFormsMiniGravityAndRotation();
  objectAnchorsAndInvisibleTriggers();
  forwardObjectPreviewsKeepAnchorsAndNativeState();
  cameraKeepsJumpAndLandingVisible();
  nativeCameraScrollCannotMoveTheWorldView();
  gravityChangeAndDualFollowStayContinuous();
  malformedAndBoundaryCoordinatesAreClipped();
  wireOrderMatchesEveryNativePixel();
  fullSolidFillMatchesGenericRasterForAllNeighborClasses();
  std::cout << "Native Geometry vectors, terrain gaps, forms, camera, anchors and guarded canvas checks passed.\n";
}
