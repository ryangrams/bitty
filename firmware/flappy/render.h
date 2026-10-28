#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_model.h"
#include "menu_font.h"
#include "bitty_ui.h"
#include "skins.h"

namespace flappy {
constexpr int PixelWidth = 144;
constexpr int PixelHeight = 256;

struct RenderOptions {
  uint8_t uiScale = 2;
  uint32_t nowMs = 0;
  BatteryStatus battery;
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
  ~Renderer() { releaseBackgroundCache(); }
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  static constexpr size_t BackgroundCacheLimit = 16 * 1024;
  void setBackgroundCacheEnabled(bool enabled, size_t limitBytes = BackgroundCacheLimit) {
    const size_t limit = limitBytes < BackgroundCacheLimit ? limitBytes : BackgroundCacheLimit;
    if (enabled != backgroundCacheEnabled_ || limit != backgroundCacheLimit_) releaseBackgroundCache();
    backgroundCacheEnabled_ = enabled;
    backgroundCacheLimit_ = uint16_t(limit);
  }
  size_t backgroundCacheBytes() const { return backgroundCacheBytes_; }

  void setSkin(uint8_t index) {
    index = index < Skins::count() ? index : 0;
    if (index != skins_.selected) releaseBackgroundCache();
    skins_.select(index);
  }
  static uint8_t skinCount() { return Skins::count(); }
  static const char* skinName(uint8_t index) { return Skins::name(index); }
  const uint64_t (*birdMasks() const)[24] { return skins_.masks(); }
  const uint16_t* pixels() const { return pixels_; }

  void drawMessage(const char* title, const char* detail) {
    rect(0, 0, PixelWidth, PixelHeight, 0);
    panel(6, 92, 132, 72);
    menuCentered(title, 108, 1, rgb(255, 220, 116));
    menuCentered(detail, 133, 1);
  }

  void drawMessageLandscape(const char* title, const char* detail) {
    const int width = 240, height = 135;
    for (int i = 0; i < width * height; ++i) pixels_[i] = 0;
    for (int y = 40; y < 96; ++y) {
      for (int x = 8; x < 232; ++x) pixels_[y * width + x] = rgb(26, 35, 49);
    }
    const int maxChars = 216 / MenuFont::Advance;
    MenuFont::draw(pixels_, width, height, title,
        (width - MenuFont::textWidth(title, 1, maxChars)) / 2, 49,
        rgb(255, 220, 116), 1, maxChars);
    MenuFont::draw(pixels_, width, height, detail,
        (width - MenuFont::textWidth(detail, 1, maxChars)) / 2, 73,
        0xffff, 1, maxChars);
  }

  void draw(const Game& game) { drawGame(game, Mode::Original, nullptr, RenderOptions()); }
  void draw(const AppModel& app, const RenderOptions& options = RenderOptions()) {
    if (skins_.selected != app.settings.skinIndex) setSkin(app.settings.skinIndex);
    if (app.menu.page == MenuPage::Skins) drawSkinPicker(app, options);
    else if (app.menuOpen()) drawMenu(app, options);
    else drawGame(app.game, app.settings.mode, &app, options);
    if (options.notice && *options.notice) {
      const int y = app.menuOpen() ? 51 : 224;
      rect(6,y,132,15,BittyUi::Panel);
      const size_t length = strlen(options.notice);
      const size_t limit = 21;
      size_t offset = length > limit ? (options.nowMs / 400) % (length - limit + 4) : 0;
      if (length > limit && offset > length - limit) offset = length - limit;
      menuText(options.notice + offset,9,y + 1,1,BittyUi::Text,limit);
    }
  }

 private:
  uint16_t* pixels_;
  Skins skins_;
  Skins::BirdPreview previewCards_[6];
  uint8_t previewFirst_ = UINT8_MAX, previewSkinCount_ = 0;
  uint8_t* backgroundCache_ = nullptr;
  size_t backgroundCacheBytes_ = 0;
  uint16_t backgroundCacheLimit_ = BackgroundCacheLimit;
  uint8_t backgroundCacheScale_ = 0;
  bool backgroundCacheEnabled_ = true;

  void releaseBackgroundCache() {
    free(backgroundCache_);
    backgroundCache_ = nullptr;
    backgroundCacheBytes_ = 0;
    backgroundCacheScale_ = 0;
  }

  void backgroundRow(int y, int scale, uint16_t* target) const {
    const auto& bg = skins_.background();
    const auto& base = FlappyAssets::background;
    const int sy = scale == 1 ? y : y / scale + 60;
    const int sourceRow = (sy < bg.height ? sy : bg.height - 1) * bg.width;
    const int baseRow = (sy < base.height ? sy : base.height - 1) * base.width;
    for (int x = 0; x < PixelWidth; ++x) {
      const int source = sourceRow + x / scale;
      uint16_t color = bg.pixels[source];
      if (color == FlappyAssets::kTransparent || bg.alpha) {
        const uint8_t alpha = color == FlappyAssets::kTransparent ? 0 : bg.alpha[source];
        if (alpha != 255) {
          const uint16_t underneath = base.pixels[baseRow + x / scale];
          color = alpha ? blend(color, underneath, alpha) : underneath;
        }
      }
      target[x] = skins_.palette(color);
    }
  }

  void buildBackgroundCache(int scale) {
    constexpr size_t RowBytes = PixelWidth * sizeof(uint16_t);
    constexpr size_t MaxRows = (BackgroundCacheLimit - PixelHeight) / RowBytes;
    if (backgroundCacheLimit_ < PixelHeight + RowBytes) return;
    const size_t capacity = (backgroundCacheLimit_ - PixelHeight) / RowBytes;
    uint8_t rowMap[PixelHeight], representatives[MaxRows];
    uint32_t hashes[MaxRows];
    uint16_t row[PixelWidth], candidate[PixelWidth];
    size_t count = 0;
    for (int y = 0; y < PixelHeight; ++y) {
      backgroundRow(y, scale, row);
      uint32_t hash = 2166136261u;
      for (int x = 0; x < PixelWidth; ++x) hash = (hash ^ row[x]) * 16777619u;
      size_t index = 0;
      for (; index < count; ++index) {
        if (hashes[index] != hash) continue;
        backgroundRow(representatives[index], scale, candidate);
        if (memcmp(row, candidate, RowBytes) == 0) break;
      }
      if (index == count) {
        if (count >= capacity) return;
        hashes[count] = hash;
        representatives[count++] = uint8_t(y);
      }
      rowMap[y] = uint8_t(index);
    }
    const size_t bytes = PixelHeight + count * RowBytes;
    uint8_t* cache = static_cast<uint8_t*>(malloc(bytes));
    if (!cache) return;
    memcpy(cache, rowMap, PixelHeight);
    uint16_t* rows = reinterpret_cast<uint16_t*>(cache + PixelHeight);
    for (size_t index = 0; index < count; ++index)
      backgroundRow(representatives[index], scale, rows + index * PixelWidth);
    backgroundCache_ = cache;
    backgroundCacheBytes_ = bytes;
  }

  bool drawCachedBackground(int scale) {
    if (!backgroundCacheEnabled_) return false;
    if (backgroundCacheScale_ != scale) {
      releaseBackgroundCache();
      backgroundCacheScale_ = uint8_t(scale);
      buildBackgroundCache(scale);
    }
    if (!backgroundCache_) return false;
    const uint16_t* rows = reinterpret_cast<const uint16_t*>(backgroundCache_ + PixelHeight);
    for (int y = 0; y < PixelHeight; ++y)
      memcpy(pixels_ + y * PixelWidth, rows + backgroundCache_[y] * PixelWidth,
             PixelWidth * sizeof(uint16_t));
    return true;
  }

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
    if (scale == 1) {
      const int firstX = x < 0 ? -x : 0, firstY = y < 0 ? -y : 0;
      const int lastX = x + asset.width > PixelWidth ? PixelWidth - x : asset.width;
      const int lastY = y + asset.height > PixelHeight ? PixelHeight - y : asset.height;
      if (firstX >= lastX || firstY >= lastY) return;
      for (int row = firstY; row < lastY; ++row) {
        const uint16_t* source = asset.pixels + row * asset.width;
        const uint8_t* alpha = asset.alpha ? asset.alpha + row * asset.width : nullptr;
        uint16_t* target = pixels_ + (y + row) * PixelWidth + x + firstX;
        if (&asset == &FlappyAssets::ground && !alpha) {
          for (int col = firstX; col < lastX;) {
            const uint16_t color = source[col];
            int end = col + 1;
            while (end < lastX && source[end] == color) ++end;
            if (color != FlappyAssets::kTransparent) {
              const uint16_t output = recolor ? skins_.palette(color) : color;
              for (int at = col; at < end; ++at) target[at - firstX] = output;
            }
            col = end;
          }
        } else for (int col = firstX; col < lastX; ++col) {
          uint16_t color = source[col];
          if (color == FlappyAssets::kTransparent) continue;
          if (recolor) color = skins_.palette(color);
          uint16_t& destination = target[col - firstX];
          destination = alpha && alpha[col] != 255 ? blend(color, destination, alpha[col]) : color;
        }
      }
      return;
    }
    const int x0 = x < 0 ? -x / scale : 0, y0 = y < 0 ? -y / scale : 0;
    const int right = (PixelWidth - x + scale - 1) / scale;
    const int bottom = (PixelHeight - y + scale - 1) / scale;
    const int x1 = right < asset.width ? right : asset.width;
    const int y1 = bottom < asset.height ? bottom : asset.height;
    for (int sy = y0; sy < y1; ++sy) {
      const int top = y + sy * scale, endY = top + scale < PixelHeight ? top + scale : PixelHeight;
      for (int sx = x0; sx < x1; ++sx) {
        const int source = sy * asset.width + sx;
        uint16_t color = asset.pixels[source];
        if (color == FlappyAssets::kTransparent) continue;
        if (recolor) color = skins_.palette(color);
        const uint8_t alpha = asset.alpha ? asset.alpha[source] : 255;
        if (!alpha) continue;
        const int left = x + sx * scale, endX = left + scale < PixelWidth ? left + scale : PixelWidth;
        for (int row = top < 0 ? 0 : top; row < endY; ++row)
          for (int col = left < 0 ? 0 : left; col < endX; ++col) {
            uint16_t& target = pixels_[row * PixelWidth + col];
            target = alpha == 255 ? color : blend(color, target, alpha);
          }
      }
    }
  }
  void bird(int frame, int x, int y, float angle, int scale, bool tint) {
    frame = frame < 3 ? frame : 1;
    const auto& asset = skins_.bird(frame);
    const uint8_t* alphaPixels = skins_.selected == 10 || skins_.selected == 11 ? nullptr : asset.alpha;
    if (angle == 0) {
      for (int sy = 0; sy < 12; ++sy) for (int sx = 0; sx < 17; ++sx) {
        uint16_t color = skins_.birdPixel(frame, sx, sy);
        if (color == FlappyAssets::kTransparent) continue;
        if (tint) color = oppositeColor(color, skins_.monochrome);
        const uint8_t alpha = alphaPixels ? alphaPixels[sy * asset.width + sx] : 255;
        if (!alpha) continue;
        for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx) {
          const int ox = x + sx * scale + dx, oy = y + sy * scale + dy;
          if (ox < 0 || ox >= PixelWidth || oy < 0 || oy >= PixelHeight) continue;
          uint16_t& target = pixels_[oy * PixelWidth + ox];
          target = alpha == 255 ? color : blend(color, target, alpha);
        }
      }
      return;
    }
    const float radians = angle * 0.01745329252f;
    const float c = cosf(radians), s = sinf(radians);
    const float cx = x + (17 * scale - 1) * 0.5f, cy = y + (12 * scale - 1) * 0.5f;
    const int radius = 12 * scale;
    const float extentX = (fabsf(c) * 17 + fabsf(s) * 12) * scale * 0.5f + 1;
    const float extentY = (fabsf(s) * 17 + fabsf(c) * 12) * scale * 0.5f + 1;
    int firstX = int(floorf(cx - extentX)), lastX = int(ceilf(cx + extentX));
    int firstY = int(floorf(cy - extentY)), lastY = int(ceilf(cy + extentY));
    if (firstX < int(floorf(cx)) - radius) firstX = int(floorf(cx)) - radius;
    if (lastX > int(ceilf(cx)) + radius) lastX = int(ceilf(cx)) + radius;
    if (firstY < int(floorf(cy)) - radius) firstY = int(floorf(cy)) - radius;
    if (lastY > int(ceilf(cy)) + radius) lastY = int(ceilf(cy)) + radius;
    if (firstX < 0) firstX = 0;
    if (lastX >= PixelWidth) lastX = PixelWidth - 1;
    if (firstY < 0) firstY = 0;
    if (lastY >= PixelHeight) lastY = PixelHeight - 1;
    for (int oy = firstY; oy <= lastY; ++oy)
      for (int ox = firstX; ox <= lastX; ++ox) {
      const float dx = ox - cx, dy = oy - cy;
      const int sx = int(roundf(c * dx - s * dy + (17 * scale - 1) * 0.5f));
      const int sy = int(roundf(s * dx + c * dy + (12 * scale - 1) * 0.5f));
      if (sx < 0 || sx >= 17 * scale || sy < 0 || sy >= 12 * scale) continue;
      uint16_t color = skins_.birdPixel(frame, sx / scale, sy / scale);
      if (color == FlappyAssets::kTransparent) continue;
      if (tint) color = oppositeColor(color, skins_.monochrome);
      const uint8_t alpha = alphaPixels ? alphaPixels[(sy / scale) * asset.width + sx / scale] : 255;
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
  void menuText(const char* str, int x, int y, int scale, uint16_t color, int maxChars = 99) {
    MenuFont::draw(pixels_, PixelWidth, PixelHeight, str, x, y, color, scale, maxChars);
  }
  void menuCentered(const char* str, int y, int scale, uint16_t color = 0xffff) {
    if (scale != 2 || MenuFont::textWidth(str, 2) > 128) scale = 1;
    const int maxChars = 128 / (MenuFont::Advance * scale);
    const int width = MenuFont::textWidth(str, scale, maxChars);
    menuText(str, (PixelWidth - width) / 2, y, scale, color, maxChars);
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
    else if (drawCachedBackground(scale)) {}
    else if (scale == 1 && skins_.selected == 0) memcpy(pixels_, background.pixels, PixelWidth * PixelHeight * sizeof(uint16_t));
    else {
      const auto& bg = skins_.background();
      const bool repeatRows = &bg == &background && !bg.alpha;
      const uint16_t* previous = nullptr;
      for (int y = 0; y < PixelHeight; ++y) {
        const int sy = scale == 1 ? y : y / scale + 60;
        const int sourceRow = (sy < bg.height ? sy : bg.height - 1) * bg.width;
        const uint16_t* row = bg.pixels + sourceRow;
        if (repeatRows && previous && (row == previous ||
            memcmp(row, previous, ((PixelWidth + scale - 1) / scale) * sizeof(uint16_t)) == 0)) {
          memcpy(pixels_ + y * PixelWidth, pixels_ + (y - 1) * PixelWidth, PixelWidth * sizeof(uint16_t));
        } else for (int x = 0; x < PixelWidth; ++x) {
          const int source = sourceRow + x / scale;
          uint16_t color = bg.pixels[source];
          if (color == kTransparent || bg.alpha) {
            const uint8_t alpha = color == kTransparent ? 0 : bg.alpha[source];
            if (alpha != 255) {
              const int baseY = sy < background.height ? sy : background.height - 1;
              const uint16_t base = background.pixels[baseY * background.width + x / scale];
              color = alpha ? blend(color, base, alpha) : base;
            }
          }
          pixels_[y * PixelWidth + x] = skins_.palette(color);
        }
        previous = row;
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
      rect(6,229,132,17,BittyUi::Panel);
      menuCentered(autoPilot ? "AUTO / ACTION TO PLAY" : "PLAY / SCORE UNSAVED",231,1,BittyUi::Text);
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
  static void skinLabelLines(const char* label, char (&first)[11], char (&second)[11]) {
    if (!label) label = "";
    const size_t length = strlen(label);
    size_t split = length < 10 ? length : 10;
    if (length > split) {
      for (size_t index = split; index > 0; --index)
        if (label[index] == ' ') { split = index; break; }
    }
    memcpy(first, label, split);
    first[split] = '\0';
    label += split;
    while (*label == ' ') ++label;
    const size_t remaining = strlen(label);
    const size_t visible = remaining > 10 ? 8 : remaining;
    memcpy(second, label, visible);
    if (remaining > 10) { second[visible] = '.'; second[visible + 1] = '.'; second[visible + 2] = '\0'; }
    else second[visible] = '\0';
  }
  void drawSkinPicker(const AppModel& app, const RenderOptions& opt) {
    const uint8_t count = uint8_t(app.menuItemCount() - 1);
    const unsigned focus = app.menu.selection < count ? app.menu.selection : count - 1u;
    const uint8_t first = uint8_t(focus / 6u * 6u);
    if (previewFirst_ != first || previewSkinCount_ != count) {
      Skins preview;
      for (uint8_t slot = 0; slot < 6 && first + slot < count; ++slot) {
        preview.select(uint8_t(first + slot));
        previewCards_[slot].capture(preview);
      }
      previewFirst_ = first;
      previewSkinCount_ = count;
    }
    const uint16_t background = BittyUi::Black, card = BittyUi::Panel;
    const uint16_t gold = BittyUi::Amber, outline = BittyUi::Rule;
    rect(0, 0, PixelWidth, PixelHeight, background);
    BittyUi::header(pixels_,PixelWidth,PixelHeight,opt.battery);
    const char* caption = app.menuLabel(app.menu.selection);
    const size_t captionLength = strlen(caption);
    const unsigned maxCaption = 15;
    unsigned captionOffset = 0;
    if (captionLength > maxCaption) {
      captionOffset = (opt.nowMs / 400) % unsigned(captionLength - maxCaption + 4);
      if (captionOffset > captionLength - maxCaption) captionOffset = unsigned(captionLength - maxCaption);
    }
    char title[23]; snprintf(title,sizeof(title),"Skin: %.15s",caption + captionOffset);
    menuCentered(title,31,1,BittyUi::Text);
    static const uint8_t animation[] = {0, 1, 2, 1};
    const uint8_t frame = animation[(opt.nowMs / 120) % 4];
    for (uint8_t slot = 0; slot < 6 && first + slot < count; ++slot) {
      const uint8_t index = uint8_t(first + slot);
      const int x = 6 + (slot % 2) * 68, y = 46 + (slot / 2) * 61;
      const bool selected = index == app.menu.selection;
      rect(x, y, 64, 60, selected ? gold : outline);
      rect(x + 2, y + 2, 60, 56, card);
      rect(x + 3, y + 3, 58, 27, rgb(142, 195, 204));
      const auto& preview = previewCards_[slot];
      for (int sy = 0; sy < 12; ++sy) for (int sx = 0; sx < 17; ++sx) {
        const uint16_t color = preview.pixel(frame, sx, sy);
        if (color == FlappyAssets::kTransparent) continue;
        const uint8_t alpha = preview.alpha(frame, sx, sy);
        if (!alpha) continue;
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
          uint16_t& target = pixels_[(y + 5 + sy * 2 + dy) * PixelWidth + x + 15 + sx * 2 + dx];
          target = alpha == 255 ? color : blend(color, target, alpha);
        }
      }
      if (index == app.settings.skinIndex) menuText("*", x + 54, y + 3, 1, rgb(30, 36, 46), 1);
      char label[11], continuation[11];
      skinLabelLines(app.menuLabel(index), label, continuation);
      const int labelY = y + (*continuation ? 32 : 38);
      menuText(label, x + (64 - MenuFont::textWidth(label)) / 2, labelY, 1, BittyUi::Text, 10);
      if (*continuation) menuText(continuation, x + (64 - MenuFont::textWidth(continuation)) / 2,
                                  y + 45, 1, BittyUi::Text, 10);
    }
    const bool back = app.menu.selection == count;
    rect(6, 230, 132, 16, back ? gold : card);
    menuCentered("Back", 232, 1, back ? BittyUi::Black : BittyUi::Text);
  }
  void drawGamePicker(const AppModel& app, const RenderOptions& opt) {
    rect(2, 10, 140, 233, BittyUi::Black);
    BittyUi::header(pixels_,PixelWidth,PixelHeight,opt.battery);
    menuCentered(app.menuTitle(), 34, 1, BittyUi::Text);
    const unsigned first = (app.menu.selection / 6) * 6;
    const unsigned count = app.menuItemCount();
    char page[8]; snprintf(page, sizeof(page), "%u/%u", first / 6 + 1, (count + 5) / 6);
    menuText(page, 116, 34, 1, BittyUi::Muted, 3);
    for (unsigned tile = 0; tile < 6 && first + tile < count; ++tile) {
      const unsigned index = first + tile;
      const bool selected = index == app.menu.selection, back = index + 1 == count;
      const bool available = back || app.gameChoiceAvailable(index);
      const int x = 8 + int(tile % 2) * 66, y = 54 + int(tile / 2) * 54;
      const uint16_t fill = selected ? BittyUi::Amber : BittyUi::Panel;
      const uint16_t ink = selected ? BittyUi::Black : available ? BittyUi::Text : BittyUi::Muted;
      rect(x, y, 62, 50, fill);
      const int cx = x + 30, cy = y + 13;
      const auto title = app.gameChoiceTitle(index);
      if (back) { menuText("<", cx - 3, cy - 5, 1, ink, 1); }
      else if (title == GameTitle::FlappyBird) {
        rect(cx - 9, cy - 5, 15, 10, ink); rect(cx - 12, cy, 7, 3, ink);
        rect(cx + 6, cy, 5, 3, ink); rect(cx + 2, cy - 3, 2, 2, fill);
      } else if (title == GameTitle::Tetris) {
        for (int i = 0; i < 4; ++i) rect(cx - 9 + (i ? (i - 1) * 6 : 6), cy - 5 + (i ? 6 : 0), 5, 5, ink);
      } else if (title == GameTitle::Galaga) {
        rect(cx - 2, cy - 7, 4, 12, ink); rect(cx - 9, cy, 18, 4, ink);
        rect(cx - 7, cy - 4, 2, 9, ink); rect(cx + 5, cy - 4, 2, 9, ink);
      } else if (title == GameTitle::Arkanoid) {
        rect(cx - 12, cy + 5, 24, 3, ink); rect(cx + 3, cy - 4, 3, 3, ink);
        for (int i = 0; i < 4; ++i) rect(cx - 13 + i * 7, cy - 10, 6, 3, ink);
      } else if (title == GameTitle::JetpackJoyride) {
        rect(cx - 3, cy - 7, 6, 5, ink); rect(cx - 5, cy - 1, 10, 8, ink);
        rect(cx - 10, cy, 4, 6, ink); rect(cx - 10, cy + 7, 3, 4, ink);
      } else if (title == GameTitle::GeometryDash) {
        rect(cx - 11, cy - 6, 12, 12, ink); rect(cx - 8, cy - 3, 2, 2, fill);
        rect(cx - 3, cy - 3, 2, 2, fill); rect(cx + 7, cy + 2, 8, 4, ink);
        rect(cx + 9, cy - 2, 4, 4, ink); rect(cx + 10, cy - 5, 2, 3, ink);
      } else {
        rect(cx - 14, cy - 5, 28, 12, ink); rect(cx - 9, cy - 3, 3, 8, fill);
        rect(cx - 12, cy, 9, 2, fill); rect(cx + 4, cy + 1, 3, 3, fill);
        rect(cx + 9, cy - 2, 3, 3, fill);
      }
      const char* label = !available && title == GameTitle::GeometryDash ? "Geometry Dash" :
          !available && title == GameTitle::Tetris ? "Tetris" :
          !available && title == GameTitle::Nes ? "NES" : app.menuLabel(index);
      // Two static lines preserve a native six-pixel advance without a marquee.
      char line[11] = {}; unsigned n = 0;
      while (label[n] && n < 10) { line[n] = label[n]; ++n; }
      unsigned split = n;
      if (label[n]) for (unsigned j = n; j > 0; --j) if (line[j - 1] == ' ') { split = j - 1; break; }
      line[split] = 0;
      menuText(line, x + 3, y + 26, 1, ink, 10);
      if (label[split]) {
        const char* rest = label + split; if (*rest == ' ') ++rest;
        menuText(rest, x + 3, y + 37, 1, ink, 10);
      }
      if (!back && *app.menuValue(index)) rect(x + 54, y + 3, 4, 4, ink);
    }
    const bool available = unsigned(app.menu.selection) + 1 == count || app.gameChoiceAvailable(app.menu.selection);
    menuCentered(available ? "M5 SELECT  RIGHT NEXT" : "ADD ROM VIA USB", 217, 1, BittyUi::Muted);
    menuCentered("HOLD RIGHT TO EXIT", 231, 1, BittyUi::Muted);
  }

  void drawMenu(const AppModel& app, const RenderOptions& opt) {
    rect(0,0,PixelWidth,PixelHeight,BittyUi::Black);
    if (app.menu.page == MenuPage::ChangeGame || app.menu.page == MenuPage::BootGame) {
      drawGamePicker(app,opt);
      return;
    }
    rect(2,10,140,233,BittyUi::Black);
    BittyUi::header(pixels_,PixelWidth,PixelHeight,opt.battery);
    const int scale = 1;
    menuCentered(app.menuTitle(), 34, 1, BittyUi::Text);
    const char* val = app.menuValue(app.menu.selection);
    if (val && *val) menuCentered(val, 51, 1, BittyUi::Glow);
    const int visible = opt.uiScale >= 3 ? 5 : 7;
    const int rowHeight = opt.uiScale >= 3 ? 27 : 20;
    const int count = app.menuItemCount();
    int first = int(app.menu.selection) - visible + 1;
    if (first < 0) first = 0;
    const int maxChars = 128 / MenuFont::Advance;
    for (int row = 0; row < visible && first + row < count; ++row) {
      const int index = first + row, y = 73 + row * rowHeight;
      const bool selected = index == app.menu.selection;
      if (selected) rect(5, y - 4, 134, rowHeight - 2, BittyUi::Amber);
      const char* label = app.menuLabel(uint8_t(index));
      const int length = int(strlen(label));
      if (selected && length > maxChars) {
        const unsigned span = unsigned(length - maxChars + 4);
        unsigned offset = (opt.nowMs / 400) % span;
        if (offset > unsigned(length - maxChars)) offset = unsigned(length - maxChars);
        label += offset;
      }
      const uint16_t color = selected ? BittyUi::Black : BittyUi::Text;
      menuText(label, 9, y, scale, color, maxChars);
      if (!selected && length > maxChars) {
        rect(9 + (maxChars - 2) * MenuFont::Advance, y,
             2 * MenuFont::Advance, MenuFont::Height, BittyUi::Black);
        menuText("..", 9 + (maxChars - 2) * MenuFont::Advance, y, scale, color, 2);
      }
    }
    menuCentered("RIGHT NEXT  M5 SELECT", 217, 1, BittyUi::Muted);
    menuCentered("HOLD RIGHT TO EXIT", 231, 1, BittyUi::Muted);
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
