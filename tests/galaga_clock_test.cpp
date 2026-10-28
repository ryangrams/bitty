#include "../firmware/flappy/galaga_clock.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

using flappy::ClockTime;
using flappy::GalagaClock;

static ClockTime timeAt(uint32_t frame,unsigned hour = 12,unsigned minute = 34) {
  ClockTime time; time.hour = uint8_t(hour); time.minute = uint8_t(minute);
  time.second = uint8_t(frame / 60 % 60); time.ms = frame * 1000 / 60;
  return time;
}

static void assertReadable(const GalagaClock& clock,const ClockTime& time) {
  assert(clock.phase() == GalagaClock::Phase::Read);
  assert(clock.remaining() == clock.enemyCount());
  std::set<unsigned> occupied;
  for (unsigned i = 0; i < clock.enemyCount(); ++i) {
    const auto& enemy = clock.enemy(i);
    assert(enemy.flight == GalagaClock::Flight::Formation);
    assert(GalagaClock::clockCell(time,enemy.cell));
    assert(occupied.insert(enemy.cell).second);
    int x,y; clock.home(enemy.cell,x,y);
    assert(enemy.x == x * GalagaClock::Unit && enemy.y == y * GalagaClock::Unit);
    assert(x >= 24 && x <= 108 && y >= 34 && y <= 172);
  }
  for (unsigned cell = 0; cell < 60; ++cell) assert(occupied.count(cell) == GalagaClock::clockCell(time,cell));
  for (unsigned i = 0; i < GalagaClock::PlayerShots; ++i) assert(!clock.playerShots()[i].active);
  for (unsigned i = 0; i < GalagaClock::EnemyShots; ++i) assert(!clock.enemyShots()[i].active);
}

static void completeSquadrons() {
  uint32_t latestCycle = 0;
  for (uint32_t seed = 1; seed <= 64; ++seed) {
    GalagaClock clock; clock.reset(seed);
    const unsigned hour = (seed * 7) % 24, minute = (seed * 11) % 60;
    bool sawRead = false, sawBattle = false, sawDive = false, sawReform = false;
    unsigned readFrames = 0, count = 0;
    uint32_t startKills = 0;
    for (uint32_t frame = 0; frame < 2700 && clock.cycles() < 2; ++frame) {
      const uint32_t previousKills = clock.enemiesDestroyed();
      clock.step(timeAt(frame,hour,minute));
      assert(clock.playerX() >= 7 && clock.playerX() <= 127);
      for (unsigned i = 0; i < clock.enemyCount(); ++i) {
        const auto& enemy = clock.enemy(i);
        assert(enemy.x >= -70 * GalagaClock::Unit && enemy.x <= 205 * GalagaClock::Unit);
        assert(enemy.y >= -32 * GalagaClock::Unit && enemy.y <= 253 * GalagaClock::Unit);
        if (enemy.flight != GalagaClock::Flight::Dead) assert(enemy.hp >= 1 && enemy.hp <= 2);
        if (enemy.flight == GalagaClock::Flight::Dive) sawDive = true;
      }
      if (clock.phase() == GalagaClock::Phase::Read) {
        assertReadable(clock,timeAt(frame,hour,minute));
        if (!sawRead) { count = clock.enemyCount(); startKills = clock.enemiesDestroyed(); }
        sawRead = true; ++readFrames;
        assert(clock.enemiesDestroyed() == previousKills);
      }
      if (clock.phase() == GalagaClock::Phase::Battle) sawBattle = true;
      if (clock.phase() == GalagaClock::Phase::Reform) {
        sawReform = true;
        assert(clock.remaining() == 0 && clock.enemiesDestroyed() - startKills == count);
      }
      if (clock.cycles() == 2) latestCycle = frame > latestCycle ? frame : latestCycle;
    }
    assert(sawRead && sawBattle && sawDive && sawReform);
    assert(readFrames == 210 && clock.cycles() == 2);
    assert(clock.shotsFired() >= count);
  }
  std::cout << "Latest complete formation cycle: " << latestCycle << " frames.\n";
}

static void allTimesAndRollover() {
  GalagaClock clock;
  for (unsigned hour = 0; hour < 24; ++hour)
    for (unsigned minute = 0; minute < 60; ++minute) {
      clock.reset(hour * 60 + minute + 1);
      const ClockTime time = timeAt(0,hour,minute);
      unsigned frame = 0;
      do { clock.step(time); ++frame; } while (clock.phase() != GalagaClock::Phase::Read && frame < 300);
      assert(frame < 300);
      assertReadable(clock,time);
    }
  clock.reset(19);
  for (unsigned frame = 0; frame < 600; ++frame) clock.step(timeAt(frame,23,59));
  assert(clock.phase() == GalagaClock::Phase::Battle);
  const unsigned oldCycles = clock.cycles();
  clock.step(timeAt(600,0,0));
  assert(clock.cycles() == oldCycles + 1 && clock.phase() == GalagaClock::Phase::Entrance);
  assert(clock.remaining() == clock.enemyCount());
  ClockTime noon = timeAt(600,12,0); noon.twentyFourHour = false;
  clock.step(noon);
  assert(clock.cycles() == oldCycles + 2);
  for (unsigned cell = 0; cell < 60; ++cell)
    assert(GalagaClock::clockCell(noon,cell) ==
      flappy::ClockScene::digitPixel(noon.digit(cell/15),cell%3,(cell%15)/3));
  assert(!GalagaClock::clockCell(noon,60) && !GalagaClock::clockCell(noon,999));
}

static void deterministicAndGuarded() {
  constexpr unsigned pixels = 135 * 240;
  std::array<uint16_t,pixels + 32> guarded;
  guarded.fill(0x4a69);
  GalagaClock a,b; a.reset(0x5f3759df); b.reset(0x5f3759df);
  for (uint32_t frame = 0; frame < 10800; ++frame) {
    const ClockTime time = timeAt(frame,8,8);
    a.step(time); b.step(time);
    assert(a.phase() == b.phase() && a.cycles() == b.cycles());
    assert(a.playerX() == b.playerX() && a.remaining() == b.remaining());
    assert(a.enemiesDestroyed() == b.enemiesDestroyed() && a.shotsFired() == b.shotsFired());
    for (unsigned i = 0; i < a.enemyCount(); ++i) {
      const auto& lhs = a.enemy(i); const auto& rhs = b.enemy(i);
      assert(lhs.x == rhs.x && lhs.y == rhs.y && lhs.flight == rhs.flight && lhs.cell == rhs.cell);
    }
    if (!(frame % 31)) {
      const uint32_t kills = a.enemiesDestroyed(), cycles = a.cycles();
      a.draw(guarded.data()+16,time);
      assert(a.enemiesDestroyed() == kills && a.cycles() == cycles);
      for (unsigned i = 0; i < 16; ++i)
        assert(guarded[i] == 0x4a69 && guarded[pixels + 16 + i] == 0x4a69);
    }
  }
  assert(a.cycles() >= 5 && a.enemiesDestroyed() >= 160);
  a.draw(nullptr,timeAt(0));
  a.reset(0);
  assert(a.cycles() == 0 && a.shotsFired() == 0 && a.enemiesDestroyed() == 0);
}

static void writePreview(const std::string& path,const GalagaClock& clock,const ClockTime& time) {
  std::array<uint16_t,135*240> pixels;
  clock.draw(pixels.data(),time);
  std::ofstream file(path,std::ios::binary);
  file << "P6\n135 240\n255\n";
  for (uint16_t value : pixels) {
    const char rgb[3] = {char(((value >> 11) & 31) * 255 / 31),
      char(((value >> 5) & 63) * 255 / 63),char((value & 31) * 255 / 31)};
    file.write(rgb,3);
  }
}

int main(int argc,char** argv) {
  completeSquadrons();
  allTimesAndRollover();
  deterministicAndGuarded();
  assert(sizeof(GalagaClock) <= 2048);
  if (argc > 1) {
    GalagaClock clock; clock.reset(19);
    bool read = false, battle = false, reform = false;
    for (unsigned frame = 0; frame < 2500 && clock.cycles() < 2; ++frame) {
      const ClockTime time = timeAt(frame); clock.step(time);
      if (frame == 100) writePreview(std::string(argv[1]) + "/entrance.ppm",clock,time);
      if (!read && clock.phase() == GalagaClock::Phase::Read) {
        read = true; writePreview(std::string(argv[1]) + "/time.ppm",clock,time);
      }
      if (!battle && clock.phase() == GalagaClock::Phase::Battle && clock.enemiesDestroyed() > 12) {
        battle = true; writePreview(std::string(argv[1]) + "/battle.ppm",clock,time);
      }
      if (!reform && clock.phase() == GalagaClock::Phase::Reform) {
        reform = true; writePreview(std::string(argv[1]) + "/reform.ppm",clock,time);
      }
    }
  }
  std::cout << "Galaga clock tests passed. State: " << sizeof(GalagaClock) << " bytes.\n";
}
