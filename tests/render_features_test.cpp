#include "../firmware/flappy/render.h"
#include "../firmware/flappy/skins.h"

#include <assert.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

constexpr size_t GuardSize = 64;
constexpr uint16_t GuardColor = 0xa55a;
constexpr size_t FramePixels = flappy::PixelWidth * flappy::PixelHeight;

struct GuardedPixels {
  std::vector<uint16_t> storage;
  size_t count;

  explicit GuardedPixels(size_t pixels)
      : storage(pixels + GuardSize * 2, GuardColor), count(pixels) {
    std::fill(data(), data() + count, uint16_t(0x1234));
  }

  uint16_t* data() { return storage.data() + GuardSize; }
  const uint16_t* data() const { return storage.data() + GuardSize; }

  void check() const {
    for (size_t i = 0; i < GuardSize; ++i) {
      assert(storage[i] == GuardColor);
      assert(storage[GuardSize + count + i] == GuardColor);
    }
  }

  std::vector<uint16_t> image() const { return std::vector<uint16_t>(data(), data() + count); }
};

static size_t differentPixels(const std::vector<uint16_t>& first,
                              const std::vector<uint16_t>& second) {
  assert(first.size() == second.size());
  size_t count = 0;
  for (size_t i = 0; i < first.size(); ++i) count += first[i] != second[i];
  return count;
}

static std::vector<uint16_t> frame(const flappy::AppModel& app,
                                   const flappy::RenderOptions& options = flappy::RenderOptions()) {
  GuardedPixels pixels(FramePixels);
  flappy::Renderer renderer(pixels.data());
  renderer.draw(app, options);
  assert(renderer.pixels() == pixels.data());
  pixels.check();
  return pixels.image();
}

static void preparePlaying(flappy::AppModel& app, flappy::Mode mode, uint8_t skin) {
  app.setSkinCount(flappy::Skins::count());
  app.setMode(mode);
  app.setSkin(skin);
  app.game.state = flappy::State::Playing;
  app.game.birdY = 150;
  app.game.rotation = 0;
  app.game.birdFrame = 0;
  app.game.score = 0;
  app.game.groundOffset = 0;
  for (int i = 0; i < flappy::PipeCount; ++i) app.game.pipes[i].x = 1000 + i * 144;
}

static uint16_t composite(uint16_t top, uint16_t base, uint8_t alpha) {
  const unsigned inverse = 255 - alpha;
  const unsigned r = (((top >> 11) * alpha + (base >> 11) * inverse + 127) / 255) << 11;
  const unsigned g = ((((top >> 5) & 63) * alpha + ((base >> 5) & 63) * inverse + 127) / 255) << 5;
  const unsigned b = ((top & 31) * alpha + (base & 31) * inverse + 127) / 255;
  return uint16_t(r | g | b);
}

static void colorTransforms() {
  assert(flappy::oppositeColor(flappy::rgb(255, 0, 0), false) == flappy::rgb(0, 255, 255));
  assert(flappy::oppositeColor(flappy::rgb(0, 255, 0), false) == flappy::rgb(255, 0, 255));
  assert(flappy::oppositeColor(flappy::rgb(0, 0, 255), false) == flappy::rgb(255, 255, 0));
  assert(flappy::oppositeColor(0, true) == 0xffff);
  assert(flappy::oppositeColor(0xffff, true) == 0);
  const uint16_t gray = flappy::rgb(24, 24, 24);
  assert(flappy::oppositeColor(gray, true) != gray);
  const uint16_t source = flappy::rgb(184, 96, 48);
  const uint16_t twice = flappy::oppositeColor(flappy::oppositeColor(source, false), false);
  assert(std::abs(int(source >> 11) - int(twice >> 11)) <= 1);
  assert(std::abs(int((source >> 5) & 63) - int((twice >> 5) & 63)) <= 1);
  assert(std::abs(int(source & 31) - int(twice & 31)) <= 1);
}

static void messageScreensClearArtAndKeepTextInsideNativeCrop() {
  GuardedPixels pixels(FramePixels);
  flappy::Renderer renderer(pixels.data());
  renderer.drawMessage("TETRIS", "STARTING GAME");
  const auto baseline = pixels.image();
  for (uint8_t skin = 0; skin < flappy::Skins::BuiltinCount; ++skin) {
    flappy::AppModel app(53);
    preparePlaying(app, flappy::Mode::Original, skin);
    renderer.draw(app);
    renderer.drawMessage("TETRIS", "STARTING GAME");
    assert(pixels.image() == baseline);
    pixels.check();
  }

  const uint16_t gold = flappy::rgb(255, 220, 116);
  const uint16_t panel = flappy::rgb(26, 35, 49);
  const auto checkPlainPanel = [&]() {
    for (int y = 0; y < flappy::PixelHeight; ++y) for (int x = 0; x < flappy::PixelWidth; ++x) {
      const uint16_t color = pixels.data()[size_t(y) * flappy::PixelWidth + x];
      if (x < 6 || x >= 138 || y < 92 || y >= 164) assert(color == 0);
      else assert(color == panel || color == gold || color == 0xffff);
    }
    pixels.check();
  };
  flappy::Viewport crop;
  crop.configure(135, 240);
  GuardedPixels output(size_t(crop.width) * crop.height);
  const auto checkText = [&](const char* label, int y, uint16_t color) {
    const int width = flappy::MenuFont::textWidth(label);
    assert(width <= 128);
    const int x = (flappy::PixelWidth - width) / 2;
    assert(x >= crop.sourceX && x + width <= crop.sourceX + crop.width);
    assert(y >= crop.sourceY && y + flappy::MenuFont::Height <= crop.sourceY + crop.height);
    for (size_t index = 0; label[index]; ++index) {
      unsigned ink = 0;
      for (int row = 0; row < flappy::MenuFont::Height; ++row) {
        const uint8_t bits = flappy::MenuFont::glyph(label[index])[row];
        for (int col = 0; col < flappy::MenuFont::Width; ++col) {
          const bool filled = (bits & (1 << (flappy::MenuFont::Width - 1 - col))) != 0;
          const int sourceX = x + int(index) * flappy::MenuFont::Advance + col;
          const uint16_t expected = filled ? color : panel;
          assert(pixels.data()[size_t(y + row) * flappy::PixelWidth + sourceX] == expected);
          assert(output.data()[size_t(y + row - crop.sourceY) * crop.width + sourceX - crop.sourceX] == expected);
          ink += filled;
        }
      }
      assert(label[index] == ' ' || ink > 0);
    }
  };
  for (const char* title : {"TETRIS", "MARIO", "TETRIS ZERO"}) {
    for (const char* detail : {"STARTING GAME", "GAME OVER"}) {
      renderer.drawMessage(title, detail);
      checkPlainPanel();
      crop.copy(pixels.data(), output.data());
      checkText(title, 108, gold);
      checkText(detail, 133, 0xffff);
      output.check();
    }
  }

  renderer.drawMessage(nullptr, nullptr);
  checkPlainPanel();
  for (int y = 93; y < 163; ++y) for (int x = 6; x < 138; ++x)
    assert(pixels.data()[size_t(y) * flappy::PixelWidth + x] == panel);
  const std::string longText(400, 'W');
  for (const char* title : {static_cast<const char*>(nullptr), "", longText.c_str()}) {
    for (const char* detail : {static_cast<const char*>(nullptr), "", longText.c_str()}) {
      std::fill(pixels.data(), pixels.data() + FramePixels, uint16_t(0xf81f));
      renderer.drawMessage(title, detail);
      checkPlainPanel();
    }
  }
}

static void landscapeMessageUsesNativeFontAndStaysInsideCanvas() {
  const int width = 240, height = 135;
  GuardedPixels pixels(size_t(width) * height);
  flappy::Renderer renderer(pixels.data());
  const uint16_t panel = flappy::rgb(26, 35, 49), gold = flappy::rgb(255, 220, 116);
  const char* title = "MARIO";
  const char* detail = "Starting game";
  renderer.drawMessageLandscape(title, detail);
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    uint16_t expected = x >= 8 && x < 232 && y >= 40 && y < 96 ? panel : 0;
    for (int line = 0; line < 2; ++line) {
      const char* text = line ? detail : title;
      const int left = (width - flappy::MenuFont::textWidth(text)) / 2;
      const int row = y - (line ? 73 : 49), col = x - left;
      if (row >= 0 && row < flappy::MenuFont::Height && col >= 0 &&
          col < flappy::MenuFont::textWidth(text)) {
        const uint8_t bits = flappy::MenuFont::glyph(text[col / flappy::MenuFont::Advance])[row];
        if (bits & (1u << (flappy::MenuFont::Width - 1 - col % flappy::MenuFont::Advance)))
          expected = line ? 0xffff : gold;
      }
    }
    assert(pixels.data()[y * width + x] == expected);
  }
  pixels.check();
  const std::string longText(400, 'W');
  for (const char* first : {static_cast<const char*>(nullptr), "", longText.c_str()}) {
    for (const char* second : {static_cast<const char*>(nullptr), "", longText.c_str()}) {
      renderer.drawMessageLandscape(first, second);
      pixels.check();
    }
  }
}

static void activeSkinMasksFollowPixels() {
  assert(flappy::Skins::count() >= 12);
  flappy::Skins skins;
  for (uint8_t index = 0; index < flappy::Skins::count(); ++index) {
    skins.select(index);
    assert(skins.selected == index);
    assert(flappy::Skins::name(index) && std::strlen(flappy::Skins::name(index)));
    assert(skins.background().width == flappy::PixelWidth);
    assert(skins.background().height == flappy::PixelHeight);
    for (int frame = 0; frame < 3; ++frame) {
      unsigned opaque = 0;
      for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 34; ++x) {
          const bool maskOpaque = (skins.masks()[frame][y] & (uint64_t(1) << x)) != 0;
          const bool pixelOpaque = skins.birdPixel(frame, x / 2, y / 2) != FlappyAssets::kTransparent;
          assert(maskOpaque == pixelOpaque);
          opaque += maskOpaque;
        }
        assert((skins.masks()[frame][y] >> 34) == 0);
      }
      assert(opaque > 0);
    }
  }
  skins.select(6);
  assert(skins.monochrome);
  for (uint8_t index = 10; index <= 11; ++index) {
    skins.select(index);
    bool animated = false;
    for (int y = 0; y < 24; ++y) animated |= skins.masks()[0][y] != skins.masks()[1][y];
    assert(animated);
  }
  skins.select(UINT8_MAX);
  assert(skins.selected == 0);
}

static void compactPreviewsMatchSkinColorsAndAlpha() {
  static_assert(sizeof(flappy::Skins::BirdPreview) <= 260, "Keep each picker preview below 260 bytes");
  static_assert(sizeof(flappy::Renderer) <= 3000, "Keep the renderer below 3 KiB");
  flappy::Skins skins;
  flappy::Skins::BirdPreview preview;
  assert(preview.alpha(0, -1, 0) == 0 && preview.alpha(0, 0, 12) == 0);
  bool sawPartialAlpha = false;
  for (uint8_t index = 0; index < flappy::Skins::count(); ++index) {
    skins.select(index);
    preview.capture(skins);
    for (unsigned color = 0; color <= 0xffff; ++color)
      assert(preview.color(uint16_t(color)) == skins.palette(uint16_t(color)));
    for (int animation = 0; animation < 3; ++animation) {
      const auto& asset = skins.bird(animation);
      for (int y = 0; y < 12; ++y) for (int x = 0; x < 17; ++x) {
        assert(preview.pixel(animation, x, y) == skins.birdPixel(animation, x, y));
        const uint8_t alpha = index == 10 || index == 11 || !asset.alpha ? 255 : asset.alpha[y * asset.width + x];
        assert(preview.alpha(animation, x, y) == alpha);
        sawPartialAlpha |= alpha > 0 && alpha < 255;
      }
    }
    const uint16_t retained = preview.pixel(1, 8, 6);
    skins.select(uint8_t((index + 1) % flappy::Skins::count()));
    assert(preview.pixel(1, 8, 6) == retained);
  }
  assert(sawPartialAlpha);
}

static void skinPickerGridAnimationPagingAndBack() {
  const uint16_t gold = flappy::rgb(255, 220, 116), outline = flappy::rgb(77, 104, 121);
  const uint16_t previewBackground = flappy::rgb(142, 195, 204), card = flappy::rgb(26, 35, 49);
  std::vector<const char*> names;
  for (uint8_t index = 0; index < flappy::Skins::count(); ++index) names.push_back(flappy::Skins::name(index));
  flappy::AppModel app(35);
  preparePlaying(app, flappy::Mode::Original, 0);
  app.setSkinNames(names.data(), uint8_t(names.size()));
  app.openMenu(flappy::MenuPage::Skins);
  GuardedPixels pixels(FramePixels);
  flappy::Renderer renderer(pixels.data());
  const auto* activeMasks = renderer.birdMasks();
  flappy::RenderOptions options;
  const uint8_t sequence[] = {0, 1, 2, 1, 0};
  flappy::Skins expected;
  for (uint8_t selected = 0; selected <= app.skinCount(); ++selected) {
    app.menu.selection = selected;
    const int first = (std::min(unsigned(selected), unsigned(app.skinCount() - 1)) / 6) * 6;
    for (uint8_t phase = 0; phase < 5; ++phase) {
      options.nowMs = phase * 120;
      renderer.draw(app, options);
      pixels.check();
      assert(renderer.birdMasks() == activeMasks && app.settings.skinIndex == 0);
      for (int slot = 0; slot < 6 && first + slot < app.skinCount(); ++slot) {
        const uint8_t index = uint8_t(first + slot);
        const int left = 6 + (slot % 2) * 68, top = 42 + (slot / 2) * 62;
        assert(pixels.data()[top * flappy::PixelWidth + left] == (selected == index ? gold : outline));
        expected.select(index);
        const uint8_t animation = sequence[phase];
        const auto& asset = expected.bird(animation);
        for (int y = 0; y < 12; ++y) for (int x = 0; x < 17; ++x) {
          uint16_t color = expected.birdPixel(animation, x, y);
          if (color == FlappyAssets::kTransparent) color = previewBackground;
          else {
            const uint8_t alpha = index == 10 || index == 11 || !asset.alpha ? 255 : asset.alpha[y * asset.width + x];
            color = composite(color, previewBackground, alpha);
          }
          for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx)
            assert(pixels.data()[(top + 5 + y * 2 + dy) * flappy::PixelWidth + left + 15 + x * 2 + dx] == color);
        }
      }
      assert(pixels.data()[230 * flappy::PixelWidth + 6] == (selected == app.skinCount() ? gold : card));
      assert(pixels.data()[245 * flappy::PixelWidth + 137] == (selected == app.skinCount() ? gold : card));
    }
  }
  app.menu.selection = 1;
  options.nowMs = 120;
  renderer.draw(app, options);
  const auto opaquePage = pixels.image();
  app.game.state = flappy::State::GameOver;
  app.game.birdY = 400;
  app.game.score = UINT16_MAX;
  app.settings.mode = flappy::Mode::Arcade;
  renderer.draw(app, options);
  assert(pixels.image() == opaquePage);
  app.closeMenu();
  renderer.draw(app, options);
  assert(pixels.image() == frame(app, options));
  app.setSkinCount(1);
  app.openMenu(flappy::MenuPage::Skins);
  app.menu.selection = 0;
  renderer.draw(app, options);
  assert(pixels.data()[42 * flappy::PixelWidth + 6] == gold);
  assert(pixels.data()[42 * flappy::PixelWidth + 74] == flappy::rgb(10, 17, 26));
  app.menu.selection = 1;
  renderer.draw(app, options);
  assert(pixels.data()[230 * flappy::PixelWidth + 6] == gold);
  pixels.check();
}

static void axisAlignedBirdPixelsMatchSource() {
  bool partialAlphaCovered = false;
  for (flappy::Mode mode : {flappy::Mode::Original, flappy::Mode::Big}) {
    for (uint8_t skin = 0; skin < flappy::Skins::count(); ++skin) {
      flappy::AppModel app(2);
      preparePlaying(app, mode, skin);
      flappy::Skins skins;
      skins.select(skin);
      for (uint8_t animation = 0; animation < 3; ++animation) {
        app.game.birdFrame = animation;
        const int scale = app.game.rules().spriteScale;
        const int left = app.game.birdX() / 2;
        const int top = int(app.game.birdY / 2);
        app.game.birdY = -1000;
        const std::vector<uint16_t> background = frame(app);
        app.game.birdY = float(top * 2);
        for (bool tinted : {false, true}) {
          flappy::RenderOptions options;
          options.localTint = tinted;
          const std::vector<uint16_t> image = frame(app, options);
          for (int y = 0; y < 12 * scale; ++y) {
            for (int x = 0; x < 17 * scale; ++x) {
              const int sourceX = x / scale, sourceY = y / scale;
              const size_t position = size_t(top + y) * flappy::PixelWidth + left + x;
              uint16_t color = skins.birdPixel(animation, sourceX, sourceY);
              uint16_t expected = background[position];
              if (color != FlappyAssets::kTransparent) {
                if (tinted) color = flappy::oppositeColor(color, skins.monochrome);
                uint8_t alpha = 255;
                if (skin != 10 && skin != 11) {
                  const auto& asset = skins.bird(animation);
                  if (asset.alpha) alpha = asset.alpha[sourceY * asset.width + sourceX];
                }
                partialAlphaCovered |= alpha > 0 && alpha < 255;
                expected = composite(color, expected, alpha);
              }
              assert(image[position] == expected);
            }
          }
          if (skin == 10 || skin == 11) assert(differentPixels(image, background) > 0);
        }
      }
    }
  }
  assert(partialAlphaCovered);
}

static void bigModeDoublesEachBirdPixel() {
  size_t changed[2] = {};
  unsigned index = 0;
  for (flappy::Mode mode : {flappy::Mode::Original, flappy::Mode::Big}) {
    flappy::AppModel app(19);
    preparePlaying(app, mode, 10);
    const auto visible = frame(app);
    app.game.birdY = -1000;
    const auto background = frame(app);
    changed[index++] = differentPixels(visible, background);
  }
  assert(changed[0] > 0);
  assert(changed[1] == changed[0] * 4);

  int pipeWidths[2] = {};
  index = 0;
  for (flappy::Mode mode : {flappy::Mode::Original, flappy::Mode::Big}) {
    flappy::AppModel app(23);
    preparePlaying(app, mode, 10);
    app.game.birdY = -1000;
    const auto background = frame(app);
    app.game.pipes[0].x = 100;
    app.game.pipes[0].gapY = 140;
    const auto withPipe = frame(app);
    int first = flappy::PixelWidth, last = -1;
    for (int y = 50; y < 70; ++y) for (int x = 0; x < flappy::PixelWidth; ++x) {
      const size_t position = size_t(y) * flappy::PixelWidth + x;
      if (background[position] != withPipe[position]) {
        first = std::min(first, x);
        last = std::max(last, x);
      }
    }
    assert(last >= first);
    pipeWidths[index++] = last - first + 1;
  }
  assert(pipeWidths[1] == pipeWidths[0] * 2);
}

static void allModesAndSkinsRespectBufferBounds() {
  for (uint8_t mode = 0; mode < uint8_t(flappy::Mode::Count); ++mode) {
    for (uint8_t skin = 0; skin < flappy::Skins::count(); ++skin) {
      flappy::AppModel app(12);
      preparePlaying(app, flappy::Mode(mode), skin);
      app.game.score = UINT16_MAX;
      app.game.bestScore = UINT16_MAX;
      app.game.newRecord = true;
      app.game.pipes[0].x = -40;
      app.game.pipes[0].gapY = 70;
      app.game.pipes[1].x = 120;
      app.game.pipes[1].gapY = 200;
      app.game.pipes[2].x = 280;
      app.game.pipes[2].gapY = 90;
      for (uint8_t state = 0; state < 4; ++state) {
        app.game.state = flappy::State(state);
        app.game.stateFrames = state == uint8_t(flappy::State::GameOver) ? 60 : 0;
        app.game.birdFrame = state % 3;
        app.game.rotation = state == uint8_t(flappy::State::Dying) ? -90 : 20;
        for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
          flappy::RenderOptions options;
          options.uiScale = uiScale;
          options.nowMs = 1234567;
          options.remoteVisible = true;
          options.remoteY = state ? 500 : -40;
          options.remoteRotation = state ? -90 : 45;
          options.remoteFrame = state % 3;
          options.remoteScore = UINT16_MAX;
          options.localReady = state % 2;
          options.remoteReady = state % 2 == 0;
          options.multipleHosts = true;
          options.coopStatus = "Waiting for other player";
          options.peerName = "M5Flap-123ABC";
          assert(frame(app, options).size() == FramePixels);
        }
      }
    }
  }
}

static void menusScrollAndRenderEverySelection() {
  const char* names[flappy::Skins::BuiltinCount];
  for (uint8_t i = 0; i < flappy::Skins::BuiltinCount; ++i) names[i] = flappy::Skins::name(i);
  flappy::AppModel app(18);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.setSkinNames(names, flappy::Skins::BuiltinCount);
  const auto noMenu = frame(app);
  for (uint8_t page = uint8_t(flappy::MenuPage::Home); page <= uint8_t(flappy::MenuPage::GeometryLevel); ++page) {
    app.openMenu(flappy::MenuPage(page));
    const uint8_t count = app.menuItemCount();
    assert(count > 1);
    for (uint8_t selected = 0; selected < count; ++selected) {
      app.menu.selection = selected;
      for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
        flappy::RenderOptions options;
        options.uiScale = uiScale;
        for (uint32_t now : {uint32_t(0), uint32_t(4000), uint32_t(72000)}) {
          options.nowMs = now;
          assert(differentPixels(noMenu, frame(app, options)) > 1000);
        }
      }
    }
    // Every submenu exposes Back as its last item, including scrolled skins.
    assert(std::strcmp(app.menuLabel(count - 1), "Back") == 0);
  }
  app.openMenu(flappy::MenuPage::Skins);
  app.menu.selection = 10;
  flappy::RenderOptions options;
  options.uiScale = 3;
  const auto titleStart = frame(app, options);
  options.nowMs = 2000;
  assert(differentPixels(titleStart, frame(app, options)) == 0);
  names[10] = "A deliberately long custom skin name";
  options.nowMs = 0;
  const auto customStart = frame(app, options);
  options.nowMs = 2000;
  assert(differentPixels(customStart, frame(app, options)) > 0);
  app.closeMenu();
}

static void actualMenuLabelsFitAndSurviveNativeCrop() {
  const char* names[flappy::Skins::BuiltinCount];
  for (uint8_t i = 0; i < flappy::Skins::BuiltinCount; ++i) names[i] = flappy::Skins::name(i);
  flappy::AppModel app(41);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.setSkinNames(names, flappy::Skins::BuiltinCount);
  app.setNesAvailable(true);
  app.setTetrisAvailable(true);
  app.setTetrisZeroAvailable(true);
  app.setGeometryDashAvailable(true);
  for (uint8_t game = 0; game < flappy::GameTitleCount; ++game) {
    assert(app.setCurrentGame(flappy::GameTitle(game)));
    for (uint8_t page = uint8_t(flappy::MenuPage::Home);
         page <= uint8_t(flappy::MenuPage::GeometryLevel); ++page) {
      app.openMenu(flappy::MenuPage(page));
      assert(flappy::MenuFont::textWidth(app.menuTitle()) <= 128);
      for (uint8_t index = 0; index < app.menuItemCount(); ++index) {
        assert(flappy::MenuFont::textWidth(app.menuLabel(index)) <= 128);
        assert(flappy::MenuFont::textWidth(app.menuValue(index)) <= 128);
      }
    }
  }
  assert(flappy::MenuFont::textWidth("MENU NEXT  A SELECT", 1) <= 128);
  assert(flappy::MenuFont::textWidth("HOLD MENU TO EXIT", 1) <= 128);

  app.setCurrentGame(flappy::GameTitle::FlappyBird);
  app.openMenu(flappy::MenuPage::Skins);
  app.menu.selection = 10;
  const auto withLastLetter = frame(app);
  names[10] = "Toast on the Ru ";
  const auto withoutLastLetter = frame(app);
  flappy::Viewport crop;
  crop.configure(135, 240);
  std::vector<uint16_t> first(size_t(crop.width) * crop.height), second(first.size());
  crop.copy(withLastLetter.data(), first.data());
  crop.copy(withoutLastLetter.data(), second.data());
  const size_t glyphPixels = differentPixels(withLastLetter, withoutLastLetter);
  assert(glyphPixels > 0);
  assert(differentPixels(first, second) == glyphPixels);

  // Cartridge titles and controller status come from runtime sources and may scroll.
  const char* longRuntimeText = "A runtime title or status longer than sixteen characters";
  app.setNesTitle(longRuntimeText);
  app.setTetrisTitle(longRuntimeText);
  app.setTetrisZeroTitle(longRuntimeText);
  app.setControllerStatus(longRuntimeText);
  assert(flappy::MenuFont::textWidth(app.gameLabel(flappy::GameTitle::Nes)) > 128);
  assert(flappy::MenuFont::textWidth(app.tetrisTitle()) > 128);
  assert(flappy::MenuFont::textWidth(app.tetrisZeroTitle()) > 128);
  for (flappy::MenuPage page : {flappy::MenuPage::BootGame, flappy::MenuPage::ChangeGame,
                               flappy::MenuPage::Controller}) {
    app.openMenu(page);
    for (uint8_t index = 0; index < app.menuItemCount(); ++index) {
      app.menu.selection = index;
      assert(frame(app).size() == FramePixels);
    }
  }
}

static void volumeMenuShowsLevelsAndCurrentValue() {
  flappy::AppModel app(23);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.openMenu(flappy::MenuPage::Home);
  app.menu.selection = 4;
  app.setVolume(0);
  const auto mutedHome = frame(app);
  app.setVolume(5);
  assert(differentPixels(mutedHome, frame(app)) > 0);
  app.openMenu(flappy::MenuPage::Volume);
  assert(app.menuItemCount() == 7);
  assert(std::strcmp(app.menuLabel(6), "Back") == 0);
  for (uint8_t index = 0; index < 6; ++index) {
    app.setVolume(index);
    app.menu.selection = index;
    for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
      flappy::RenderOptions options;
      options.uiScale = uiScale;
      const auto active = frame(app, options);
      app.setVolume(uint8_t((index + 1) % 6));
      assert(differentPixels(active, frame(app, options)) > 0);
      app.setVolume(index);
    }
  }
}

static void sleepMenuShowsChoicesAndCurrentValue() {
  flappy::AppModel app(237);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.openMenu();
  app.menu.selection = 6;
  assert(std::strcmp(app.menuLabel(6), "Sleep") == 0);
  const auto defaultHome = frame(app);
  app.setSleep(5);
  assert(differentPixels(defaultHome, frame(app)) > 0);
  app.openMenu(flappy::MenuPage::Sleep);
  assert(app.menuItemCount() == 7 && std::strcmp(app.menuLabel(6), "Back") == 0);
  for (uint8_t index = 0; index < flappy::AppModel::SleepChoiceCount; ++index) {
    app.setSleep(index);
    app.menu.selection = index;
    for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
      flappy::RenderOptions options;
      options.uiScale = uiScale;
      const auto selected = frame(app, options);
      app.setSleep(uint8_t((index + 1) % flappy::AppModel::SleepChoiceCount));
      assert(differentPixels(selected, frame(app, options)) > 0);
      app.setSleep(index);
    }
  }
}

static void launcherMenusShowAvailabilityAndTitles() {
  flappy::AppModel app(27);
  preparePlaying(app, flappy::Mode::Original, 10);
  for (flappy::MenuPage page : {flappy::MenuPage::BootGame, flappy::MenuPage::ChangeGame}) {
    app.openMenu(page);
    app.menu.selection = 1;
    const auto missing = frame(app);
    app.setNesAvailable(true);
    assert(differentPixels(missing, frame(app)) > 0);
    app.setNesTitle("Super Mario Bros.");
    assert(differentPixels(missing, frame(app)) > 0);
    for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
      flappy::RenderOptions options;
      options.uiScale = uiScale;
      for (uint8_t selected = 0; selected < app.menuItemCount(); ++selected) {
        app.menu.selection = selected;
        assert(frame(app, options).size() == FramePixels);
      }
    }
    app.setNesAvailable(false);
    app.setNesTitle(nullptr);
  }
  for (flappy::MenuPage page : {flappy::MenuPage::BootGame, flappy::MenuPage::ChangeGame}) {
    app.openMenu(page);
    app.menu.selection = 2;
    const auto missing = frame(app);
    app.setTetrisAvailable(true);
    assert(differentPixels(missing, frame(app)) > 0);
    app.setTetrisTitle("TetrisGYM");
    assert(differentPixels(missing, frame(app)) > 0);
    app.setTetrisAvailable(false);
    app.setTetrisTitle(nullptr);
  }
  app.setNesAvailable(true);
  const std::string longTitle(200, 'N');
  app.setNesTitle(longTitle.c_str());
  assert(app.setCurrentGame(flappy::GameTitle::Nes));
  app.openMenu();
  assert(app.menuItemCount() == 8);
  for (uint8_t selected = 0; selected < app.menuItemCount(); ++selected) {
    app.menu.selection = selected;
    flappy::RenderOptions options;
    options.uiScale = 3;
    for (uint32_t now : {uint32_t(0), uint32_t(4000), uint32_t(72000)}) {
      options.nowMs = now;
      assert(frame(app, options).size() == FramePixels);
    }
  }
}

static void controllerMenuShowsStatusAndActions() {
  flappy::AppModel app(29);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.openMenu();
  app.menu.selection = 5;
  const auto off = frame(app);
  app.setControllerStatus("Searching");
  assert(differentPixels(off, frame(app)) > 0);
  app.openMenu(flappy::MenuPage::Controller);
  for (uint8_t selected = 0; selected < app.menuItemCount(); ++selected) {
    app.menu.selection = selected;
    for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
      flappy::RenderOptions options;
      options.uiScale = uiScale;
      options.nowMs = 72000;
      assert(frame(app, options).size() == FramePixels);
    }
  }
}

static void gameSettingsReflectEachSelectedGame() {
  flappy::AppModel app(31);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.setNesAvailable(true);
  app.setTetrisAvailable(true);
  app.setTetrisZeroAvailable(true);
  app.setGeometryDashAvailable(true);
  std::vector<uint16_t> previous;
  for (flappy::GameTitle game : {flappy::GameTitle::FlappyBird, flappy::GameTitle::Nes,
                                flappy::GameTitle::Tetris, flappy::GameTitle::TetrisZero, flappy::GameTitle::GeometryDash}) {
    assert(app.setCurrentGame(game));
    app.openMenu(flappy::MenuPage::GameSettings);
    const uint8_t count = app.menuItemCount();
    assert(count == (game == flappy::GameTitle::FlappyBird ? 5 : game == flappy::GameTitle::Tetris ? 9 :
                     game == flappy::GameTitle::TetrisZero ? 8 : game == flappy::GameTitle::GeometryDash ? 3 : 2));
    const auto current = frame(app);
    if (!previous.empty()) assert(differentPixels(previous, current) > 0);
    previous = current;
    for (uint8_t selected = 0; selected < count; ++selected) {
      app.menu.selection = selected;
      for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
        flappy::RenderOptions options;
        options.uiScale = uiScale;
        assert(frame(app, options).size() == FramePixels);
      }
    }
    assert(std::strcmp(app.menuLabel(count - 1), "Back") == 0);
  }
}

static void geometryLevelsFitNativeFontAndScrollVertically() {
  flappy::AppModel app(39);
  app.setGeometryDashAvailable(true);
  assert(app.setCurrentGame(flappy::GameTitle::GeometryDash));
  for (uint8_t level = 0; level < flappy::GeometryLevelCount; ++level) {
    app.setGeometryLevel(level);
    app.openMenu(flappy::MenuPage::GameSettings);
    assert(std::strcmp(app.menuLabel(0), "Level") == 0);
    assert(std::strcmp(app.menuValue(0), flappy::geometryLevelLabel(level)) == 0);
    assert(flappy::MenuFont::textWidth(app.menuValue(0)) <= 128);
    app.openMenu(flappy::MenuPage::GeometryLevel);
    assert(app.menu.selection == level);
    assert(flappy::MenuFont::textWidth(app.menuLabel(level)) <= 128);
    for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
      flappy::RenderOptions options;
      options.uiScale = uiScale;
      const auto initial = frame(app, options);
      options.nowMs = 4000;
      assert(differentPixels(initial, frame(app, options)) == 0);
      if (level + 1 < flappy::GeometryLevelCount) {
        app.menu.selection = level + 1;
        assert(differentPixels(initial, frame(app, options)) > 0);
        app.menu.selection = level;
      }
    }
  }
  app.openMenu(flappy::MenuPage::GeometryLevel);
  app.menu.selection = flappy::GeometryLevelCount;
  assert(std::strcmp(app.menuLabel(app.menu.selection), "Back") == 0);
  for (uint8_t uiScale : {uint8_t(2), uint8_t(3)}) {
    flappy::RenderOptions options;
    options.uiScale = uiScale;
    assert(frame(app, options).size() == FramePixels);
  }
}

static void ghostRotationsAndColorAssignment() {
  for (uint8_t skin = 0; skin < flappy::Skins::count(); ++skin) {
    flappy::AppModel app(9);
    preparePlaying(app, flappy::Mode::Coop, skin);
    app.game.birdY = 100;
    flappy::RenderOptions options;
    options.remoteVisible = true;
    options.remoteY = 250;
    for (int8_t angle : {int8_t(-90), int8_t(-45), int8_t(0), int8_t(20), int8_t(45)}) {
      options.remoteRotation = angle;
      options.remoteFrame = uint8_t(unsigned(angle + 90) % 3);
      options.remoteTint = false;
      const auto original = frame(app, options);
      options.remoteTint = true;
      const auto opponent = frame(app, options);
      assert(differentPixels(original, opponent) > 0);
      options.remoteVisible = false;
      assert(differentPixels(opponent, frame(app, options)) > 0);
      options.remoteVisible = true;
    }
  }
}

static void clockDigitsFollowHourMinuteAndFormat() {
  flappy::AppModel app(5);
  preparePlaying(app, flappy::Mode::Clock, 10);
  flappy::RenderOptions options;
  app.setClock(1, 1, 0);
  const auto early = frame(app, options);
  app.setClock(2, 1, 0);
  const auto hourChanged = frame(app, options);
  assert(differentPixels(early, hourChanged) > 0);
  app.setClock(2, 2, 0);
  const auto minuteChanged = frame(app, options);
  assert(differentPixels(hourChanged, minuteChanged) > 0);
  options.nowMs = 60000;
  assert(differentPixels(minuteChanged, frame(app, options)) > 0);
  app.setClock(13, 22, 0);
  options.nowMs = 0;
  app.settings.clock24Hour = true;
  const auto fullDay = frame(app, options);
  app.settings.clock24Hour = false;
  assert(differentPixels(fullDay, frame(app, options)) > 0);
  app.setClock(1, 22, 0);
  const auto morning = frame(app, options);
  app.setClock(13, 22, 0);
  assert(differentPixels(morning, frame(app, options)) > 0);
}

static void scrollingGroundMatchesSourceAndAlphaAtBothScales() {
  for (uint8_t skin = 0; skin < flappy::Skins::count(); ++skin) {
    if (skin == 10 || skin == 11) continue;
    flappy::Skins colors;
    colors.select(skin);
    const auto& ground = colors.ground();
    const auto& bg = colors.background();
    const auto& base = FlappyAssets::background;
    for (flappy::Mode mode : {flappy::Mode::Original, flappy::Mode::Big}) {
      flappy::AppModel app(211);
      preparePlaying(app, mode, skin);
      app.game.birdY = -1000;
      const int scale = app.game.rules().spriteScale;
      const int groundY = app.game.groundY() / 2;
      for (int offset : {0, 1, 47, 255}) {
        app.game.groundOffset = uint8_t(offset);
        const auto image = frame(app);
        for (int y = groundY; y < flappy::PixelHeight && y < groundY + ground.height * scale; ++y) {
          const int bgY = scale == 1 ? y : y / scale + 60;
          const int bgRow = (bgY < bg.height ? bgY : bg.height - 1) * bg.width;
          const int baseRow = (bgY < base.height ? bgY : base.height - 1) * base.width;
          for (int x = 0; x < flappy::PixelWidth; ++x) {
            const int bgSource = bgRow + x / scale;
            uint16_t expected = bg.pixels[bgSource];
            if (expected == FlappyAssets::kTransparent || bg.alpha) {
              const uint8_t alpha = expected == FlappyAssets::kTransparent ? 0 : bg.alpha[bgSource];
              if (alpha != 255) expected = composite(expected, base.pixels[baseRow + x / scale], alpha);
            }
            expected = colors.palette(expected);
            const int source = (y - groundY) / scale * ground.width +
                ((x + offset / 2) % (ground.width * scale)) / scale;
            const uint16_t color = ground.pixels[source];
            if (color != FlappyAssets::kTransparent) expected = composite(colors.palette(color), expected,
                ground.alpha ? ground.alpha[source] : 255);
            assert(image[size_t(y) * flappy::PixelWidth + x] == expected);
          }
        }
      }
    }
  }
}

static void backgroundCachePreservesPixelsAndReleasesMemory() {
  GuardedPixels cachedPixels(FramePixels), plainPixels(FramePixels);
  flappy::Renderer cached(cachedPixels.data()), plain(plainPixels.data());
  plain.setBackgroundCacheEnabled(false);
  flappy::AppModel app(193);
  flappy::RenderOptions options;
  options.remoteVisible = true;
  options.remoteY = 210;
  options.remoteRotation = -35;
  for (uint8_t skin = 0; skin < flappy::Skins::count(); ++skin) {
    for (flappy::Mode mode : {flappy::Mode::Original, flappy::Mode::Big}) {
      preparePlaying(app, mode, skin);
      for (int frameIndex = 0; frameIndex < 4; ++frameIndex) {
        app.game.state = frameIndex == 0 ? flappy::State::Ready :
            frameIndex == 3 ? flappy::State::GameOver : flappy::State::Playing;
        app.game.rotation = float(frameIndex * 31 - 65);
        app.game.groundOffset = float(frameIndex * 13);
        app.game.birdFrame = uint8_t(frameIndex % 3);
        app.game.score = uint16_t(frameIndex * 57);
        for (int index = 0; index < flappy::PipeCount; ++index) {
          app.game.pipes[index].x = float(30 + index * 144 - frameIndex * 8);
          app.game.pipes[index].gapY = 90 + index * 33;
        }
        cached.draw(app, options);
        plain.draw(app, options);
        assert(cachedPixels.image() == plainPixels.image());
        assert(cached.backgroundCacheBytes() <= flappy::Renderer::BackgroundCacheLimit);
        assert(plain.backgroundCacheBytes() == 0);
        if (skin == 10 || skin == 11) assert(cached.backgroundCacheBytes() == 0);
        cachedPixels.check();
        plainPixels.check();
      }
    }
  }

  preparePlaying(app, flappy::Mode::Original, 1);
  cached.draw(app);
  const size_t warmBytes = cached.backgroundCacheBytes();
  assert(warmBytes > 0);
  cached.setBackgroundCacheEnabled(true);
  assert(cached.backgroundCacheBytes() == warmBytes);
  cached.draw(app);
  assert(cached.backgroundCacheBytes() == warmBytes);
  cached.setBackgroundCacheEnabled(false);
  assert(cached.backgroundCacheBytes() == 0);
  plain.draw(app);
  cached.draw(app);
  assert(cachedPixels.image() == plainPixels.image());

  for (size_t limit : {size_t(0), size_t(1), size_t(543), size_t(544),
                      size_t(1024), size_t(4096), size_t(65536)}) {
    cached.setBackgroundCacheEnabled(true, limit);
    assert(cached.backgroundCacheBytes() == 0);
    cached.draw(app);
    assert(cachedPixels.image() == plainPixels.image());
    assert(cached.backgroundCacheBytes() <= std::min(limit, size_t(flappy::Renderer::BackgroundCacheLimit)));
    if (limit < 544) assert(cached.backgroundCacheBytes() == 0);
    const size_t bytes = cached.backgroundCacheBytes();
    cached.draw(app);
    assert(cached.backgroundCacheBytes() == bytes);
    cachedPixels.check();
  }

  cached.setSkin(10);
  assert(cached.backgroundCacheBytes() == 0);
  cached.setBackgroundCacheEnabled(true);
  app.setSkin(0);
  app.openMenu(flappy::MenuPage::Skins);
  cached.draw(app);
  assert(cached.backgroundCacheBytes() == 0);
  app.closeMenu();
  cached.draw(app);
  assert(cached.backgroundCacheBytes() > 0);
  cached.setSkin(UINT8_MAX);
  cached.draw(app);
  assert(cached.backgroundCacheBytes() > 0);
  cached.setBackgroundCacheEnabled(false);
  assert(cached.backgroundCacheBytes() == 0);
}

static void viewportCopiesStayInsideBuffers() {
  GuardedPixels source(FramePixels);
  for (size_t i = 0; i < FramePixels; ++i) source.data()[i] = uint16_t(i * 23 + 7);
  const int dimensions[][2] = {{80, 160}, {135, 240}, {144, 256}, {240, 320}, {320, 480}};
  for (const auto& dimensionsForScreen : dimensions) {
    flappy::Viewport viewport;
    viewport.configure(dimensionsForScreen[0], dimensionsForScreen[1]);
    assert(viewport.width > 0 && viewport.height > 0);
    assert(viewport.outputX >= 0 && viewport.outputY >= 0);
    assert(viewport.outputX + viewport.width <= dimensionsForScreen[0]);
    assert(viewport.outputY + viewport.height <= dimensionsForScreen[1]);
    GuardedPixels output(size_t(viewport.width) * viewport.height);
    viewport.copy(source.data(), output.data());
    for (int y = 0; y < viewport.height; ++y) {
      for (int x = 0; x < viewport.width; ++x) {
        const int sourceX = viewport.native ? x + viewport.sourceX : x * flappy::PixelWidth / viewport.width;
        const int sourceY = viewport.native ? y + viewport.sourceY : y * flappy::PixelHeight / viewport.height;
        assert(output.data()[y * viewport.width + x] == source.data()[sourceY * flappy::PixelWidth + sourceX]);
      }
    }
    GuardedPixels segmented(size_t(viewport.width) * viewport.height);
    for (int row = 0; row < viewport.height;) {
      const int rows = std::min(17, viewport.height - row);
      viewport.copyRows(source.data(), segmented.data() + row * viewport.width, row, rows);
      row += rows;
    }
    assert(output.image() == segmented.image());
    output.check();
    segmented.check();
  }
  source.check();
}

}  // namespace

int main() {
  colorTransforms();
  messageScreensClearArtAndKeepTextInsideNativeCrop();
  landscapeMessageUsesNativeFontAndStaysInsideCanvas();
  activeSkinMasksFollowPixels();
  compactPreviewsMatchSkinColorsAndAlpha();
  skinPickerGridAnimationPagingAndBack();
  axisAlignedBirdPixelsMatchSource();
  bigModeDoublesEachBirdPixel();
  allModesAndSkinsRespectBufferBounds();
  menusScrollAndRenderEverySelection();
  actualMenuLabelsFitAndSurviveNativeCrop();
  volumeMenuShowsLevelsAndCurrentValue();
  sleepMenuShowsChoicesAndCurrentValue();
  launcherMenusShowAvailabilityAndTitles();
  controllerMenuShowsStatusAndActions();
  gameSettingsReflectEachSelectedGame();
  geometryLevelsFitNativeFontAndScrollVertically();
  ghostRotationsAndColorAssignment();
  clockDigitsFollowHourMinuteAndFormat();
  scrollingGroundMatchesSourceAndAlphaAtBothScales();
  backgroundCachePreservesPixelsAndReleasesMemory();
  viewportCopiesStayInsideBuffers();
  std::cout << "Render feature checks passed.\n";
}
