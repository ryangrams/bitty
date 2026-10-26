#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../firmware/flappy/render.h"

int main(int argc, char** argv) {
  if (argc != 4) return 1;
  flappy::Game game(2);
  const int state = std::atoi(argv[3]);
  if (state == 1) {
    game.state = flappy::State::Playing;
    game.birdY = 195;
    game.rotation = 12;
    game.score = 7;
    game.pipes[0] = {135, 150, false};
    game.pipes[1] = {279, 180, false};
  } else if (state == 2) {
    game.state = flappy::State::GameOver;
    game.stateFrames = 60;
    game.birdY = flappy::GroundY - flappy::BirdHeight;
    game.rotation = -90;
    game.score = 12;
    game.bestScore = 24;
    game.pipes[0] = {135, 150, false};
  }
  std::vector<uint16_t> pixels(flappy::PixelWidth * flappy::PixelHeight);
  flappy::Renderer render(pixels.data());
  render.draw(game);
  const int width = std::atoi(argv[1]);
  const int height = std::atoi(argv[2]);
  flappy::Viewport view;
  view.configure(width, height);
  std::vector<uint16_t> output(view.width * view.height);
  view.copy(pixels.data(), output.data());
  std::printf("P6\n%d %d\n255\n", width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint16_t color = 0;
      if (x >= view.outputX && x < view.outputX + view.width &&
          y >= view.outputY && y < view.outputY + view.height) {
        color = output[(y - view.outputY) * view.width + x - view.outputX];
      }
      const uint8_t rgb[] = {uint8_t(((color >> 11) & 31) * 255 / 31),
        uint8_t(((color >> 5) & 63) * 255 / 63), uint8_t((color & 31) * 255 / 31)};
      std::fwrite(rgb, 1, 3, stdout);
    }
  }
}
