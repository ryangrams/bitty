#include "../firmware/flappy/mario_viewport.h"

#include <assert.h>
#include <iostream>
#include <vector>

using flappy::MarioViewport;

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
  for (int camera : {121, 90, 32}) {
    viewport.resetCamera();
    viewport.trackPlayer(camera + 18, 16, true);
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
  assert(viewport.worldTop() == 121);
  viewport.trackPlayer(100, 16, true);
  assert(viewport.worldTop() == 82);
  viewport.trackPlayer(110, 16, true);
  assert(viewport.worldTop() == 82);
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

static void guardedLauncher() {
  flappy::MarioCartridge cartridge;
  std::vector<uint8_t> unsupported(524304, 0);
  assert(!cartridge.configure(nullptr, 524304));
  assert(!cartridge.configure(unsupported.data(), 12));
  assert(!cartridge.configure(unsupported.data(), unsupported.size()));
  assert(!cartridge.supported());
  flappy::MarioLauncher launcher;
  launcher.reset(false);
  for (unsigned i = 0; i < 1000; ++i) {
    const auto input = launcher.step(false);
    assert(!input.buttons && !input.complete && !input.failed);
  }
  launcher.reset(true);
  for (unsigned i = 1; i <= 120; ++i) {
    const auto input = launcher.step(false);
    assert(input.buttons == (i % 90 == 30 ? 8 : 0));
    assert(!input.complete && !input.failed);
  }
  for (unsigned i = 0; i < 8; ++i) {
    const auto input = launcher.step(true);
    assert(!input.buttons && input.complete == (i == 7));
  }
  assert(launcher.step(false).complete);
  launcher.reset(true);
  for (unsigned i = 1; i <= 900; ++i) {
    const auto input = launcher.step(false);
    assert(input.failed == (i == 900));
  }
  assert(launcher.step(true).failed);
}

int main() {
  pixelsAndBounds();
  cameraAndPartialRectangles();
  guardedLauncher();
  std::cout << "Mario native pixel mapping, camera, bounds and guarded launcher passed\n";
}
