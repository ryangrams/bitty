#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "geometry_scene.h"
#include "geometry_collision.h"
#include "menu_font.h"

namespace flappy {

struct GeometryRenderOptions {
  const char* status = nullptr;
  const char* footer = nullptr;
  const char* levelTitle = nullptr;
  uint32_t attempt = 0;
  uint16_t backgroundColor = 0;
  uint8_t progress = 255;
  bool hud = true, useSourceColor = false, wireOrder = false;
  bool fullSolidFastPath = true, physicalScanOrder = false;
};

// Native vectors consume a read-only cartridge snapshot at the source's pixel scale.
class GeometryRenderer {
 public:
  enum { Width = 240, Height = 135, BlockSize = 16, SourceX = 40 };

  void reset() { cameraReady_ = false; previousState_ = 255; previousLevel_ = 255; cropY_ = 80; cropX_ = 0; cameraVelocity_ = 0; previousAttempt_ = 0; }
  int cameraY() const { return cropY_; }
  int cameraX() const { return cropX_; }
  int worldCameraY() const { return (worldTop_ + 128) >> 8; }

  static const char* levelName(uint8_t level) {
    static const char* const names[] = {
      "STEREO MADNESS", "BACK ON TRACK", "POLARGEIST", "DRY OUT", "BASE AFTER BASE",
      "CANT LET GO", "JUMPER", "TIME MACHINE", "CYCLES", "XSTEP", "CLUTTERFUNK",
      "THEORY OF EVERYTHING", "ELECTROMAN ADVENTURES", "CLUBSTEP", "ELECTRODYNAMIX",
      "HEXAGON FORCE", "BLAST PROCESSING", "THEORY OF EVERYTHING 2", "GEOMETRICAL DOMINATOR",
      "DEADLOCKED", "FINGERDASH", "DASH"
    };
    return level < 22 ? names[level] : "COMMUNITY LEVEL";
  }

  static const char* formName(uint8_t mode) {
    static const char* const names[] = {"CUBE", "SHIP", "BALL", "UFO", "ROBOT", "SPIDER", "WAVE", "SWING", "NINJA"};
    return mode < 9 ? names[mode] : "CUBE";
  }

  template <class ReadRam, class ReadRom>
  void draw(const GeometryScene& scene, ReadRam readRam, ReadRom readRom,
            uint16_t* canvas, const GeometryRenderOptions& options = GeometryRenderOptions()) {
    if (!canvas) return;
    pixels_ = canvas;
    wireOrder_ = options.wireOrder;
    fullSolidFastPath_ = options.fullSolidFastPath;
    physicalScanOrder_ = options.physicalScanOrder;
    cropX_ = scene.playing() ? clamp(scene.players[0].pixelX() - 40, 0, SourceX) : 0;
    background(scene.playing() ? scene.scrollX + cropX_ : 0, options);
    if (!scene.playing()) {
      drawReady(scene, options);
      cameraReady_ = false;
    } else {
      updateCamera(scene, readRam, readRom);
      drawTerrain(scene, readRam, readRom);
      for (unsigned i = 0; i < GeometryScene::ObjectCount; ++i) drawObject(scene.objects[i]);
      for (unsigned i = 0; i < scene.previewCount && i < GeometryScene::PreviewCount; ++i) drawObject(scene.previews[i]);
      for (unsigned i = 0; i < (scene.dual ? 2u : 1u); ++i) {
        const GeometryPlayer& player = scene.players[i];
        drawPlayer(player, player.pixelX() - cropX_, player.pixelY() - cropY_, scene.mode, i != 0);
      }
      if (options.hud) drawHud(scene, options);
      if (scene.dual) offscreenPlayer(scene.players[1]);
    }
    previousState_ = scene.state;
    previousLevel_ = scene.level;
    previousAttempt_ = scene.attempt;
  }

 private:
  struct Point { int x, y; };
  struct Neighbors { uint16_t top = 0, bottom = 0, left = 0, right = 0; };
  uint16_t* pixels_ = nullptr;
  int cropY_ = 80, cropX_ = 0;
  int32_t worldTop_ = 0, cameraVelocity_ = 0, previousPlayerY_ = 0;
  uint32_t previousAttempt_ = 0;
  uint8_t previousState_ = 255, previousLevel_ = 255;
  bool cameraReady_ = false, wireOrder_ = false, fullSolidFastPath_ = true, physicalScanOrder_ = false;

  uint16_t rgb(int r, int g, int b) const {
    const uint16_t color = uint16_t((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
    return wireOrder_ ? uint16_t((color >> 8) | (color << 8)) : color;
  }
  static int minimum(int a, int b) { return a < b ? a : b; }
  static int maximum(int a, int b) { return a > b ? a : b; }
  static int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
  static uint16_t lowBits(int count) { return count >= 16 ? 0xffff : count <= 0 ? 0 : uint16_t((1u << count) - 1); }
  static bool fullSolid(uint8_t collision) { return collision == 7 || collision == 9 || collision == 0x1f; }

  static uint16_t solidColumn(uint8_t collision, bool first) {
    if (fullSolid(collision)) return 0xffff;
    if (!collision) return 0;
    uint16_t result = 0;
    const uint16_t edge = first ? 1 : 0x8000;
    for (int row = 0; row < 16; ++row) if (solidRow(collision, row) & edge) result |= uint16_t(1u << row);
    return result;
  }

  static uint16_t solidRow(uint8_t collision, int row) { return GeometryCollisionMask::solidRow(collision, row); }

  template <class ReadRam, class ReadRom>
  int landing(const GeometryScene& scene, ReadRam readRam, ReadRom readRom, const GeometryPlayer& p) const {
    const int direction = p.gravity ? -1 : 1;
    const int start = scene.linearScrollY() + p.pixelY() + (p.gravity ? (p.mini ? 3 : -1) : p.mini ? 12 : 16);
    const int64_t worldX = int64_t(scene.scrollX) + p.pixelX() + (p.mini ? 4 : 8);
    for (int y = start; y >= 0 && y < 960;) {
      const GeometryTile tile = scene.tileAtWorld(readRam, readRom, worldX, y);
      if (tile.valid && fullSolid(tile.collision)) return y;
      const int next = (y & ~15) + (direction > 0 ? 16 : -1);
      if (tile.valid && tile.collision)
        for (int row = y; row != next; row += direction)
          if (solidRow(tile.collision, row & 15) & (1u << (unsigned(worldX) & 15))) return row;
      y = next;
    }
    return -1;
  }

  template <class ReadRam, class ReadRom>
  void updateCamera(const GeometryScene& scene, ReadRam readRam, ReadRom readRom) {
    const GeometryPlayer& player = scene.players[0];
    const int linearY = scene.linearScrollY();
    const int32_t playerY = int32_t(linearY) * 256 + player.y;
    const int top = linearY + player.pixelY() + (player.mini ? 4 : 0);
    const int bottom = top + (player.mini ? 8 : 16);
    const int floor = landing(scene, readRam, readRom, player);
    const bool restart = !cameraReady_ || previousState_ != 2 || previousLevel_ != scene.level ||
      (scene.attempt && previousAttempt_ && scene.attempt != previousAttempt_);
    if (restart) {
      worldTop_ = int32_t(floor >= 0 ? floor - (player.gravity ? 20 : 115) : top - 99) * 256;
      cameraVelocity_ = 0;
    } else {
      int followTop = top, followBottom = bottom;
      if (scene.dual) {
        const auto& second = scene.players[1];
        const int secondTop = linearY + second.pixelY() + (second.mini ? 4 : 0);
        const int secondBottom = secondTop + (second.mini ? 8 : 16);
        if (maximum(bottom, secondBottom) - minimum(top, secondTop) <= 111) {
          followTop = minimum(top, secondTop); followBottom = maximum(bottom, secondBottom);
        }
      }
      int32_t error = 0;
      if (int32_t(followTop) * 256 < worldTop_ + 24 * 256) error = int32_t(followTop - 24) * 256 - worldTop_;
      if (int32_t(followBottom) * 256 > worldTop_ + 119 * 256) error = int32_t(followBottom - 119) * 256 - worldTop_;
      const int32_t velocity = clamp(playerY - previousPlayerY_, -8 * 256, 8 * 256);
      if (floor >= 0 && !player.gravity && velocity >= 0 && floor - top < 80)
        error = maximum(error, int32_t(floor - 115) * 256 - worldTop_);
      if (floor >= 0 && player.gravity && velocity <= 0 && bottom - floor < 80)
        error = minimum(error, int32_t(floor - 20) * 256 - worldTop_);
      const int wanted = error ? clamp(error / 8 + velocity, -8 * 256, 8 * 256) : 0;
      cameraVelocity_ += clamp(wanted - cameraVelocity_, -256, 256);
      worldTop_ += cameraVelocity_;
    }
    cropY_ = worldCameraY() - linearY;
    previousPlayerY_ = playerY;
    cameraReady_ = true;
  }

  template <class ReadRam, class ReadRom>
  void drawTerrain(const GeometryScene& scene, ReadRam readRam, ReadRom readRom) {
    const int firstX = -int((scene.scrollX + cropX_) & 15);
    const int firstY = -((scene.linearScrollY() + cropY_) & 15);
    for (int y = firstY; y < Height; y += BlockSize) {
      for (int x = firstX; x < Width; x += BlockSize) {
        const int sampleX = x + cropX_ + 8, sampleY = y + cropY_ + 8;
        const GeometryTile tile = scene.tileAtScreen(readRam, readRom, sampleX, sampleY);
        if (!tile.valid || (!tile.collision && tile.id != 0x78)) continue;
        Neighbors neighbors;
        if (tile.hasSolidRegion()) {
          const GeometryTile above = scene.tileAtScreen(readRam, readRom, sampleX, sampleY - 16);
          const GeometryTile below = scene.tileAtScreen(readRam, readRom, sampleX, sampleY + 16);
          const GeometryTile left = scene.tileAtScreen(readRam, readRom, sampleX - 16, sampleY);
          const GeometryTile right = scene.tileAtScreen(readRam, readRom, sampleX + 16, sampleY);
          neighbors.top = above.valid ? fullSolid(above.collision) ? 0xffff : solidRow(above.collision, 15) : 0;
          neighbors.bottom = below.valid ? fullSolid(below.collision) ? 0xffff : solidRow(below.collision, 0) : 0;
          neighbors.left = left.valid ? solidColumn(left.collision, false) : 0;
          neighbors.right = right.valid ? solidColumn(right.collision, true) : 0;
        }
        drawTile(tile, x, y, neighbors);
      }
    }
  }

  void drawTile(const GeometryTile& tile, int x, int y, const Neighbors& neighbors) {
    const uint16_t white = rgb(255, 255, 255), dark = rgb(0, 24, 65), cyan = rgb(8, 192, 247);
    if (fullSolidFastPath_ && fullSolid(tile.collision)) {
      rect(x, y, 16, 16, dark);
      rect(x + 2, y, 1, 16, cyan);
      rect(x, y + 2, 16, 1, cyan);
      edgeRuns(x, y, uint16_t(~neighbors.top), false, white);
      edgeRuns(x, y + 15, uint16_t(~neighbors.bottom), false, white);
      edgeRuns(x, y, uint16_t(~neighbors.left), true, white);
      edgeRuns(x + 15, y, uint16_t(~neighbors.right), true, white);
      return;
    }
    if (tile.hasSolidRegion()) for (int row = 0; row < 16; ++row) {
      const uint16_t mask = solidRow(tile.collision, row);
      const uint16_t fromLeft = uint16_t((mask << 1) | ((neighbors.left & (1u << row)) ? 1 : 0));
      const uint16_t fromRight = uint16_t((mask >> 1) | ((neighbors.right & (1u << row)) ? 0x8000 : 0));
      const uint16_t above = row ? solidRow(tile.collision, row - 1) : neighbors.top;
      const uint16_t below = row == 15 ? neighbors.bottom : solidRow(tile.collision, row + 1);
      const uint16_t inner = uint16_t(mask & fromLeft & fromRight & above & below);
      for (int column = 0; column < 16; ++column) if (mask & (1u << column))
        pixel(x + column, y + row, !(inner & (1u << column)) ? white : row == 2 || column == 2 ? cyan : dark);
    }
    if (tile.isHazard() || tile.id == 0x78) drawHazard(tile, x, y);
  }

  static uint16_t triangleRow(int left, int top, int width, int height, int direction, int row) {
    if (row < top || row >= top + height || width < 2 || height < 2) return 0;
    const int localY = row - top;
    if (direction < 2) {
      const int distance = direction ? localY : height - 1 - localY;
      const int inset = (width - 1) * distance / (2 * (height - 1));
      return GeometryCollisionMask::span(left + inset, left + width - 1 - inset);
    }
    const int offset = 2 * localY - (height - 1);
    const int distance = offset < 0 ? -offset : offset;
    const int reach = width - 1 - distance * (width - 1) / (height - 1);
    return direction == 3 ? GeometryCollisionMask::span(left, left + reach) :
      GeometryCollisionMask::span(left + width - 1 - reach, left + width - 1);
  }
  static bool sawPart(uint8_t id, int& cx, int& cy, int& radius) {
    if (id == 4 || id == 0x7d || id == 0x7f) {
      cx = 8; cy = id == 4 ? 0 : id == 0x7f ? 16 : 8; radius = 8; return true;
    }
    if (id >= 8 && id <= 11) {
      cx = id & 1 ? 0 : 16; cy = id < 10 ? 16 : 0; radius = 16; return true;
    }
    if (id >= 0x74 && id <= 0x7c) {
      cx = 24 - int((id - 0x74) % 3) * 16;
      cy = 24 - int((id - 0x74) / 3) * 16;
      radius = 24; return true;
    }
    return false;
  }
  static bool sawPoint(int x, int y, int cx, int cy, int radius) {
    const int dx = x - cx, dy = y - cy;
    const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    const int high = maximum(ax, ay), low = minimum(ax, ay);
    const bool tooth = low * 4 <= high || low * 4 >= high * 3;
    const int reach = tooth ? radius : radius * 4 / 5;
    return dx * dx + dy * dy <= reach * reach;
  }
  static uint16_t hazardArtRow(uint8_t id, uint8_t c, int row) {
    int cx, cy, radius;
    if (sawPart(id, cx, cy, radius)) {
      uint16_t mask = 0;
      for (int x = 0; x < 16; ++x) if (sawPoint(x, row, cx, cy, radius)) mask |= uint16_t(1u << x);
      return mask;
    }
    if (row < 0 || row > 15) return 0;
    if (id == 0xf6 || id == 0xfb) return uint16_t(triangleRow(0, 0, 8, 16, id == 0xfb, row) |
      triangleRow(8, 0, 8, 16, id == 0xfb, row));
    if (id == 0x0c || id == 0x13 || id == 0x18 || id == 0x5c) return uint16_t(triangleRow(0, 8, 8, 8, 0, row) | triangleRow(8, 8, 8, 8, 0, row));
    if (id == 0x0d || id == 0x14 || id == 0x17 || id == 0x5d) return uint16_t(triangleRow(0, 0, 8, 8, 1, row) | triangleRow(8, 0, 8, 8, 1, row));
    if (id == 0x82 || id == 0x83) return uint16_t(triangleRow(0, 0, 8, 8, 1, row) | triangleRow(8, 0, 8, 8, 1, row) |
      triangleRow(0, 8, 8, 8, 0, row) | triangleRow(8, 8, 8, 8, 0, row));
    if (c == 1) return triangleRow(8, 0, 8, 16, 3, row);
    if (c == 2) return triangleRow(0, 0, 8, 16, 2, row);
    if (c == 3) return triangleRow(0, 0, 16, 8, 1, row);
    if (c == 4) return triangleRow(0, 8, 16, 8, 0, row);
    if (c == 8) return triangleRow(0, 0, 16, 16, id == 0x1a ? 2 : id == 0x1c ? 3 : id == 0x1b ? 1 : 0, row);
    if (c >= 0x28 && c <= 0x32) {
      uint16_t mask = 0;
      const bool bottomHalf = c <= 0x2a;
      const int top = bottomHalf ? 8 : 0, direction = c >= 0x30 ? 1 : 0;
      if (c == 0x28 || c == 0x2a || c == 0x2b || c == 0x2d || c == 0x2f || c == 0x30 || c == 0x32)
        mask |= triangleRow(0, top, 8, 8, direction, row);
      if (c == 0x29 || c == 0x2a || c == 0x2c || c == 0x2e || c == 0x2f || c == 0x31 || c == 0x32)
        mask |= triangleRow(8, top, 8, 8, direction, row);
      return mask;
    }
    if (c >= 0x33 && c <= 0x36) {
      const bool down = c <= 0x34, left = c == 0x34 || c == 0x36;
      return uint16_t(triangleRow(0, down ? 0 : 8, 16, 8, down ? 1 : 0, row) |
        triangleRow(left ? 0 : 8, 0, 8, 16, left ? 2 : 3, row));
    }
    if (c == 0x3b || c == 0x3d) return triangleRow(4, 8, 8, 8, 1, row);
    if (c == 0x3c) return triangleRow(4, 0, 8, 8, 0, row);
    return 0;
  }
  static uint32_t hazardExtendedRow(const GeometryTile& tile, int row) {
    uint32_t result = uint32_t(hazardArtRow(tile.id, tile.collision, row) |
      GeometryCollisionMask::hazardRow(tile.collision, row)) << 1;
    int cx, cy, radius;
    if (sawPart(tile.id, cx, cy, radius)) {
      if (sawPoint(-1, row, cx, cy, radius)) result |= 1;
      if (sawPoint(16, row, cx, cy, radius)) result |= 1u << 17;
    }
    return result;
  }
  void drawHazard(const GeometryTile& tile, int x, int y) {
    uint32_t rows[18];
    for (int row = -1; row <= 16; ++row) rows[row + 1] = hazardExtendedRow(tile, row);
    const uint16_t white = rgb(255, 255, 255), dark = rgb(0, 15, 40), cyan = rgb(8, 192, 247);
    int cx, cy, radius;
    const bool saw = sawPart(tile.id, cx, cy, radius);
    for (int row = 0; row < 16; ++row) {
      const uint32_t mask = rows[row + 1];
      const uint32_t inner = mask & (mask << 1) & (mask >> 1) & rows[row] & rows[row + 2];
      for (int column = 0; column < 16; ++column) {
        const uint32_t bit = 1u << (column + 1);
        if (!(mask & bit)) continue;
        const int dx = saw ? column - cx : 0, dy = saw ? row - cy : 0;
        pixel(x + column, y + row, !(inner & bit) ? white : saw && dx * dx + dy * dy < radius * radius / 9 ? cyan : dark);
      }
    }
  }

  void background(uint32_t scroll, const GeometryRenderOptions& options) {
    int red = 0, green = 116, blue = 246;
    if (options.useSourceColor) {
      const int sourceRed = ((options.backgroundColor >> 11) & 31) * 255 / 31;
      const int sourceGreen = ((options.backgroundColor >> 5) & 63) * 255 / 63;
      const int sourceBlue = (options.backgroundColor & 31) * 255 / 31;
      const int high = maximum(sourceRed, maximum(sourceGreen, sourceBlue));
      const int low = minimum(sourceRed, minimum(sourceGreen, sourceBlue));
      if (high >= 16 && high - low >= 32) {
        const int range = high - low;
        red = 32 + (sourceRed - low) * 200 / range;
        green = 32 + (sourceGreen - low) * 200 / range;
        blue = 32 + (sourceBlue - low) * 200 / range;
      }
    }
    const bool blueTheme = blue > red + 80 && blue > green + 55;
    if (blueTheme) { blue = maximum(blue, 246); green = maximum(green, 116); }
    uint16_t shades[Height];
    for (int y = 0; y < Height; ++y) {
      const int light = blueTheme ? 60 + y * 40 / (Height - 1) : 75 + y * 25 / (Height - 1);
      const int blueLight = blueTheme ? 85 + y * 15 / (Height - 1) : light;
      shades[y] = rgb(red * light / 100, green * light / 100, blue * blueLight / 100);
    }
    const int parallax = int((scroll / 5) % 48);
    const uint16_t firstFill = rgb(red * 70 / 100, green * 70 / 100, blue * (blueTheme ? 85 : 78) / 100);
    const uint16_t firstEdge = rgb(red * 78 / 100, green * 78 / 100, blue * (blueTheme ? 92 : 86) / 100);
    const uint16_t secondFill = rgb(red * 85 / 100, green * 85 / 100, blue * (blueTheme ? 92 : 88) / 100);
    const uint16_t secondEdge = rgb(red * 90 / 100, green * 90 / 100, blue * (blueTheme ? 98 : 94) / 100);
    if (physicalScanOrder_) {
      for (int y = 0; y < Height / 2; ++y) {
        const uint16_t shade = shades[y]; shades[y] = shades[Height - 1 - y]; shades[Height - 1 - y] = shade;
      }
      for (int x = 0; x < 48; ++x) {
        uint16_t* line = pixels_ + x * Height;
        memcpy(line, shades, sizeof(shades));
        const int first = (x + parallax) % 48;
        if (first < 34) {
          line[Height - 1 - 29] = firstEdge; line[Height - 72] = firstEdge;
          const uint16_t color = first && first < 33 ? firstFill : firstEdge;
          for (int y = Height - 71; y < Height - 30; ++y) line[y] = color;
        }
        const int second = (x + parallax + 30) % 48;
        if (second < 23) {
          line[Height - 1 - 78] = secondEdge; line[Height - 107] = secondEdge;
          const uint16_t color = second && second < 22 ? secondFill : secondEdge;
          for (int y = Height - 106; y < Height - 79; ++y) line[y] = color;
        }
      }
      for (int x = 48; x < Width; x += 48)
        memcpy(pixels_ + x * Height, pixels_, 48 * Height * sizeof(uint16_t));
      return;
    }
    for (int y = 0; y < Height; ++y) {
      uint16_t* line = pixels_ + y * Width;
      for (int x = 0; x < Width; ++x) line[x] = shades[y];
    }
    for (int x = -48; x < Width + 48; x += 48) {
      box(x - parallax, 29, 34, 43, firstFill, firstEdge);
      box(x + 18 - parallax, 78, 23, 29, secondFill, secondEdge);
    }
  }

  void drawReady(const GeometryScene& scene, const GeometryRenderOptions& options) {
    const uint16_t white = rgb(255, 255, 255);
    centered(scene.state == 3 ? "LEVEL COMPLETE" : "GEOMETRY DASH", 13, white, 2);
    box(16, 46, 208, 51, rgb(0, 55, 140), rgb(20, 220, 255));
    centered(options.levelTitle ? options.levelTitle : levelName(scene.level), 52, white);
    GeometryPlayer player;
    drawPlayer(player, 112, 73, 0, false);
    centered(options.status ? options.status : scene.state == 3 ? "COMPLETE" : scene.state == 1 || scene.state == 6 ? "LOADING LEVEL" : "STARTING", 104, white);
    if (options.footer) centered(options.footer, 121, white);
  }

  void drawHud(const GeometryScene& scene, const GeometryRenderOptions& options) {
    text(options.levelTitle ? options.levelTitle : levelName(scene.level), 5, 1, rgb(255, 255, 255), 1, 30);
    char value[20];
    if (options.progress <= 100) snprintf(value, sizeof(value), "%u%%", unsigned(options.progress));
    else snprintf(value, sizeof(value), "%s", formName(scene.mode));
    const int x = Width - MenuFont::textWidth(value) - 5;
    text(value, x, 1, rgb(255, 255, 255));
    if (options.attempt) {
      rect(0, 123, Width, 12, rgb(0, 18, 52));
      snprintf(value, sizeof(value), "A%lu", static_cast<unsigned long>(options.attempt));
      text(value, 4, 121, rgb(255, 255, 255));
    }
    if (options.progress <= 100) {
      if (!options.attempt) rect(0, 123, Width, 12, rgb(0, 18, 52));
      const int left = options.attempt ? 53 : 5;
      box(left, 125, Width - left - 5, 5, rgb(0, 29, 80), rgb(220, 251, 255));
      rect(left + 2, 127, (Width - left - 9) * options.progress / 100, 1, rgb(255, 230, 0));
    }
  }

  uint16_t objectColor(uint8_t type) const {
    if (type == 5 || type == 8 || type == 0x0d || type == 0x0e || type == 0x7b) return rgb(0, 230, 255);
    if (type == 6 || type == 0x25 || type == 0x26 || type == 0x18) return rgb(255, 88, 225);
    if (type == 0x27 || type == 0x7c || type == 0x65 || type == 0x17) return rgb(82, 255, 98);
    if (type == 0x28 || type == 0x52 || type == 0x53) return rgb(255, 70, 75);
    if (type == 0x44 || type == 0x79) return rgb(39, 47, 63);
    if (type == 0x7a) return rgb(255, 255, 255);
    return rgb(255, 229, 0);
  }

  void drawObject(const GeometryObject& object) {
    if (!object.visible()) return;
    const int x = int(object.x) - cropX_, y = int(object.y) + 1 - cropY_;
    const auto kind = object.kind();
    const uint16_t color = objectColor(object.type), white = rgb(255, 255, 255), dark = rgb(0, 19, 52);
    if (kind == GeometryObjectKind::Portal) {
      const bool horizontal = object.type >= 0x10 && object.type <= 0x13;
      const int width = horizontal ? 48 : 24, height = horizontal ? 32 : 48;
      const int left = x - (object.type == 0x18 ? 8 : 0), top = y - (horizontal ? 16 : 0);
      ellipse(left, top, width, height, white);
      ellipse(left + 1, top + 1, width - 2, height - 2, color);
      ellipse(left + 4, top + 4, width - 8, height - 8, dark);
      const int cy = top + height / 2;
      line(left + width / 2 - 3, cy - 4, left + width / 2 + 3, cy, color);
      line(left + width / 2 + 3, cy, left + width / 2 - 3, cy + 4, color);
    } else if (kind == GeometryObjectKind::Orb) {
      circle(x + 8, y + 8, 8, white);
      circle(x + 8, y + 8, 7, color);
      circle(x + 8, y + 8, 4, object.activated ? white : dark);
      if (object.type == 0x45 || object.type == 0x46 || (object.type >= 0x4c && object.type <= 0x51) || (object.type >= 0x5b && object.type <= 0x5e)) {
        line(x + 5, y + 5, x + 11, y + 8, white);
        line(x + 11, y + 8, x + 5, y + 11, white);
      }
    } else if (kind == GeometryObjectKind::Pad) {
      const bool up = object.type == 0x0c || object.type == 0x0e || object.type == 0x26 || object.type == 0x53 || object.type == 0x56;
      const int top = y + (up ? 0 : -8);
      ellipse(x, top, 16, 8, white);
      ellipse(x + 1, top + 1, 14, 6, color);
      rect(x + 2, top + (up ? 0 : 5), 12, 3, dark);
    } else if (kind == GeometryObjectKind::Coin) {
      const bool collected = object.collectedCoin();
      circle(x + 8, y + 8, 7, collected ? rgb(85, 156, 194) : white);
      circle(x + 8, y + 8, 6, collected ? dark : rgb(255, 204, 0));
      line(x + 8, y + 4, x + 8, y + 12, collected ? rgb(85, 156, 194) : rgb(143, 85, 0));
    }
  }

  static int sine(uint8_t angle) {
    static const int values[] = {0,25,50,74,98,121,142,162,181,198,213,226,237,245,251,255,256};
    const unsigned quadrant = (angle & 63) / 16, offset = angle & 15;
    const int value = values[quadrant & 1 ? 16 - offset : offset];
    return quadrant >= 2 ? -value : value;
  }
  Point transform(Point point, int x, int y, const GeometryPlayer& player, uint8_t angle) const {
    if (player.gravity) point.y = 15 - point.y;
    const int dx = point.x * 2 - 15, dy = point.y * 2 - 15;
    const int s = sine(angle), c = sine(uint8_t(angle + 16));
    const int scale = player.mini ? 2 : 1;
    return {x + (15 * 256 + dx * c - dy * s + 256) / (512 * scale),
            y + (player.mini ? 4 : 0) + (15 * 256 + dx * s + dy * c + 256) / (512 * scale)};
  }
  void iconPolygon(const Point* points, unsigned count, int x, int y, const GeometryPlayer& player, uint8_t angle, uint16_t color) {
    Point transformed[16];
    if (count > 16) return;
    for (unsigned i = 0; i < count; ++i) transformed[i] = transform(points[i], x, y, player, angle);
    polygon(transformed, count, color);
  }
  void iconBox(int x0, int y0, int x1, int y1, int x, int y, const GeometryPlayer& p, uint8_t angle, uint16_t color) {
    const Point points[] = {{x0,y0}, {x1,y0}, {x1,y1}, {x0,y1}};
    iconPolygon(points, 4, x, y, p, angle, color);
  }
  void iconLine(int x0, int y0, int x1, int y1, int x, int y, const GeometryPlayer& p, uint8_t angle, uint16_t color) {
    const Point a = transform({x0,y0}, x, y, p, angle), b = transform({x1,y1}, x, y, p, angle);
    line(a.x, a.y, b.x, b.y, color);
  }

  void drawPlayer(const GeometryPlayer& p, int x, int y, uint8_t mode, bool second) {
    const uint16_t dark = rgb(0, 14, 35), main = second ? rgb(255, 91, 211) : rgb(255, 229, 0), cyan = rgb(3, 241, 255), white = rgb(255, 255, 255);
    const uint8_t cubeAngle = uint8_t((uint32_t(p.rotation) * 64 / (24 * 256)) & 63);
    const uint8_t pitch = uint8_t(clamp(int(p.velY) / 384, -5, 5) & 63);
    const uint8_t angle = mode == 0 || mode == 2 ? cubeAngle : mode == 1 || mode == 6 || mode == 7 ? pitch : 0;
    if (mode == 1) {
      const Point hull[] = {{-4,10}, {20,7}, {10,16}, {-1,16}};
      const Point inset[] = {{-1,11}, {16,9}, {9,14}, {1,14}};
      iconPolygon(hull, 4, x, y, p, angle, dark);
      iconPolygon(inset, 4, x, y, p, angle, cyan);
      iconBox(3, 1, 12, 10, x, y, p, angle, dark);
      iconBox(4, 2, 11, 8, x, y, p, angle, main);
      iconBox(7, 4, 9, 5, x, y, p, angle, dark);
    } else if (mode == 2 || mode == 7) {
      const Point outer[] = {{5,0}, {10,0}, {15,5}, {15,10}, {10,15}, {5,15}, {0,10}, {0,5}};
      const Point inner[] = {{5,2}, {10,2}, {13,5}, {13,10}, {10,13}, {5,13}, {2,10}, {2,5}};
      iconPolygon(outer, 8, x, y, p, angle, dark); iconPolygon(inner, 8, x, y, p, angle, main);
      if (mode == 2) {
        iconLine(3, 3, 12, 12, x, y, p, angle, dark);
        iconLine(12, 3, 3, 12, x, y, p, angle, cyan);
      } else {
        const Point wing[] = {{-4,4}, {2,7}, {-3,11}};
        iconPolygon(wing, 3, x, y, p, angle, cyan);
        const Point tail[] = {{19,4}, {13,7}, {18,11}};
        iconPolygon(tail, 3, x, y, p, angle, cyan);
        iconBox(5, 5, 6, 7, x, y, p, angle, dark); iconBox(9, 5, 10, 7, x, y, p, angle, dark);
      }
    } else if (mode == 3) {
      const Point dome[] = {{3,7}, {4,2}, {11,2}, {12,7}};
      iconPolygon(dome, 4, x, y, p, 0, dark);
      iconBox(5, 3, 10, 6, x, y, p, 0, main);
      const Point saucer[] = {{-2,8}, {17,8}, {12,13}, {3,13}};
      iconPolygon(saucer, 4, x, y, p, 0, cyan);
      iconLine(0, 8, 15, 8, x, y, p, 0, white);
      iconLine(3, 14, 1, 16, x, y, p, 0, dark); iconLine(12, 14, 14, 16, x, y, p, 0, dark);
    } else if (mode == 4) {
      iconBox(2, 0, 13, 10, x, y, p, 0, dark); iconBox(3, 1, 12, 8, x, y, p, 0, main);
      iconBox(8, 3, 11, 5, x, y, p, 0, dark);
      iconBox(4, 9, 7, 13, x, y, p, 0, dark); iconBox(9, 9, 12, 13, x, y, p, 0, dark);
      iconBox(1, 13, 7, 15, x, y, p, 0, cyan); iconBox(10, 13, 15, 15, x, y, p, 0, cyan);
    } else if (mode == 5) {
      for (int leg = 0; leg < 3; ++leg) {
        iconLine(4, 6 + leg * 2, -2, 2 + leg * 5, x, y, p, 0, cyan);
        iconLine(11, 6 + leg * 2, 17, 2 + leg * 5, x, y, p, 0, cyan);
      }
      iconBox(3, 3, 12, 12, x, y, p, 0, dark); iconBox(4, 4, 11, 10, x, y, p, 0, main);
      iconBox(5, 6, 6, 7, x, y, p, 0, dark); iconBox(9, 6, 10, 7, x, y, p, 0, dark);
    } else if (mode == 6) {
      const Point outer[] = {{0,0}, {17,8}, {0,15}, {4,8}}, inner[] = {{3,3}, {13,8}, {3,12}, {6,8}};
      iconPolygon(outer, 4, x, y, p, angle, dark); iconPolygon(inner, 4, x, y, p, angle, main);
      iconLine(-7, 8, -1, 8, x, y, p, angle, cyan);
    } else if (mode == 8) {
      const Point head[] = {{4,0}, {11,0}, {15,5}, {15,11}, {11,15}, {4,15}, {0,11}, {0,5}};
      iconPolygon(head, 8, x, y, p, 0, dark); iconBox(3, 2, 12, 13, x, y, p, 0, main);
      iconBox(2, 5, 13, 8, x, y, p, 0, dark);
      iconBox(5, 6, 6, 7, x, y, p, 0, white); iconBox(10, 6, 11, 7, x, y, p, 0, white);
      iconLine(-2, 12, 17, 1, x, y, p, 0, cyan);
    } else {
      iconBox(0, 0, 15, 15, x, y, p, angle, dark); iconBox(1, 1, 14, 14, x, y, p, angle, main);
      iconBox(4, 4, 6, 6, x, y, p, angle, dark); iconBox(9, 4, 11, 6, x, y, p, angle, dark);
      iconBox(4, 9, 11, 11, x, y, p, angle, dark);
    }
  }

  void offscreenPlayer(const GeometryPlayer& player) {
    const int y = player.pixelY() - cropY_;
    if (y >= 0 && y + 16 < Height) return;
    const int x = clamp(player.pixelX() - cropX_ + 8, 5, Width - 6), top = y < 0 ? 16 : Height - 7;
    const Point arrow[] = {{x, top + (y < 0 ? 0 : 5)}, {x - 3, top + (y < 0 ? 5 : 0)}, {x + 3, top + (y < 0 ? 5 : 0)}};
    polygon(arrow, 3, rgb(255, 91, 211));
  }

  void pixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < Width && y >= 0 && y < Height)
      pixels_[physicalScanOrder_ ? x * Height + Height - 1 - y : y * Width + x] = color;
  }
  void rect(int x, int y, int width, int height, uint16_t color) {
    const int left = maximum(0, x), right = minimum(Width, x + width), top = maximum(0, y), bottom = minimum(Height, y + height);
    if (physicalScanOrder_) {
      for (int column = left; column < right; ++column) {
        uint16_t* line = pixels_ + column * Height;
        for (int row = Height - bottom; row < Height - top; ++row) line[row] = color;
      }
    } else {
      for (int row = top; row < bottom; ++row) for (int column = left; column < right; ++column) pixels_[row * Width + column] = color;
    }
  }
  void box(int x, int y, int width, int height, uint16_t fill, uint16_t edge) {
    if (width <= 0 || height <= 0) return;
    rect(x, y, width, height, edge); rect(x + 1, y + 1, width - 2, height - 2, fill);
  }
  void edgeRuns(int x, int y, uint16_t mask, bool vertical, uint16_t color) {
    while (mask) {
      const unsigned start = unsigned(__builtin_ctz(unsigned(mask)));
      const unsigned length = unsigned(__builtin_ctz((~(unsigned(mask) >> start)) | 0x10000u));
      rect(x + (vertical ? 0 : int(start)), y + (vertical ? int(start) : 0),
          vertical ? 1 : int(length), vertical ? int(length) : 1, color);
      mask &= uint16_t(~(unsigned(lowBits(int(length))) << start));
    }
  }
  void line(int x0, int y0, int x1, int y1, uint16_t color) {
    const int dx = x0 < x1 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    const int dy = y0 < y1 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      pixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      const int doubled = 2 * error;
      if (doubled >= dy) { error += dy; x0 += sx; }
      if (doubled <= dx) { error += dx; y0 += sy; }
    }
  }
  void ellipse(int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0) return;
    const int limit = width * width * height * height;
    for (int dy = maximum(0, -y); dy < height && y + dy < Height; ++dy) {
      const int vy = 2 * dy + 1 - height;
      const int horizontalLimit = limit - vy * vy * width * width;
      for (int dx = maximum(0, -x); dx < width && x + dx < Width; ++dx) {
        const int vx = 2 * dx + 1 - width;
        if (vx * vx * height * height <= horizontalLimit) pixel(x + dx, y + dy, color);
      }
    }
  }
  void circle(int x, int y, int radius, uint16_t color) { ellipse(x - radius, y - radius, radius * 2 + 1, radius * 2 + 1, color); }
  void polygon(const Point* points, unsigned count, uint16_t color) {
    if (count < 3) return;
    int top = Height, bottom = -1;
    for (unsigned i = 0; i < count; ++i) { top = minimum(top, points[i].y); bottom = maximum(bottom, points[i].y); }
    for (int y = maximum(0, top); y <= bottom && y < Height; ++y) {
      int left = Width, right = -1;
      for (unsigned i = 0; i < count; ++i) {
        const Point a = points[i], b = points[(i + 1) % count];
        if (y < minimum(a.y, b.y) || y > maximum(a.y, b.y)) continue;
        if (a.y == b.y) { left = minimum(left, minimum(a.x, b.x)); right = maximum(right, maximum(a.x, b.x)); }
        else { const int x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y); left = minimum(left, x); right = maximum(right, x); }
      }
      if (right >= left) rect(left, y, right - left + 1, 1, color);
    }
  }
  void spike(int x, int y, int width, int height, int direction) {
    Point outer[3], inner[3];
    if (direction < 2) {
      const int tip = direction ? y + height - 1 : y, base = direction ? y : y + height - 1;
      outer[0] = {x + width / 2, tip}; outer[1] = {x, base}; outer[2] = {x + width - 1, base};
      inner[0] = {x + width / 2, tip + (direction ? -3 : 3)};
      inner[1] = {x + 2, base + (direction ? 1 : -1)}; inner[2] = {x + width - 3, base + (direction ? 1 : -1)};
    } else {
      const int tip = direction == 3 ? x + width - 1 : x, base = direction == 3 ? x : x + width - 1;
      outer[0] = {tip, y + height / 2}; outer[1] = {base, y}; outer[2] = {base, y + height - 1};
      inner[0] = {tip + (direction == 3 ? -3 : 3), y + height / 2};
      inner[1] = {base + (direction == 3 ? 1 : -1), y + 2}; inner[2] = {base + (direction == 3 ? 1 : -1), y + height - 3};
    }
    polygon(outer, 3, rgb(255, 255, 255)); polygon(inner, 3, rgb(0, 15, 40));
  }
  void text(const char* label, int x, int y, uint16_t color, int scale = 1, int maxChars = 39) {
    glyphText(label, x + 1, y + 1, rgb(0, 18, 57), scale, maxChars);
    glyphText(label, x, y, color, scale, maxChars);
  }
  void glyphText(const char* label, int x, int y, uint16_t color, int scale, int maxChars) {
    if (!physicalScanOrder_) { MenuFont::draw(pixels_, Width, Height, label, x, y, color, scale, maxChars); return; }
    if (scale == 1) {
      const int top = maximum(0, -y), bottom = minimum(MenuFont::Height, Height - y);
      if (top >= bottom) return;
      for (int i = 0; label && label[i] && i < maxChars; ++i) {
        const uint8_t* rows = MenuFont::glyph(label[i]);
        const int startX = x + i * MenuFont::Advance;
        const int left = maximum(0, -startX), right = minimum(MenuFont::Width, Width - startX);
        for (int column = left; column < right; ++column) {
          uint16_t* out = pixels_ + (startX + column) * Height + Height - y - bottom;
          const unsigned bit = 1u << (MenuFont::Width - 1 - column);
          for (int row = bottom - 1; row >= top; --row, ++out) if (rows[row] & bit) *out = color;
        }
      }
      return;
    }
    for (int i = 0; label && label[i] && i < maxChars; ++i) {
      const uint8_t* rows = MenuFont::glyph(label[i]);
      for (int row = 0; row < MenuFont::Height; ++row)
        for (int column = 0; column < MenuFont::Width; ++column)
          if (rows[row] & (1u << (MenuFont::Width - 1 - column)))
            rect(x + (i * MenuFont::Advance + column) * scale, y + row * scale, scale, scale, color);
    }
  }
  void centered(const char* label, int y, uint16_t color, int scale = 1) {
    text(label, (Width - MenuFont::textWidth(label, scale, 39)) / 2, y, color, scale);
  }
};

}  // namespace flappy
