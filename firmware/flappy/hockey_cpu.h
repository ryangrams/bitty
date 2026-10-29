#pragma once
#include "hockey_game.h"

namespace flappy {
class HockeyCpu {
 public:
  void reset(uint32_t seed) { rng_ = seed ? seed : 1; wait_ = 0; target_ = 67; }
  void setDifficulty(uint8_t value) { difficulty_ = value < 4 ? value : 1; }
  int16_t input(const HockeyGame& game,unsigned who) {
    who &= 1;
    static const uint8_t reaction[] = {24,14,8,3};
    static const uint8_t error[] = {19,11,5,2};
    static const int16_t speed[] = {580,770,930,1024};
    if (!wait_) {
      wait_ = reaction[difficulty_];
      float soonest = 10, target = 67;
      for (unsigned i = 0; i < 3; ++i) {
        const auto& puck = game.puck(i);
        if (!puck.active || (who ? puck.vy <= 0 : puck.vy >= 0)) continue;
        const float seconds = (HockeyGame::paddleY(who) - puck.y) / puck.vy;
        if (seconds < 0 || seconds >= soonest) continue;
        soonest = seconds;
        // Predict side-wall rebounds. Map bumpers and later hits can defeat it.
        target = puck.x + puck.vx * seconds;
        target = fmodf(target - 9,232.f);
        if (target < 0) target += 232.f;
        target = 9 + (target > 116 ? 232 - target : target);
      }
      const auto frame = game.frame();
      if (soonest == 10 && frame.pickupOwner == who && frame.pickup != hockey::Power::None) target = frame.pickupX;
      const int scatter = int(random() % (error[difficulty_] * 2u + 1u)) - error[difficulty_];
      target_ = fmaxf(16,fminf(118,target + float(scatter)));
    }
    --wait_;
    const float delta = target_ - game.player(who).x;
    int amount = int(delta * 82.f);
    const int maximum = speed[difficulty_];
    return int16_t(amount < -maximum ? -maximum : amount > maximum ? maximum : amount);
  }
 private:
  uint32_t rng_ = 1;
  float target_ = 67;
  uint8_t wait_ = 0, difficulty_ = 1;
  uint32_t random() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
};
}
