#pragma once

#include <stdint.h>

namespace flappy {

constexpr int Width = 288;
constexpr int Height = 512;
constexpr int GroundY = 404;
constexpr int BirdWidth = 34;
constexpr int BirdHeight = 24;
constexpr int BirdX = 57;
constexpr int PipeWidth = 52;
constexpr int PipeHeight = 320;
constexpr int PipeGap = 100;
constexpr int PipeSpacing = 144;
constexpr int PipeCount = 4;
constexpr int RestartDelayFrames = 42;
constexpr float Gravity = 0.25f;
constexpr float FlapVelocity = -4.5f;
constexpr float MaxFallVelocity = 5.0f;
constexpr float PipeSpeed = 2.0f;

enum class State : uint8_t { Ready, Playing, Dying, GameOver };

enum Event : uint8_t {
  NoEvent = 0,
  Flap = 1 << 0,
  Point = 1 << 1,
  Hit = 1 << 2,
  Die = 1 << 3,
  GameOver = 1 << 4
};

struct Pipe {
  float x;
  int16_t gapY;
  bool scored;
};

// Coordinates and physics use the 288 x 512 reference canvas at 60 ticks/sec.
// Reference: github.com/sourabhv/FlapPyBird, commit 4fc78596c9f4abf0071a5490734cee1c33fd4fa0.
class Game {
 public:
  State state = State::Ready;
  float birdY = 244.0f;
  float velocityY = 0.0f;
  float rotation = 0.0f;
  uint8_t birdFrame = 0;
  uint8_t groundOffset = 0;
  uint16_t score = 0;
  uint16_t bestScore = 0;
  bool newRecord = false;
  uint32_t stateFrames = 0;
  Pipe pipes[PipeCount];

  explicit Game(uint32_t seed = 0x8f6e37a1U) { reset(seed); }

  void setBestScore(uint16_t best) { bestScore = best; }

  // The caller owns masks generated from its locally supplied artwork.
  void setBirdMasks(const uint64_t (*masks)[BirdHeight]) { birdMasks_ = masks; }

  // A zero seed continues the current random sequence between rounds.
  void reset(uint32_t seed = 0) {
    if (seed != 0) randomState_ = seed;
    state = State::Ready;
    birdY = 244.0f;
    velocityY = 0.0f;
    rotation = 0.0f;
    birdFrame = 0;
    groundOffset = 0;
    score = 0;
    newRecord = false;
    stateFrames = 0;
    animationFrames_ = 0;
    animationPhase_ = 0;
    groundCrash_ = false;
    deathSoundPlayed_ = false;
    for (int i = 0; i < PipeCount; ++i) {
      pipes[i] = {float(Width + 200 + i * PipeSpacing), randomGap(), false};
    }
  }

  // Pass a press edge, after debouncing and hold detection in the device layer.
  uint8_t step(bool flapEdge) {
    ++stateFrames;
    if (state == State::GameOver) {
      if (flapEdge && stateFrames >= RestartDelayFrames) reset();
      return NoEvent;
    }
    if (state == State::Dying) return stepDying();

    groundOffset = uint8_t((groundOffset + 2) % 48);
    animateBird(state == State::Ready ? 10 : 6);

    if (state == State::Ready) {
      if (!flapEdge) {
        const int bobFrame = int(stateFrames % 64);
        const int triangle = bobFrame <= 16 ? bobFrame :
                             (bobFrame <= 48 ? 32 - bobFrame : bobFrame - 64);
        birdY = 244.0f + float(triangle) * 0.5f;
        return NoEvent;
      }
      state = State::Playing;
      stateFrames = 0;
    }

    uint8_t events = NoEvent;
    if (flapEdge && birdY > -2 * BirdHeight) {
      velocityY = FlapVelocity;
      rotation = 45.0f;
      events |= Flap;
    } else if (velocityY < MaxFallVelocity) {
      velocityY += Gravity;
    }

    birdY += velocityY;
    if (birdY < -2 * BirdHeight) {
      birdY = float(-2 * BirdHeight);
      velocityY = 0.0f;
    }
    if (rotation > -90.0f) rotation -= 1.5f;
    if (rotation < -90.0f) rotation = -90.0f;

    for (int i = 0; i < PipeCount; ++i) pipes[i].x -= PipeSpeed;

    if (birdY + BirdHeight >= GroundY) {
      birdY = float(GroundY - BirdHeight);
      return uint8_t(events | crash(true));
    }
    for (int i = 0; i < PipeCount; ++i) {
      if (collidesWithPipe(pipes[i])) return uint8_t(events | crash(false));
    }

    for (int i = 0; i < PipeCount; ++i) {
      Pipe& pipe = pipes[i];
      if (!pipe.scored && pipe.x + PipeWidth * 0.5f <= BirdX + BirdWidth * 0.5f) {
        pipe.scored = true;
        if (score < UINT16_MAX) ++score;
        if (score > bestScore) {
          bestScore = score;
          newRecord = true;
        }
        events |= Point;
      }
    }
    for (int i = 0; i < PipeCount; ++i) {
      if (pipes[i].x < -PipeWidth) {
        float rightmost = pipes[0].x;
        for (int j = 1; j < PipeCount; ++j) {
          if (pipes[j].x > rightmost) rightmost = pipes[j].x;
        }
        pipes[i] = {rightmost + PipeSpacing, randomGap(), false};
      }
    }
    return events;
  }

  float visibleRotation() const { return rotation > 20.0f ? 20.0f : rotation; }

  bool collidesWithPipe(const Pipe& pipe) const {
    const int pipeLeft = int(pipe.x);
    if (pipeLeft >= BirdX + BirdWidth || pipeLeft + PipeWidth <= BirdX) return false;
    const int birdTop = int(birdY);
    const uint8_t frame = birdFrame < 3 ? birdFrame : 1;
    for (int y = 0; y < BirdHeight; ++y) {
      const int worldY = birdTop + y;
      int inset;
      if (worldY >= pipe.gapY - PipeHeight && worldY < pipe.gapY) {
        inset = worldY >= pipe.gapY - 24 ? 0 : 2;
      } else if (worldY >= pipe.gapY + PipeGap &&
                 worldY < pipe.gapY + PipeGap + PipeHeight) {
        inset = worldY < pipe.gapY + PipeGap + 24 ? 0 : 2;
      } else {
        continue;
      }
      int start = pipeLeft + inset - BirdX;
      int end = pipeLeft + PipeWidth - inset - BirdX;
      if (start < 0) start = 0;
      if (end > BirdWidth) end = BirdWidth;
      if (start < end) {
        const uint64_t overlap = ((uint64_t(1) << (end - start)) - 1) << start;
        const uint64_t solid = birdMasks_ ? birdMasks_[frame][y] : ((uint64_t(1) << BirdWidth) - 1);
        if (solid & overlap) return true;
      }
    }
    return false;
  }

 private:
  uint32_t randomState_ = 0x8f6e37a1U;
  uint16_t animationFrames_ = 0;
  uint8_t animationPhase_ = 0;
  bool groundCrash_ = false;
  bool deathSoundPlayed_ = false;
  const uint64_t (*birdMasks_)[BirdHeight] = nullptr;

  int16_t randomGap() {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    return int16_t(80 + randomState_ % 142);
  }

  void animateBird(uint8_t period) {
    if (++animationFrames_ >= period) {
      animationFrames_ = 0;
      animationPhase_ = uint8_t((animationPhase_ + 1) % 4);
      constexpr uint8_t frames[] = {0, 1, 2, 1};
      birdFrame = frames[animationPhase_];
    }
  }

  uint8_t crash(bool ground) {
    state = State::Dying;
    stateFrames = 0;
    groundCrash_ = ground;
    deathSoundPlayed_ = false;
    if (ground) velocityY = 0.0f;
    return Hit;
  }

  uint8_t stepDying() {
    uint8_t events = NoEvent;
    velocityY += 0.5f;
    if (velocityY > 7.5f) velocityY = 7.5f;
    birdY += velocityY;
    if (!groundCrash_) {
      rotation -= 4.0f;
      if (rotation < -90.0f) rotation = -90.0f;
      if (!deathSoundPlayed_ && (stateFrames >= 8 || birdY + BirdHeight >= GroundY)) {
        deathSoundPlayed_ = true;
        events |= Die;
      }
    }
    if (birdY + BirdHeight >= GroundY) {
      birdY = float(GroundY - BirdHeight);
      velocityY = 0.0f;
      state = State::GameOver;
      stateFrames = 0;
      events |= GameOver;
    }
    return events;
  }

};

}  // namespace flappy
