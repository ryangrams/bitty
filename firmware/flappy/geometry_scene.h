#pragma once
#include <stddef.h>
#include <stdint.h>

namespace flappy {

// Compatibility facts for the author's Famidash v1.2.8 main release.
// Source: tfdsoft/famidash, commit 0df680d0e02f0ccb438f1224d162a0ab073b7204.
enum class GeometryObjectKind : uint8_t { Hidden, Portal, Orb, Pad, Coin, Decoration };

enum class GeometryCollision : uint8_t {
  None = 0, DeathRight = 1, DeathLeft = 2, DeathTop = 3, DeathBottom = 4,
  Top = 5, Bottom = 6, All = 7, Death = 8, FloorCeiling = 9,
  SlopeRightDown45 = 0x0b, SlopeLeftDown45 = 0x0c,
  SlopeRightDown22Right = 0x0d, SlopeLeftDown22Right = 0x0e,
  SlopeRightDown22Left = 0x0f, SlopeLeftDown22Left = 0x10,
  SlopeRightDown66Top = 0x11, SlopeLeftDown66Top = 0x12,
  SlopeRightDown66Bottom = 0x13, SlopeLeftDown66Bottom = 0x14,
  SlopeRightUp45 = 0x15, SlopeLeftUp45 = 0x16,
  SlopeRightUp22Left = 0x17, SlopeRightUp22Right = 0x18,
  SlopeLeftUp22Left = 0x19, SlopeLeftUp22Right = 0x1a,
  SlopeRightUp66Top = 0x1b, SlopeLeftUp66Top = 0x1c,
  SlopeRightUp66Bottom = 0x1d, SlopeLeftUp66Bottom = 0x1e,
  NoSide = 0x1f, UpperLeft = 0x20, UpperRight = 0x21,
  LowerLeft = 0x22, LowerRight = 0x23, Left = 0x24, Right = 0x25,
  UpperLeftLowerRight = 0x26, UpperRightLowerLeft = 0x27,
  DownLeftSpike = 0x28, DownRightSpike = 0x29, DownBothSpikes = 0x2a,
  LeftSpikeBlock = 0x2b, RightSpikeBlock = 0x2c,
  BottomLeftSpike = 0x2d, BottomRightSpike = 0x2e, BottomSpikes = 0x2f,
  UpLeftSpike = 0x30, UpRightSpike = 0x31, UpBothSpikes = 0x32,
  DeathTopRight = 0x33, DeathTopLeft = 0x34,
  DeathBottomRight = 0x35, DeathBottomLeft = 0x36,
  BottomRightStairs = 0x37, BottomLeftStairs = 0x38,
  TopRightStairs = 0x39, TopLeftStairs = 0x3a, TopSpikes = 0x3b,
  BottomCenterSpike = 0x3c, TopCenterSpike = 0x3d
};

struct GeometryTile {
  uint8_t id = 0;
  uint8_t collision = 0;
  bool valid = false;

  bool isSlope() const { return valid && collision >= 0x0b && collision <= 0x1e; }
  bool isFloor() const { return valid && collision == 9; }
  bool isHazard() const {
    return valid && ((collision >= 1 && collision <= 4) || collision == 8 ||
                     (collision >= 0x28 && collision <= 0x36) ||
                     (collision >= 0x3b && collision <= 0x3d));
  }
  bool hasSolidRegion() const {
    return valid && ((collision >= 5 && collision <= 7) || collision == 9 ||
                     (collision >= 0x0b && collision <= 0x27) ||
                     collision == 0x2b || collision == 0x2c ||
                     (collision >= 0x37 && collision <= 0x3a) ||
                     collision == 0x3c || collision == 0x3d);
  }
};

struct GeometryPlayer {
  uint16_t x = 0, y = 0, rotation = 0;
  int16_t velX = 0, velY = 0;
  bool gravity = false, mini = false;

  int pixelX() const { return x >> 8; }
  int pixelY() const { return y >> 8; }
};

struct GeometryObject {
  uint8_t type = 0xff, x = 0, y = 0;
  bool active = false, activated = false;

  GeometryObjectKind kind() const {
    if ((type >= 0x2a && type <= 0x3f) || type == 0x4a)
      return GeometryObjectKind::Decoration;
    if (type <= 4 || (type >= 8 && type <= 9) ||
        (type >= 0x10 && type <= 0x19) || (type >= 0x20 && type <= 0x24) ||
        type == 0x4b || type == 0x4e || type == 0x4f ||
        (type >= 0x58 && type <= 0x5a) || (type >= 0x5f && type <= 0x64) ||
        (type >= 0x66 && type <= 0x6d)) return GeometryObjectKind::Portal;
    switch (type) {
      case 5: case 6: case 0x0b: case 0x1f: case 0x27: case 0x28: case 0x29:
      case 0x44: case 0x45: case 0x46: case 0x49: case 0x4c: case 0x4d:
      case 0x50: case 0x51: case 0x54: case 0x55:
      case 0x5b: case 0x5c: case 0x5d: case 0x5e:
      case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d:
        return GeometryObjectKind::Orb;
      case 0x0a: case 0x0c: case 0x0d: case 0x0e: case 0x25: case 0x26:
      case 0x52: case 0x53: case 0x56: case 0x57: case 0x65:
        return GeometryObjectKind::Pad;
      case 7: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x6e:
        return GeometryObjectKind::Coin;
      default: return GeometryObjectKind::Hidden;
    }
  }
  bool visible() const {
    return active && x <= 0xf0 && y <= 0xf0 && kind() != GeometryObjectKind::Hidden;
  }
  bool collectedCoin() const { return type >= 0x1c && type <= 0x1e; }
};

struct GeometryScene {
  static constexpr size_t RomBytes = 786448;
  static constexpr uint32_t RomCrc32 = 0x5bf705b3;
  static constexpr uint32_t CollisionTableFileOffset = 0x7dae0;
  static constexpr uint32_t LevelLengthsFileOffset = 0x7fd32;
  static constexpr unsigned OfficialLevelCount = 22, LevelCount = 43;
  static constexpr unsigned PlayerCount = 2, ObjectCount = 16;

  uint8_t state = 0, level = 0, mode = 0;
  uint32_t scrollX = 0;
  uint32_t attempt = 0;
  uint16_t scrollY = 0;
  bool dual = false;
  GeometryPlayer players[PlayerCount];
  GeometryObject objects[ObjectCount];

  static bool matchesCartridge(size_t bytes, uint32_t crc32) {
    return bytes == RomBytes && crc32 == RomCrc32;
  }

  // Readers return one byte from a CPU RAM address or a ROM file offset.
  // Callers verify matchesCartridge before sampling this release-specific layout.
  template <class ReadRam> void sample(ReadRam readRam) {
    state = readRam(0x0498); level = readRam(0x047b); mode = readRam(0x007a);
    dual = readRam(0x0096) != 0;
    scrollX = read32(readRam, 0x04a2); scrollY = read16(readRam, 0x04a6);
    attempt = 0;
    uint32_t place = 1;
    for (unsigned i = 0; i < 7; ++i, place *= 10)
      attempt += uint32_t(readRam(uint16_t(0x05ab + i))) * place;
    for (unsigned i = 0; i < PlayerCount; ++i) {
      GeometryPlayer& p = players[i];
      p.x = read16(readRam, uint16_t(0x043d + i * 2));
      p.y = read16(readRam, uint16_t(0x0441 + i * 2));
      p.velX = signed16(read16(readRam, uint16_t(0x0445 + i * 2)));
      p.velY = signed16(read16(readRam, uint16_t(0x0449 + i * 2)));
      p.rotation = read16(readRam, uint16_t(0x007d + i * 2));
      p.gravity = readRam(uint16_t(0x044d + i)) != 0;
      p.mini = readRam(uint16_t(0x044f + i)) != 0;
    }
    for (unsigned i = 0; i < ObjectCount; ++i) {
      GeometryObject& o = objects[i];
      o.type = readRam(uint16_t(0x0515 + i));
      o.x = readRam(uint16_t(0x0545 + i)); o.y = readRam(uint16_t(0x0555 + i));
      o.active = readRam(uint16_t(0x0565 + i)) != 0;
      o.activated = readRam(uint16_t(0x0575 + i)) != 0;
    }
  }

  bool playing() const { return state == 2; }
  int linearScrollY() const { return (scrollY >> 8) * 240 + (scrollY & 255); }
  int groundY() const { return 912 - linearScrollY(); }

  template <class ReadRom> uint8_t percentage(ReadRom readRom) const {
    if (level >= LevelCount) return 0;
    if (state == 3) return 100;
    // Native reset can briefly subtract past zero before the run starts.
    if (scrollX & 0x80000000u) return 0;
    const uint8_t pixelsPerPercent = readRom(LevelLengthsFileOffset + level);
    if (!pixelsPerPercent) return 0;
    const uint32_t value = scrollX / pixelsPerPercent;
    return uint8_t(value < 99 ? value : 99);
  }

  template <class ReadRam, class ReadRom>
  GeometryTile tileAtWorld(ReadRam readRam, ReadRom readRom, int64_t worldX, int worldY) const {
    if (worldX < 0 || worldY < 0 || worldY >= 960) return {};
    const unsigned page = unsigned(worldY / 240);
    const unsigned row = unsigned(worldY % 240) / 16;
    const unsigned column = unsigned(uint64_t(worldX) >> 4) & 15;
    const uint16_t address = uint16_t(0x6000 + page * 256 + row * 16 + column);
    GeometryTile tile;
    tile.id = readRam(address);
    tile.collision = readRom(CollisionTableFileOffset + tile.id);
    tile.valid = true;
    return tile;
  }

  template <class ReadRam, class ReadRom>
  GeometryTile tileAtScreen(ReadRam readRam, ReadRom readRom, int screenX, int screenY) const {
    const int64_t worldY = int64_t(linearScrollY()) + screenY;
    if (worldY < 0 || worldY >= 960) return {};
    return tileAtWorld(readRam, readRom, int64_t(scrollX) + screenX, int(worldY));
  }

 private:
  template <class ReadRam> static uint16_t read16(ReadRam r, uint16_t a) {
    return uint16_t(uint16_t(r(a)) | uint16_t(uint16_t(r(uint16_t(a + 1))) << 8));
  }
  template <class ReadRam> static uint32_t read32(ReadRam r, uint16_t a) {
    return uint32_t(read16(r, a)) | (uint32_t(read16(r, uint16_t(a + 2))) << 16);
  }
  static int16_t signed16(uint16_t value) {
    return value < 0x8000 ? int16_t(value) : int16_t(int32_t(value) - 0x10000);
  }
};

}  // namespace flappy
