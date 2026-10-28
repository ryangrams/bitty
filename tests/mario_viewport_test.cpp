#include "../firmware/flappy/mario_viewport.h"

#include <assert.h>
#include <iostream>
#include <vector>

using flappy::MarioViewport;

namespace flappy {
struct MarioCartridgeTestAccess {
  static void enable(MarioCartridge& cartridge) { cartridge.supported_ = true; }
};
}

static void stableBodyCamera() {
  struct Runtime {
    uint8_t ram[2048] = {};
    bool ready() const { return true; }
    uint8_t readRam(unsigned address) const { return ram[address]; }
  } runtime;
  flappy::MarioCartridge cartridge;
  runtime.ram[0x770] = 1;
  runtime.ram[0x772] = 3;
  runtime.ram[0x0e] = 8;
  runtime.ram[0xb5] = 1;
  runtime.ram[0xce] = 96;
  runtime.ram[0x3ad] = 60;
  assert(!cartridge.sample(runtime).valid);
  flappy::MarioCartridgeTestAccess::enable(cartridge);
  MarioViewport view;
  view.configure(240, 135);
  // No OAM or pattern data exists in this fixture. Tracking survives blinking.
  for (int frame = 0; frame < 60; ++frame) {
    runtime.ram[0x79e] = frame;
    const auto player = cartridge.sample(runtime);
    assert(player.valid && player.playing && player.x == 60);
    assert(player.y == 97 && player.height == 32);
    view.trackPlayer(player);
    assert(view.worldTop() == 91);
  }
  runtime.ram[0x754] = 1;
  assert(cartridge.sample(runtime).y == 113 && cartridge.sample(runtime).height == 16);
  runtime.ram[0x754] = 0;
  runtime.ram[0x714] = 1;
  assert(cartridge.sample(runtime).y == 105 && cartridge.sample(runtime).height == 24);
  runtime.ram[0x714] = 0;
  runtime.ram[0xce] = 106;
  view.trackPlayer(cartridge.sample(runtime));
  assert(view.worldTop() == 101);
  runtime.ram[0x770] = 2;
  assert(cartridge.sample(runtime).valid); // Flag and victory descent.
  runtime.ram[0x0e] = 6;
  assert(!cartridge.sample(runtime).valid);
  runtime.ram[0x0e] = 11;
  assert(!cartridge.sample(runtime).valid);
  runtime.ram[0x0e] = 8;
  runtime.ram[0xb5] = 2;
  assert(!cartridge.sample(runtime).valid);
  runtime.ram[0xb5] = 1;
  runtime.ram[0x770] = 0;
  assert(!cartridge.sample(runtime).valid);
}

static void nativePlatformCamera() {
  struct Runtime {
    uint8_t ram[2048] = {};
    bool ready() const { return true; }
    uint8_t readRam(unsigned address) const { return ram[address]; }
  } runtime;
  runtime.ram[0x770] = 1;
  runtime.ram[0x772] = 3;
  runtime.ram[0x0e] = 8;
  runtime.ram[0xb5] = 1;
  runtime.ram[0xce] = 195;
  runtime.ram[0x3ad] = 120;
  runtime.ram[0x71a] = 3;
  runtime.ram[0x71c] = 240;
  runtime.ram[0x0f] = 1;
  runtime.ram[0x16] = 0x25;
  runtime.ram[0x6e] = 4;
  runtime.ram[0x87] = 100;
  runtime.ram[0xb6] = 1;
  runtime.ram[0xcf] = 227;
  flappy::MarioCartridge cartridge;
  flappy::MarioCartridgeTestAccess::enable(cartridge);
  MarioViewport view;
  assert(view.configure(240,135));
  auto player = cartridge.sample(runtime);
  assert(player.landingBottom == 239);
  view.trackPlayer(player);
  assert(view.worldTop() == 120);
  for (int row = 228; row < 236; ++row) assert(view.targetRow(row) >= 16);
  // The platform stays below the jumping player instead of being cut off at
  // the old row-223 limit. Native Mario coordinates remain untouched.
  runtime.ram[0xce] = 131;
  player = cartridge.sample(runtime);
  assert(player.landingBottom == 239);
  view.trackPlayer(player);
  assert(view.worldTop() == 120 && view.targetRow(player.y) >= 16);
  for (int row = 228; row < 236; ++row) assert(view.targetRow(row) >= 16);
  runtime.ram[0x87] = 20;  // Platform outside Mario's horizontal reach.
  assert(cartridge.sample(runtime).landingBottom == -1);
  runtime.ram[0x87] = 100;
  runtime.ram[0x16] = 6;  // An enemy cannot become a landing camera target.
  assert(cartridge.sample(runtime).landingBottom == -1);
  runtime.ram[0x16] = 0x25;
  runtime.ram[0xb6] = 2;
  assert(cartridge.sample(runtime).landingBottom == -1);
  runtime.ram[0xb6] = 1;
  runtime.ram[0xcf] = 80;  // Surface above Mario cannot hold the camera down.
  assert(cartridge.sample(runtime).landingBottom == -1);
  runtime.ram[0x16] = 0x2b;
  runtime.ram[0xcf] = 99;  // Second small-lift surface is at 228.
  runtime.ram[0xce] = 195;
  assert(cartridge.sample(runtime).landingBottom == 239);
  runtime.ram[0xcf] = 227; // Wrapped second surface is at 100.
  runtime.ram[0xce] = 67;
  assert(cartridge.sample(runtime).landingBottom == 111);
  runtime.ram[0x0f] = 0;
  assert(cartridge.sample(runtime).landingBottom == -1);
}

static void pixelsAndBounds() {
  MarioViewport viewport;
  assert(!viewport.configure(239, 135));
  assert(!viewport.configure(240, 134));
  assert(viewport.configure(320, 240));
  assert(viewport.width == 240 && viewport.height == 135 && viewport.x == 40 && viewport.y == 52);
  const unsigned guard = 9;
  std::vector<uint16_t> output(guard + 244 * 135 + guard, 0xa55a);
  std::vector<uint16_t> source(256 * 240);
  for (unsigned i = 0; i < source.size(); ++i) source[i] = i;
  for (int camera : {121, 120, 105, 90, 32}) {
    viewport.resetCamera();
    viewport.trackPlayer(camera + 6, 16, true, camera + MarioViewport::WorldRows);
    assert(viewport.worldTop() == camera);
    for (int y = 0; y < 240; y += 8)
      assert(viewport.consume(0, y, 255, y + 7, source.data() + y * 256,
                              output.data() + guard, 244) == (y == 232));
    for (int y = 0; y < 135; ++y) {
      for (int x = 0; x < 240; ++x)
        assert(output[guard + y * 244 + x] == source[viewport.sourceY(y) * 256 + x + 8]);
      for (int x = 240; x < 244; ++x) assert(output[guard + y * 244 + x] == 0xa55a);
    }
  }
  for (unsigned i = 0; i < guard; ++i) assert(output[i] == 0xa55a && output[output.size() - 1 - i] == 0xa55a);
  assert(!viewport.consume(-1, 0, 255, 7, source.data(), output.data()));
  assert(!viewport.consume(0, 0, 256, 7, source.data(), output.data()));
  assert(!viewport.consume(0, 0, 255, 240, source.data(), output.data()));
  assert(!viewport.consume(0, 8, 255, 7, source.data(), output.data()));
  assert(!viewport.consume(0, 0, 255, 7, nullptr, output.data()));
  assert(!viewport.consume(0, 0, 255, 7, source.data(), nullptr));
  assert(!viewport.consume(0, 0, 255, 7, source.data(), output.data(), 239));
}

static void cameraAndPartialRectangles() {
  MarioViewport viewport;
  assert(viewport.configure(240, 135));
  viewport.trackPlayer(193, 16, true);
  assert(viewport.worldTop() == 105);
  viewport.trackPlayer(100, 16, true);
  assert(viewport.worldTop() == 94);
  viewport.trackPlayer(110, 16, true);
  assert(viewport.worldTop() == 104);
  viewport.trackPlayer(190, 16, true);
  assert(viewport.worldTop() == 105);
  viewport.trackPlayer(239, 64, true);
  assert(viewport.worldTop() == 121);
  viewport.trackPlayer(32, 16, true);
  assert(viewport.worldTop() == 32);
  viewport.trackPlayer(200, 0, true);
  viewport.trackPlayer(200, 16, false);
  viewport.trackPlayer(-1, 16, true);
  assert(viewport.worldTop() == 32);
  assert(viewport.targetRow(0) == -1 && viewport.targetRow(15) == -1);
  assert(viewport.targetRow(16) == 0 && viewport.targetRow(31) == 15);
  assert(viewport.targetRow(32) == 16 && viewport.targetRow(150) == 134 && viewport.targetRow(151) == -1);
  std::vector<uint16_t> output(240 * 135, 0);
  const uint16_t block[] = {1, 2, 3, 4};
  assert(!viewport.consume(16, 32, 17, 33, block, output.data()));
  assert(output[16 * 240 + 8] == 1 && output[16 * 240 + 9] == 2);
  assert(output[17 * 240 + 8] == 3 && output[17 * 240 + 9] == 4);
}

static void ridingAndJumpingLiftDamage() {
  MarioViewport viewport;
  assert(viewport.configure(240,135));
  const uint16_t sky = 0x1234, platform = 0xfd20, playerColor = 0xe1c4;
  std::vector<uint16_t> source(256 * 240), output(240 * 135,0), lcd(240 * 135,0);
  unsigned cameraMoves = 0;
  int previous = viewport.worldTop();
  for (unsigned frame = 0; frame < 720; ++frame) {
    const int cycle = int(frame % 240);
    const int platformTop = 105 + (cycle < 120 ? cycle : 240 - cycle);
    const int jumpFrame = int(frame % 120);
    const int jump = jumpFrame < 64 ? (jumpFrame < 32 ? jumpFrame * 2 : (64 - jumpFrame) * 2) : 0;
    const int playerHeight = frame < 360 ? 32 : 16;
    const int top = platformTop - playerHeight - jump;
    viewport.trackPlayer(top,playerHeight,true,platformTop + 11);
    cameraMoves += viewport.worldTop() != previous;
    previous = viewport.worldTop();
    std::fill(source.begin(),source.end(),sky);
    for (int y = platformTop; y < platformTop + 8; ++y)
      for (int x = 100; x < 148; ++x) source[y * 256 + x] = platform;
    // The camera ignores OAM blinking, while each visible frame reaches LCD.
    if (frame & 1) for (int y = top; y < top + playerHeight; ++y)
      if (y >= 32 && y < 240) for (int x = 112; x < 128; ++x) source[y * 256 + x] = playerColor;
    for (int y = 0; y < 240; y += 8) {
      viewport.consume(0,y,255,y+7,source.data() + y*256,output.data());
      for (int sy = y; sy < y+8; ++sy) {
        const int dy = viewport.targetRow(sy);
        if (dy >= 0 && viewport.rowChanged(dy))
          std::copy(output.begin()+dy*240,output.begin()+(dy+1)*240,lcd.begin()+dy*240);
      }
    }
    for (int row = 0; row < 135; ++row)
      for (int x = 0; x < 240; ++x)
        assert(lcd[row*240+x] == source[viewport.sourceY(row)*256+x+8]);
    if (platformTop + 11 - (top - MarioViewport::Headroom) <= MarioViewport::WorldRows && top >= 38)
      for (int y = platformTop; y < platformTop+8; ++y) {
        const int dy = viewport.targetRow(y);
        assert(dy >= 16 && lcd[dy*240+112-8] == platform);
      }
  }
  assert(cameraMoves > 100);
  viewport.trackPlayer(177,32,true);
  assert(viewport.worldTop() == MarioViewport::RestWorldTop);
}


static void exactRowDamage() {
  MarioViewport viewport;
  assert(viewport.configure(240, 135));
  std::vector<uint16_t> source(256 * 240, 0x35a1), output(240 * 135, 0x35a1);
  auto render = [&]() {
    for (int y = 0; y < 240; y += 8)
      viewport.consume(0, y, 255, y + 7, source.data() + y * 256, output.data());
  };
  render();
  for (int row = 0; row < 135; ++row) assert(viewport.rowChanged(row));
  render();
  for (int row = 0; row < 135; ++row) assert(!viewport.rowChanged(row));
  source[16 * 256 + 8] ^= 1;
  source[180 * 256 + 180] ^= 1;
  source[64 * 256 + 64] ^= 1;  // Hidden source row.
  render();
  for (int row = 0; row < 135; ++row)
    assert(viewport.rowChanged(row) == (row == 0 || row == viewport.targetRow(180)));
  viewport.invalidate();
  render();
  for (int row = 0; row < 135; ++row) assert(viewport.rowChanged(row));
  assert(!viewport.rowChanged(-1) && !viewport.rowChanged(135));
  // A descending jump restores lower ground immediately.
  viewport.trackPlayer(100, 16, true);
  assert(viewport.worldTop() + 118 >= 208);
  viewport.trackPlayer(110, 16, true);
  assert(viewport.worldTop() == 104);
}

static void alternatingSpriteFrames() {
  MarioViewport viewport;
  assert(viewport.configure(240, 135));
  const uint16_t sky = 0x35a1, sprite = 0xe1c4;
  std::vector<uint16_t> source(256 * 240, sky), output(240 * 135, 0), lcd(240 * 135, 0);
  unsigned visibleFrames = 0, hiddenFrames = 0, phaseVisible[2] = {}, phaseHidden[2] = {};
  bool displayed[2] = {};
  for (int frame = 0; frame < 24; ++frame) {
    const bool visible = (frame & 1) == 0;
    for (int y = 193; y < 209; ++y)
      for (int x = 40; x < 56; ++x) source[y * 256 + x] = visible ? sprite : sky;
    viewport.trackPlayer(193, 16, visible);
    for (int y = 0; y < 240; y += 8)
      viewport.consume(0, y, 255, y + 7, source.data() + y * 256, output.data());
    for (int row = 0; row < 135; ++row) {
      if (frame)
        assert(viewport.rowChanged(row) == (row >= viewport.targetRow(193) && row <= viewport.targetRow(208)));
      if (viewport.rowChanged(row))
        for (int x = 0; x < 240; ++x) lcd[row * 240 + x] = output[row * 240 + x];
      for (int x = 0; x < 240; ++x)
        assert(lcd[row * 240 + x] == source[viewport.sourceY(row) * 256 + x + 8]);
    }
    const bool onLcd = lcd[viewport.targetRow(200) * 240 + 40 - 8] == sprite;
    assert(onLcd == visible);
    visibleFrames += onLcd;
    hiddenFrames += !onLcd;
    const unsigned phase = unsigned(frame) & 1;
    displayed[phase] = onLcd;
    phaseVisible[phase] += displayed[phase];
    phaseHidden[phase] += !displayed[phase];
  }
  assert(visibleFrames == 12 && hiddenFrames == 12);
  assert(phaseVisible[0] == 12 && phaseHidden[0] == 0);
  assert(phaseVisible[1] == 0 && phaseHidden[1] == 12);
}

int main() {
  stableBodyCamera();
  nativePlatformCamera();
  pixelsAndBounds();
  cameraAndPartialRectangles();
  exactRowDamage();
  alternatingSpriteFrames();
  ridingAndJumpingLiftDamage();
  std::cout << "Mario native pixels, lift riding and jump camera, alternating sprite damage and bounds passed\n";
}
