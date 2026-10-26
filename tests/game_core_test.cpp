#include "../firmware/flappy/game_core.h"

#include <assert.h>
#include <cmath>
#include <iostream>

using flappy::Game;
using flappy::State;

static void near(float actual, float expected) { assert(std::fabs(actual - expected) < 0.001f); }

static void readyAndFirstFlap() {
  Game game(123);
  float previousY = game.birdY;
  for (int i = 0; i < 128; ++i) {
    assert(game.step(false) == flappy::NoEvent);
    assert(game.state == State::Ready);
    assert(game.birdY >= 236 && game.birdY <= 252);
    assert(std::fabs(game.birdY - previousY) <= 0.5f);
    previousY = game.birdY;
    assert(game.birdFrame < 3);
  }
  const float start = game.birdY;
  assert(game.step(true) & flappy::Flap);
  assert(game.state == State::Playing);
  near(game.velocityY, flappy::FlapVelocity);
  near(game.birdY, start + flappy::FlapVelocity);
  assert(!(game.step(false) & flappy::Flap));
  near(game.velocityY, flappy::FlapVelocity + flappy::Gravity);
  assert(game.step(true) & flappy::Flap);
  near(game.velocityY, flappy::FlapVelocity);
}

static void scoresOnceAndRecycles() {
  Game game(42);
  game.step(true);
  game.birdY = 190;
  game.velocityY = 0;
  game.pipes[0] = {49, 160, false};
  assert(game.step(false) & flappy::Point);
  assert(game.score == 1);
  assert(game.bestScore == 1);
  for (int i = 0; i < 12; ++i) {
    game.birdY = 190;
    game.velocityY = 0;
    assert(!(game.step(false) & flappy::Point));
  }
  assert(game.score == 1);
  game.pipes[0].x = -53;
  const float previousRight = game.pipes[3].x;
  game.step(false);
  near(game.pipes[0].x, previousRight - flappy::PipeSpeed + flappy::PipeSpacing);
  assert(!game.pipes[0].scored);
  assert(game.pipes[0].gapY >= 80 && game.pipes[0].gapY < 222);
}

static void alphaCollision() {
  uint64_t masks[3][flappy::BirdHeight] = {};
  masks[1][8] = uint64_t(1) << 10;
  Game game;
  game.setBirdMasks(masks);
  game.birdFrame = 1;
  game.birdY = 100;
  flappy::Pipe pipe = {float(flappy::BirdX), 109, false};
  assert(game.collidesWithPipe(pipe));
  pipe.gapY = 108;
  assert(!game.collidesWithPipe(pipe));
  game.birdFrame = 0;
  assert(!game.collidesWithPipe(pipe));
  game.birdFrame = 1;
  pipe.gapY = 109;
  assert(game.collidesWithPipe(pipe));
  pipe.x = float(flappy::BirdX + flappy::BirdWidth);
  assert(!game.collidesWithPipe(pipe));
  game.reset();
  game.birdFrame = 1;
  game.birdY = 100;
  pipe.x = float(flappy::BirdX);
  assert(game.collidesWithPipe(pipe));
}

static void deathAndRestart() {
  Game game;
  game.setBestScore(7);
  game.step(true);
  game.birdY = flappy::GroundY - flappy::BirdHeight - 1;
  game.velocityY = flappy::MaxFallVelocity;
  assert(game.step(false) & flappy::Hit);
  assert(game.state == State::Dying);
  near(game.birdY, float(flappy::GroundY - flappy::BirdHeight));
  uint8_t events = game.step(true);
  assert(events & flappy::GameOver);
  assert(!(events & flappy::Die));
  assert(game.state == State::GameOver);
  const auto offset = game.groundOffset;
  for (int i = 0; i < flappy::RestartDelayFrames - 1; ++i) {
    game.step(true);
    assert(game.state == State::GameOver);
    assert(game.groundOffset == offset);
  }
  game.step(true);
  assert(game.state == State::Ready);
  assert(game.score == 0);
  assert(game.bestScore == 7);
  assert(game.step(true) & flappy::Flap);
  assert(game.state == State::Playing);
}

static void pipeDeathAndOffscreenGrace() {
  Game game;
  game.step(true);
  game.birdY = -47;
  assert(game.step(true) & flappy::Flap);
  near(game.birdY, -48);
  assert(game.state == State::Playing);
  assert(!(game.step(true) & flappy::Flap));
  assert(game.state == State::Playing);

  game.birdY = 100;
  game.velocityY = 0;
  game.pipes[0] = {float(flappy::BirdX), 150, false};
  assert(game.step(false) & flappy::Hit);
  assert(game.state == State::Dying);
  const float frozenPipeX = game.pipes[0].x;
  int dieCount = 0;
  for (int i = 0; i < 180 && game.state == State::Dying; ++i) {
    const uint8_t events = game.step(true);
    dieCount += (events & flappy::Die) ? 1 : 0;
    assert(!(events & flappy::Flap));
    near(game.pipes[0].x, frozenPipeX);
  }
  assert(game.state == State::GameOver);
  assert(dieCount == 1);
}

static void seededPipesAreDeterministic() {
  Game first(195), second(195), different(196);
  bool anyDifferent = false;
  for (int i = 0; i < flappy::PipeCount; ++i) {
    near(first.pipes[i].x, second.pipes[i].x);
    assert(first.pipes[i].gapY == second.pipes[i].gapY);
    anyDifferent |= first.pipes[i].gapY != different.pipes[i].gapY;
  }
  assert(anyDifferent);
}

static void twentyPipesWithPressesOnly() {
  Game game(2);
  game.step(true);
  unsigned pointEvents = 0;
  unsigned ticks = 0;
  while (game.state == State::Playing && game.score < 20 && ticks++ < 3000) {
    const flappy::Pipe* nearest = nullptr;
    for (int i = 0; i < flappy::PipeCount; ++i) {
      const auto& pipe = game.pipes[i];
      if (pipe.x + flappy::PipeWidth > flappy::BirdX &&
          (!nearest || pipe.x < nearest->x)) nearest = &pipe;
    }
    assert(nearest);
    const float target = nearest->gapY + 64.0f;
    const bool press = game.velocityY >= -0.5f && game.birdY + game.velocityY * 4 > target;
    const uint8_t events = game.step(press);
    pointEvents += (events & flappy::Point) ? 1 : 0;
  }
  assert(game.state == State::Playing);
  assert(game.score == 20);
  assert(pointEvents == 20);
  assert(game.bestScore == 20);
}

int main() {
  readyAndFirstFlap();
  scoresOnceAndRecycles();
  alphaCollision();
  deathAndRestart();
  pipeDeathAndOffscreenGrace();
  seededPipesAreDeterministic();
  twentyPipesWithPressesOnly();
  std::cout << "Gameplay checks passed.\n";
}
