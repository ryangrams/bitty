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
  preparePlaying(app, flappy::Mode::Clock, 10);
  app.setSkinNames(names, flappy::Skins::BuiltinCount);
  const auto noMenu = frame(app);
  for (uint8_t page = uint8_t(flappy::MenuPage::Home); page <= uint8_t(flappy::MenuPage::Coop); ++page) {
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
  assert(differentPixels(titleStart, frame(app, options)) > 0);
  app.closeMenu();
}

static void volumeMenuShowsLevelsAndCurrentValue() {
  flappy::AppModel app(23);
  preparePlaying(app, flappy::Mode::Original, 10);
  app.openMenu(flappy::MenuPage::Home);
  app.menu.selection = 2;
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
  activeSkinMasksFollowPixels();
  axisAlignedBirdPixelsMatchSource();
  bigModeDoublesEachBirdPixel();
  allModesAndSkinsRespectBufferBounds();
  menusScrollAndRenderEverySelection();
  volumeMenuShowsLevelsAndCurrentValue();
  ghostRotationsAndColorAssignment();
  clockDigitsFollowHourMinuteAndFormat();
  viewportCopiesStayInsideBuffers();
  std::cout << "Render feature checks passed.\n";
}
