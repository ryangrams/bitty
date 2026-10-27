#pragma once

#include <stdint.h>
#include <string.h>

namespace flappy {

// Row hashes can collide. A full refresh every second limits a missed change's duration.
class DamageRows {
 public:
  enum { MaxRows = 256 };
  static const uint32_t RefreshMs = 1000;

  bool beginFrame(int width, int height, uint32_t nowMs) {
    if (width <= 0 || width > 65535 || height <= 0 || height > MaxRows) {
      invalidate();
      return false;
    }
    if (active_ || width_ != width || height_ != height) invalidate();
    width_ = uint16_t(width);
    height_ = uint16_t(height);
    frameMs_ = nowMs;
    full_ = !haveFull_ || uint32_t(nowMs - fullMs_) >= RefreshMs;
    memset(seen_, 0, sizeof(seen_));
    active_ = true;
    return true;
  }

  bool changed(int row, const uint16_t* pixels) {
    if (!active_ || !pixels || row < 0 || row >= height_) return false;
    uint32_t hash = 2166136261u;
    for (uint32_t x = 0; x < width_; ++x) {
      const uint16_t pixel = pixels[x];
      hash = (hash ^ uint8_t(pixel)) * 16777619u;
      hash = (hash ^ uint8_t(pixel >> 8)) * 16777619u;
    }
    const uint32_t mask = uint32_t(1) << (unsigned(row) & 31u);
    const unsigned word = unsigned(row) >> 5;
    const bool different = full_ || !(valid_[word] & mask) || hashes_[row] != hash;
    hashes_[row] = hash;
    seen_[word] |= mask;
    return different;
  }

  void endFrame() {
    if (!active_) return;
    bool complete = true;
    for (unsigned word = 0; word < 8; ++word) valid_[word] |= seen_[word];
    for (unsigned row = 0; row < height_; ++row) {
      if (!(seen_[row >> 5] & (uint32_t(1) << (row & 31u)))) complete = false;
    }
    if (full_ && complete) {
      fullMs_ = frameMs_;
      haveFull_ = true;
    }
    active_ = false;
  }

  void invalidate() {
    memset(valid_, 0, sizeof(valid_));
    haveFull_ = false;
    active_ = false;
  }

 private:
  uint32_t hashes_[MaxRows] = {};
  uint32_t valid_[8] = {};
  uint32_t seen_[8] = {};
  uint32_t frameMs_ = 0;
  uint32_t fullMs_ = 0;
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  bool haveFull_ = false;
  bool full_ = false;
  bool active_ = false;
};

}  // namespace flappy
