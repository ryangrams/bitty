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

struct Rules {
  uint8_t spriteScale = 1;
  int16_t groundY = GroundY;
  int16_t birdX = BirdX;
  int16_t pipeGap = PipeGap;
  int16_t pipeSpacing = PipeSpacing;
  int16_t gapMin = 80;
  int16_t gapMaxExclusive = 222;
  int16_t startDistance = 200;
  uint16_t pointsPerPipe = 1;
  float gravity = Gravity;
  float flapVelocity = FlapVelocity;
  float maxFallVelocity = MaxFallVelocity;
  float pipeSpeed = PipeSpeed;
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
  uint32_t worldFrames = 0;
  Pipe pipes[PipeCount];

  explicit Game(uint32_t seed = 0x8f6e37a1U) { reset(seed); }

  void setBestScore(uint16_t best) { bestScore = best; }
  void setHighScoreEligible(bool eligible) { highScoreEligible_ = eligible; }
  bool highScoreEligible() const { return highScoreEligible_; }
  const Rules& rules() const { return rules_; }
  void setRules(const Rules& rules) { rules_ = rules; }
  int birdWidth() const { return BirdWidth * rules_.spriteScale; }
  int birdHeight() const { return BirdHeight * rules_.spriteScale; }
  int pipeWidth() const { return PipeWidth * rules_.spriteScale; }
  int pipeHeight() const { return PipeHeight * rules_.spriteScale; }
  int birdX() const { return rules_.birdX; }
  int groundY() const { return rules_.groundY; }
  uint32_t worldTick() const { return worldFrames; }

  void enableSharedMap(uint32_t seed) {
    sharedMap_ = true;
    sharedSeed_ = seed ? seed : 1;
    for (int i = 0; i < PipeCount; ++i) sharedPipeIds_[i] = UINT32_MAX;
    syncWorldTick(worldFrames);
  }

  void disableSharedMap() { sharedMap_ = false; }

  // A shared map continues to scroll after either bird crashes.
  void syncWorldTick(uint32_t tick) {
    worldFrames = tick;
    if (!sharedMap_) return;
    const double distance = double(tick) * rules_.pipeSpeed;
    const double base = Width + rules_.startDistance;
    const double expired = distance - base - pipeWidth();
    uint32_t first = 0;
    if (expired > 0) {
      first = uint32_t(expired / rules_.pipeSpacing);
      if (double(first) * rules_.pipeSpacing < expired) ++first;
    }
    Pipe previous[PipeCount];
    uint32_t previousIds[PipeCount];
    for (int i = 0; i < PipeCount; ++i) {
      previous[i] = pipes[i];
      previousIds[i] = sharedPipeIds_[i];
    }
    for (int i = 0; i < PipeCount; ++i) {
      const uint32_t id = first + uint32_t(i);
      bool scored = false;
      for (int j = 0; j < PipeCount; ++j) if (previousIds[j] == id) scored = previous[j].scored;
      pipes[i] = {float(base + double(id) * rules_.pipeSpacing - distance), sharedGap(id), scored};
      sharedPipeIds_[i] = id;
    }
  }

  // The caller owns masks generated from its locally supplied artwork.
  void setBirdMasks(const uint64_t (*masks)[BirdHeight]) { birdMasks_ = masks; }

  // A zero seed continues the current random sequence between rounds.
  void reset(uint32_t seed = 0) {
    if (seed != 0) randomState_ = seed;
    state = State::Ready;
    birdY = readyY();
    velocityY = 0.0f;
    rotation = 0.0f;
    birdFrame = 0;
    groundOffset = 0;
    score = 0;
    newRecord = false;
    stateFrames = 0;
    worldFrames = 0;
    animationFrames_ = 0;
    animationPhase_ = 0;
    groundCrash_ = false;
    deathSoundPlayed_ = false;
    for (int i = 0; i < PipeCount; ++i) sharedPipeIds_[i] = UINT32_MAX;
    for (int i = 0; i < PipeCount; ++i) {
      pipes[i] = {float(Width + rules_.startDistance + i * rules_.pipeSpacing), randomGap(), false};
    }
    if (sharedMap_) syncWorldTick(0);
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
        birdY = readyY() + float(triangle) * 0.5f;
        return NoEvent;
      }
      state = State::Playing;
      stateFrames = 0;
    }
    ++worldFrames;

    uint8_t events = NoEvent;
    if (flapEdge && birdY > -2 * birdHeight()) {
      velocityY = rules_.flapVelocity;
      rotation = 45.0f;
      events |= Flap;
    } else if (velocityY < rules_.maxFallVelocity) {
      velocityY += rules_.gravity;
    }

    birdY += velocityY;
    if (birdY < -2 * birdHeight()) {
      birdY = float(-2 * birdHeight());
      velocityY = 0.0f;
    }
    if (rotation > -90.0f) rotation -= 1.5f;
    if (rotation < -90.0f) rotation = -90.0f;

    if (sharedMap_) syncWorldTick(worldFrames);
    else for (int i = 0; i < PipeCount; ++i) pipes[i].x -= rules_.pipeSpeed;

    if (birdY + birdHeight() >= rules_.groundY) {
      birdY = float(rules_.groundY - birdHeight());
      return uint8_t(events | crash(true));
    }
    for (int i = 0; i < PipeCount; ++i) {
      if (collidesWithPipe(pipes[i])) return uint8_t(events | crash(false));
    }

    for (int i = 0; i < PipeCount; ++i) {
      Pipe& pipe = pipes[i];
      if (!pipe.scored && pipe.x + pipeWidth() * 0.5f <= rules_.birdX + birdWidth() * 0.5f) {
        pipe.scored = true;
        score = uint16_t(uint32_t(score) + rules_.pointsPerPipe > UINT16_MAX ?
                         UINT16_MAX : score + rules_.pointsPerPipe);
        if (highScoreEligible_ && score > bestScore) {
          bestScore = score;
          newRecord = true;
        }
        events |= Point;
      }
    }
    for (int i = 0; i < PipeCount; ++i) {
      if (!sharedMap_ && pipes[i].x < -pipeWidth()) {
        float rightmost = pipes[0].x;
        for (int j = 1; j < PipeCount; ++j) {
          if (pipes[j].x > rightmost) rightmost = pipes[j].x;
        }
        pipes[i] = {rightmost + rules_.pipeSpacing, randomGap(), false};
      }
    }
    return events;
  }

  float visibleRotation() const { return rotation > 20.0f ? 20.0f : rotation; }

  bool collidesWithPipe(const Pipe& pipe) const {
    const int pipeLeft = int(pipe.x);
    if (pipeLeft >= rules_.birdX + birdWidth() || pipeLeft + pipeWidth() <= rules_.birdX) return false;
    const int birdTop = int(birdY);
    const uint8_t frame = birdFrame < 3 ? birdFrame : 1;
    const int scale = rules_.spriteScale;
    for (int y = 0; y < birdHeight(); ++y) {
      const int worldY = birdTop + y;
      int inset;
      if (worldY >= pipe.gapY - pipeHeight() && worldY < pipe.gapY) {
        inset = worldY >= pipe.gapY - 24 * scale ? 0 : 2 * scale;
      } else if (worldY >= pipe.gapY + rules_.pipeGap &&
                 worldY < pipe.gapY + rules_.pipeGap + pipeHeight()) {
        inset = worldY < pipe.gapY + rules_.pipeGap + 24 * scale ? 0 : 2 * scale;
      } else {
        continue;
      }
      int start = pipeLeft + inset - rules_.birdX;
      int end = pipeLeft + pipeWidth() - inset - rules_.birdX;
      if (start < 0) start = 0;
      if (end > birdWidth()) end = birdWidth();
      if (start < end) {
        const int firstPixel = start / scale;
        const int lastPixel = (end - 1) / scale + 1;
        const uint64_t overlap = ((uint64_t(1) << (lastPixel - firstPixel)) - 1) << firstPixel;
        const uint64_t solid = birdMasks_ ? birdMasks_[frame][y / scale] : ((uint64_t(1) << BirdWidth) - 1);
        if (solid & overlap) return true;
      }
    }
    return false;
  }

 private:
  Rules rules_;
  bool highScoreEligible_ = true;
  bool sharedMap_ = false;
  uint32_t sharedSeed_ = 1;
  uint32_t sharedPipeIds_[PipeCount] = {};
  uint32_t randomState_ = 0x8f6e37a1U;
  uint16_t animationFrames_ = 0;
  uint8_t animationPhase_ = 0;
  bool groundCrash_ = false;
  bool deathSoundPlayed_ = false;
  const uint64_t (*birdMasks_)[BirdHeight] = nullptr;

  float readyY() const { return rules_.spriteScale == 1 ? 244.0f : 232.0f; }

  int16_t sharedGap(uint32_t index) const {
    uint32_t value = sharedSeed_ ^ (index + 1U) * 0x9e3779b9U;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    const int range = rules_.gapMaxExclusive - rules_.gapMin;
    return int16_t(rules_.gapMin + (range > 0 ? value % unsigned(range) : 0));
  }

  int16_t randomGap() {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    const int range = rules_.gapMaxExclusive - rules_.gapMin;
    return int16_t(rules_.gapMin + (range > 0 ? randomState_ % unsigned(range) : 0));
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
      if (!deathSoundPlayed_ && (stateFrames >= 8 || birdY + birdHeight() >= rules_.groundY)) {
        deathSoundPlayed_ = true;
        events |= Die;
      }
    }
    if (birdY + birdHeight() >= rules_.groundY) {
      birdY = float(rules_.groundY - birdHeight());
      velocityY = 0.0f;
      state = State::GameOver;
      stateFrames = 0;
      events |= GameOver;
    }
    return events;
  }
};

}  // namespace flappy
