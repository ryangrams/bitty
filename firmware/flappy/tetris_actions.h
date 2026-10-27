#pragma once

#include <stdint.h>

namespace flappy {

// Extra controller actions use the cartridge's board and ordinary lock phase.
class TetrisActions {
 public:
  void reset() {
    previousPad_ = 0; heldOrientation_ = NoPiece;
    heldPiece_ = hardDropped_ = heldThisFrame_ = chord_ = false;
  }
  bool hardDropped() const { return hardDropped_; }
  bool swapped() const { return heldThisFrame_; }
  bool heldThisFrame() const { return heldThisFrame_; }
  bool hasHeldPiece() const { return heldOrientation_ != NoPiece; }
  uint8_t heldOrientation() const { return heldOrientation_; }
  uint8_t heldType() const {
    const uint8_t types[] = {2, 7, 8, 10, 11, 14, 18};
    for (uint8_t i = 0; i < 7; ++i) if (heldOrientation_ == types[i]) return i;
    return NoPiece;
  }

  template <class Runtime>
  uint8_t apply(Runtime& runtime, bool zero, uint8_t player, uint8_t pad) {
    const uint8_t pressed = uint8_t(pad & ~previousPad_);
    previousPad_ = pad;
    hardDropped_ = heldThisFrame_ = false;
    const uint16_t live = liveBase(runtime, zero, player);
    const uint8_t phase = runtime.readRam(live + 8);
    if (phase >= 2 && phase <= 7) heldPiece_ = false;
    if (!active(runtime, zero, player, live)) {
      if (chord_) {
        if (!(pad & (A | B))) chord_ = false;
        return uint8_t(pad & ~(A | B));
      }
      return pad;
    }
    uint8_t result = uint8_t(pad & ~Up);
    const bool chord = (pad & (A | B)) == (A | B);
    if (chord) chord_ = true;
    if (chord_) result &= uint8_t(~(A | B));
    if (chord) {
      if ((pressed & (A | B)) && !heldPiece_) {
        heldThisFrame_ = hold(runtime, zero, player, live);
        heldPiece_ |= heldThisFrame_;
      }
    } else if (pressed & Up) hardDropped_ = hardDrop(runtime, zero, player, live);
    if (!(pad & (A | B))) chord_ = false;
    return result;
  }

  static uint8_t spawnOrientation(uint8_t orientation) {
    if (orientation < 4) return 2;
    if (orientation < 8) return 7;
    if (orientation < 10) return 8;
    if (orientation == 10) return 10;
    if (orientation < 13) return 11;
    if (orientation < 17) return 14;
    return orientation < 19 ? 18 : 255;
  }

  // Coordinates are relative to the native piece pivot, in board cells.
  static bool cell(uint8_t orientation, uint8_t mino, int& x, int& y) {
    if (orientation >= 19 || mino >= 4) return false;
    static const int8_t shapes[7][4][2] = {
      {{-1, 0}, {0, 0}, {1, 0}, {0, -1}},
      {{0, -1}, {0, 0}, {-1, 1}, {0, 1}},
      {{-1, 0}, {0, 0}, {0, 1}, {1, 1}},
      {{-1, 0}, {0, 0}, {-1, 1}, {0, 1}},
      {{0, 0}, {1, 0}, {-1, 1}, {0, 1}},
      {{0, -1}, {0, 0}, {0, 1}, {1, 1}},
      {{0, -2}, {0, -1}, {0, 0}, {0, 1}}
    };
    uint8_t family = 0, turns = 0;
    if (orientation < 4) turns = orientation;
    else if (orientation < 8) { family = 1; turns = uint8_t(orientation - 4); }
    else if (orientation < 10) { family = 2; turns = orientation == 9 ? 3 : 0; }
    else if (orientation == 10) family = 3;
    else if (orientation < 13) { family = 4; turns = orientation == 12 ? 3 : 0; }
    else if (orientation < 17) { family = 5; turns = uint8_t(orientation - 13); }
    else { family = 6; turns = orientation == 18 ? 3 : 0; }
    x = shapes[family][mino][0];
    y = shapes[family][mino][1];
    while (turns--) { const int previousX = x; x = -y; y = previousX; }
    return true;
  }

 private:
  enum : uint8_t { A = 1, B = 2, Up = 16, NoPiece = 255 };
  uint8_t previousPad_ = 0;
  uint8_t heldOrientation_ = NoPiece;
  bool heldPiece_ = false, hardDropped_ = false, heldThisFrame_ = false, chord_ = false;

  template <class Runtime>
  static uint16_t liveBase(const Runtime& runtime, bool zero, uint8_t player) {
    return zero && runtime.readRam(0xbe) == 2 && player == 1 ? 0x60 : 0x40;
  }

  template <class Runtime>
  static bool active(const Runtime& runtime, bool zero, uint8_t player, uint16_t live) {
    if (!runtime.ready() || runtime.readRam(0xc0) != 4 || runtime.readRam(live + 8) != 1) return false;
    if (zero) return player >= 1 && player <= runtime.readRam(0xbe) &&
        runtime.readRam(0xbe) <= 2 && runtime.readRam(0xa7) == 2;
    if (player != 1 || runtime.readRam(0xbd) != 3 || runtime.readRam(0xa7) != 2) return false;
    const uint8_t signature[] = {'T', '-', 'G', 'Y', 'M'};
    for (uint8_t i = 0; i < 5; ++i) if (runtime.readRam(0x75b + i) != signature[i]) return false;
    return true;
  }

  template <class Runtime>
  static bool valid(const Runtime& runtime, uint16_t board, uint8_t orientation, int x, int y) {
    for (uint8_t mino = 0; mino < 4; ++mino) {
      int dx, dy;
      if (!cell(orientation, mino, dx, dy)) return false;
      const int column = x + dx, row = y + dy;
      if (column < 0 || column >= 10 || row < -2 || row >= 20) return false;
      const uint8_t index = uint8_t(row * 10 + column);
      if (runtime.readRam(board + index) < 0xef) return false;
    }
    return true;
  }

  template <class Runtime>
  static void writeField(Runtime& runtime, bool zero, uint8_t player, uint8_t offset, uint8_t value) {
    if (!zero) runtime.writeRam(0x40 + offset, value);
    else {
      runtime.writeRam((player == 2 ? 0x80 : 0x60) + offset, value);
      if (runtime.readRam(0xbe) == 1 || player == 2) runtime.writeRam(0x40 + offset, value);
      if (player == 2 && offset == 2) runtime.writeRam(0x7ff0, value);
    }
  }

  template <class Runtime>
  static bool hardDrop(Runtime& runtime, bool zero, uint8_t player, uint16_t live) {
    const uint8_t orientation = runtime.readRam(zero && player == 2 ? 0x7ff0 : live + 2);
    const int x = int8_t(runtime.readRam(live));
    int y = int8_t(runtime.readRam(live + 1));
    const uint16_t board = zero && player == 2 ? 0x500 : 0x400;
    if (!valid(runtime, board, orientation, x, y)) return false;
    while (valid(runtime, board, orientation, x, y + 1)) ++y;
    writeField(runtime, zero, player, 1, uint8_t(y));
    writeField(runtime, zero, player, 5, 0);
    writeField(runtime, zero, player, 14, 0);
    writeField(runtime, zero, player, 9, 32);
    writeField(runtime, zero, player, 8, 2);
    return true;
  }

  template <class Runtime>
  bool hold(Runtime& runtime, bool zero, uint8_t player, uint16_t live) {
    const uint16_t nextAddress = zero && player == 2 ? 0x7ff1 : 0xbf;
    const uint8_t incoming = hasHeldPiece() ? heldOrientation_ : spawnOrientation(runtime.readRam(nextAddress));
    const uint8_t outgoing = spawnOrientation(runtime.readRam(zero && player == 2 ? 0x7ff0 : live + 2));
    const uint16_t board = zero && player == 2 ? 0x500 : 0x400;
    if (incoming == 255 || outgoing == 255 || !valid(runtime, board, incoming, 5, 0)) return false;
    if (!hasHeldPiece()) {
      uint8_t next = NoPiece;
      if (!runtime.runTetrisRandomizer(zero, next)) return false;
      runtime.writeRam(nextAddress, next);
    }
    writeField(runtime, zero, player, 0, 5);
    writeField(runtime, zero, player, 1, 0);
    writeField(runtime, zero, player, 2, incoming);
    writeField(runtime, zero, player, 5, 0);
    writeField(runtime, zero, player, 14, 0);
    writeField(runtime, zero, player, 15, 0);
    heldOrientation_ = outgoing;
    return true;
  }
};

}  // namespace flappy
