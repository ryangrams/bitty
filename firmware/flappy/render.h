#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_model.h"
#include "skins.h"

namespace flappy {
constexpr int PixelWidth = 144;
constexpr int PixelHeight = 256;

struct RenderOptions {
  uint8_t uiScale = 2;
  uint32_t nowMs = 0;
  const char* coopStatus = "";
  const char* peerName = "";
  const char* notice = "";
  bool localReady = false, remoteReady = false, multipleHosts = false;
  bool localTint = false, remoteTint = true, remoteVisible = false;
  float remoteY = 0;
  int8_t remoteRotation = 0;
  uint8_t remoteFrame = 0;
  uint16_t remoteScore = 0;
};

class Renderer {
 public:
  explicit Renderer(uint16_t* pixels) : pixels_(pixels) { skins_.select(0); }
  void setSkin(uint8_t index) { skins_.select(index); }
  static uint8_t skinCount() { return Skins::count(); }
  static const char* skinName(uint8_t index) { return Skins::name(index); }
  const uint64_t (*birdMasks() const)[24] { return skins_.masks(); }
  const uint16_t* pixels() const { return pixels_; }

  void draw(const Game& game) { drawGame(game, Mode::Original, nullptr, RenderOptions()); }
  void draw(const AppModel& app, const RenderOptions& options = RenderOptions()) {
    if (skins_.selected != app.settings.skinIndex) setSkin(app.settings.skinIndex);
    drawGame(app.game, app.settings.mode, &app, options);
    if (app.menuOpen()) drawMenu(app, options);
    if (options.notice && *options.notice) {
      panel(3, app.menuOpen() ? 48 : 222, 138, 9);
      text(options.notice, 5, app.menuOpen() ? 50 : 224, 1, 0xffff, 33);
    }
  }

 private:
  uint16_t* pixels_;
  Skins skins_;

  static uint16_t blend(uint16_t top, uint16_t base, uint8_t alpha) {
    const unsigned inverse = 255 - alpha;
    const unsigned r = (((top >> 11) * alpha + (base >> 11) * inverse + 127) / 255) << 11;
    const unsigned g = ((((top >> 5) & 63) * alpha + ((base >> 5) & 63) * inverse + 127) / 255) << 5;
    const unsigned b = ((top & 31) * alpha + (base & 31) * inverse + 127) / 255;
    return uint16_t(r | g | b);
  }
  void pixel(int x, int y, uint16_t c) {
    if (x >= 0 && x < PixelWidth && y >= 0 && y < PixelHeight) pixels_[y * PixelWidth + x] = c;
  }
  void rect(int x, int y, int w, int h, uint16_t c) {
    const int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int x1 = x + w > PixelWidth ? PixelWidth : x + w;
    const int y1 = y + h > PixelHeight ? PixelHeight : y + h;
    for (int row = y0; row < y1; ++row) for (int col = x0; col < x1; ++col) pixels_[row * PixelWidth + col] = c;
  }
  void panel(int x, int y, int w, int h) {
    rect(x, y, w, h, rgb(26, 35, 49));
    rect(x, y, w, 1, rgb(255, 220, 116));
    rect(x, y + h - 1, w, 1, rgb(255, 220, 116));
  }
  void sprite(const FlappyAssets::Sprite& asset, int x, int y, int scale = 1, bool recolor = true) {
    const int x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0;
    const int x1 = x + asset.width * scale > PixelWidth ? PixelWidth - x : asset.width * scale;
    const int y1 = y + asset.height * scale > PixelHeight ? PixelHeight - y : asset.height * scale;
    for (int row = y0; row < y1; ++row) for (int col = x0; col < x1; ++col) {
      const int source = (row / scale) * asset.width + col / scale;
      uint16_t c = asset.pixels[source];
      if (c == FlappyAssets::kTransparent) continue;
      if (recolor) c = skins_.palette(c);
      uint16_t& target = pixels_[(y + row) * PixelWidth + x + col];
      target = asset.alpha ? blend(c, target, asset.alpha[source]) : c;
    }
  }
  void bird(int frame, int x, int y, float angle, int scale, bool tint) {
    frame = frame < 3 ? frame : 1;
    const float radians = angle * 0.01745329252f;
    const float c = cosf(radians), s = sinf(radians);
    const float cx = x + (17 * scale - 1) * 0.5f, cy = y + (12 * scale - 1) * 0.5f;
    const int radius = 12 * scale;
    for (int oy = int(floorf(cy)) - radius; oy <= int(ceilf(cy)) + radius; ++oy)
      for (int ox = int(floorf(cx)) - radius; ox <= int(ceilf(cx)) + radius; ++ox) {
      if (ox < 0 || ox >= PixelWidth || oy < 0 || oy >= PixelHeight) continue;
      const float dx = ox - cx, dy = oy - cy;
      const int sx = int(roundf(c * dx - s * dy + (17 * scale - 1) * 0.5f));
      const int sy = int(roundf(s * dx + c * dy + (12 * scale - 1) * 0.5f));
      if (sx < 0 || sx >= 17 * scale || sy < 0 || sy >= 12 * scale) continue;
      uint16_t color = skins_.birdPixel(frame, sx / scale, sy / scale);
      if (color == FlappyAssets::kTransparent) continue;
      if (tint) color = oppositeColor(color, skins_.monochrome);
      uint8_t alpha = 255;
      if (skins_.selected != 10 && skins_.selected != 11) {
        const auto& asset = skins_.bird(frame);
        if (asset.alpha) alpha = asset.alpha[(sy / scale) * asset.width + sx / scale];
      }
      uint16_t& target = pixels_[oy * PixelWidth + ox];
      target = alpha == 255 ? color : blend(color, target, alpha);
    }
  }
  void number(uint16_t value, const FlappyAssets::Sprite* font, int x, int y, bool alignRight, int scale = 1) {
    uint8_t glyphs[5];
    int count = 0, width = 0;
    do { glyphs[count++] = value % 10; value /= 10; } while (value && count < 5);
    for (int i = 0; i < count; ++i) width += (font[glyphs[i]].width + 1) * scale;
    width -= scale;
    x -= alignRight ? width : width / 2;
    for (int i = count - 1; i >= 0; --i) {
      const auto& glyph = font[glyphs[i]];
      sprite(glyph, x, y, scale, false);
      x += (glyph.width + 1) * scale;
    }
  }
  static const char* glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    switch (c) {
      case 'A': return "010101111101101"; case 'B': return "110101110101110";
      case 'C': return "011100100100011"; case 'D': return "110101101101110";
      case 'E': return "111100110100111"; case 'F': return "111100110100100";
      case 'G': return "011100101101011"; case 'H': return "101101111101101";
      case 'I': return "111010010010111"; case 'J': return "001001001101010";
      case 'K': return "101101110101101"; case 'L': return "100100100100111";
      case 'M': return "101111111101101"; case 'N': return "101111111111101";
      case 'O': return "010101101101010"; case 'P': return "110101110100100";
      case 'Q': return "010101101111011"; case 'R': return "110101110101101";
      case 'S': return "011100010001110"; case 'T': return "111010010010010";
      case 'U': return "101101101101111"; case 'V': return "101101101101010";
      case 'W': return "101101111111101"; case 'X': return "101101010101101";
      case 'Y': return "101101010010010"; case 'Z': return "111001010100111";
      case '0': return "111101101101111"; case '1': return "010110010010111";
      case '2': return "110001010100111"; case '3': return "110001010001110";
      case '4': return "101101111001001"; case '5': return "111100110001110";
      case '6': return "011100111101111"; case '7': return "111001010010010";
      case '8': return "111101111101111"; case '9': return "111101111001110";
      case ':': return "000010000010000"; case '/': return "001001010100100";
      case '-': return "000000111000000"; case '>': return "100010001010100";
      case '.': return "000000000000010"; case '+': return "000010111010000";
      case '*': return "000101010101000";
      case '%': return "101001010100101";
      default: return "000000000000000";
    }
  }
  void text(const char* str, int x, int y, int scale, uint16_t color = 0xffff, int maxChars = 99) {
    for (int i = 0; str && str[i] && i < maxChars; ++i) {
      const char* bits = glyph(str[i]);
      for (int row = 0; row < 5; ++row) for (int col = 0; col < 3; ++col)
        if (bits[row * 3 + col] == '1') rect(x + col * scale, y + row * scale, scale, scale, color);
      x += 4 * scale;
    }
  }
  void centered(const char* str, int y, int scale, uint16_t color = 0xffff) {
    text(str, (PixelWidth - (int(strlen(str)) * 4 - 1) * scale) / 2, y, scale, color);
  }
  void ownBackground(int scale) {
    const bool toast = skins_.selected == 10;
    rect(0, 0, PixelWidth, PixelHeight, toast ? rgb(178, 218, 213) : rgb(189, 206, 217));
    const int tile = 24 * scale;
    for (int y = 0; y < PixelHeight; y += tile) {
      rect(0, y, PixelWidth, scale, toast ? rgb(150, 192, 188) : rgb(162, 184, 204));
      if (toast) for (int x = (y / tile % 2) * tile / 2; x < PixelWidth; x += tile) rect(x, y, scale, tile, rgb(150, 192, 188));
    }
    if (toast) {
      rect(105, 105, 17, 22, rgb(204, 57, 85)); rect(103, 102, 21, 4, rgb(245, 220, 145));
    } else {
      rect(95, 75, 35, 52, rgb(241, 233, 199));
      for (int y = 83; y < 120; y += 8) rect(100, y, 23, 1, rgb(93, 123, 140));
    }
  }
  void pipe(int x, int lipY, bool top, int scale) {
    if (skins_.selected != 10 && skins_.selected != 11) {
      const auto& image = skins_.pipe(top);
      sprite(image, x, top ? lipY - image.height * scale : lipY, scale);
      return;
    }
    const int w = 26 * scale;
    const int start = top ? 0 : lipY, h = top ? lipY : PixelHeight - lipY;
    const bool toast = skins_.selected == 10;
    rect(x + 2 * scale, start, w - 4 * scale, h, toast ? rgb(89, 102, 110) : rgb(76, 102, 112));
    rect(x + 4 * scale, start, w - 9 * scale, h, toast ? rgb(192, 201, 198) : rgb(142, 165, 159));
    if (toast) {
      rect(x, top ? lipY - 7 * scale : lipY, w, 7 * scale, rgb(59, 65, 72));
      rect(x + 4 * scale, top ? lipY - 5 * scale : lipY + 2 * scale, w - 8 * scale, 2 * scale, rgb(225, 114, 55));
    } else {
      for (int y = start + 5 * scale; y < start + h; y += 19 * scale) {
        rect(x + 5 * scale, y, 15 * scale, scale, rgb(58, 82, 88));
        rect(x + 10 * scale, y + 5 * scale, 5 * scale, 2 * scale, rgb(227, 215, 173));
      }
      rect(x, top ? lipY - 4 * scale : lipY, w, 4 * scale, rgb(59, 76, 83));
    }
  }
  void drawGame(const Game& game, Mode mode, const AppModel* app, const RenderOptions& opt) {
    using namespace FlappyAssets;
    const int scale = game.rules().spriteScale;
    if (skins_.selected == 10 || skins_.selected == 11) ownBackground(scale);
    else if (scale == 1 && skins_.selected == 0) memcpy(pixels_, background.pixels, PixelWidth * PixelHeight * sizeof(uint16_t));
    else {
      const auto& bg = skins_.background();
      for (int y = 0; y < PixelHeight; ++y) for (int x = 0; x < PixelWidth; ++x) {
        const int sy = scale == 1 ? y : y / scale + 60;
        const int source = (sy < bg.height ? sy : bg.height - 1) * bg.width + x / scale;
        uint16_t color = bg.pixels[source];
        if (color == kTransparent || bg.alpha) {
          const int baseY = sy < background.height ? sy : background.height - 1;
          const uint16_t base = background.pixels[baseY * background.width + x / scale];
          color = color == kTransparent ? base : blend(color, base, bg.alpha[source]);
        }
        pixels_[y * PixelWidth + x] = skins_.palette(color);
      }
    }
    if (game.state != State::Ready) {
      int index = 0;
      for (const auto& p : game.pipes) {
        const int x = int(floorf(p.x * 0.5f));
        pipe(x, p.gapY / 2, true, scale);
        pipe(x, (p.gapY + game.rules().pipeGap) / 2, false, scale);
        if (mode == Mode::Clock && app) {
          uint8_t h = app->clockHour(opt.nowMs);
          const uint8_t m = app->clockMinute(opt.nowMs);
          if (!app->settings.clock24Hour) h = h % 12 ? h % 12 : 12;
          const uint8_t values[] = {uint8_t(h / 10), uint8_t(h % 10), uint8_t(m / 10), uint8_t(m % 10)};
          const unsigned digit = (game.worldTick() / 95 + unsigned(index)) % 4;
          const auto& image = digits[values[digit]];
          sprite(image, x + (game.pipeWidth() / 2 - image.width * 2) / 2,
                 p.gapY / 2 + game.rules().pipeGap / 4 - image.height, 2, false);
        }
        ++index;
      }
    }
    if (opt.remoteVisible) bird(opt.remoteFrame, game.birdX() / 2 + 4, int(floorf(opt.remoteY * 0.5f)), opt.remoteRotation, scale, opt.remoteTint);
    bird(game.birdFrame, game.birdX() / 2, int(floorf(game.birdY * 0.5f)), game.visibleRotation(), scale, opt.localTint);
    const int groundY = game.groundY() / 2;
    if (skins_.selected == 10 || skins_.selected == 11) {
      rect(0, groundY, PixelWidth, PixelHeight - groundY, skins_.selected == 10 ? rgb(218, 187, 129) : rgb(124, 78, 49));
      rect(0, groundY, PixelWidth, 3 * scale, rgb(52, 65, 68));
      for (int x = -int(game.groundOffset / 2); x < PixelWidth; x += 16 * scale) rect(x, groundY + 6 * scale, 7 * scale, scale, rgb(95, 64, 43));
    } else {
      for (int x = -int(game.groundOffset / 2); x < PixelWidth; x += skins_.ground().width * scale) sprite(skins_.ground(), x, groundY, scale);
    }
    if (mode == Mode::Clock && app) drawClock(*app, opt);
    else if (mode == Mode::Coop) {
      number(game.score, digits, 42, 18, false);
      number(opt.remoteScore, digits, 108, 18, false);
      text("YOU", 28, 7, 1); text("PEER", 94, 7, 1);
    } else if (mode == Mode::Arcade && app) {
      panel(8, 8, 128, 58);
      centered("TICKETS", 13, 2, rgb(255, 224, 103));
      number(app->arcadeTickets(), digits, 72, 28, false, 2);
      if (game.state == State::Playing) {
        char caption[24]; snprintf(caption, sizeof(caption), "BONUS IN %u", unsigned(app->arcadePointsRemaining()));
        centered(caption, 69, 1);
      }
    } else if (game.state == State::Ready) {
      if (mode == Mode::Original) {
        sprite(getReady, (PixelWidth - getReady.width) / 2, 55, 1, false);
        sprite(tapInstruction, (PixelWidth - tapInstruction.width) / 2, 127, 1, false);
      } else { centered(modeName(mode), 55, 3); centered("ACTION TO FLAP", 140, 2); }
    } else if (game.state != State::GameOver) number(game.score, digits, PixelWidth / 2, 18, false, scale);
    if (game.state == State::GameOver && mode != Mode::Clock && mode != Mode::Coop) drawResult(game, mode, app);
    if (mode == Mode::Demo || mode == Mode::Clock) {
      const bool autoPilot = app && app->autoPlaying();
      panel(0, 241, 144, 15);
      centered(autoPilot ? "AUTO / ACTION TO PLAY" : "PLAY / SCORE UNSAVED", 246, 1);
    }
    if (mode == Mode::Coop && (game.state == State::Ready || game.state == State::GameOver)) drawLobby(opt);
    if (game.state == State::Dying && game.stateFrames < 5) {
      const uint8_t opacity = uint8_t(170 - game.stateFrames * 34);
      for (int i = 0; i < PixelWidth * PixelHeight; ++i) pixels_[i] = blend(0xffff, pixels_[i], opacity);
    }
  }
  void drawClock(const AppModel& app, const RenderOptions& opt) {
    unsigned hour = app.clockHour(opt.nowMs);
    const unsigned minute = app.clockMinute(opt.nowMs);
    if (!app.settings.clock24Hour) hour = hour % 12 ? hour % 12 : 12;
    const unsigned values[] = {hour / 10, hour % 10, minute / 10, minute % 10};
    int width = 10;
    int height = 0;
    for (int i = 0; i < 4; ++i) {
      width += FlappyAssets::digits[values[i]].width * 2;
      const int glyphHeight = FlappyAssets::digits[values[i]].height * 2;
      if (glyphHeight > height) height = glyphHeight;
    }
    panel(7, 10, 130, height + 14);
    int x = (PixelWidth - width) / 2;
    for (int i = 0; i < 4; ++i) {
      const auto& digit = FlappyAssets::digits[values[i]];
      sprite(digit, x, 17, 2, false);
      x += digit.width * 2;
      if (i == 1) {
        rect(x + 3, 17 + height / 3, 2, 2, 0xffff);
        rect(x + 3, 17 + height * 2 / 3, 2, 2, 0xffff);
        x += 8;
      } else if (i < 3) ++x;
    }
    if (!app.settings.clock24Hour) centered(app.clockHour(opt.nowMs) < 12 ? "AM" : "PM", height + 27, 1);
  }
  void drawResult(const Game& game, Mode mode, const AppModel* app) {
    using namespace FlappyAssets;
    if (mode == Mode::Original || mode == Mode::Demo) {
      const int slide = game.stateFrames < 20 ? int(20 - game.stateFrames) * 4 : 0;
      sprite(gameOver, (PixelWidth - gameOver.width) / 2, 49, 1, false);
      const int x = (PixelWidth - scoreboard.width) / 2, y = 95 + slide;
      sprite(scoreboard, x, y, 1, false);
      number(game.score, panelDigits, x + 111, y + 18, true);
      number(game.bestScore, panelDigits, x + 111, y + 40, true);
      if (game.score >= 10) sprite(medals[game.score >= 40 ? 3 : game.score >= 30 ? 2 : game.score >= 20 ? 1 : 0], x + 13, y + 22, 1, false);
      if (game.newRecord) sprite(newBest, x + 66, y + 32, 1, false);
      if (game.stateFrames >= RestartDelayFrames) sprite(replay, (PixelWidth - replay.width) / 2, 164, 1, false);
    } else {
      panel(8, 87, 128, 105);
      centered("GAME OVER", 96, 2);
      centered(mode == Mode::Arcade ? "VIRTUAL TICKETS" : "PIPES", 115, 1);
      number(mode == Mode::Arcade && app ? app->arcadeTickets() : game.score, digits, 72, 129, false, 2);
      centered("ACTION: RETRY", 175, 2);
    }
  }
  void drawLobby(const RenderOptions& opt) {
    panel(5, 70, 134, 132);
    centered("CO-OP", 79, 3);
    centered(opt.coopStatus && *opt.coopStatus ? opt.coopStatus : "CHOOSE HOST OR JOIN", 103, 1);
    if (opt.peerName && *opt.peerName) centered(opt.peerName, 116, 1);
    centered(opt.localReady ? "YOU: READY" : "ACTION: READY", 135, 2);
    centered(opt.remoteReady ? "PEER: READY" : "WAITING FOR PEER", 153, 1);
    centered(opt.multipleHosts ? "MULTIPLE HOSTS FOUND" : "MENU ENDS RACE", 177, 1);
    centered("HOST / JOIN IN MENU", 188, 1);
  }
  void drawMenu(const AppModel& app, const RenderOptions& opt) {
    for (int i = 0; i < PixelWidth * PixelHeight; ++i) pixels_[i] = blend(0, pixels_[i], 170);
    panel(2, 10, 140, 233);
    const int scale = opt.uiScale >= 3 ? 3 : 2;
    centered(app.menuTitle(), 20, 2, rgb(255, 224, 103));
    const char* val = app.menuValue(app.menu.selection);
    if (val && *val) centered(val, 40, 1, rgb(166, 224, 232));
    const int visible = scale == 3 ? 5 : 6;
    const int rowHeight = scale == 3 ? 29 : 25;
    const int count = app.menuItemCount();
    int first = int(app.menu.selection) - visible + 1;
    if (first < 0) first = 0;
    const int maxChars = 128 / (4 * scale);
    for (int row = 0; row < visible && first + row < count; ++row) {
      const int index = first + row, y = 61 + row * rowHeight;
      const bool selected = index == app.menu.selection;
      if (selected) rect(5, y - 4, 134, rowHeight - 2, rgb(239, 199, 91));
      const char* label = app.menuLabel(uint8_t(index));
      const int length = int(strlen(label));
      if (selected && length > maxChars) {
        const unsigned span = unsigned(length - maxChars + 4);
        unsigned offset = (opt.nowMs / 400) % span;
        if (offset > unsigned(length - maxChars)) offset = unsigned(length - maxChars);
        label += offset;
      }
      text(label, 9, y, scale, selected ? rgb(30, 36, 46) : 0xffff, maxChars);
    }
    centered("MENU: NEXT  ACTION: SELECT", 220, 1);
    centered("HOLD MENU: EXIT", 231, 1);
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
