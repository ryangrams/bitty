#pragma once

#include "clock_scene.h"
#include <stdint.h>

namespace flappy {

// A clock sculpture made from falling tetrominoes. A clear well lets the
// automatic player dismantle the sculpture with ordinary line clears.
class TetrisClock {
 public:
  enum { Columns = 10, Rows = 20, Cell = 10, BoardX = 17, BoardY = 25 };
  enum class Phase : uint8_t { Assemble, Read, Play, Wipe };
  enum class Family : uint8_t { I, O, T, S, Z, J, L };
  struct Piece { uint8_t family, rotation; int8_t x, y; };
  static constexpr uint16_t CycleFrames = 1080;

  void reset(uint32_t seed) {
    random_ = seed ? seed : 1;
    for (unsigned i = 0; i < 4; ++i) ClockScene::random(random_);
    cycles_ = locked_ = lines_ = 0;
    initialized_ = active_ = false;
    phase_ = Phase::Assemble;
    cycleFrame_ = phaseFrame_ = 0;
    clearMask_ = 0;
    clearFrame_ = 0;
    bagIndex_ = 7;
    for (unsigned y = 0; y < Rows; ++y)
      for (unsigned x = 0; x < Columns; ++x) board_[y][x] = 0;
  }

  void step(const ClockTime& time) {
    const uint16_t key = uint16_t(time.minuteKey() | (time.twentyFourHour ? 0x8000 : 0));
    if (!initialized_ || key != minuteKey_ || cycleFrame_ >= CycleFrames) begin(time, key);
    ++cycleFrame_;
    ++phaseFrame_;
    if (cycleFrame_ >= CycleFrames - 48 && phase_ != Phase::Wipe) {
      phase_ = Phase::Wipe; phaseFrame_ = 0; active_ = false; clearMask_ = 0;
    }
    if (phase_ == Phase::Wipe) {
      if (!(phaseFrame_ & 1) && phaseFrame_ / 2 < Rows)
        for (unsigned x = 0; x < Columns; ++x) board_[phaseFrame_ / 2][x] = 0;
      return;
    }
    if (clearMask_) {
      if (++clearFrame_ >= 14) collapse();
      return;
    }
    if (phase_ == Phase::Read) {
      if (phaseFrame_ >= 240) { phase_ = Phase::Play; phaseFrame_ = 0; }
      return;
    }
    if (!active_) {
      if (phase_ == Phase::Assemble) {
        if (planIndex_ >= planCount_) {
          phase_ = Phase::Read; phaseFrame_ = 0; return;
        }
        launch(plan_[planIndex_++]);
      } else if (scaffoldRows_) {
        const Piece piece = well_ == 0 ? Piece{1, 0, 4, 0}
            : Piece{0, 1, int8_t(well_ == 1 ? 5 : 4), 0};
        Piece drop = piece;
        drop.y = int8_t(landing(drop));
        if (drop.y < 0) { phase_ = Phase::Wipe; phaseFrame_ = 0; return; }
        launch(drop);
      } else if (!chooseMove()) {
        phase_ = Phase::Wipe; phaseFrame_ = 0; return;
      }
    }
    speedQ8_ = uint16_t(speedQ8_ + 128 < 896 ? speedQ8_ + 128 : 896);
    activeYQ8_ = int16_t(activeYQ8_ + speedQ8_);
    if (activeYQ8_ >= int16_t(piece_.y * 256)) {
      activeYQ8_ = int16_t(piece_.y * 256);
      lock(time);
    }
  }

  void draw(uint16_t* pixels, const ClockTime&) const {
    if (!pixels) return;
    using namespace ClockScene;
    clear(pixels, 0x0862);
    rect(pixels, BoardX - 3, BoardY - 3, Columns * Cell + 6, Rows * Cell + 6, 0x39e8);
    rect(pixels, BoardX - 1, BoardY - 1, Columns * Cell + 2, Rows * Cell + 2, 0x0000);
    for (int y = 0; y < Rows; ++y) {
      const bool flashing = (clearMask_ & (1u << y)) && clearFrame_ < 8;
      for (int x = 0; x < Columns; ++x) {
        const uint8_t cell = board_[y][x];
        if (!cell) {
          rect(pixels, BoardX + x * Cell + 4, BoardY + y * Cell + 4, 1, 1, 0x1083);
          continue;
        }
        uint16_t color = (cell & Ink) ? inkColor(uint8_t((cell & 7) - 1))
            : (cell & Scaffold) ? dimColor(uint8_t((cell & 7) - 1))
            : pieceColor(uint8_t((cell & 7) - 1));
        if (flashing) color = clearFrame_ & 2 ? 0x39e8 : 0xffff;
        block(pixels, BoardX + x * Cell, BoardY + y * Cell, color, Cell);
      }
    }
    if (active_) {
      for (unsigned i = 0; i < 4; ++i) {
        int dx, dy; tetrominoCell(piece_.family, piece_.rotation, i, dx, dy);
        const int px = BoardX + (piece_.x + dx) * Cell;
        const int py = BoardY + int(activeYQ8_ * Cell / 256) + dy * Cell;
        if (py + Cell > BoardY && py < BoardY + Rows * Cell)
          block(pixels, px, py, pieceColor(piece_.family), Cell, BoardY);
      }
    }
    const uint8_t nextFamily = phase_ == Phase::Assemble && planIndex_ < planCount_
        ? plan_[planIndex_].family : 0;
    for (unsigned i = 0; i < 4; ++i) {
      int x, y; tetrominoCell(nextFamily, 0, i, x, y);
      block(pixels, 58 + x * 4, 5 + y * 4, pieceColor(nextFamily), 4);
    }
    const uint16_t progress = uint16_t(cycleFrame_ * 104u / CycleFrames);
    rect(pixels, 15, 233, 104, 2, 0x2126);
    rect(pixels, 15, 233, progress, 2, 0x8cb5);
  }

  Phase phase() const { return phase_; }
  uint8_t cell(unsigned x, unsigned y) const { return x < Columns && y < Rows ? board_[y][x] : 0; }
  bool hasActivePiece() const { return active_; }
  Piece activePiece() const { return piece_; }
  int16_t activeYQ8() const { return activeYQ8_; }
  uint16_t cycleFrame() const { return cycleFrame_; }
  uint32_t cycles() const { return cycles_; }
  uint32_t piecesLocked() const { return locked_; }
  uint32_t linesCleared() const { return lines_; }
  uint8_t well() const { return well_; }
  uint32_t clearingRows() const { return clearMask_; }

  // Each family has four connected cells. Rotations are normalized to a
  // bounding box so the planner and the visible piece use the same geometry.
  static bool tetrominoCell(unsigned family, unsigned rotation, unsigned index, int& x, int& y) {
    if (family >= 7 || rotation >= 4 || index >= 4) return false;
    static const int8_t shapes[7][4][2] = {
      {{0,0},{1,0},{2,0},{3,0}}, {{0,0},{1,0},{0,1},{1,1}},
      {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{0,1},{1,1}},
      {{0,0},{1,0},{1,1},{2,1}}, {{0,0},{0,1},{1,1},{2,1}},
      {{2,0},{0,1},{1,1},{2,1}}
    };
    int minX = 4, minY = 4, wantedX = 0, wantedY = 0;
    for (unsigned i = 0; i < 4; ++i) {
      int sx = shapes[family][i][0], sy = shapes[family][i][1];
      for (unsigned r = 0; r < rotation; ++r) { const int oldX = sx; sx = -sy; sy = oldX; }
      if (sx < minX) minX = sx;
      if (sy < minY) minY = sy;
      if (i == index) { wantedX = sx; wantedY = sy; }
    }
    x = wantedX - minX; y = wantedY - minY;
    return true;
  }

  static bool clockCell(const ClockTime& time, unsigned x, unsigned y) {
    if (x >= Columns || y < 4 || y >= Rows || (x >= 4 && x < 6)) return false;
    const unsigned digitIndex = (y >= 13 ? 2u : 0u) + (x >= 6 ? 1u : 0u);
    const unsigned row = y >= 13 ? y - 13 : y - 4;
    if (row >= 7) return false;
    const unsigned column = x >= 6 ? x - 6 : x;
    static const uint8_t rows[10][7] = {
      {15,9,9,9,9,9,15}, {6,2,2,2,2,2,15}, {15,1,1,15,8,8,15},
      {15,1,1,15,1,1,15}, {9,9,9,15,1,1,1}, {15,8,8,15,1,1,15},
      {15,8,8,15,9,9,15}, {15,1,1,2,2,4,4}, {15,9,9,15,9,9,15},
      {15,9,9,15,1,1,15}
    };
    return (rows[time.digit(digitIndex)][row] & (8u >> column)) != 0;
  }

 private:
  enum : uint8_t { Ink = 0x80, Scaffold = 0x40, MaxPlan = 36 };
  uint8_t board_[Rows][Columns] = {};
  Piece plan_[MaxPlan] = {};
  Piece piece_ = {};
  uint8_t bag_[7] = {}, bagIndex_ = 7;
  uint8_t planCount_ = 0, planIndex_ = 0, well_ = 0, scaffoldRows_ = 0, clearFrame_ = 0;
  uint16_t cycleFrame_ = 0, phaseFrame_ = 0, minuteKey_ = 0, speedQ8_ = 0;
  int16_t activeYQ8_ = 0;
  uint32_t random_ = 1, cycles_ = 0, locked_ = 0, lines_ = 0, clearMask_ = 0;
  Phase phase_ = Phase::Assemble;
  bool initialized_ = false, active_ = false;

  static uint16_t pieceColor(unsigned family) {
    static const uint16_t colors[] = {0x2d9c,0xfe29,0xb2ff,0x5e6b,0xf9ca,0x435f,0xfd24};
    return colors[family < 7 ? family : 0];
  }
  static uint16_t inkColor(unsigned family) {
    static const uint16_t colors[] = {0xbfff,0xfff4,0xef3f,0xdff7,0xff59,0xb6df,0xff74};
    return colors[family < 7 ? family : 0];
  }
  static uint16_t dimColor(unsigned family) {
    const uint16_t color = pieceColor(family);
    return uint16_t((((color >> 11) & 31) / 5 << 11) |
                    (((color >> 5) & 63) / 5 << 5) | ((color & 31) / 5));
  }
  static void clippedRect(uint16_t* pixels, int x, int y, int width, int height, uint16_t color, int top) {
    if (y < top) { height -= top - y; y = top; }
    ClockScene::rect(pixels, x, y, width, height, color);
  }
  static void block(uint16_t* pixels, int x, int y, uint16_t color, int size, int top = 0) {
    if (size <= 1) return;
    clippedRect(pixels, x + 1, y + 1, size - 1, size - 1, color, top);
    if (size >= 5) {
      const unsigned r = (color >> 11) & 31, g = (color >> 5) & 63, b = color & 31;
      const uint16_t highlight = uint16_t(((r < 31 ? r + 1 : 31) << 11) |
          ((g < 63 ? g + 1 : 63) << 5) | (b < 31 ? b + 1 : 31));
      const uint16_t shade = uint16_t((color & 0xf7de) >> 1);
      clippedRect(pixels, x + 1, y + 1, size - 2, 1, highlight, top);
      clippedRect(pixels, x + 1, y + 2, 1, size - 3, highlight, top);
      clippedRect(pixels, x + 1, y + size - 1, size - 1, 1, shade, top);
      clippedRect(pixels, x + size - 1, y + 1, 1, size - 1, shade, top);
    }
  }

  void begin(const ClockTime&, uint16_t key) {
    initialized_ = true; minuteKey_ = key; ++cycles_;
    cycleFrame_ = phaseFrame_ = 0; phase_ = Phase::Assemble;
    active_ = false; clearMask_ = 0; clearFrame_ = 0;
    planCount_ = planIndex_ = 0; scaffoldRows_ = 16;
    for (unsigned y = 0; y < Rows; ++y)
      for (unsigned x = 0; x < Columns; ++x) board_[y][x] = 0;
    well_ = uint8_t(ClockScene::random(random_) % 3);
    // Each 4 by 4 mosaic is tiled with four tetrominoes in a legal vertical
    // drop order. The two towers leave a one-cell or two-cell clear well.
    static const uint8_t mosaics[16][4][4] = {
      {{6,0,1,2},{3,0,1,1},{5,1,0,1},{0,0,0,0}},
      {{5,0,0,2},{4,0,0,1},{6,3,2,1},{0,0,0,0}},
      {{1,0,1,2},{6,3,2,1},{5,1,0,1},{0,0,0,0}},
      {{5,3,2,1},{3,0,0,2},{2,1,0,0},{2,2,1,0}},
      {{5,3,2,1},{6,1,0,1},{1,0,1,1},{0,0,0,0}},
      {{6,1,0,1},{4,0,1,2},{5,2,1,1},{0,0,0,0}},
      {{6,0,1,2},{4,1,2,0},{5,1,1,0},{0,1,0,0}},
      {{2,0,0,2},{3,1,2,1},{2,1,0,0},{5,2,1,0}},
      {{6,0,1,2},{1,0,1,1},{5,2,1,0},{0,1,0,0}},
      {{5,0,1,2},{1,0,2,1},{6,2,1,0},{0,1,0,0}},
      {{5,3,2,1},{3,0,0,2},{6,2,0,1},{0,0,0,0}},
      {{5,3,2,1},{4,1,1,1},{6,2,1,0},{0,1,0,0}},
      {{0,0,0,3},{1,0,1,1},{6,3,2,0},{5,1,0,0}},
      {{6,1,0,1},{4,0,1,2},{2,3,2,0},{2,2,0,0}},
      {{5,0,1,2},{3,1,1,0},{6,3,2,0},{0,1,0,0}},
      {{6,1,1,1},{3,1,2,1},{5,2,1,0},{0,1,0,0}}
    };
    for (int band = 3; band >= 0; --band) {
      const unsigned left = ClockScene::random(random_) & 15u;
      const unsigned right = ClockScene::random(random_) & 15u;
      const unsigned first = ClockScene::random(random_) & 1u;
      for (unsigned i = 0; i < 4; ++i) {
        for (unsigned order = 0; order < 2; ++order) {
          const unsigned side = order ^ first;
          const uint8_t* p = mosaics[side ? right : left][i];
          plan_[planCount_++] = Piece{p[0],p[1],int8_t(p[2] + (side ? 6 : 0)),int8_t(p[3] + 4 + band * 4)};
        }
      }
      if (well_) plan_[planCount_++] = Piece{0,1,int8_t(well_ == 1 ? 4 : 5),int8_t(4 + band * 4)};
    }
  }

  bool fits(const Piece& piece, int y) const {
    for (unsigned i = 0; i < 4; ++i) {
      int dx, dy; tetrominoCell(piece.family, piece.rotation, i, dx, dy);
      const int x = piece.x + dx, row = y + dy;
      if (x < 0 || x >= Columns || row >= Rows || (row >= 0 && board_[row][x])) return false;
    }
    return true;
  }
  int landing(const Piece& piece) const {
    int y = -4;
    if (!fits(piece, y)) return -4;
    while (fits(piece, y + 1)) ++y;
    return y;
  }
  void launch(const Piece& piece) {
    piece_ = piece; active_ = true; activeYQ8_ = -4 * 256; speedQ8_ = 256;
  }
  void lock(const ClockTime& time) {
    for (unsigned i = 0; i < 4; ++i) {
      int dx, dy; tetrominoCell(piece_.family, piece_.rotation, i, dx, dy);
      const int x = piece_.x + dx, y = piece_.y + dy;
      if (x < 0 || x >= Columns || y < 0 || y >= Rows) continue;
      board_[y][x] = uint8_t(piece_.family + 1);
      if (phase_ == Phase::Assemble)
        board_[y][x] |= uint8_t(Scaffold | (clockCell(time, unsigned(x), unsigned(y)) ? Ink : 0));
    }
    active_ = false; ++locked_;
    if (phase_ == Phase::Assemble) return;
    clearMask_ = 0;
    for (unsigned y = 0; y < Rows; ++y) {
      bool full = true;
      for (unsigned x = 0; x < Columns; ++x) if (!board_[y][x]) { full = false; break; }
      if (full) clearMask_ |= 1u << y;
    }
    clearFrame_ = 0;
  }
  void collapse() {
    int destination = Rows - 1;
    uint8_t removed = 0;
    for (int source = Rows - 1; source >= 0; --source) {
      if (clearMask_ & (1u << source)) { ++removed; continue; }
      for (unsigned x = 0; x < Columns; ++x) board_[destination][x] = board_[source][x];
      --destination;
    }
    while (destination >= 0) {
      for (unsigned x = 0; x < Columns; ++x) board_[destination][x] = 0;
      --destination;
    }
    lines_ += removed;
    scaffoldRows_ = uint8_t(scaffoldRows_ > removed ? scaffoldRows_ - removed : 0);
    clearMask_ = 0; clearFrame_ = 0;
  }
  uint8_t nextFamily() {
    if (bagIndex_ >= 7) {
      for (unsigned i = 0; i < 7; ++i) bag_[i] = uint8_t(i);
      for (unsigned i = 6; i > 0; --i) {
        const unsigned j = ClockScene::random(random_) % (i + 1);
        const uint8_t value = bag_[i]; bag_[i] = bag_[j]; bag_[j] = value;
      }
      bagIndex_ = 0;
    }
    return bag_[bagIndex_++];
  }
  int evaluate(const Piece& piece) {
    for (unsigned i = 0; i < 4; ++i) {
      int dx, dy; tetrominoCell(piece.family, piece.rotation, i, dx, dy);
      board_[piece.y + dy][piece.x + dx] = uint8_t(piece.family + 1);
    }
    int heights[Columns] = {}, holes = 0, complete = 0, totalHeight = 0, bump = 0;
    for (unsigned x = 0; x < Columns; ++x) {
      bool seen = false;
      for (unsigned y = 0; y < Rows; ++y) {
        if (board_[y][x]) {
          if (!seen) heights[x] = Rows - int(y);
          seen = true;
        } else if (seen) ++holes;
      }
      totalHeight += heights[x];
      if (x) { const int delta = heights[x] - heights[x-1]; bump += delta < 0 ? -delta : delta; }
    }
    for (unsigned y = 0; y < Rows; ++y) {
      bool full = true;
      for (unsigned x = 0; x < Columns; ++x) if (!board_[y][x]) { full = false; break; }
      if (full) ++complete;
    }
    for (unsigned i = 0; i < 4; ++i) {
      int dx, dy; tetrominoCell(piece.family, piece.rotation, i, dx, dy);
      board_[piece.y + dy][piece.x + dx] = 0;
    }
    return complete * 40 - holes * 35 - totalHeight * 4 - bump * 3;
  }
  bool chooseMove() {
    Piece best = {nextFamily(),0,0,0};
    int bestScore = -32767;
    bool found = false;
    const unsigned first = ClockScene::random(random_) % Columns;
    for (unsigned rotation = 0; rotation < 4; ++rotation) {
      for (unsigned order = 0; order < Columns; ++order) {
        Piece candidate = {best.family,uint8_t(rotation),int8_t((first + order) % Columns),0};
        const int y = landing(candidate);
        if (y < 0) continue;
        candidate.y = int8_t(y);
        const int score = evaluate(candidate);
        if (!found || score > bestScore) { best = candidate; bestScore = score; found = true; }
      }
    }
    if (found) launch(best);
    return found;
  }
};

}  // namespace flappy
