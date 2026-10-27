#pragma once
#include "nes_runtime.h"

namespace flappy {

// Use with the validated MMC3_118_v2 cartridge. Menus receive normal NES input.
struct TetrisZero {
  struct Options {
    uint8_t level = 0;
    uint8_t type = 0;
    uint8_t players = 2;
  };

  struct Inputs {
    uint8_t player1 = 0;
    uint8_t player2 = 0;
    bool complete = false;
  };

  static uint8_t stage(const NesRuntime& runtime) { return runtime.readRam(0xc0); }
  static uint8_t players(const NesRuntime& runtime) {
    const uint8_t value = runtime.readRam(0xbe);
    return value == 1 || value == 2 ? value : 0;
  }
  static bool playing(const NesRuntime& runtime) {
    return runtime.ready() && stage(runtime) == 4;
  }
  static uint8_t pieceY(const NesRuntime& runtime, uint8_t player = 1) {
    return runtime.readRam(playerBase(runtime, player) + 1);
  }
  static bool active(const NesRuntime& runtime, uint8_t player = 1) {
    return playing(runtime) && player >= 1 && player <= players(runtime) &&
           runtime.readRam(playerBase(runtime, player) + 8) == 1;
  }
  static uint8_t level(const NesRuntime& runtime, uint8_t player = 1) {
    return runtime.readRam(playerBase(runtime, player) + 4);
  }
  static uint16_t lines(const NesRuntime& runtime, uint8_t player = 1) {
    const uint16_t address = playerBase(runtime, player) + 0x10;
    const uint8_t low = runtime.readRam(address);
    return uint16_t(runtime.readRam(address + 1) * 100 + (low >> 4) * 10 + (low & 15));
  }
  static bool finished(const NesRuntime& runtime) {
    if (!playing(runtime)) return false;
    if (runtime.readRam(playerBase(runtime, 1) + 8) == 10) return true;
    const uint8_t mode = runtime.readRam(0xa7);
    return runtime.readRam(0xc1) == 1 && (mode == 4 || mode == 5);
  }

  class Launcher {
   public:
    void reset(const Options& options) {
      options_ = options;
      if (options_.level > 9) options_.level = 9;
      if (options_.type > 1) options_.type = 0;
      if (options_.players != 1 && options_.players != 2) options_.players = 2;
      stage_ = 255;
      stable_ = cooldown_ = 0;
      frames_ = 0;
      complete_ = false;
    }

    // Call once before each emulated frame, starting from a cartridge reset.
    Inputs step(const NesRuntime& runtime) {
      Inputs result;
      if (complete_) { result.complete = true; return result; }
      const uint8_t current = TetrisZero::stage(runtime);
      if (stage_ != current) { stage_ = current; stable_ = cooldown_ = 0; }
      else if (stable_ < 255) ++stable_;
      ++frames_;
      if (active(runtime) && (options_.players == 1 || active(runtime, 2))) {
        complete_ = result.complete = true;
        return result;
      }
      if (stable_ < 15) return result;
      if (cooldown_) { --cooldown_; return result; }
      switch (current) {
        case 0:
          if (frames_ % 60 == 15) result.player1 = NesRuntime::Start;
          break;
        case 1:
          result.player1 = TetrisZero::players(runtime) == options_.players ? NesRuntime::Start :
              options_.players == 2 ? NesRuntime::Right : NesRuntime::Left;
          break;
        case 2:
          result.player1 = runtime.readRam(0xc1) == options_.type ? NesRuntime::Start :
              options_.type ? NesRuntime::Right : NesRuntime::Left;
          break;
        case 3:
          result.player1 = direction(runtime.readRam(0x47), options_.level);
          if (options_.players == 2) result.player2 = direction(runtime.readRam(0x67), options_.level);
          if (!result.player1 && !result.player2) result.player1 = NesRuntime::Start;
          break;
        default: break;
      }
      if (result.player1 || result.player2) cooldown_ = 3;
      return result;
    }

   private:
    Options options_;
    uint32_t frames_ = 0;
    uint8_t stage_ = 255, stable_ = 0, cooldown_ = 0;
    bool complete_ = false;

    static uint8_t direction(uint8_t current, uint8_t target) {
      if (current > 9) return NesRuntime::Up;
      if (current / 5 < target / 5) return NesRuntime::Down;
      if (current / 5 > target / 5) return NesRuntime::Up;
      if (current % 5 < target % 5) return NesRuntime::Right;
      if (current % 5 > target % 5) return NesRuntime::Left;
      return 0;
    }
  };

 private:
  // The cartridge leaves P2 in the working block after a two-player frame.
  static uint16_t playerBase(const NesRuntime& runtime, uint8_t player) {
    return players(runtime) == 2 ? (player == 2 ? 0x40 : 0x60) : (player == 2 ? 0x60 : 0x40);
  }
};

}  // namespace flappy
