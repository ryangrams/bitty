#include "../firmware/flappy/buddy_renderer.h"
#include "../firmware/flappy/buddy_clock.h"
#include <array>
#include <cstdio>
#include <iostream>

int main() {
  flappy::BuddyGame game;
  flappy::BuddyClock clock;
  clock.reset(47);
  std::array<uint16_t, 135 * 240> pixels;
  int steps, lane, step, jump, roll, pressed, restart, level, shirt, mask, home;
  uint32_t seed = 47, best = 0, frames = 0;
  int appliedShirt = -1, appliedMask = -1;
  while (std::cin >> steps >> lane >> step >> jump >> roll >> pressed >> restart >> level >> shirt >> mask >> home) {
    if (restart) {
      game.setStartingLevel(uint8_t(level));
      game.setLook(uint8_t(shirt), uint8_t(mask));
      game.reset(++seed, 1);
      appliedShirt = shirt; appliedMask = mask;
    }
    if (appliedShirt != shirt || appliedMask != mask) {
      game.setLook(uint8_t(shirt), uint8_t(mask));
      appliedShirt = shirt; appliedMask = mask;
    }
    flappy::BuddyInput input;
    input.lane = int8_t(lane);
    input.step = int8_t(step);
    input.jump = jump != 0;
    input.roll = roll != 0;
    input.pressed = pressed != 0;
    flappy::ClockTime time;
    for (int i = 0; i < steps; ++i) {
      if (home) {
        time.hour = 12;
        time.minute = uint8_t((34 + frames / 3600) % 60);
        time.second = uint8_t(frames / 60 % 60);
        time.ms = frames * 1000 / 60;
        clock.step(time);
      } else {
        game.tick(input);
        if (game.score() > best) best = game.score();
      }
      input.step = 0;
      input.pressed = false;
      ++frames;
    }
    if (home) clock.draw(pixels.data(), time);
    else flappy::BuddyRenderer::draw(pixels.data(), game, best);
    if (std::fwrite(pixels.data(), sizeof(uint16_t), pixels.size(), stdout) != pixels.size()) return 1;
    std::fflush(stdout);
  }
}
