#include "../firmware/flappy/tetris_clock.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

using flappy::ClockTime;
using flappy::TetrisClock;

static ClockTime clockAt(uint32_t frame, unsigned hour = 12, unsigned minute = 34) {
  ClockTime time;
  time.hour = uint8_t(hour); time.minute = uint8_t(minute);
  time.second = uint8_t((frame / 60) % 60); time.ms = frame * 1000 / 60;
  return time;
}

static std::array<uint8_t, 200> cells(const TetrisClock& clock) {
  std::array<uint8_t, 200> board = {};
  for (unsigned y = 0; y < 20; ++y)
    for (unsigned x = 0; x < 10; ++x) board[y * 10 + x] = clock.cell(x, y);
  return board;
}

static void genuineTetrominoes() {
  std::set<std::set<std::pair<int,int>>> unique;
  for (unsigned family = 0; family < 7; ++family) {
    for (unsigned rotation = 0; rotation < 4; ++rotation) {
      std::set<std::pair<int,int>> shape;
      for (unsigned i = 0; i < 4; ++i) {
        int x = -1, y = -1;
        assert(TetrisClock::tetrominoCell(family, rotation, i, x, y));
        assert(x >= 0 && x < 4 && y >= 0 && y < 4);
        shape.insert({x,y});
      }
      assert(shape.size() == 4);
      std::set<std::pair<int,int>> reached = {*shape.begin()};
      for (unsigned pass = 0; pass < 4; ++pass)
        for (auto point : shape)
          for (auto other : shape)
            if (reached.count(other) && abs(point.first - other.first) + abs(point.second - other.second) == 1)
              reached.insert(point);
      assert(reached == shape);
      unique.insert(shape);
    }
  }
  assert(unique.size() == 19);
  int x = 123, y = 456;
  assert(!TetrisClock::tetrominoCell(7,0,0,x,y));
  assert(!TetrisClock::tetrominoCell(0,4,0,x,y));
  assert(!TetrisClock::tetrominoCell(0,0,4,x,y));
  assert(x == 123 && y == 456);
}

static void assertValidDrop(const TetrisClock& clock) {
  if (!clock.hasActivePiece()) return;
  const TetrisClock::Piece piece = clock.activePiece();
  assert(piece.family < 7 && piece.rotation < 4 && piece.y >= 0);
  bool supported = false;
  for (unsigned i = 0; i < 4; ++i) {
    int dx, dy; TetrisClock::tetrominoCell(piece.family,piece.rotation,i,dx,dy);
    const int x = piece.x + dx, y = piece.y + dy;
    assert(x >= 0 && x < 10 && y >= 0 && y < 20);
    assert(clock.cell(unsigned(x),unsigned(y)) == 0);
    if (y == 19 || clock.cell(unsigned(x),unsigned(y+1))) supported = true;
    for (int row = 0; row < y; ++row) assert(clock.cell(unsigned(x),unsigned(row)) == 0);
  }
  assert(supported);
  assert(clock.activeYQ8() >= -4 * 256 && clock.activeYQ8() <= piece.y * 256);
}

static void assemblyAndClears() {
  std::set<std::array<uint8_t,200>> arrangements;
  std::set<unsigned> families, wells;
  for (uint32_t seed = 1; seed <= 48; ++seed) {
    TetrisClock clock;
    clock.reset(seed);
    uint32_t readStarted = 0, readEnded = 0;
    uint32_t firstClear = 0;
    unsigned maximumChanged = 0;
    bool sawRead = false, sawWipe = false;
    for (uint32_t frame = 0; frame < 1081; ++frame) {
      const auto before = cells(clock);
      const auto phaseBefore = clock.phase();
      const unsigned lockedBefore = clock.piecesLocked();
      const unsigned linesBefore = clock.linesCleared();
      clock.step(clockAt(frame));
      assertValidDrop(clock);
      if (clock.hasActivePiece()) families.insert(clock.activePiece().family);
      const auto after = cells(clock);
      if (clock.piecesLocked() > lockedBefore && phaseBefore == TetrisClock::Phase::Assemble) {
        unsigned changed = 0;
        for (unsigned i = 0; i < 200; ++i) {
          if (before[i] != after[i]) { assert(before[i] == 0 && after[i] != 0); ++changed; }
        }
        assert(changed == 4);
        maximumChanged = changed > maximumChanged ? changed : maximumChanged;
      }
      if (clock.phase() == TetrisClock::Phase::Read && !sawRead) {
        sawRead = true; readStarted = frame; wells.insert(clock.well());
        arrangements.insert(after);
        for (unsigned y = 0; y < 20; ++y)
          for (unsigned x = 0; x < 10; ++x) {
            const bool ink = (clock.cell(x,y) & 0x80) != 0;
            assert(ink == TetrisClock::clockCell(clockAt(frame),x,y));
            if (x < 4 || x >= 6) assert((clock.cell(x,y) != 0) == (y >= 4));
            if (y < 4) assert(clock.cell(x,y) == 0);
          }
      }
      if (sawRead && !readEnded && clock.phase() == TetrisClock::Phase::Play) readEnded = frame;
      if (clock.linesCleared() != linesBefore) {
        const unsigned increment = clock.linesCleared() - linesBefore;
        assert(increment >= 1 && increment <= 4);
        if (!firstClear) firstClear = frame;
        assert(frame > readEnded && readEnded > readStarted);
      }
      if (clock.phase() == TetrisClock::Phase::Wipe) sawWipe = true;
      for (uint8_t value : after) assert(value == 0 || ((value & 7) >= 1 && (value & 7) <= 7));
    }
    assert(maximumChanged == 4 && sawRead && sawWipe);
    assert(readStarted < 420 && readEnded - readStarted == 240);
    assert(firstClear && clock.linesCleared() >= 16);
    assert(clock.cycles() == 2);
  }
  std::cerr << "well variants=" << wells.size() << " families=" << families.size() << " arrangements=" << arrangements.size() << "\n";
  assert(wells.size() == 3 && families.size() == 7 && arrangements.size() >= 40);
}

static void deterministicAndRollover() {
  TetrisClock a, b;
  a.reset(0x1234); b.reset(0x1234);
  for (uint32_t frame = 0; frame < 5400; ++frame) {
    ClockTime time = clockAt(frame,23,59);
    if (frame >= 3600) { time.hour = 0; time.minute = 0; }
    a.step(time); b.step(time);
    assert(a.phase() == b.phase() && a.cycleFrame() == b.cycleFrame());
    assert(a.cycles() == b.cycles() && a.piecesLocked() == b.piecesLocked());
    assert(a.linesCleared() == b.linesCleared() && cells(a) == cells(b));
    if (frame == 3600) assert(a.phase() == TetrisClock::Phase::Assemble && a.cycleFrame() == 1);
  }
  ClockTime time = clockAt(0,12,0); time.twentyFourHour = false;
  assert(TetrisClock::clockCell(time,1,4));
  a.step(time);
  const uint32_t cycles = a.cycles();
  time.twentyFourHour = true;
  a.step(time);
  assert(a.cycles() == cycles + 1 && a.cycleFrame() == 1);
  a.reset(0);
  assert(a.cycles() == 0 && a.piecesLocked() == 0 && a.linesCleared() == 0);
  a.step(time);
  assert(a.cycles() == 1);
}

static void everyTimeIsReadable() {
  TetrisClock clock;
  for (unsigned hour = 0; hour < 24; ++hour)
    for (unsigned minute = 0; minute < 60; ++minute) {
      clock.reset(hour * 60 + minute + 1);
      ClockTime time = clockAt(0,hour,minute);
      unsigned frame = 0;
      while (clock.phase() != TetrisClock::Phase::Read && frame < 500) { clock.step(time); ++frame; }
      assert(frame < 500);
      for (unsigned y = 0; y < 20; ++y)
        for (unsigned x = 0; x < 10; ++x)
          assert(((clock.cell(x,y) & 0x80) != 0) == TetrisClock::clockCell(time,x,y));
    }
}

static void renderingAndLongRun() {
  constexpr unsigned pixels = flappy::ClockScene::Width * flappy::ClockScene::Height;
  std::array<uint16_t,pixels + 32> guarded;
  guarded.fill(0x4a69);
  for (unsigned seed = 1; seed <= 16; ++seed) {
    TetrisClock clock; clock.reset(seed * 919);
    for (uint32_t frame = 0; frame < 1080 * 6; ++frame) {
      ClockTime time = clockAt(frame,7,8);
      clock.step(time);
      if (!(frame % 31)) {
        const auto before = cells(clock);
        clock.draw(guarded.data() + 16,time);
        assert(cells(clock) == before);
        for (unsigned i = 0; i < 16; ++i)
          assert(guarded[i] == 0x4a69 && guarded[pixels + 16 + i] == 0x4a69);
      }
      assertValidDrop(clock);
    }
    assert(clock.cycles() == 6 && clock.linesCleared() >= 96);
    assert(clock.piecesLocked() >= 6 * 40);
    clock.draw(nullptr,clockAt(0));
  }
}

static void writePreview(const std::string& path, const TetrisClock& clock, const ClockTime& time) {
  std::array<uint16_t,135 * 240> pixels;
  clock.draw(pixels.data(),time);
  std::ofstream file(path,std::ios::binary);
  file << "P6\n135 240\n255\n";
  for (uint16_t value : pixels) {
    const char rgb[3] = {char(((value >> 11) & 31) * 255 / 31),
      char(((value >> 5) & 63) * 255 / 63),char((value & 31) * 255 / 31)};
    file.write(rgb,3);
  }
}

int main(int argc, char** argv) {
  genuineTetrominoes();
  assemblyAndClears();
  deterministicAndRollover();
  everyTimeIsReadable();
  renderingAndLongRun();
  assert(sizeof(TetrisClock) <= 1024);
  if (argc > 1) {
    TetrisClock clock; clock.reset(0x72632);
    bool savedRead = false, savedClear = false, savedPlay = false;
    for (uint32_t frame = 0; frame < 1080; ++frame) {
      const ClockTime time = clockAt(frame);
      clock.step(time);
      if (frame == 120) writePreview(std::string(argv[1]) + "/assembling.ppm",clock,time);
      if (!savedRead && clock.phase() == TetrisClock::Phase::Read) {
        savedRead = true; writePreview(std::string(argv[1]) + "/time.ppm",clock,time);
      }
      if (!savedClear && clock.clearingRows()) {
        savedClear = true; writePreview(std::string(argv[1]) + "/clearing.ppm",clock,time);
      }
      if (!savedPlay && clock.linesCleared() > 18) {
        savedPlay = true; writePreview(std::string(argv[1]) + "/playing.ppm",clock,time);
      }
    }
  }
  std::cout << "Tetris clock tests passed. State: " << sizeof(TetrisClock) << " bytes.\n";
}
