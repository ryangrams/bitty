#pragma once
namespace flappy {
namespace StickInput {
// Use right-tap fallback only when the S3 power key could not be configured.
inline bool nextGame(bool rightFallback, bool home, bool menu, bool leftClick, bool rightClick) {
  return home && !menu && (rightFallback ? rightClick : leftClick);
}
inline const char* nextLabel(bool rightFallback) { return rightFallback ? "RIGHT NEXT" : "LEFT NEXT"; }
inline const char* homeLabel(bool rightFallback) { return rightFallback ? "M5 PLAY RIGHT NEXT" : "M5 PLAY LEFT NEXT"; }
// S3 portrait Y+ points right. Lowering that edge gives negative acceleration.
// https://docs.m5stack.com/en/core/StickS3 (IMU orientation diagram)
inline float horizontalTilt(bool stickS3, float x, float y) { return stickS3 ? -y : -x; }
inline const char* neutralKey(bool stickS3, bool tetris) {
  return stickS3 ? (tetris ? "s3tetneuy" : "s3arcneuy") : (tetris ? "tetneutral" : "arcneutral");
}
} // namespace StickInput
} // namespace flappy
