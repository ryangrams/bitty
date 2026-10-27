#include "../firmware/flappy/geometry_scene.h"

#include <array>
#include <assert.h>
#include <iostream>
#include <limits>

using namespace flappy;

struct Memory {
  std::array<uint8_t, 32768> ram = {};
  std::array<uint8_t, 256> collisions = {};
  unsigned ramReads = 0, romReads = 0;
  uint16_t lastRam = 0;
  uint32_t lastRom = 0;

  uint8_t readRam(uint16_t address) {
    assert(address < ram.size()); ++ramReads; lastRam = address; return ram[address];
  }
  uint8_t readRom(uint32_t offset) {
    assert(offset >= GeometryScene::CollisionTableFileOffset);
    assert(offset < GeometryScene::CollisionTableFileOffset + 256);
    ++romReads; lastRom = offset;
    return collisions[offset - GeometryScene::CollisionTableFileOffset];
  }
  void word(uint16_t address, uint16_t value) {
    ram[address] = uint8_t(value); ram[address + 1] = uint8_t(value >> 8);
  }
};

static void snapshotsUseTheVerifiedLayout() {
  Memory m;
  m.ram[0x498] = 2; m.ram[0x47b] = 21; m.ram[0x7a] = 6; m.ram[0x96] = 1;
  m.word(0x4a2, 0x1234); m.word(0x4a4, 0x89ab); m.word(0x4a6, 0x2ef);
  m.word(0x43d, 0x5012); m.word(0x43f, 0x9876);
  m.word(0x441, 0xb123); m.word(0x443, 0x4567);
  m.word(0x445, 0x7fff); m.word(0x447, 0x8000);
  m.word(0x449, 0xffff); m.word(0x44b, 0xff00);
  m.word(0x7d, 0x179a); m.word(0x7f, 0x0123);
  m.ram[0x44d] = 0; m.ram[0x44e] = 2;
  m.ram[0x44f] = 3; m.ram[0x450] = 0;
  for (unsigned i = 0; i < 7; ++i) m.ram[0x5ab + i] = uint8_t(i + 1);
  for (unsigned i = 0; i < 16; ++i) {
    m.ram[0x515 + i] = uint8_t(i); m.ram[0x545 + i] = uint8_t(30 + i);
    m.ram[0x555 + i] = uint8_t(80 + i); m.ram[0x565 + i] = uint8_t(i & 1);
    m.ram[0x575 + i] = uint8_t(i & 2);
  }
  const auto before = m.ram;
  GeometryScene s; s.sample([&](uint16_t a) { return m.readRam(a); });
  assert(s.playing() && s.state == 2 && s.level == 21 && s.mode == 6 && s.dual);
  assert(s.scrollX == 0x89ab1234 && s.scrollY == 0x2ef);
  assert(s.attempt == 7654321);
  assert(s.linearScrollY() == 719 && s.groundY() == 193);
  assert(s.players[0].x == 0x5012 && s.players[1].x == 0x9876);
  assert(s.players[0].y == 0xb123 && s.players[1].y == 0x4567);
  assert(s.players[0].pixelX() == 80 && s.players[0].pixelY() == 177);
  assert(s.players[0].velX == 32767 && s.players[1].velX == -32768);
  assert(s.players[0].velY == -1 && s.players[1].velY == -256);
  assert(s.players[0].rotation == 0x179a && s.players[1].rotation == 0x0123);
  assert(!s.players[0].gravity && s.players[1].gravity);
  assert(s.players[0].mini && !s.players[1].mini);
  for (unsigned i = 0; i < 16; ++i) {
    const auto& o = s.objects[i];
    assert(o.type == i && o.x == 30 + i && o.y == 80 + i);
    assert(o.active == bool(i & 1) && o.activated == bool(i & 2));
  }
  assert(m.ram == before && m.romReads == 0);
  m.ram.fill(0); s.sample([&](uint16_t a) { return m.readRam(a); });
  assert(!s.playing() && !s.dual && !s.objects[15].active && s.scrollX == 0);
}

static void mapPagesAndColumnRingMatchNativeCoordinates() {
  Memory m;
  for (unsigned id = 0; id < 256; ++id) m.collisions[id] = uint8_t(id ^ 0xa5);
  for (unsigned page = 0; page < 4; ++page)
    for (unsigned row = 0; row < 15; ++row)
      for (unsigned column = 0; column < 16; ++column)
        m.ram[0x6000 + page * 256 + row * 16 + column] =
          uint8_t(page * 60 + row * 3 + column);
  auto ram = [&](uint16_t a) { return m.readRam(a); };
  auto rom = [&](uint32_t a) { return m.readRom(a); };
  GeometryScene s;
  for (int y = 0; y < 960; ++y) {
    for (int x : {0, 15, 16, 239, 240, 255, 256, 4095, 65536}) {
      const auto tile = s.tileAtWorld(ram, rom, x, y);
      const unsigned page = unsigned(y / 240), row = unsigned(y % 240 / 16);
      const unsigned column = unsigned(x / 16 % 16);
      const uint8_t expected = uint8_t(page * 60 + row * 3 + column);
      assert(tile.valid && tile.id == expected && tile.collision == uint8_t(expected ^ 0xa5));
      assert(m.lastRam == 0x6000 + page * 256 + row * 16 + column);
      assert((m.lastRam & 255) < 240);
    }
  }
  s.scrollX = 253; s.scrollY = 0x2ef;
  s.tileAtScreen(ram, rom, 3, 193);
  assert(m.lastRam == 0x63c0);
  s.tileAtScreen(ram, rom, 2, 192);
  assert(m.lastRam == 0x63bf);
  s.tileAtScreen(ram, rom, 2, 240);
  assert(m.lastRam == 0x63ef);
  s.scrollY = 0x00ef; s.tileAtScreen(ram, rom, 0, 1);
  assert(m.lastRam == 0x610f);
  s.scrollY = 0x0100; s.tileAtScreen(ram, rom, 0, -1);
  assert(m.lastRam == 0x60ef);
  for (const auto pair : {std::array<int, 2>{0x0000, 0}, {0x00ef, 239},
                          {0x0100, 240}, {0x0200, 480}, {0x02ef, 719}, {0x0300, 720}}) {
    s.scrollY = uint16_t(pair[0]); assert(s.linearScrollY() == pair[1]);
  }
}

static void invalidSamplesDoNotReadMemory() {
  Memory m; GeometryScene s;
  auto ram = [&](uint16_t a) { return m.readRam(a); };
  auto rom = [&](uint32_t a) { return m.readRom(a); };
  for (int y : {-1000, -1, 960, 1000, std::numeric_limits<int>::max()})
    assert(!s.tileAtWorld(ram, rom, 10, y).valid);
  assert(!s.tileAtWorld(ram, rom, -1, 1).valid);
  assert(!s.tileAtWorld(ram, rom, std::numeric_limits<int64_t>::min(), 1).valid);
  assert(!s.tileAtScreen(ram, rom, -1, 0).valid);
  assert(!s.tileAtScreen(ram, rom, 0, -1).valid);
  s.scrollY = 0xffff;
  assert(!s.tileAtScreen(ram, rom, 0, std::numeric_limits<int>::max()).valid);
  assert(m.ramReads == 0 && m.romReads == 0);
  s.scrollY = 0; s.scrollX = 0xffffffff;
  assert(s.tileAtScreen(ram, rom, std::numeric_limits<int>::max(), 0).valid);
}

static void collisionKindsPreserveGapsAndPartialShapes() {
  GeometryTile tile; tile.valid = true; tile.id = 7;
  assert(!tile.hasSolidRegion() && !tile.isHazard() && !tile.isSlope());
  tile.collision = uint8_t(GeometryCollision::All);
  assert(tile.hasSolidRegion() && !tile.isHazard() && !tile.isFloor());
  tile.collision = uint8_t(GeometryCollision::Death);
  assert(!tile.hasSolidRegion() && tile.isHazard());
  tile.collision = uint8_t(GeometryCollision::FloorCeiling);
  assert(tile.hasSolidRegion() && tile.isFloor() && !tile.isHazard());
  for (uint8_t collision = 0x0b; collision <= 0x1e; ++collision) {
    tile.collision = collision; assert(tile.isSlope() && tile.hasSolidRegion() && !tile.isHazard());
  }
  for (auto collision : {GeometryCollision::LeftSpikeBlock, GeometryCollision::RightSpikeBlock,
                         GeometryCollision::BottomCenterSpike, GeometryCollision::TopCenterSpike}) {
    tile.collision = uint8_t(collision); assert(tile.hasSolidRegion() && tile.isHazard());
  }
  tile.collision = 0xff; assert(!tile.hasSolidRegion() && !tile.isHazard());
  tile.valid = false; tile.collision = 7; assert(!tile.hasSolidRegion());
}

static void objectsKeepPortalsPadsCoinsAndTriggersDistinct() {
  GeometryObject o; o.active = true;
  for (unsigned type = 0; type <= 255; ++type) {
    o.type = uint8_t(type);
    if (type >= 0x80) assert(o.kind() == GeometryObjectKind::Hidden && !o.visible());
  }
  for (uint8_t type : {0, 1, 2, 3, 4, 8, 9, 0x14, 0x17, 0x18, 0x22, 0x24, 0x4b, 0x64}) {
    o.type = type; assert(o.kind() == GeometryObjectKind::Portal && o.visible());
  }
  for (uint8_t type : {5, 6, 0x0b, 0x27, 0x44, 0x45, 0x54, 0x7b}) {
    o.type = type; assert(o.kind() == GeometryObjectKind::Orb && o.visible());
  }
  for (uint8_t type : {0x0a, 0x0c, 0x0d, 0x0e, 0x25, 0x26, 0x52, 0x53, 0x56, 0x57, 0x65}) {
    o.type = type; assert(o.kind() == GeometryObjectKind::Pad && o.visible());
  }
  for (uint8_t type : {7, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x6e}) {
    o.type = type; assert(o.kind() == GeometryObjectKind::Coin);
    assert(o.collectedCoin() == (type >= 0x1c && type <= 0x1e));
  }
  for (uint8_t type : {0x0f, 0x40, 0x41, 0x42, 0x43, 0x47, 0x48, 0x6f, 0x70, 0x78, 0x7f}) {
    o.type = type; assert(o.kind() == GeometryObjectKind::Hidden && !o.visible());
  }
  o.type = 0x2e; assert(o.kind() == GeometryObjectKind::Decoration);
  o.type = 0x4a; assert(o.kind() == GeometryObjectKind::Decoration);
  o.type = 0; o.x = 240; o.y = 240; assert(o.visible());
  o.x = 241; assert(!o.visible()); o.x = 10; o.y = 241; assert(!o.visible());
  o.y = 10; o.active = false; assert(!o.visible());
}

static void progressUsesTheNativeLevelDivisor() {
  GeometryScene s; s.level = 21; s.state = 2;
  unsigned reads = 0;
  auto rom = [&](uint32_t offset) {
    assert(offset == GeometryScene::LevelLengthsFileOffset + 21);
    ++reads; return uint8_t(179);
  };
  s.scrollX = 0; assert(s.percentage(rom) == 0);
  s.scrollX = 178; assert(s.percentage(rom) == 0);
  s.scrollX = 179; assert(s.percentage(rom) == 1);
  s.scrollX = 179 * 50 + 178; assert(s.percentage(rom) == 50);
  s.scrollX = 179 * 100; assert(s.percentage(rom) == 99);
  s.scrollX = 0xffffffff; assert(s.percentage(rom) == 0);
  assert(reads == 5);
  s.state = 3; assert(s.percentage(rom) == 100 && reads == 5);
  s.level = 43; assert(s.percentage(rom) == 0 && reads == 5);
  s.level = 21; s.state = 2;
  assert(s.percentage([](uint32_t) { return uint8_t(0); }) == 0);
}

int main() {
  assert(GeometryScene::matchesCartridge(786448, 0x5bf705b3));
  assert(!GeometryScene::matchesCartridge(786447, 0x5bf705b3));
  assert(!GeometryScene::matchesCartridge(786448, 0));
  snapshotsUseTheVerifiedLayout();
  mapPagesAndColumnRingMatchNativeCoordinates();
  invalidSamplesDoNotReadMemory();
  collisionKindsPreserveGapsAndPartialShapes();
  objectsKeepPortalsPadsCoinsAndTriggersDistinct();
  progressUsesTheNativeLevelDivisor();
  std::cout << "Geometry scene checks passed\n";
}
