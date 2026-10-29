#pragma once
#include <math.h>
#include <stdint.h>
namespace flappy {
namespace StickInput {
// Use right-tap fallback only when the S3 power key could not be configured.
inline bool nextGame(bool rightFallback, bool home, bool menu, bool leftClick, bool rightClick) {
  return home && !menu && (rightFallback ? rightClick : leftClick);
}
inline const char* nextLabel(bool rightFallback) { return rightFallback ? "RIGHT NEXT" : "LEFT NEXT"; }
inline const char* homeLabel(bool rightFallback) { return rightFallback ? "M5 PLAY RIGHT NEXT" : "M5 PLAY LEFT NEXT"; }
// Positive means screen-right movement in the portrait gameplay orientation.
// The S3's rightward roll reads negative Y in the verified play orientation.
// The original Plus uses negative X.
inline float horizontalTilt(bool stickS3, float x, float y) { return stickS3 ? -y : -x; }
inline const char* neutralKey(bool stickS3, bool tetris) {
  return stickS3 ? (tetris ? "s3tetney3" : "s3arcney3") : (tetris ? "tetneutral" : "arcneutral");
}
// Collect a short stable window at each gameplay launch. Launch zeros stay in
// RAM so restarting a game does not repeatedly write flash preferences.
class LaunchZero {
 public:
  void begin(uint32_t now) { pending_ = true; start_ = now; count_ = 0; mean_ = 0; }
  void cancel() { pending_ = false; }
  bool pending() const { return pending_; }
  bool expired(uint32_t now) const { return pending_ && uint32_t(now - start_) >= 800; }
  float neutral() const { return mean_; }
  bool sample(float value, uint32_t now) {
    if (!pending_ || !isfinite(value) || value < -1.5f || value > 1.5f) return false;
    if (count_ && fabsf(value - mean_) > .08f) count_ = 0;
    if (!count_) mean_ = value;
    else mean_ += (value - mean_) / float(count_ + 1);
    ++count_;
    if (count_ >= 8 || (count_ >= 3 && uint32_t(now - start_) >= 600)) {
      pending_ = false; return true;
    }
    return false;
  }
 private:
  uint32_t start_ = 0;
  float mean_ = 0;
  uint8_t count_ = 0;
  bool pending_ = false;
};
} // namespace StickInput
} // namespace flappy
