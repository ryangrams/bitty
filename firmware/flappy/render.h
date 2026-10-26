#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "game_core.h"
#include "generated_assets.h"

namespace flappy {

constexpr int PixelWidth = 144;
constexpr int PixelHeight = 256;

class Renderer {
 public:
  explicit Renderer(uint16_t* pixels) : pixels_(pixels) {}

  void draw(const Game& game) {
    using namespace FlappyAssets;
    memcpy(pixels_, background.pixels, PixelWidth * PixelHeight * sizeof(uint16_t));
    if (game.state != State::Ready) {
      for (const auto& p : game.pipes) {
        sprite(pipeTop, int(floorf(p.x * 0.5f)), p.gapY / 2 - pipeTop.height);
        sprite(FlappyAssets::pipe, int(floorf(p.x * 0.5f)), (p.gapY + PipeGap) / 2);
      }
    }
    const Sprite* birds[] = {&birdUp, &birdMid, &birdDown};
    bird(*birds[game.birdFrame < 3 ? game.birdFrame : 1],
         BirdX / 2, int(floorf(game.birdY * 0.5f)), game.visibleRotation());
    for (int x = -int(game.groundOffset / 2); x < PixelWidth; x += ground.width) {
      sprite(ground, x, GroundY / 2);
    }
    if (game.state == State::Ready) {
      sprite(getReady, (PixelWidth - getReady.width) / 2, 55);
      sprite(tapInstruction, (PixelWidth - tapInstruction.width) / 2, 127);
    } else if (game.state == State::GameOver) {
      const int slide = game.stateFrames < 20 ? int(20 - game.stateFrames) * 4 : 0;
      sprite(gameOver, (PixelWidth - gameOver.width) / 2, 49);
      const int panelX = (PixelWidth - scoreboard.width) / 2;
      const int panelY = 95 + slide;
      sprite(scoreboard, panelX, panelY);
      number(game.score, panelDigits, panelX + 111, panelY + 18, true);
      number(game.bestScore, panelDigits, panelX + 111, panelY + 40, true);
      if (game.score >= 10) {
        const int medal = game.score >= 40 ? 3 : game.score >= 30 ? 2 : game.score >= 20 ? 1 : 0;
        sprite(medals[medal], panelX + 13, panelY + 22);
      }
      if (game.newRecord) {
        sprite(newBest, panelX + 66, panelY + 32);
      }
      if (game.stateFrames >= RestartDelayFrames) {
        sprite(replay, (PixelWidth - replay.width) / 2, 164);
      }
    } else {
      number(game.score, digits, PixelWidth / 2, 18, false);
    }
    if (game.state == State::Dying && game.stateFrames < 5) {
      const uint8_t opacity = uint8_t(170 - game.stateFrames * 34);
      for (int i = 0; i < PixelWidth * PixelHeight; ++i) pixels_[i] = blend(0xffff, pixels_[i], opacity);
    }
  }

  const uint16_t* pixels() const { return pixels_; }

 private:
  uint16_t* pixels_;

  static uint16_t blend(uint16_t top, uint16_t base, uint8_t alpha) {
    const unsigned inverse = 255 - alpha;
    const unsigned red = (((top >> 11) * alpha + (base >> 11) * inverse + 127) / 255) << 11;
    const unsigned green = ((((top >> 5) & 63) * alpha + ((base >> 5) & 63) * inverse + 127) / 255) << 5;
    const unsigned blue = ((top & 31) * alpha + (base & 31) * inverse + 127) / 255;
    return uint16_t(red | green | blue);
  }

  void sprite(const FlappyAssets::Sprite& asset, int x, int y) {
    const int x0 = x < 0 ? -x : 0;
    const int y0 = y < 0 ? -y : 0;
    const int x1 = x + asset.width > PixelWidth ? PixelWidth - x : asset.width;
    const int y1 = y + asset.height > PixelHeight ? PixelHeight - y : asset.height;
    for (int row = y0; row < y1; ++row) {
      for (int col = x0; col < x1; ++col) {
        const int source = row * asset.width + col;
        const uint16_t color = asset.pixels[source];
        if (color == FlappyAssets::kTransparent) continue;
        uint16_t& target = pixels_[(y + row) * PixelWidth + x + col];
        target = asset.alpha ? blend(color, target, asset.alpha[source]) : color;
      }
    }
  }

  void bird(const FlappyAssets::Sprite& asset, int x, int y, float angle) {
    if (fabsf(angle) < 0.5f) {
      sprite(asset, x, y);
      return;
    }
    const float radians = angle * 0.01745329252f;
    const float c = cosf(radians), s = sinf(radians);
    const float cx = x + (asset.width - 1) * 0.5f;
    const float cy = y + (asset.height - 1) * 0.5f;
    for (int dy = -11; dy <= 11; ++dy) {
      for (int dx = -11; dx <= 11; ++dx) {
        const int outX = int(roundf(cx + dx)), outY = int(roundf(cy + dy));
        if (outX < 0 || outX >= PixelWidth || outY < 0 || outY >= PixelHeight) continue;
        const int sx = int(roundf(c * dx - s * dy + (asset.width - 1) * 0.5f));
        const int sy = int(roundf(s * dx + c * dy + (asset.height - 1) * 0.5f));
        if (sx < 0 || sx >= asset.width || sy < 0 || sy >= asset.height) continue;
        const uint16_t color = asset.pixels[sy * asset.width + sx];
        if (color != FlappyAssets::kTransparent) pixels_[outY * PixelWidth + outX] = color;
      }
    }
  }

  void number(uint16_t value, const FlappyAssets::Sprite* font, int x, int y, bool alignRight) {
    uint8_t glyphs[5];
    int count = 0, width = 0;
    do {
      glyphs[count++] = value % 10;
      value /= 10;
    } while (value && count < 5);
    for (int i = 0; i < count; ++i) width += font[glyphs[i]].width + 1;
    --width;
    x -= alignRight ? width : width / 2;
    for (int i = count - 1; i >= 0; --i) {
      const auto& glyph = font[glyphs[i]];
      sprite(glyph, x, y);
      x += glyph.width + 1;
    }
  }
};

struct Viewport {
  int width = 0, height = 0;
  int sourceX = 0, sourceY = 0;
  int outputX = 0, outputY = 0;
  bool native = true;

  void configure(int screenWidth, int screenHeight) {
    native = screenWidth >= 128 && screenHeight >= 224;
    if (native) {
      width = screenWidth < PixelWidth ? screenWidth : PixelWidth;
      height = screenHeight < PixelHeight ? screenHeight : PixelHeight;
      sourceX = (PixelWidth - width) / 2;
      sourceY = (PixelHeight - height) / 2;
    } else {
      width = screenWidth;
      height = screenWidth * PixelHeight / PixelWidth;
      if (height > screenHeight) {
        height = screenHeight;
        width = screenHeight * PixelWidth / PixelHeight;
      }
    }
    outputX = (screenWidth - width) / 2;
    outputY = (screenHeight - height) / 2;
  }

  void copy(const uint16_t* source, uint16_t* output) const {
    copyRows(source, output, 0, height);
  }

  void copyRows(const uint16_t* source, uint16_t* output, int firstRow, int rows) const {
    for (int row = 0; row < rows; ++row) {
      const int y = row + firstRow;
      if (native) {
        memcpy(output + row * width, source + (y + sourceY) * PixelWidth + sourceX, width * sizeof(uint16_t));
      } else {
        const int sy = y * PixelHeight / height;
        for (int x = 0; x < width; ++x) output[row * width + x] = source[sy * PixelWidth + x * PixelWidth / width];
      }
    }
  }
};
}  // namespace flappy
