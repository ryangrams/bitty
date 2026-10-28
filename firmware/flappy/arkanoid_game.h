#pragma once

#include <stdint.h>
#include "arcade_support.h"

namespace flappy {

// The field and artwork are original and use the panel's native pixel grid.
class ArkanoidGame {
 public:
  enum { Columns = 9, Rows = 8, BrickCount = Columns * Rows, MaxBalls = 3,
         MaxCapsules = 6, MaxLasers = 8, Unit = 256, Left = 5, Right = 129,
         Ceiling = 28, PaddleY = 216, BrickLeft = 9, BrickTop = 45,
         BrickPitchX = 13, BrickPitchY = 9, BrickWidth = 12, BrickHeight = 7 };
  enum class Power : uint8_t { Expand, Laser, Catch, Slow, Multi, Life, Count };
  struct Ball {
    int32_t x = 0, y = 0, vx = 0, vy = 0;
    int16_t offset = 0;
    uint16_t held = 0;
    bool active = false, attached = false;
  };
  struct Brick { uint8_t hits = 0, color = 0; };
  struct Capsule {
    int32_t x = 0, y = 0;
    Power power = Power::Expand;
    bool active = false;
  };
  struct Laser { int32_t y = 0; int16_t x = 0; bool active = false; };

  ArkanoidGame() { reset(); }

  void reset(uint32_t seed = 1, uint8_t difficulty = 1) {
    seed_ = seed ? seed : 1;
    random_ = seed_;
    difficulty_ = difficulty > 2 ? 2 : difficulty;
    score_ = 0;
    stage_ = 1;
    lives_ = difficulty_ == 0 ? 4 : 3;
    nextLife_ = 15000;
    frame_ = 0;
    over_ = false;
    cue_ = ArcadeCue::None;
    paddleX_ = 67 * Unit;
    paddleVelocity_ = 0;
    clearPowers();
    buildStage();
    serve();
  }

  void tick(const ArcadeInput& input) {
    ++frame_;
    if (over_) {
      if (input.pressed) reset(seed_, difficulty_);
      return;
    }
    movePaddle(input.move);
    if (stageDelay_) {
      if (--stageDelay_ == 0) {
        if (stage_ < UINT16_MAX) ++stage_;
        clearPowers();
        buildStage();
        serve();
      }
      return;
    }
    updatePowers();
    if (deathDelay_) { --deathDelay_; positionAttached(); return; }
    if (input.pressed) launchAttached();
    if (input.action && powerTicks_[unsigned(Power::Laser)] && !laserCooldown_)
      fireLasers();
    if (laserCooldown_) --laserCooldown_;
    updateLasers();
    for (unsigned i = 0; i < MaxBalls; ++i) {
      Ball& ball = balls_[i];
      if (!ball.active) continue;
      if (ball.attached) {
        positionBall(ball);
        if (ball.held && --ball.held == 0) launchBall(ball);
      } else {
        moveBall(ball);
      }
    }
    updateCapsules();
    if (!remaining_) {
      stageDelay_ = 90;
      addScore(uint32_t(stage_) * 500);
      clearObjects();
      setCue(ArcadeCue::Win);
    } else if (!activeBalls()) {
      if (lives_) --lives_;
      clearPowers();
      clearObjects();
      setCue(ArcadeCue::Death);
      if (!lives_) over_ = true;
      else { serve(); deathDelay_ = 45; }
    }
  }

  bool gameOver() const { return over_; }
  uint32_t score() const { return score_; }
  uint16_t stage() const { return stage_; }
  uint8_t lives() const { return lives_; }
  uint16_t remainingBricks() const { return remaining_; }
  uint16_t powerTicks(Power power) const {
    return unsigned(power) < unsigned(Power::Count) ? powerTicks_[unsigned(power)] : 0;
  }
  int paddleX() const { return int(paddleX_ / Unit); }
  int paddleWidth() const { return powerTicks_[unsigned(Power::Expand)] ? 38 : 24; }
  const Ball& ball(unsigned index) const { return balls_[index < MaxBalls ? index : 0]; }
  const Brick& brick(unsigned index) const { return bricks_[index < BrickCount ? index : 0]; }
  const Capsule& capsule(unsigned index) const { return capsules_[index < MaxCapsules ? index : 0]; }
  uint8_t activeBalls() const {
    uint8_t count = 0;
    for (const Ball& ball : balls_) if (ball.active) ++count;
    return count;
  }
  ArcadeCue takeCue() { const ArcadeCue cue = cue_; cue_ = ArcadeCue::None; return cue; }

  void draw(uint16_t* pixels, uint32_t best = 0) const {
    if (!pixels) return;
    ArcadeCanvas c(pixels);
    const uint16_t ink = c.rgb(7, 12, 28), dim = c.rgb(23, 39, 61);
    const uint16_t white = c.rgb(236, 248, 255), cyan = c.rgb(91, 229, 244);
    c.clear(ink);
    for (int y = 35; y < 228; y += 16)
      for (int x = 11 + ((y / 16) & 1) * 8; x < 130; x += 16) c.pixel(x, y, dim);
    c.text("S", 4, 0, cyan);
    c.number(score_, 16, 0, white);
    c.text("HI", 4, 12, c.rgb(145, 174, 195));
    c.number(best > score_ ? best : score_, 22, 12, cyan);
    c.text("ST", 87, 12, c.rgb(145, 174, 195));
    c.number(stage_, 105, 12, white);
    c.rect(2, Ceiling - 3, 131, 2, c.rgb(88, 125, 150));
    c.rect(2, Ceiling - 1, 3, 202, c.rgb(38, 65, 91));
    c.rect(130, Ceiling - 1, 3, 202, c.rgb(38, 65, 91));
    for (int y = 36; y < 225; y += 18) {
      c.rect(2, y, 3, 4, c.rgb(111, 153, 176));
      c.rect(130, y, 3, 4, c.rgb(111, 153, 176));
    }
    for (unsigned i = 0; i < BrickCount; ++i) if (bricks_[i].hits) drawBrick(c, i);
    for (const Capsule& capsule : capsules_) if (capsule.active) drawCapsule(c, capsule);
    const int px = paddleX(), width = paddleWidth(), left = px - width / 2;
    if (!(deathDelay_ && (frame_ & 4))) {
      c.rect(left, PaddleY, width, 5, c.rgb(49, 143, 180));
      c.rect(left + 3, PaddleY, width - 6, 2, white);
      c.rect(left + 3, PaddleY + 2, width - 6, 2, cyan);
      c.rect(left, PaddleY, 3, 4, c.rgb(244, 87, 119));
      c.rect(left + width - 3, PaddleY, 3, 4, c.rgb(244, 87, 119));
      if (powerTicks_[unsigned(Power::Laser)]) {
        c.rect(left + 1, PaddleY - 3, 2, 3, white);
        c.rect(left + width - 3, PaddleY - 3, 2, 3, white);
      }
      if (powerTicks_[unsigned(Power::Catch)]) c.rect(px - 3, PaddleY - 1, 6, 1, c.rgb(117, 245, 136));
    }
    for (const Laser& laser : lasers_) if (laser.active) {
      c.rect(laser.x, int(laser.y / Unit), 1, 6, c.rgb(255, 108, 128));
      c.pixel(laser.x, int(laser.y / Unit), white);
    }
    for (const Ball& ball : balls_) if (ball.active) {
      const int x = int(ball.x / Unit), y = int(ball.y / Unit);
      c.rect(x - 1, y - 1, 3, 3, white);
      c.pixel(x - 2, y, cyan); c.pixel(x + 2, y, cyan);
      c.pixel(x, y - 2, cyan); c.pixel(x, y + 2, cyan);
    }
    c.line(6, 230, 128, 230, dim);
    for (unsigned i = 0; i < lives_; ++i) {
      const int x = 7 + int(i) * 10;
      c.rect(x, 234, 7, 2, cyan);
      c.pixel(x, 233, white); c.pixel(x + 6, 233, white);
    }
    if (stageDelay_) {
      panel(c, "FIELD CLEAR", "NEXT STAGE", 138);
    } else if (over_) {
      panel(c, "GAME OVER", "TAP TO RETRY", 140);
    } else {
      bool held = false;
      for (const Ball& ball : balls_) held |= ball.active && ball.attached;
      if (held && !deathDelay_) c.text("TAP TO LAUNCH", 28, 187, cyan);
    }
    unsigned iconX = 128;
    for (unsigned i = 0; i < 4; ++i) if (powerTicks_[i]) {
      const int length = int((powerTicks_[i] * 22u + 1199u) / 1200u);
      c.rect(int(iconX) - length, 233 + int(i), length, 1, powerColor(c, Power(i)));
    }
  }

 private:
  friend struct ArkanoidTestAccess;
  Brick bricks_[BrickCount];
  Ball balls_[MaxBalls];
  Capsule capsules_[MaxCapsules];
  Laser lasers_[MaxLasers];
  uint16_t powerTicks_[unsigned(Power::Count)] = {};
  uint32_t random_ = 1, seed_ = 1, score_ = 0, frame_ = 0, nextLife_ = 15000;
  int32_t paddleX_ = 67 * Unit, paddleVelocity_ = 0;
  int16_t nominalSpeed_ = 640;
  uint16_t stage_ = 1, remaining_ = 0, stageDelay_ = 0;
  uint8_t lives_ = 3, difficulty_ = 1, laserCooldown_ = 0, deathDelay_ = 0, hitCount_ = 0;
  bool over_ = false;
  ArcadeCue cue_ = ArcadeCue::None;

  static int32_t absolute(int32_t n) { return n < 0 ? -n : n; }
  static int32_t clamp(int32_t n, int32_t lo, int32_t hi) { return n < lo ? lo : n > hi ? hi : n; }
  static uint32_t root(uint32_t n) {
    uint32_t value = 0, bit = uint32_t(1) << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
      if (n >= value + bit) { n -= value + bit; value = (value >> 1) + bit; }
      else value >>= 1;
      bit >>= 2;
    }
    return value;
  }
  uint32_t random() {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return random_;
  }
  void setCue(ArcadeCue cue) {
    if (cue == ArcadeCue::Death || cue_ == ArcadeCue::None ||
        (cue_ != ArcadeCue::Death && unsigned(cue) > unsigned(cue_))) cue_ = cue;
  }
  void addScore(uint32_t points) {
    score_ = UINT32_MAX - score_ < points ? UINT32_MAX : score_ + points;
    if (score_ >= nextLife_ && nextLife_ < UINT32_MAX) {
      if (lives_ < 9) ++lives_;
      nextLife_ = UINT32_MAX - nextLife_ < 30000 ? UINT32_MAX : nextLife_ + 30000;
    }
  }
  void clearObjects() {
    for (Ball& ball : balls_) ball = Ball{};
    for (Capsule& capsule : capsules_) capsule = Capsule{};
    for (Laser& laser : lasers_) laser = Laser{};
  }
  void clearPowers() {
    for (uint16_t& ticks : powerTicks_) ticks = 0;
    laserCooldown_ = 0;
  }
  void buildStage() {
    remaining_ = 0;
    stageDelay_ = 0;
    deathDelay_ = 0;
    hitCount_ = 0;
    nominalSpeed_ = int16_t(530 + difficulty_ * 70 + (stage_ < 21 ? stage_ : 21) * 18);
    const unsigned pattern = (stage_ - 1) % 6;
    bool gold = false;
    for (unsigned row = 0; row < Rows; ++row) for (unsigned col = 0; col < Columns; ++col) {
      const unsigned center = col > 4 ? col - 4 : 4 - col;
      bool occupied = true;
      switch (pattern) {
        case 0: occupied = row < 6 && (row != 2 || col % 3 != 1); break;
        case 1: occupied = row >= center && row < center + 4; break;
        case 2: occupied = col % 3 != 1 || row % 3 == 1; break;
        case 3: occupied = row == 0 || row == 7 || col == 0 || col == 8 || (row + col) % 3 == 0; break;
        case 4: occupied = ((row * 2 + col + stage_ / 6) % 5) < 3; break;
        default: occupied = row > center / 2 && (row + center) % 4 != 0; break;
      }
      Brick& brick = bricks_[row * Columns + col];
      brick.color = uint8_t((row + stage_ + col / 3) % 6);
      brick.hits = occupied ? 1 : 0;
      if (occupied && row > 0 && ((col * 3 + row + stage_) % 13 == 0))
        brick.hits = uint8_t(2 + (stage_ > 6 ? 1 : 0));
      if (occupied && stage_ > 1 && row > 1 && row < 6 &&
          ((col + row * 5 + stage_) % 23 == 0)) brick.hits = 255;
      if (brick.hits && brick.hits != 255) ++remaining_;
      gold |= brick.hits == 255;
    }
    if (stage_ > 1 && !gold) for (unsigned offset = 0; offset < 4 * Columns; ++offset) {
      const unsigned index = 2 * Columns + (offset + stage_) % (4 * Columns);
      if (!bricks_[index].hits) continue;
      bricks_[index].hits = 255;
      --remaining_;
      break;
    }
  }
  void serve() {
    clearObjects();
    Ball& ball = balls_[0];
    ball.active = ball.attached = true;
    ball.offset = 0;
    positionBall(ball);
  }
  void movePaddle(int16_t movement) {
    const int32_t target = clamp(movement, -1024, 1024) * 960 / 1024;
    paddleVelocity_ = (paddleVelocity_ + target * 3) / 4;
    const int half = paddleWidth() / 2;
    const int32_t next = paddleX_ + paddleVelocity_;
    paddleX_ = clamp(next, (Left + half) * Unit, (Right - half) * Unit);
    if (paddleX_ != next) paddleVelocity_ = 0;
  }
  void positionBall(Ball& ball) {
    const int half = paddleWidth() / 2 - 3;
    ball.offset = int16_t(clamp(ball.offset, -half * Unit, half * Unit));
    ball.x = paddleX_ + ball.offset;
    ball.y = (PaddleY - 3) * Unit;
  }
  void positionAttached() { for (Ball& ball : balls_) if (ball.active && ball.attached) positionBall(ball); }
  int32_t speed() const {
    return powerTicks_[unsigned(Power::Slow)] ? nominalSpeed_ * 3 / 4 : nominalSpeed_;
  }
  void setSpeed(Ball& ball) {
    const uint32_t magnitude = root(uint32_t(ball.vx * ball.vx + ball.vy * ball.vy));
    if (!magnitude) { ball.vx = speed() / 3; ball.vy = -speed() * 15 / 16; return; }
    ball.vx = ball.vx * speed() / int32_t(magnitude);
    ball.vy = ball.vy * speed() / int32_t(magnitude);
  }
  void launchBall(Ball& ball) {
    ball.attached = false;
    ball.held = 0;
    ball.vx = speed() * (paddleVelocity_ < 0 ? -80 : 80) / Unit;
    ball.vy = -int32_t(root(uint32_t(speed() * speed() - ball.vx * ball.vx)));
    setCue(ArcadeCue::Fire);
  }
  void launchAttached() {
    for (Ball& ball : balls_) if (ball.active && ball.attached) launchBall(ball);
  }
  void paddleBounce(Ball& ball) {
    static const int16_t sines[9] = {0, 36, 71, 104, 135, 164, 189, 212, 232};
    const int32_t relative = clamp((ball.x - paddleX_) * 1024 / (paddleWidth() * Unit / 2), -1024, 1024);
    const int32_t angle = absolute(relative) * 8;
    const unsigned index = unsigned(angle / 1024);
    int32_t sine = sines[index];
    if (index < 8) sine += (sines[index + 1] - sine) * (angle % 1024) / 1024;
    ball.vx = speed() * sine / Unit * (relative < 0 ? -1 : 1);
    ball.vx += paddleVelocity_ / 12;
    ball.vx = clamp(ball.vx, -speed() * 232 / Unit, speed() * 232 / Unit);
    if (absolute(ball.vx) < speed() / 10) ball.vx = speed() / 10 * (ball.vx < 0 ? -1 : 1);
    ball.vy = -int32_t(root(uint32_t(speed() * speed() - ball.vx * ball.vx)));
    ball.y = (PaddleY - 2) * Unit - 1;
    if (powerTicks_[unsigned(Power::Catch)]) {
      ball.attached = true;
      ball.offset = int16_t(ball.x - paddleX_);
      ball.held = 240;
      positionBall(ball);
    }
    setCue(ArcadeCue::Hit);
  }
  void moveBall(Ball& ball) {
    const int32_t fastest = absolute(ball.vx) > absolute(ball.vy) ? absolute(ball.vx) : absolute(ball.vy);
    const int steps = int((fastest + 127) / 128) + 1;
    for (int step = 0; step < steps && ball.active && !ball.attached; ++step) {
      const int32_t oldX = ball.x, oldY = ball.y;
      ball.x += ball.vx / steps;
      ball.y += ball.vy / steps;
      if (ball.x < (Left + 2) * Unit) {
        ball.x = (Left + 2) * Unit; ball.vx = absolute(ball.vx); setCue(ArcadeCue::Hit);
      } else if (ball.x > (Right - 2) * Unit) {
        ball.x = (Right - 2) * Unit; ball.vx = -absolute(ball.vx); setCue(ArcadeCue::Hit);
      }
      if (ball.y < (Ceiling + 2) * Unit) {
        ball.y = (Ceiling + 2) * Unit; ball.vy = absolute(ball.vy); setCue(ArcadeCue::Hit);
      }
      if (ball.vy > 0 && oldY <= (PaddleY - 2) * Unit && ball.y >= (PaddleY - 2) * Unit &&
          ball.x + 2 * Unit >= paddleX_ - paddleWidth() * Unit / 2 &&
          ball.x - 2 * Unit <= paddleX_ + paddleWidth() * Unit / 2) paddleBounce(ball);
      hitBrick(ball, oldX, oldY);
      if (ball.y > 242 * Unit) ball.active = false;
    }
  }
  void hitBrick(Ball& ball, int32_t oldX, int32_t oldY) {
    if (ball.y < (BrickTop - 2) * Unit || ball.y > (BrickTop + Rows * BrickPitchY) * Unit) return;
    for (unsigned i = 0; i < BrickCount; ++i) {
      if (!bricks_[i].hits) continue;
      const int32_t left = (BrickLeft + int(i % Columns) * BrickPitchX - 2) * Unit;
      const int32_t right = left + (BrickWidth + 4) * Unit;
      const int32_t top = (BrickTop + int(i / Columns) * BrickPitchY - 2) * Unit;
      const int32_t bottom = top + (BrickHeight + 4) * Unit;
      if (ball.x <= left || ball.x >= right || ball.y <= top || ball.y >= bottom) continue;
      if (oldY <= top) { ball.y = top; ball.vy = -absolute(ball.vy); }
      else if (oldY >= bottom) { ball.y = bottom; ball.vy = absolute(ball.vy); }
      else if (oldX <= left) { ball.x = left; ball.vx = -absolute(ball.vx); }
      else if (oldX >= right) { ball.x = right; ball.vx = absolute(ball.vx); }
      else {
        const int32_t xDepth = ball.vx > 0 ? ball.x - left : right - ball.x;
        const int32_t yDepth = ball.vy > 0 ? ball.y - top : bottom - ball.y;
        if (xDepth < yDepth) { ball.x = ball.vx > 0 ? left : right; ball.vx = -ball.vx; }
        else { ball.y = ball.vy > 0 ? top : bottom; ball.vy = -ball.vy; }
      }
      damageBrick(i);
      return;
    }
  }
  void damageBrick(unsigned i) {
    Brick& brick = bricks_[i];
    if (!brick.hits) return;
    setCue(ArcadeCue::Hit);
    if (brick.hits == 255) return;
    --brick.hits;
    if (brick.hits) { addScore(10); return; }
    --remaining_;
    addScore(40 + (Rows - i / Columns) * 10);
    if ((++hitCount_ & 7) == 0 && nominalSpeed_ < 1350) {
      nominalSpeed_ = int16_t(nominalSpeed_ + 24);
      for (Ball& ball : balls_) if (ball.active && !ball.attached) setSpeed(ball);
    }
    if (random() % 7 == 0) {
      for (Capsule& capsule : capsules_) if (!capsule.active) {
        capsule.active = true;
        capsule.x = (BrickLeft + int(i % Columns) * BrickPitchX + BrickWidth / 2) * Unit;
        capsule.y = (BrickTop + int(i / Columns) * BrickPitchY + BrickHeight / 2) * Unit;
        capsule.power = Power(random() % unsigned(Power::Count));
        break;
      }
    }
  }
  void updatePowers() {
    for (unsigned i = 0; i < unsigned(Power::Count); ++i) if (powerTicks_[i]) {
      --powerTicks_[i];
      if (!powerTicks_[i] && i == unsigned(Power::Slow))
        for (Ball& ball : balls_) if (ball.active && !ball.attached) setSpeed(ball);
    }
    const int half = paddleWidth() / 2;
    paddleX_ = clamp(paddleX_, (Left + half) * Unit, (Right - half) * Unit);
  }
  void collect(Power power) {
    addScore(250);
    setCue(ArcadeCue::Power);
    switch (power) {
      case Power::Expand: powerTicks_[unsigned(power)] = 1200; break;
      case Power::Laser: powerTicks_[unsigned(power)] = 1200; break;
      case Power::Catch: powerTicks_[unsigned(power)] = 1200; break;
      case Power::Slow:
        powerTicks_[unsigned(power)] = 1200;
        for (Ball& ball : balls_) if (ball.active && !ball.attached) setSpeed(ball);
        break;
      case Power::Life: if (lives_ < 9) ++lives_; break;
      case Power::Multi: {
        unsigned source = 0;
        while (source < MaxBalls && !balls_[source].active) ++source;
        if (source == MaxBalls) break;
        if (balls_[source].attached || (!balls_[source].vx && !balls_[source].vy))
          launchBall(balls_[source]);
        const Ball original = balls_[source];
        unsigned added = 0;
        for (Ball& ball : balls_) if (!ball.active) {
          ball = original;
          const int sign = (++added & 1) ? 1 : -1;
          ball.vx = (original.vx * 222 - original.vy * 128 * sign) / Unit;
          ball.vy = (original.vy * 222 + original.vx * 128 * sign) / Unit;
          if (absolute(ball.vy) < speed() / 3) ball.vy = (ball.vy > 0 ? 1 : -1) * speed() / 3;
          setSpeed(ball);
        }
        break;
      }
      default: break;
    }
    const int half = paddleWidth() / 2;
    paddleX_ = clamp(paddleX_, (Left + half) * Unit, (Right - half) * Unit);
    positionAttached();
  }
  void updateCapsules() {
    for (Capsule& capsule : capsules_) if (capsule.active) {
      const int32_t oldY = capsule.y;
      capsule.y += 176;
      if (oldY - 3 * Unit <= (PaddleY + 5) * Unit && capsule.y + 3 * Unit >= PaddleY * Unit &&
          absolute(capsule.x - paddleX_) < (paddleWidth() / 2 + 5) * Unit) {
        capsule.active = false;
        collect(capsule.power);
      }
      if (capsule.y > 244 * Unit) capsule.active = false;
    }
  }
  void fireLasers() {
    unsigned free = 0;
    for (const Laser& laser : lasers_) if (!laser.active) ++free;
    if (free < 2) return;
    unsigned side = 0;
    for (Laser& laser : lasers_) if (!laser.active && side < 2) {
      laser.active = true;
      laser.x = int16_t(paddleX() + (side++ ? 1 : -1) * (paddleWidth() / 2 - 2));
      laser.y = (PaddleY - 8) * Unit;
    }
    laserCooldown_ = 12;
    setCue(ArcadeCue::Fire);
  }
  void updateLasers() {
    for (Laser& laser : lasers_) if (laser.active) {
      const int32_t oldY = laser.y;
      laser.y -= 8 * Unit;
      for (int row = Rows - 1; row >= 0 && laser.active; --row)
        for (unsigned col = 0; col < Columns && laser.active; ++col) {
          const unsigned index = unsigned(row) * Columns + col;
          const int x = BrickLeft + int(col) * BrickPitchX;
          const int y = BrickTop + row * BrickPitchY;
          if (bricks_[index].hits && laser.x >= x && laser.x < x + BrickWidth &&
              oldY >= y * Unit && laser.y <= (y + BrickHeight) * Unit) {
            damageBrick(index);
            laser.active = false;
          }
        }
      if (laser.y < Ceiling * Unit) laser.active = false;
    }
  }
  static uint16_t powerColor(ArcadeCanvas& c, Power power) {
    switch (power) {
      case Power::Expand: return c.rgb(77, 190, 255);
      case Power::Laser: return c.rgb(255, 93, 123);
      case Power::Catch: return c.rgb(98, 239, 136);
      case Power::Slow: return c.rgb(247, 181, 77);
      case Power::Multi: return c.rgb(181, 132, 255);
      default: return c.rgb(247, 235, 151);
    }
  }
  void drawBrick(ArcadeCanvas& c, unsigned index) const {
    static const uint8_t palette[6][3] = {{245, 102, 128}, {252, 173, 85}, {238, 219, 116},
        {111, 216, 154}, {90, 180, 240}, {177, 137, 232}};
    const Brick& brick = bricks_[index];
    const int x = BrickLeft + int(index % Columns) * BrickPitchX;
    const int y = BrickTop + int(index / Columns) * BrickPitchY;
    const uint8_t* color = palette[brick.color % 6];
    uint16_t face = c.rgb(color[0], color[1], color[2]);
    if (brick.hits == 255) face = c.rgb(206, 162, 65);
    else if (brick.hits > 1) face = c.rgb(155, 182, 200);
    c.rect(x, y, BrickWidth, BrickHeight, c.rgb(21, 32, 47));
    c.rect(x, y, BrickWidth - 1, BrickHeight - 1, face);
    c.line(x, y, x + BrickWidth - 2, y, c.rgb(248, 248, 227));
    c.line(x, y, x, y + BrickHeight - 2, c.rgb(231, 239, 231));
    if (brick.hits == 255) {
      c.rect(x + 4, y + 2, 3, 3, c.rgb(255, 222, 124));
      c.pixel(x + 5, y + 3, c.rgb(136, 94, 31));
    } else if (brick.hits > 1) {
      for (unsigned i = 0; i < brick.hits; ++i) c.pixel(x + 3 + int(i) * 2, y + 3, c.rgb(53, 80, 105));
    }
  }
  static void drawCapsule(ArcadeCanvas& c, const Capsule& capsule) {
    const int x = int(capsule.x / Unit) - 5, y = int(capsule.y / Unit) - 3;
    const uint16_t color = powerColor(c, capsule.power), white = c.rgb(239, 249, 250);
    c.rect(x + 1, y, 8, 7, color);
    c.rect(x, y + 1, 10, 5, color);
    c.line(x + 2, y, x + 7, y, white);
    const uint16_t dark = c.rgb(14, 30, 51);
    switch (capsule.power) {
      case Power::Expand:
        c.line(x + 2, y + 3, x + 7, y + 3, dark);
        c.line(x + 2, y + 2, x + 2, y + 4, dark); c.line(x + 7, y + 2, x + 7, y + 4, dark); break;
      case Power::Laser:
        c.line(x + 4, y + 2, x + 4, y + 4, dark); c.line(x + 4, y + 4, x + 6, y + 4, dark); break;
      case Power::Catch:
        c.line(x + 3, y + 2, x + 3, y + 4, dark); c.line(x + 3, y + 4, x + 6, y + 4, dark); break;
      case Power::Slow:
        c.line(x + 3, y + 2, x + 6, y + 2, dark); c.pixel(x + 4, y + 3, dark);
        c.line(x + 3, y + 4, x + 6, y + 4, dark); break;
      case Power::Multi:
        c.pixel(x + 3, y + 4, dark); c.pixel(x + 5, y + 2, dark); c.pixel(x + 7, y + 4, dark); break;
      default:
        c.line(x + 3, y + 3, x + 6, y + 3, dark); c.line(x + 5, y + 2, x + 5, y + 4, dark); break;
    }
  }
  static void panel(ArcadeCanvas& c, const char* title, const char* subtitle, int y) {
    c.rect(16, y, 103, 38, c.rgb(12, 26, 46));
    c.line(16, y, 118, y, c.rgb(98, 190, 213));
    unsigned first = 0, second = 0;
    while (title[first]) ++first;
    while (subtitle[second]) ++second;
    c.text(title, (135 - int(first) * 6) / 2, y + 4, c.rgb(239, 248, 255));
    c.text(subtitle, (135 - int(second) * 6) / 2, y + 21, c.rgb(106, 212, 229));
  }
};

}  // namespace flappy
