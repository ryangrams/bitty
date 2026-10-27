#include "../firmware/flappy/tetris_actions.h"

#include <assert.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

using flappy::TetrisActions;
enum : uint8_t { A = 1, B = 2, Up = 16, Down = 32, Left = 64 };

struct Runtime {
  std::array<uint8_t, 32768> ram = {};
  bool available = true;
  std::vector<uint16_t> writes;

  Runtime() {
    for (unsigned address = 0x400; address < 0x600; ++address) ram[address] = 0xef;
    ram[0xc0] = 4; ram[0xa7] = 2; ram[0xbe] = 2; ram[0xbd] = 3;
    for (unsigned base : {0x40u, 0x60u, 0x80u}) {
      ram[base] = 5; ram[base + 1] = 4; ram[base + 2] = 11;
      ram[base + 5] = 17; ram[base + 6] = 18; ram[base + 8] = 1;
      ram[base + 9] = 32; ram[base + 14] = 19; ram[base + 15] = 20;
    }
    const char signature[] = "T-GYM";
    std::copy(signature, signature + 5, ram.begin() + 0x75b);
    ram[0xbf] = 10; ram[0x7ff0] = 11; ram[0x7ff1] = 10;
  }
  bool ready() const { return available; }
  uint8_t readRam(uint16_t address) const { assert(address < ram.size()); return ram[address]; }
  bool writeRam(uint16_t address, uint8_t value) {
    assert(address < ram.size()); writes.push_back(address); ram[address] = value; return true;
  }
};

static unsigned live(bool zero, uint8_t players, uint8_t player) {
  return zero && players == 2 && player == 1 ? 0x60 : 0x40;
}

static void geometryAndSpawnOrientations() {
  const uint8_t spawn[] = {2,2,2,2,7,7,7,7,8,8,10,11,11,14,14,14,14,18,18};
  std::set<std::set<std::pair<int, int>>> orientations;
  for (uint8_t orientation = 0; orientation < 19; ++orientation) {
    assert(TetrisActions::spawnOrientation(orientation) == spawn[orientation]);
    assert(TetrisActions::spawnOrientation(spawn[orientation]) == spawn[orientation]);
    std::set<std::pair<int, int>> cells;
    for (uint8_t mino = 0; mino < 4; ++mino) {
      int x = 99, y = 99;
      assert(TetrisActions::cell(orientation, mino, x, y));
      assert(x >= -2 && x <= 2 && y >= -2 && y <= 2);
      cells.insert({x, y});
    }
    assert(cells.size() == 4);
    orientations.insert(cells);
    int x = 99, y = 99;
    assert(!TetrisActions::cell(orientation, 4, x, y));
  }
  assert(orientations.size() == 19);
  for (uint8_t invalid : {uint8_t(19), uint8_t(255)}) {
    int x = 99, y = 99;
    assert(!TetrisActions::cell(invalid, 0, x, y));
    assert(TetrisActions::spawnOrientation(invalid) == 255);
  }
}

static void guardedRuntimesKeepInputAndRam() {
  for (int condition = 0; condition < 12; ++condition) {
    Runtime runtime;
    bool zero = condition < 7;
    uint8_t player = 1;
    if (condition == 0) runtime.available = false;
    if (condition == 1) runtime.ram[0xc0] = 3;
    if (condition == 2) runtime.ram[0x68] = 2;
    if (condition == 3) runtime.ram[0xa7] = 7;
    if (condition == 4) runtime.ram[0xbe] = 3;
    if (condition == 5) player = 0;
    if (condition == 6) player = 3;
    if (condition == 7) runtime.ram[0xbd] = 2;
    if (condition == 8) runtime.ram[0x75b] = '?';
    if (condition == 9) player = 2;
    if (condition == 10) runtime.ram[0xa7] = 1;
    if (condition == 11) runtime.ram[0xa7] = 7;
    const auto before = runtime.ram;
    TetrisActions actions;
    const uint8_t pad = A | B | Up | Down;
    assert(actions.apply(runtime, zero, player, pad) == pad);
    assert(!actions.hardDropped() && !actions.swapped());
    assert(runtime.writes.empty() && runtime.ram == before);
  }
}

static void dropsUseTheFloorAndStack() {
  for (uint8_t orientation = 0; orientation < 19; ++orientation) {
    Runtime runtime;
    runtime.ram[0x62] = orientation;
    int maximumY = -99;
    for (uint8_t mino = 0; mino < 4; ++mino) {
      int x, y; assert(TetrisActions::cell(orientation, mino, x, y));
      maximumY = std::max(maximumY, y);
    }
    const auto before = runtime.ram;
    TetrisActions actions;
    assert(actions.apply(runtime, true, 1, Up | Left) == Left);
    assert(actions.hardDropped() && !actions.swapped());
    assert(runtime.ram[0x61] == 19 - maximumY && runtime.ram[0x68] == 2);
    assert(runtime.ram[0x69] == 32 && runtime.ram[0x65] == 0);
    assert(runtime.ram[0x6e] == 0 && runtime.ram[0x6f] == before[0x6f]);
    for (unsigned address = 0x400; address < 0x600; ++address) assert(runtime.ram[address] == before[address]);
  }
  Runtime runtime;
  runtime.ram[0x400 + 185] = 1;
  TetrisActions actions;
  assert(actions.apply(runtime, true, 1, Up) == 0);
  assert(actions.hardDropped() && runtime.ram[0x61] == 16);
  runtime.ram[0x68] = 1;
  runtime.writes.clear();
  actions.apply(runtime, true, 1, Up);
  assert(!actions.hardDropped() && runtime.writes.empty());
  actions.apply(runtime, true, 1, 0);
  actions.apply(runtime, true, 1, Up);
  assert(actions.hardDropped());
}

static void swapIsOncePerNativePiece() {
  Runtime runtime;
  runtime.ram[0x62] = 12;
  TetrisActions actions;
  assert(actions.apply(runtime, true, 1, A | B | Left) == Left);
  assert(actions.swapped() && !actions.hardDropped());
  assert(runtime.ram[0x62] == 10 && runtime.ram[0xbf] == 11);
  assert(runtime.ram[0x60] == 5 && runtime.ram[0x61] == 0);
  assert(runtime.ram[0x65] == 0 && runtime.ram[0x66] == 0);
  assert(runtime.ram[0x6e] == 0 && runtime.ram[0x6f] == 0);
  runtime.writes.clear();
  runtime.ram[0xa7] = 7;
  actions.apply(runtime, true, 1, 0);
  runtime.ram[0xa7] = 2;
  actions.apply(runtime, true, 1, A | B);
  assert(!actions.swapped() && runtime.writes.empty());
  actions.apply(runtime, true, 1, A | B);
  assert(!actions.swapped() && runtime.writes.empty());
  actions.apply(runtime, true, 1, 0);
  actions.apply(runtime, true, 1, A | B);
  assert(!actions.swapped() && runtime.writes.empty());
  runtime.ram[0x68] = 2;
  actions.apply(runtime, true, 1, 0);
  runtime.ram[0x68] = 1;
  runtime.ram[0x62] = 2;
  runtime.ram[0xbf] = 7;
  actions.apply(runtime, true, 1, A | B);
  assert(actions.swapped() && runtime.ram[0x62] == 7 && runtime.ram[0xbf] == 2);
  actions.reset();
  actions.apply(runtime, true, 1, A | B);
  assert(actions.swapped());
}

static void rejectedActionsAreAtomic() {
  for (int condition = 0; condition < 5; ++condition) {
    Runtime runtime;
    if (condition == 0) runtime.ram[0x62] = 19;
    if (condition == 1) runtime.ram[0xbf] = 255;
    if (condition == 2) runtime.ram[0x400 + 4] = 1;
    if (condition == 3) runtime.ram[0x60] = 0;
    if (condition == 4) runtime.ram[0x400 + 45] = 1;
    const auto before = runtime.ram;
    TetrisActions actions;
    const uint8_t input = condition <= 2 ? uint8_t(A | B) : uint8_t(Up);
    assert(actions.apply(runtime, true, 1, input) == 0);
    assert(!actions.swapped() && !actions.hardDropped());
    assert(runtime.writes.empty() && runtime.ram == before);
  }
  Runtime runtime;
  runtime.ram[0x400 + 4] = 1;
  TetrisActions actions;
  actions.apply(runtime, true, 1, A | B);
  assert(!actions.swapped());
  runtime.ram[0x400 + 4] = 0xef;
  actions.apply(runtime, true, 1, 0);
  actions.apply(runtime, true, 1, A | B);
  assert(actions.swapped());
}

static void mirrorsStayInsideTheSelectedPlayer() {
  for (int mode = 0; mode < 4; ++mode) {
    Runtime runtime;
    const bool zero = mode != 0;
    const uint8_t players = mode == 1 ? 1 : 2;
    const uint8_t player = mode == 3 ? 2 : 1;
    runtime.ram[0xbe] = players;
    const unsigned current = live(zero, players, player);
    runtime.ram[current + 2] = 12;
    const auto before = runtime.ram;
    TetrisActions actions;
    actions.apply(runtime, zero, player, A | B);
    assert(actions.swapped());
    std::set<unsigned> allowed;
    const unsigned next = zero && player == 2 ? 0x7ff1 : 0xbf;
    allowed.insert(next);
    const auto allowBlock = [&](unsigned base) {
      for (unsigned offset : {0u, 1u, 2u, 5u, 6u, 14u, 15u}) allowed.insert(base + offset);
    };
    if (!zero) allowBlock(0x40);
    else {
      allowBlock(player == 1 ? 0x60 : 0x80);
      if (players == 1 || player == 2) allowBlock(0x40);
      if (player == 2) allowed.insert(0x7ff0);
    }
    for (uint16_t address : runtime.writes) assert(allowed.count(address));
    for (unsigned address = 0; address < runtime.ram.size(); ++address)
      if (!allowed.count(address)) assert(runtime.ram[address] == before[address]);
    assert(runtime.ram[current + 2] == 10 && runtime.ram[next] == 11);
    if (zero && player == 2) assert(runtime.ram[0x82] == 10 && runtime.ram[0x7ff0] == 10);
    if (zero && players == 1) assert(runtime.ram[0x62] == 10);
  }
}

static void nativeButtonsPassThrough() {
  for (uint8_t pad : {uint8_t(A), uint8_t(B), uint8_t(A | Down), uint8_t(B | Left), uint8_t(Down | Left)}) {
    Runtime runtime;
    const auto before = runtime.ram;
    TetrisActions actions;
    assert(actions.apply(runtime, true, 1, pad) == pad);
    assert(!actions.hardDropped() && !actions.swapped());
    assert(runtime.writes.empty() && runtime.ram == before);
  }
}

int main() {
  geometryAndSpawnOrientations();
  guardedRuntimesKeepInputAndRam();
  dropsUseTheFloorAndStack();
  swapIsOncePerNativePiece();
  rejectedActionsAreAtomic();
  mirrorsStayInsideTheSelectedPlayer();
  nativeButtonsPassThrough();
  std::cout << "Tetris action geometry, native lock setup, swap lifecycle and RAM isolation passed.\n";
}
