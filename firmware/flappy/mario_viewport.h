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

// The RAM layout was checked with normal NES input on these cartridges.
class MarioCartridge {
 public:
  enum class Screen : uint8_t { Other, Title, GameOver, Playing };
  bool configure(const uint8_t* rom, size_t length) {
    supported_ = false;
    standalone_ = false;
    if (!rom || (length != 524304u && length != 40976u)) return false;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
      crc ^= rom[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    const uint32_t checksum = crc ^ 0xffffffffu;
    standalone_ = length == 40976u && checksum == 0x3337ec46u;
    supported_ = standalone_ ||
        (length == 524304u && checksum == 0x4aa87799u);
    return supported_;
  }

  bool supported() const { return supported_; }
  bool clockSupported() const { return standalone_; }

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
    player.x = runtime.readRam(0x3ad);
    // Player_Y_Position anchors a 32-row body. Small Mario uses the lower
    // 16 rows and crouching big Mario uses the lower 24 rows. Animation ink and
    // hidden invincibility OAM must never change the camera position or make
    // another actor its tracking target.
    player.height = runtime.readRam(0x754) != 0 ? 16 : runtime.readRam(0x714) != 0 ? 24 : 32;
    player.y = int(runtime.readRam(0xce)) + 33 - player.height;
    player.valid = player.y < 240;
    return player;
  }

 private:
  friend struct MarioCartridgeTestAccess;
  bool supported_ = false;
  bool standalone_ = false;
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
