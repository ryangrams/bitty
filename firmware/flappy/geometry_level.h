#pragma once
#include "geometry_scene.h"
#include <string.h>

namespace flappy {

// Read-only decoder for the fingerprinted Famidash v1.2.8 cartridge.
// These offsets identify its pointer tables. Level bytes remain in the supplied ROM.
class GeometryLevelReader {
 public:
  static constexpr uint32_t LevelLow = 0x7fca5, LevelHigh = 0x7fcd0, LevelBank = 0x7fcfb;
  static constexpr uint32_t ChunkLow = 0x7fd26, ChunkHigh = 0x7fd2a, ChunkBank = 0x7fd2e;
  static constexpr unsigned ChunkCount = 4, LevelRows = 57;

  void reset() { initialized_ = false; failed_ = false; }
  bool failed() const { return failed_; }
  uint8_t height() const { return height_; }
  uint32_t nextColumn() const { return nextColumn_; }

  template <class ReadRom> bool begin(uint8_t level, ReadRom readRom) {
    reset();
    if (level >= GeometryScene::OfficialLevelCount) return fail();
    uint32_t header = 0;
    if (!tableOffset(readRom, LevelLow, LevelHigh, LevelBank, level, header)) return fail();
    if (header + 18 > bankEnd(header)) return fail();
    height_ = readRom(header + 17);
    if (!height_ || height_ > LevelRows) return fail();
    level_ = level;
    nextColumn_ = 0;
    runLeft_ = 0;
    initLz(header + 18);
    const uint16_t spriteAddress = uint16_t(readRom(header)) |
                                    uint16_t(uint16_t(readRom(header + 1)) << 8);
    const uint8_t spriteBank = readRom(header + 2);
    if (spriteAddress >= 0xa000 && spriteAddress < 0xc000 && spriteBank < 64) {
      objectCursor_ = PrgStart + uint32_t(spriteBank) * 8192 + spriteAddress - 0xa000;
      objectEnd_ = bankEnd(objectCursor_);
    } else objectCursor_ = objectEnd_ = 0;
    initialized_ = true;
    return true;
  }

  template <class ReadRom> bool decodeColumn(ReadRom readRom, uint8_t* rows) {
    if (!initialized_ || failed_) return false;
    unsigned row = LevelRows - height_;
    memset(rows, 0, row);
    while (row < LevelRows) {
      if (!runLeft_) {
        if (!nextTile(readRom, rows[row])) return false;
        ++row;
      } else {
        const unsigned remaining = LevelRows - row;
        const unsigned count = runLeft_ < remaining ? runLeft_ : remaining;
        memset(rows + row, runValue_, count);
        row += count;
        runLeft_ = uint8_t(runLeft_ - count);
      }
    }
    ++nextColumn_;
    return true;
  }

  template <class ReadRom>
  bool prepare(GeometryScene& scene, ReadRom readRom, GeometryWorld& world,
               unsigned aheadPixels = 320, unsigned columnBudget = 24) {
    if (!world.prepareFrame(scene)) {
      reset();
      return false;
    }
    if (!initialized_ || level_ != scene.level || attempt_ != scene.attempt ||
        scene.scrollX < lastScrollX_) {
      if (!begin(scene.level, readRom)) return false;
      attempt_ = scene.attempt;
    }
    lastScrollX_ = scene.scrollX;
    const uint64_t target = (uint64_t(scene.scrollX) + aheadPixels) >> 4;
    if (target > 65535) return fail();
    const uint32_t base = scene.scrollX >> 4;
    while (nextColumn_ <= target && columnBudget) {
      --columnBudget;
      const uint32_t column = nextColumn_;
      if (!decodeColumn(readRom, column_)) return false;
      // A bounded seek can cross old columns before reaching the visible window.
      if (uint64_t(column) + GeometryWorld::ColumnCount > base)
        world.storeLevelColumn(column, column_);
    }
    preparePreviews(scene, readRom);
    return nextColumn_ > target;
  }

  template <class ReadRom>
  bool preparePreviews(GeometryScene& scene, ReadRom readRom, unsigned recordBudget = 64) {
    scene.previewCount = 0;
    if (!initialized_ || !objectCursor_ || !scene.playing() ||
        (scene.scrollX & 0x80000000u)) return false;
    const uint64_t first = uint64_t(scene.scrollX) + 241, last = uint64_t(scene.scrollX) + 320;
    uint16_t x = 0, y = 0; uint8_t type = 0;
    // Official streams follow tile order, with at most24px of backwards offset.
    const unsigned orderSlack = 32;
    while (recordBudget) {
      --recordBudget;
      if (!objectRecord(readRom, objectCursor_, x, y, type)) return true;
      if (uint64_t(x) + orderSlack >= first) break;
      objectCursor_ += 5;
    }
    if (!recordBudget) return false;
    uint32_t cursor = objectCursor_;
    while (recordBudget--) {
      if (!objectRecord(readRom, cursor, x, y, type)) return true;
      if (uint64_t(x) > last + orderSlack) return true;
      if (uint64_t(x) >= first && uint64_t(x) <= last) {
        GeometryObject object;
        object.type = type;
        object.x = int16_t(uint64_t(x) - scene.scrollX);
        const int screenY = int(y) - scene.linearScrollY() - 1;
        object.y = int16_t(screenY >= -1024 && screenY <= 1024 ? screenY : 2048);
        object.active = object.preview = true;
        if (object.visible() && object.kind() != GeometryObjectKind::Decoration &&
            scene.previewCount < GeometryScene::PreviewCount)
          scene.previews[scene.previewCount++] = object;
      }
      cursor += 5;
    }
    return false;
  }

 private:
  static constexpr uint32_t PrgStart = 16, PrgEnd = 0x80010;
  uint8_t history_[256] = {}, pending_[128] = {}, column_[LevelRows] = {};
  uint32_t position_ = 0, end_ = 0, nextColumn_ = 0;
  uint32_t objectCursor_ = 0, objectEnd_ = 0;
  uint32_t attempt_ = 0, lastScrollX_ = 0;
  uint8_t historyPosition_ = 0, pendingPosition_ = 0, pendingCount_ = 0;
  uint8_t runLeft_ = 0, runValue_ = 0, level_ = 0, height_ = 0;
  bool initialized_ = false, failed_ = false, done_ = false;

  template <class ReadRom>
  bool objectRecord(ReadRom readRom, uint32_t cursor, uint16_t& x, uint16_t& y,
                    uint8_t& type) const {
    if (cursor >= objectEnd_ || readRom(cursor) == 0xff || cursor + 5 > objectEnd_) return false;
    x = uint16_t(readRom(cursor)) | uint16_t(uint16_t(readRom(cursor + 1)) << 8);
    y = uint16_t(readRom(cursor + 2)) | uint16_t(uint16_t(readRom(cursor + 3)) << 8);
    type = readRom(cursor + 4);
    return true;
  }

  bool fail() { failed_ = true; return false; }
  static uint32_t bankEnd(uint32_t offset) {
    return PrgStart + (((offset - PrgStart) >> 13) + 1) * 8192;
  }
  template <class ReadRom>
  static bool tableOffset(ReadRom readRom, uint32_t lo, uint32_t hi, uint32_t bank,
                          uint8_t index, uint32_t& offset) {
    const uint16_t address = uint16_t(readRom(lo + index)) |
                             uint16_t(uint16_t(readRom(hi + index)) << 8);
    const uint8_t page = readRom(bank + index);
    if (address < 0xa000 || address >= 0xc000 || page >= 64) return false;
    offset = PrgStart + uint32_t(page) * 8192 + (address - 0xa000);
    return offset < PrgEnd;
  }
  void initLz(uint32_t position) {
    position_ = position;
    end_ = bankEnd(position);
    historyPosition_ = pendingPosition_ = pendingCount_ = 0;
    done_ = false;
    for (unsigned i = 0; i < 256; ++i) history_[i] = 0;
  }
  template <class ReadRom> bool compressed(ReadRom readRom, uint8_t& value) {
    if (position_ >= end_ || position_ >= PrgEnd) return fail();
    value = readRom(position_++);
    return true;
  }
  bool emit(uint8_t value, uint8_t& output) {
    history_[historyPosition_++] = value;
    output = value;
    return true;
  }
  template <class ReadRom> bool nextByte(ReadRom readRom, uint8_t& value) {
    if (pendingPosition_ < pendingCount_) return emit(pending_[pendingPosition_++], value);
    if (done_) { value = 0; return true; }
    uint8_t token = 0;
    if (!compressed(readRom, token)) return false;
    if (token == 0xff) { done_ = true; value = 0; return true; }
    if (token == 0x80) {
      uint8_t literal = 0;
      return compressed(readRom, literal) && emit(literal, value);
    }
    if (token > 0x7f) return fail();
    uint8_t source = 0, literal = 0;
    if (!compressed(readRom, source) || !compressed(readRom, literal)) return false;
    for (unsigned i = 0; i < token; ++i)
      pending_[i] = history_[uint8_t(source + i)];
    pending_[token] = literal;
    pendingCount_ = uint8_t(token + 1);
    pendingPosition_ = 1;
    return emit(pending_[0], value);
  }
  template <class ReadRom> bool nextTile(ReadRom readRom, uint8_t& value) {
    unsigned continuations = 0;
    while (!runLeft_) {
      uint8_t token = 0;
      if (!nextByte(readRom, token)) return false;
      if (token & 0x80) { runLeft_ = 1; runValue_ = token & 0x7f; }
      else {
        if (!nextByte(readRom, runValue_)) return false;
        if (token == 0x7f && runValue_ == 0x7f) {
          uint8_t chunk = 0; uint32_t offset = 0;
          if (++continuations > ChunkCount || !nextByte(readRom, chunk) || chunk >= ChunkCount ||
              !tableOffset(readRom, ChunkLow, ChunkHigh, ChunkBank, chunk, offset)) return fail();
          initLz(offset);
        } else runLeft_ = uint8_t(token + 1);
      }
    }
    --runLeft_;
    value = runValue_;
    return true;
  }
};

}  // namespace flappy
