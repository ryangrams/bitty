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
  bool configure(const uint8_t* rom, size_t length) {
    supported_ = false;
    if (!rom || length != 524304u) return false;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
      crc ^= rom[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    supported_ = (crc ^ 0xffffffffu) == 0x4aa87799u;
    return supported_;
  }

  bool supported() const { return supported_; }

  template<class Runtime>
  MarioPlayer sample(const Runtime& runtime) const {
    MarioPlayer player;
    if (!supported_ || !runtime.ready()) return player;
    player.playing = runtime.readRam(0x770) == 1 && runtime.readRam(0x772) == 3;
    if (!player.playing || runtime.readRam(0xb5) != 1 || runtime.readRam(0x0e) != 8) return player;
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
// 119-row world window follows the player between source rows 32 and 239.
struct MarioViewport {
  enum { width = 240, height = 135 };
  int x = 0, y = 0;

  bool configure(int screenWidth, int screenHeight) {
    if (screenWidth < 240 || screenHeight < 135) return false;
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;
    resetCamera();
    return true;
  }

  void resetCamera() { worldTop_ = 121; }
  int worldTop() const { return worldTop_; }

  // Call once before each emulated frame so all source strips share a camera.
  void trackPlayer(int sourceY, int playerHeight, bool valid) {
    if (!valid || sourceY < 32 || sourceY > 239 || playerHeight < 1 || playerHeight > 64) return;
    const int bottom = sourceY + playerHeight;
    if (sourceY < worldTop_ + 18) worldTop_ = sourceY - 18;
    else if (bottom > worldTop_ + 101) worldTop_ = bottom - 101;
    if (worldTop_ < 32) worldTop_ = 32;
    if (worldTop_ > 121) worldTop_ = 121;
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
               uint16_t* output, int stride = 0) const {
    if (!source || !output || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return false;
    if (!stride) stride = width;
    if (stride < width) return false;
    if (x1 == 0 && y1 == 0)
      for (int row = 0; row < height; ++row) memset(output + row * stride, 0, width * sizeof(uint16_t));
    const int sourceWidth = x2 - x1 + 1;
    const int left = x1 > 8 ? x1 : 8;
    const int right = x2 < 247 ? x2 : 247;
    if (right >= left) {
      for (int sy = y1; sy <= y2; ++sy) {
        const int dy = targetRow(sy);
        if (dy < 0) continue;
        memcpy(output + dy * stride + left - 8, source + (sy - y1) * sourceWidth + left - x1,
               size_t(right - left + 1) * sizeof(uint16_t));
      }
    }
    return y2 == 239;
  }

 private:
  int worldTop_ = 121;
};

}  // namespace flappy
