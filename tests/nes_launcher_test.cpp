#include "../firmware/flappy/app_model.h"
#include "../firmware/flappy/mario_viewport.h"
#include "../firmware/flappy/nes_runtime.h"
#include "../firmware/flappy/rom_fingerprint.h"
#include "../firmware/flappy/tetris_gym.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <new>
#include <vector>

namespace flappy { namespace coop { enum class Role { Off, Host }; } }

// The real launcher runs below with a real emulator and small hardware adapters.
namespace FlappyRom { const uint8_t* data = nullptr; size_t length = 0; }
namespace TetrisRom { const uint8_t* data = nullptr; size_t length = 0; }
namespace TetrisZeroRom { const uint8_t* data = nullptr; size_t length = 0; }

struct Resettable { void reset() {} void reset(bool) {} };
struct BoardViewport { bool configure(int, int) { return true; } };
struct Renderer {
  bool cacheEnabled = true;
  void setBackgroundCacheEnabled(bool enabled) { cacheEnabled = enabled; }
} rendererStorage;
struct Hardware {
  struct Button { bool isPressed() const { return false; } } BtnA;
  struct DisplayType { int width() const { return 240; } int height() const { return 135; } } Display;
} M5;
struct Link {
  struct Session { flappy::coop::Role role() const { return flappy::coop::Role::Off; } } value;
  const Session& session() const { return value; }
} coopLink;

flappy::AppModel app;
flappy::NesRuntime nes;
flappy::MarioCartridge marioCartridge;
flappy::MarioViewport marioViewport;
flappy::MarioLauncher marioLauncher;
flappy::MarioController marioController;
flappy::TetrisGym::Options tetrisOptions;
Renderer* renderer = &rendererStorage;
Resettable nesSystemButton, tetrisButtons, tetrisSoftDrop, tetrisActions[2];
BoardViewport zeroViewport, tetrisViewport;
uint32_t handledGameRequest = UINT32_MAX, nextTick = 0, nextNesTick = 0;
uint32_t tetrisFrontendAt = 0;
constexpr uint32_t TickMicros = 16667;
uint8_t pendingNesButtons = 0, pendingControllerNesButtons = 0, gameMessageShown = 0;
uint8_t tetrisLaunchStage = 0, tetrisStableFrames = 0;
bool piezoDriver = true, pendingTetrisDrop = false, pendingControllerTetrisDrop = false;
bool controllerDownTracking = false, pendingPadAction = false, pendingAction = false;
bool tetrisLaunching = false, zeroLaunching = false, marioLaunching = false, nativeMario = false;
bool landscape = false;
int16_t (*nesAudioBuffers)[512] = nullptr;
const char* runtimeNotice = "";
unsigned audioStops = 0;

void stopGameAudio() { ++audioStops; }
void invalidateDisplay() {}
void stopCoop() {}
bool emulatedGame() { return app.currentGame != flappy::GameTitle::FlappyBird; }
void setNesDisplay(bool enabled) { landscape = enabled; }
void nesVideo(void*, int, int, int, int, const uint16_t*) {}
void nesAudio(void*, const int16_t*, size_t, uint32_t) {}
void configureNesPerformance() { nes.setPcmEnabled(false); }
bool beginZero(int) { return false; }
int zeroSettings() { return 0; }
uint32_t micros() { return 1234; }

#include "generated_nes_launcher.h"

static std::vector<uint8_t> cartridge(bool nes2, bool pal, unsigned mapper) {
  const unsigned prg = mapper == 37 ? 16 : 2, chr = mapper == 37 ? 32 : 1;
  std::vector<uint8_t> rom(16 + prg * 16384 + chr * 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = uint8_t(prg); rom[5] = uint8_t(chr);
  rom[6] = uint8_t((mapper & 15) << 4);
  rom[7] = uint8_t((mapper & 0xf0) | (nes2 ? 8 : 0));
  rom[nes2 ? 12 : 9] = pal ? 1 : 0;
  for (unsigned bank = 0; bank < prg * 2; ++bank) {
    const size_t base = 16 + bank * 8192;
    rom[base] = 0x4c; rom[base + 1] = 0; rom[base + 2] = 0x80;
    for (unsigned offset = 0x1ffa; offset < 0x2000; offset += 2) {
      rom[base + offset] = 0; rom[base + offset + 1] = 0x80;
    }
  }
  return rom;
}

static void checkLoad(bool nes2, bool pal, unsigned mapper) {
  auto rom = cartridge(nes2, pal, mapper);
  app = flappy::AppModel();
  app.setNesAvailable(true);
  assert(app.setCurrentGame(flappy::GameTitle::Nes));
  FlappyRom::data = rom.data(); FlappyRom::length = rom.size();
  handledGameRequest = UINT32_MAX;
  pendingNesButtons = pendingControllerNesButtons = 0xff;
  tetrisLaunching = zeroLaunching = marioLaunching = nativeMario = true;
  handleGameRequest();
  assert(pendingNesButtons == 0 && pendingControllerNesButtons == 0);
  assert(!tetrisLaunching && !zeroLaunching);
  if (pal) {
    assert(!nes.ready());
    assert(app.currentGame == flappy::GameTitle::FlappyBird);
    assert(app.menu.page == flappy::MenuPage::ChangeGame);
    assert(std::strcmp(runtimeNotice, "Import an NTSC ROM") == 0);
    assert(renderer->cacheEnabled);
  } else {
    assert(nes.ready() && nes.frameRate() == 60);
    assert(app.currentGame == flappy::GameTitle::Nes && landscape);
    assert(!renderer->cacheEnabled && runtimeNotice[0] == 0);
    for (unsigned frame = 0; frame < 3; ++frame) assert(nes.step(0));
  }
  const unsigned stops = audioStops;
  handleGameRequest();
  assert(stops == audioStops);
  assert(app.setCurrentGame(flappy::GameTitle::FlappyBird));
  handleGameRequest();
  assert(!nes.ready() && renderer->cacheEnabled);
}

int main() {
  checkLoad(false, false, 0);
  checkLoad(false, true, 0);
  checkLoad(true, false, 0);
  checkLoad(true, true, 0);
  checkLoad(true, true, 37);
  checkLoad(true, false, 37);
  assert(nesAudioBuffers == nullptr);
  std::cout << "Firmware launcher accepts NTSC, rejects PAL and resets game state\n";
}
