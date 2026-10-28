#include "../firmware/flappy/geometry_level.h"
#include <assert.h>
#include <iostream>
#include <vector>

using namespace flappy;
struct Cartridge {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(GeometryScene::RomBytes, 0);
  unsigned reads = 0;
  uint8_t read(uint32_t offset) { assert(offset < bytes.size()); ++reads; return bytes[offset]; }
  void pointer(uint32_t lo, uint32_t hi, uint32_t bank, uint8_t index, uint32_t file) {
    const uint32_t relative = file - 16;
    const uint16_t cpu = uint16_t(0xa000 + (relative & 8191));
    bytes[lo + index] = uint8_t(cpu); bytes[hi + index] = uint8_t(cpu >> 8);
    bytes[bank + index] = uint8_t(relative >> 13);
  }
  void header(uint8_t height, const std::vector<uint8_t>& stream) {
    pointer(GeometryLevelReader::LevelLow, GeometryLevelReader::LevelHigh,
            GeometryLevelReader::LevelBank, 0, 0x2010);
    bytes[0x2010 + 17] = height;
    for (unsigned i = 0; i < stream.size(); ++i) bytes[0x2010 + 18 + i] = stream[i];
  }
  void objects(const std::vector<uint8_t>& records) {
    bytes[0x2010] = 0; bytes[0x2011] = 0xa0; bytes[0x2012] = 10;
    for (unsigned i = 0; i < records.size(); ++i) bytes[0x14010 + i] = records[i];
    bytes[0x14010 + records.size()] = 0xff;
  }
  void chunk(uint8_t index, const std::vector<uint8_t>& stream) {
    const uint32_t file = 0x4010 + index * 8192;
    pointer(GeometryLevelReader::ChunkLow, GeometryLevelReader::ChunkHigh,
            GeometryLevelReader::ChunkBank, index, file);
    for (unsigned i = 0; i < stream.size(); ++i) bytes[file + i] = stream[i];
  }
  static std::vector<uint8_t> literal(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    for (uint8_t b : data) { out.push_back(0x80); out.push_back(b); }
    out.push_back(0xff); return out;
  }
};

static void nativeRunsAndBottomAlignmentArePreserved() {
  Cartridge rom; rom.header(2, Cartridge::literal({1, 9, 0x85, 2, 17}));
  GeometryLevelReader reader;
  auto read = [&](uint32_t a) { return rom.read(a); };
  assert(reader.begin(0, read) && reader.height() == 2);
  uint8_t rows[57];
  for (unsigned column = 0; column < 3; ++column) {
    assert(reader.decodeColumn(read, rows));
    for (unsigned y = 0; y < 55; ++y) assert(rows[y] == 0);
    const uint8_t expected[3][2] = {{9, 9}, {5, 17}, {17, 17}};
    assert(rows[55] == expected[column][0] && rows[56] == expected[column][1]);
  }
  assert(reader.nextColumn() == 3);
  assert(reader.decodeColumn(read, rows) && rows[55] == 0 && rows[56] == 0);
}

static void historyCopiesUseASnapshotAndWrapAt256Bytes() {
  Cartridge rom;
  std::vector<uint8_t> data(256, 0x81); data.back() = 0x82;
  auto encoded = Cartridge::literal(data); encoded.pop_back();
  encoded.insert(encoded.end(), {2, 255, 0x83, 0xff});
  rom.header(1, encoded);
  GeometryLevelReader reader; auto read = [&](uint32_t a) { return rom.read(a); };
  assert(reader.begin(0, read)); uint8_t rows[57];
  for (unsigned col = 0; col < 259; ++col) {
    assert(reader.decodeColumn(read, rows));
    const uint8_t expected = col == 255 || col == 256 ? 2 : (col == 258 ? 3 : 1);
    assert(rows[56] == expected);
  }
}

static void continuationSwitchesPreservePartialColumns() {
  Cartridge rom; rom.header(2, Cartridge::literal({0x87, 0x7f, 0x7f, 0}));
  rom.chunk(0, Cartridge::literal({1, 9, 0x8a}));
  GeometryLevelReader reader; auto read = [&](uint32_t a) { return rom.read(a); };
  assert(reader.begin(0, read)); uint8_t rows[57];
  assert(reader.decodeColumn(read, rows) && rows[55] == 7 && rows[56] == 9);
  assert(reader.decodeColumn(read, rows) && rows[55] == 9 && rows[56] == 10);
}

static void prefetchIsBoundedAndKeepsUnknownColumnsInvalid() {
  Cartridge rom; rom.header(1, Cartridge::literal({127, 9}));
  GeometryLevelReader reader; GeometryWorld world; GeometryScene scene;
  scene.state = 2; scene.attempt = 1;
  auto read = [&](uint32_t a) { return rom.read(a); };
  assert(!reader.prepare(scene, read, world, 320, 2));
  assert(reader.nextColumn() == 2);
  uint8_t id = 0;
  assert(world.tileId(0, 896, id) && id == 9);
  assert(!world.tileId(32, 896, id));
  for (unsigned i = 0; i < 10; ++i) reader.prepare(scene, read, world, 320, 2);
  assert(reader.nextColumn() == 21);
  assert(world.tileId(320, 896, id) && id == 9);
  assert(!world.tileId(336, 896, id));
  scene.attempt = 2;
  assert(!reader.prepare(scene, read, world, 320, 1) && reader.nextColumn() == 1);
  scene.scrollX = 0xffffff00u;
  assert(!reader.prepare(scene, read, world));
}

static void immutablePreparationNeedsNoNativeTerrainReads() {
  Cartridge rom; rom.header(2, Cartridge::literal({127, 9}));
  GeometryLevelReader reader; GeometryWorld world; GeometryScene scene;
  scene.state = 2; scene.attempt = 1;
  auto read = [&](uint32_t a) { return rom.read(a); };
  assert(reader.prepare(scene, read, world));
  uint8_t id = 0;
  assert(world.tileId(320, 880, id) && id == 9);
  assert(world.tileId(0, 0, id) && id == 0);
  scene.attempt = 2;
  assert(!reader.prepare(scene, read, world, 320, 1));
  assert(world.tileId(0, 880, id) && id == 9);
  assert(!world.tileId(320, 880, id));
  scene.scrollX = 0xffffff00u;
  assert(!reader.prepare(scene, read, world));
  assert(!world.tileId(0, 880, id));
}

static void previewObjectsCoverTheForwardBandWithoutDuplicates() {
  Cartridge rom; rom.header(1, Cartridge::literal({127, 0}));
  rom.objects({241, 0, 112, 3, 5, 0, 1, 113, 3, 0, 250, 0, 114, 3, 10,
               74, 1, 112, 3, 6});
  auto read = [&](uint32_t a) { return rom.read(a); };
  GeometryLevelReader reader; assert(reader.begin(0, read));
  GeometryScene scene; scene.state = 2; scene.scrollY = 0x2ef;
  assert(reader.preparePreviews(scene, read) && scene.previewCount == 3);
  assert(scene.previews[0].x == 241 && scene.previews[0].y == 160);
  assert(scene.previews[1].x == 256 && scene.previews[1].y == 161);
  assert(scene.previews[2].x == 250 && scene.previews[2].y == 162);
  for (unsigned i = 0; i < scene.previewCount; ++i) {
    const auto& object = scene.previews[i];
    assert(object.preview && object.active && !object.activated && object.visible());
    assert(object.x > 240);
  }
  scene.scrollX = 1;
  assert(reader.preparePreviews(scene, read) && scene.previewCount == 2);
  const unsigned before = rom.reads;
  assert(!reader.preparePreviews(scene, read, 0) && scene.previewCount == 0);
  assert(rom.reads == before);
}

static void malformedStreamsFailWithinBoundedReads() {
  Cartridge rom; GeometryLevelReader reader;
  auto read = [&](uint32_t a) { return rom.read(a); };
  assert(!reader.begin(22, read) && rom.reads == 0);
  rom.header(58, {0xff}); assert(!reader.begin(0, read));
  rom.header(1, {0x81}); assert(reader.begin(0, read));
  uint8_t rows[57]; assert(!reader.decodeColumn(read, rows) && reader.failed());
  rom.header(1, Cartridge::literal({0x7f, 0x7f, 4}));
  assert(reader.begin(0, read)); assert(!reader.decodeColumn(read, rows));
  rom.header(1, Cartridge::literal({0x7f, 0x7f, 0}));
  rom.chunk(0, Cartridge::literal({0x7f, 0x7f, 0}));
  assert(reader.begin(0, read)); const unsigned before = rom.reads;
  assert(!reader.decodeColumn(read, rows) && rom.reads - before < 100);
}

int main() {
  nativeRunsAndBottomAlignmentArePreserved();
  historyCopiesUseASnapshotAndWrapAt256Bytes();
  continuationSwitchesPreservePartialColumns();
  prefetchIsBoundedAndKeepsUnknownColumnsInvalid();
  immutablePreparationNeedsNoNativeTerrainReads();
  previewObjectsCoverTheForwardBandWithoutDuplicates();
  malformedStreamsFailWithinBoundedReads();
  std::cout << "Geometry level decoder checks passed\n";
}
