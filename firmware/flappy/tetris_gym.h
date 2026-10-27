#pragma once
#include "nes_runtime.h"

namespace flappy {

// Address facts from TetrisGYM 6.0.0, commit f487037746c9ff3d6e679fb7e4381e9264364bac.
// The cartridge source and graphics stay outside the public project.
struct TetrisGym {
  struct Options {
    uint8_t level = 0;
    uint8_t trainer = 0;
    uint8_t height = 0;
    bool pal = false;
    uint8_t seed[3] = {0x89, 0x88, 0};
  };

  static bool recognized(const NesRuntime& runtime) {
    const uint8_t signature[] = {0x54, 0x2d, 0x47, 0x59, 0x4d};
    if (!runtime.ready()) return false;
    for (uint8_t i = 0; i < 5; ++i) if (runtime.readRam(0x75b + i) != signature[i]) return false;
    return true;
  }

  static uint8_t stage(const NesRuntime& runtime) { return runtime.readRam(0xc0); }
  static bool playing(const NesRuntime& runtime) {
    return recognized(runtime) && stage(runtime) == 4 && runtime.readRam(0xbd) == 3;
  }

  // Call only while the ROM is waiting in its trainer or level menu, then send a normal Start press.
  static bool configureMenu(NesRuntime& runtime, const Options& options) {
    if (!recognized(runtime) || options.height > 8) return false;
    if (options.trainer != 0 && options.trainer != 2 && options.trainer != 6) return false;
    const uint8_t current = stage(runtime);
    if (current != 2 && current != 3) return false;
    if (options.trainer == 2 && options.seed[0] == 0 && (options.seed[1] & 0xfe) == 0) return false;
    runtime.writeRam(0x600, options.trainer);
    runtime.writeRam(0x770, options.height);
    runtime.writeRam(0x789, options.pal ? 1 : 0);
    runtime.setPal(options.pal);
    if (options.trainer == 2) {
      for (uint8_t i = 0; i < 3; ++i) runtime.writeRam(0x37 + i, options.seed[i]);
    }
    if (current == 3) {
      runtime.writeRam(0x764, 1);
      runtime.writeRam(0x765, options.level);
    }
    return true;
  }

  static uint32_t score(const NesRuntime& runtime) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) value |= uint32_t(runtime.readRam(8 + i)) << (i * 8);
    return value;
  }

  // The low byte holds two BCD digits. The high byte is a binary count of hundreds.
  // B-Type reports its remaining target lines here.
  static uint16_t lines(const NesRuntime& runtime) {
    const uint8_t low = runtime.readRam(0x50);
    return uint16_t(runtime.readRam(0x51) * 100 + (low >> 4) * 10 + (low & 15));
  }

  static uint8_t level(const NesRuntime& runtime) { return runtime.readRam(0x44); }

  static bool finished(const NesRuntime& runtime) {
    return recognized(runtime) && stage(runtime) == 4 && runtime.readRam(0x48) == 10;
  }

  static bool typeBComplete(const NesRuntime& runtime) {
    return finished(runtime) && runtime.readRam(0x600) == 6 && lines(runtime) == 0;
  }

  static bool toppedOut(const NesRuntime& runtime) {
    return finished(runtime) && !typeBComplete(runtime);
  }
};

}  // namespace flappy
