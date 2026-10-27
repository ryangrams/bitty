#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {

struct MarioPlayer {
  int x = 0;
  int y = 0;
  int height = 0;
  bool playing = false;
  bool valid = false;
};

// The RAM and OAM layout was checked with normal NES input on this cartridge.
class MarioCartridge {
 public:
  enum class Screen : uint8_t { Other, Title, GameOver, Playing };
  bool configure(const uint8_t* rom, size_t length) {
    supported_ = false;
    if (!rom || (length != 524304u && length != 40976u)) return false;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
      crc ^= rom[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    const uint32_t checksum = crc ^ 0xffffffffu;
    supported_ = (length == 40976u && checksum == 0x3337ec46u) ||
        (length == 524304u && checksum == 0x4aa87799u);
    return supported_;
  }

  bool supported() const { return supported_; }

  template<class Runtime>
  Screen screen(const Runtime& runtime) const {
    if (!supported_ || !runtime.ready()) return Screen::Other;
    const uint8_t mode = runtime.readRam(0x770);
    const uint8_t task = runtime.readRam(0x772);
    if (mode == 0 && task == 3) return Screen::Title;
    if (mode == 3) return Screen::GameOver;
    if (mode == 1 && task == 3) return Screen::Playing;
    return Screen::Other;
  }

  template<class Runtime>
  MarioPlayer sample(const Runtime& runtime) const {
    MarioPlayer player;
    if (!supported_ || !runtime.ready()) return player;
    const uint8_t mode = runtime.readRam(0x770);
    const uint8_t engine = runtime.readRam(0x0e);
    player.playing = mode == 1 && runtime.readRam(0x772) == 3;
    const bool cameraEligible = player.playing || mode == 2;
    if (!cameraEligible || runtime.readRam(0xb5) != 1 || engine == 6 || engine == 11) return player;
    const auto* core = runtime.core();
    if (!core) return player;
    player.x = runtime.readRam(0x3ad);
    int top = 240, bottom = -1;
    // OAM entries 1..8 contain the player's two-column, four-row sprite block.
    // Empty tiles in that block are excluded from the camera's visible bounds.
    for (unsigned i = 1; i <= 8; ++i) {
      const auto& sprite = core->nes_ppu.sprite_info[i];
      if (sprite.y >= 239 || (sprite.x != player.x && sprite.x != uint8_t(player.x + 8))) continue;
      const unsigned height = core->nes_ppu.CTRL_H ? 16u : 8u;
      const unsigned base = height == 16u ? (sprite.tile_index_number & 1u) * 4u :
                            (core->nes_ppu.CTRL_S ? 4u : 0u);
      for (unsigned row = 0; row < height; ++row) {
        const unsigned patternRow = sprite.flip_v ? height - 1u - row : row;
        const unsigned tile = height == 16u ? (sprite.tile_index_number & 254u) + patternRow / 8u :
                              sprite.tile_index_number;
        const uint8_t* bank = core->nes_ppu.pattern_table[base + tile / 64u];
        if (!bank) continue;
        const uint8_t* pattern = bank + (tile % 64u) * 16u + patternRow % 8u;
        if (!(pattern[0] | pattern[8])) continue;
        const int y = int(sprite.y) + 1 + int(row);
        if (y >= 240) continue;
        if (y < top) top = y;
        if (y > bottom) bottom = y;
      }
    }
    if (top <= bottom) {
      player.y = top;
      player.height = bottom - top + 1;
      player.valid = true;
    }
    return player;
  }

 private:
  bool supported_ = false;
};

// Short title-screen inputs become native Start or A+Start. A brief grace period
// lets two separate Bluetooth reports form a chord before a new game starts.
class MarioController {
 public:
  void reset() { previous_ = blocked_ = pending_ = wait_ = phase_ = 0; }

  uint8_t step(MarioCartridge::Screen screen, uint8_t pad) {
    const uint8_t pressed = pad & uint8_t(~previous_);
    previous_ = pad;
    blocked_ &= pad;
    uint8_t output = pad & uint8_t(~blocked_);
    const bool title = screen == MarioCartridge::Screen::Title;
    const bool gameOver = screen == MarioCartridge::Screen::GameOver;
    if (!title && !gameOver) {
      if (screen == MarioCartridge::Screen::Playing) pending_ = wait_ = phase_ = 0;
      return output;
    }
    if (phase_ == 1) {
      phase_ = 2;
      return 1;  // Hold A before the Start edge.
    }
    if (phase_ == 2) {
      const uint8_t buttons = pending_ == 2 ? 9 : 8;
      blocked_ |= pad & 0x33;
      pending_ = wait_ = phase_ = 0;
      return buttons;
    }
    const bool chord = (pad & 3) == 3 && (pressed & 3);
    if (chord) { pending_ = 2; wait_ = 6; }
    else if ((pressed & 0x32) && pending_ != 2) { pending_ = 1; wait_ = 6; }
    output &= uint8_t(~0x32);  // Title-only B, Up and Down shortcuts.
    if (pending_) {
      output &= uint8_t(~3);
      if (title && wait_ && --wait_ == 0) phase_ = pending_ == 2 ? 1 : 2;
    }
    return output;
  }

 private:
  uint8_t previous_ = 0, blocked_ = 0, pending_ = 0, wait_ = 0, phase_ = 0;
};

class MarioLauncher {
 public:
  struct Inputs {
    uint8_t buttons = 0;
    bool complete = false;
    bool failed = false;
  };

  void reset(bool supported) {
    enabled_ = supported;
    complete_ = failed_ = false;
    frames_ = 0;
    stable_ = 0;
  }

  Inputs step(bool playing) {
    Inputs input;
    if (!enabled_) return input;
    if (complete_ || failed_) {
      input.complete = complete_;
      input.failed = failed_;
      return input;
    }
    if (playing) {
      if (++stable_ >= 8) complete_ = input.complete = true;
    } else stable_ = 0;
    if (++frames_ >= 900 && !complete_) failed_ = input.failed = true;
    if (!playing && !complete_ && !failed_ && frames_ % 90u == 30u) input.buttons = 8;
    return input;
  }

 private:
  uint16_t frames_ = 0;
  uint8_t stable_ = 0;
  bool enabled_ = false, complete_ = false, failed_ = false;
};

// Every output pixel copies one source pixel. The HUD remains fixed while the
// 119-row world window follows the player while favoring the landing area.
struct MarioViewport {
  enum { width = 240, height = 135, MinWorldTop = 32, MaxWorldTop = 105, Headroom = 6 };
  int x = 0, y = 0;

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 240 || screenHeight < 135) return false;
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;
    resetCamera();
    return true;
  }

  void resetCamera() { worldTop_ = MaxWorldTop; invalidate(); }
  void invalidate() { force_ = true; }
  int worldTop() const { return worldTop_; }
  bool rowChanged(int row) const {
    return row >= 0 && row < height && (dirty_[unsigned(row) >> 5] & (uint32_t(1) << (row & 31)));
  }

  // Call once before each emulated frame so all source strips share a camera.
  void trackPlayer(int sourceY, int playerHeight, bool valid) {
    if (!valid || sourceY < 32 || sourceY > 239 || playerHeight < 1 || playerHeight > 64) return;
    // Keep the window as low as possible while leaving six rows above Mario.
    // It returns toward the landing area immediately as Mario descends.
    worldTop_ = sourceY - Headroom;
    if (worldTop_ < MinWorldTop) worldTop_ = MinWorldTop;
    if (worldTop_ > MaxWorldTop) worldTop_ = MaxWorldTop;
  }

  void trackPlayer(const MarioPlayer& player) { trackPlayer(player.y, player.height, player.valid); }
  int sourceX(int column) const { return column + 8; }
  int sourceY(int row) const { return row < 16 ? row + 16 : worldTop_ + row - 16; }
  int targetRow(int sourceRow) const {
    if (sourceRow >= 16 && sourceRow < 32) return sourceRow - 16;
    return sourceRow >= worldTop_ && sourceRow < worldTop_ + 119 ? sourceRow - worldTop_ + 16 : -1;
  }

  // Source rectangles include both endpoints. The caller owns 240x135 pixels.
  bool consume(int x1, int y1, int x2, int y2, const uint16_t* source,
               uint16_t* output, int stride = 0) {
    if (!source || !output || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    if (x1 == 0 && y1 == 0) memset(dirty_, 0, sizeof(dirty_));
    const int sourceWidth = x2 - x1 + 1;
    const int left = x1 > 8 ? x1 : 8;
    const int right = x2 < 247 ? x2 : 247;
    if (right >= left) {
      for (int sy = y1; sy <= y2; ++sy) {
        const int dy = targetRow(sy);
        if (dy < 0) continue;
        uint16_t* target = output + dy * stride + left - 8;
        const uint16_t* pixels = source + (sy - y1) * sourceWidth + left - x1;
        const size_t bytes = size_t(right - left + 1) * sizeof(uint16_t);
        const bool different = memcmp(target, pixels, bytes) != 0;
        if (force_ || different) dirty_[unsigned(dy) >> 5] |= uint32_t(1) << (dy & 31);
        if (different) memcpy(target, pixels, bytes);
      }
    }
    if (y2 == 239) { force_ = false; return true; }
    return false;
  }

 private:
  int worldTop_ = MaxWorldTop;
  uint32_t dirty_[5] = {};
  bool force_ = true;
};

}  // namespace flappy
