#include "../firmware/flappy/geometry_renderer.h"

#include <algorithm>
#include <array>
#include <assert.h>
#include <iostream>
#include <vector>

namespace {
constexpr unsigned Width = 240, Height = 135, Pixels = Width * Height, Guards = 64;
constexpr uint16_t GuardColor = 0xa55a;
unsigned checkedFrames = 0;

struct Canvas {
  std::vector<uint16_t> storage;
  Canvas() : storage(Pixels + Guards * 2, GuardColor) {}
  uint16_t* data() { return storage.data() + Guards; }
  const uint16_t* data() const { return storage.data() + Guards; }
  void check() const {
    for (unsigned i = 0; i < Guards; ++i)
      assert(storage[i] == GuardColor && storage[Pixels + Guards + i] == GuardColor);
  }
};

struct Fixture {
  flappy::GeometryScene scene;
  flappy::GeometryRenderer normal, physical;
  flappy::GeometryRenderOptions options;
  std::array<uint8_t, 1024> ram{};
  std::array<uint8_t, 256> collisions{};
  Canvas normalCanvas, physicalCanvas;

  Fixture() {
    scene.state = 2;
    scene.scrollY = 0x300;
    scene.attempt = 1;
    scene.players[0].x = 80 << 8;
    scene.players[0].y = 176 << 8;
    scene.players[1] = scene.players[0];
    scene.players[1].x = 160 << 8;
    options.progress = 43;
    options.attempt = 12;
    for (unsigned i = 0; i < collisions.size(); ++i) collisions[i] = uint8_t(i % 62);
    collisions[0] = 0;
    for (unsigned page = 0; page < 4; ++page)
      for (unsigned row = 0; row < 15; ++row)
        for (unsigned col = 0; col < 16; ++col)
          ram[page * 256 + row * 16 + col] = uint8_t((row * 3 + col * 7) % 62);
    for (unsigned col = 0; col < 16; ++col)
      for (unsigned row = 12; row < 15; ++row) ram[3 * 256 + row * 16 + col] = 9;
  }

  void reset() { normal.reset(); physical.reset(); }

  void compare(bool reverseLayouts = false) {
    const auto readRam = [&](uint16_t address) -> uint8_t {
      assert(address >= 0x6000 && address < 0x6400);
      return ram[address - 0x6000];
    };
    const auto readRom = [&](uint32_t offset) -> uint8_t {
      assert(offset >= flappy::GeometryScene::CollisionTableFileOffset);
      assert(offset < flappy::GeometryScene::CollisionTableFileOffset + collisions.size());
      return collisions[offset - flappy::GeometryScene::CollisionTableFileOffset];
    };
    const auto originalRam = ram;
    auto normalOptions = options, physicalOptions = options;
    normalOptions.physicalScanOrder = reverseLayouts;
    physicalOptions.physicalScanOrder = !reverseLayouts;
    normal.draw(scene, readRam, readRom, normalCanvas.data(), normalOptions);
    physical.draw(scene, readRam, readRom, physicalCanvas.data(), physicalOptions);
    assert(normal.cameraY() == physical.cameraY());
    assert(normal.worldCameraY() == physical.worldCameraY());
    for (unsigned y = 0; y < Height; ++y) for (unsigned x = 0; x < Width; ++x) {
      const unsigned landscapeIndex = y * Width + x;
      const unsigned portraitIndex = x * Height + (Height - 1 - y);
      const unsigned normalIndex = reverseLayouts ? portraitIndex : landscapeIndex;
      const unsigned physicalIndex = reverseLayouts ? landscapeIndex : portraitIndex;
      assert(normalCanvas.data()[normalIndex] == physicalCanvas.data()[physicalIndex]);
    }
    assert(ram == originalRam);
    normalCanvas.check(); physicalCanvas.check();
    ++checkedFrames;
  }
};

static void everyOriginalLevelAndNativeTextState() {
  Fixture f;
  const char* const statuses[] = {nullptr, "PREPARING LEVEL", "LEVEL COMPLETE"};
  for (unsigned level = 0; level < 22; ++level)
    for (uint8_t state : {uint8_t(0), uint8_t(1), uint8_t(3), uint8_t(5), uint8_t(6)})
      for (bool wire : {false, true}) {
        f.scene.level = uint8_t(level); f.scene.state = state;
        f.options.wireOrder = wire;
        f.options.levelTitle = level & 1 ? flappy::GeometryRenderer::levelName(uint8_t(level)) : nullptr;
        f.options.status = statuses[level % 3];
        f.options.footer = level & 1 ? "Hold right: menu" : "A JUMP / HOLD TO FLY";
        f.compare();
      }
}

static void formsObjectsColorsAndWireOrder() {
  Fixture f;
  const uint16_t hues[] = {0xf800, 0x07e0, 0x001f, 0xf81f, 0xffff, 0};
  for (uint8_t mode = 0; mode < 9; ++mode)
    for (unsigned variant = 0; variant < 32; ++variant) {
      f.reset();
      f.scene.mode = mode;
      f.scene.level = uint8_t((mode * 3 + variant) % 22);
      f.scene.dual = (variant & 4) != 0;
      for (unsigned player = 0; player < 2; ++player) {
        auto& p = f.scene.players[player];
        p.mini = (variant & 1) != 0; p.gravity = (variant & 2) != 0;
        p.y = uint16_t((player ? 112 : 160) << 8);
        p.rotation = uint16_t((variant % 4) * 6 * 256);
        p.velY = int16_t((int(variant % 7) - 3) * 384);
      }
      for (unsigned object = 0; object < 16; ++object) {
        auto& o = f.scene.objects[object];
        o.active = true; o.activated = (variant & 1) != 0;
        o.type = uint8_t((variant * 16 + object) & 255);
        o.x = uint8_t(object * 16); o.y = uint8_t(80 + object % 5 * 16);
      }
      f.options.wireOrder = (variant & 8) != 0;
      f.options.useSourceColor = true;
      f.options.backgroundColor = hues[variant % 6];
      f.options.hud = (variant & 16) == 0;
      f.options.progress = uint8_t(variant % 3 ? variant * 3 : 255);
      f.compare();
    }
}

static void terrainClassesAndClippedTileEdges() {
  Fixture f;
  f.scene.players[0].y = 100 << 8;
  for (unsigned collision = 0; collision <= 0x3d; ++collision)
    for (uint32_t scroll : {uint32_t(0), uint32_t(1), uint32_t(7), uint32_t(15)}) {
      f.reset();
      f.ram.fill(128); f.collisions[128] = uint8_t(collision);
      f.scene.scrollX = scroll; f.scene.scrollY = uint16_t(0x300 + scroll);
      f.options.wireOrder = (collision & 1) != 0;
      f.options.fullSolidFastPath = (collision & 2) != 0;
      f.compare();
    }
}

static void movingCameraAndRepeatedLayoutSwitches() {
  Fixture f;
  for (unsigned frame = 0; frame < 300; ++frame) {
    f.scene.scrollX = frame * 3;
    f.scene.scrollY = uint16_t(frame < 150 ? 0x300 : 0x2ef);
    f.scene.players[0].y = uint16_t((40 + frame % 150) << 8);
    f.scene.players[0].velY = int16_t(frame % 31 * 192 - 3000);
    f.scene.mode = uint8_t(frame / 35 % 9);
    f.scene.attempt = 1 + frame / 100;
    f.scene.dual = (frame / 25 & 1) != 0;
    f.options.wireOrder = (frame & 1) != 0;
    f.options.progress = uint8_t(frame % 101);
    f.options.attempt = f.scene.attempt;
    f.compare((frame & 2) != 0);
  }
}

static void malformedCoordinatesKeepBothLayoutsInsideTheirCanvas() {
  Fixture f;
  for (uint32_t scrollX : {uint32_t(0), uint32_t(255), uint32_t(UINT32_MAX)})
    for (uint16_t scrollY : {uint16_t(0), uint16_t(0x3ef), uint16_t(0xffff)})
      for (uint8_t mode = 0; mode < 9; ++mode)
        for (uint16_t position : {uint16_t(0), uint16_t(0xffff)}) {
          f.reset();
          f.scene.scrollX = scrollX; f.scene.scrollY = scrollY; f.scene.mode = mode;
          f.scene.dual = true;
          for (auto& p : f.scene.players) {
            p.x = position; p.y = position; p.rotation = 65535;
            p.mini = (mode & 1) != 0; p.gravity = (mode & 2) != 0;
          }
          for (auto& o : f.scene.objects) {
            o.active = true; o.type = mode; o.x = uint8_t(position); o.y = uint8_t(position);
          }
          f.options.wireOrder = (mode & 1) != 0;
          f.compare();
        }
}

static void streamedPreviewObjectsKeepPhysicalScanOrderAndClipping() {
  Fixture f;
  f.ram.fill(0);
  for (unsigned col = 0; col < 16; ++col)
    for (unsigned row = 12; row < 15; ++row) f.ram[3 * 256 + row * 16 + col] = 9;
  f.options.hud = false;
  f.scene.previewCount = 0;
  f.compare();
  const auto baseline = f.normalCanvas.storage;
  auto& visible = f.scene.previews[0];
  visible.active = visible.preview = true;
  visible.type = 5; visible.x = 241; visible.y = 170;
  f.scene.previewCount = 1;
  f.compare();
  assert(f.normalCanvas.storage != baseline);
  for (uint8_t type : {uint8_t(0), uint8_t(0x10), uint8_t(5), uint8_t(0x0a), uint8_t(7)})
    for (int16_t x : {int16_t(241), int16_t(248), int16_t(272), int16_t(280)})
      for (int16_t y : {int16_t(-64), int16_t(-1), int16_t(0), int16_t(75),
                        int16_t(134), int16_t(239), int16_t(300)})
        for (bool wire : {false, true}) {
          f.reset(); f.scene.previewCount = 1;
          auto& object = f.scene.previews[0];
          object.active = object.preview = true;
          object.type = type; object.x = x; object.y = y;
          f.options.wireOrder = wire;
          f.compare();
        }
  for (unsigned i = 0; i < flappy::GeometryScene::PreviewCount; ++i) {
    auto& object = f.scene.previews[i];
    object.active = object.preview = true;
    object.type = uint8_t(i % 2 ? 0 : 5);
    object.x = int16_t(241 + i * 5); object.y = int16_t(95 + i * 12);
  }
  for (uint8_t count : {uint8_t(flappy::GeometryScene::PreviewCount), uint8_t(255)})
    for (bool wire : {false, true}) {
      f.scene.previewCount = count; f.options.wireOrder = wire;
      f.compare();
    }
}
}

int main() {
  everyOriginalLevelAndNativeTextState();
  formsObjectsColorsAndWireOrder();
  terrainClassesAndClippedTileEdges();
  movingCameraAndRepeatedLayoutSwitches();
  malformedCoordinatesKeepBothLayoutsInsideTheirCanvas();
  streamedPreviewObjectsKeepPhysicalScanOrderAndClipping();
  std::cout << "Geometry physical scan order matches every decoded pixel in " << checkedFrames << " guarded frames.\n";
}
