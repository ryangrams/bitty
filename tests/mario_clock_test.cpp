#include "../firmware/flappy/mario_clock.h"
#include "../firmware/flappy/mario_viewport.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

using namespace flappy;

static void courses() {
  std::set<std::vector<uint8_t>> unique;
  for (uint32_t seed = 0; seed < 1000; ++seed) {
    MarioClock clock, repeat;
    clock.generate(seed); repeat.generate(seed);
    assert(clock.levelBytes() < MarioClock::MaxLevelBytes && clock.levelBytes() >= 30);
    assert(clock.level()[clock.levelBytes() - 1] == 0xfd);
    assert(clock.levelBytes() == repeat.levelBytes());
    assert(!memcmp(clock.level(),repeat.level(),clock.levelBytes()));
    assert(clock.pages() >= 8 && clock.pages() <= 10);
    assert(clock.clockCount() >= 2 && clock.clockCount() <= 3);
    unsigned page = 0, previousX = 0;
    for (unsigned i = 2; i + 1 < clock.levelBytes(); i += 2) {
      const uint8_t first = clock.level()[i], second = clock.level()[i + 1];
      assert(first != 0xfd);
      page += (second & 0x80) != 0;
      const unsigned x = page * 256 + (first >> 4) * 16;
      assert(x >= previousX); previousX = x;
      assert((first & 15) != 12 && (first & 15) != 14); // Continuous floor, no attribute commands.
    }
    assert(page == clock.pages() - 1);
    for (unsigned i = 0; i < clock.obstacleCount(); ++i) {
      const auto obstacle = clock.obstacle(i);
      assert(obstacle.height == 16 || obstacle.height == 32);
      assert(obstacle.width == 16 || obstacle.width == 32);
      for (unsigned c = 0; c < clock.clockCount(); ++c)
        assert(obstacle.x + obstacle.width + 64 < clock.clockX(c) ||
            obstacle.x > clock.clockX(c) + MarioClock::ClockWidth + 64);
    }
    unique.insert(std::vector<uint8_t>(clock.level(),clock.level() + clock.levelBytes()));
  }
  assert(unique.size() > 900);
}

static void nativeClockPixelsAndDamage() {
  MarioClock clock; clock.generate(1);
  std::vector<uint16_t> source(256 * 8,0x1234), scratch(256 * 8 + 16,0x5aa5);
  uint16_t* target = scratch.data() + 8;
  assert(!clock.overlay(0,128,255,135,source.data(),target,256*8,300));
  for (unsigned i = 0; i < 256 * 8; ++i) source[i] = uint16_t(0x8000 + i);
  clock.captureTile(0,208,255,215,source.data()); assert(clock.tileReady());
  const auto original = source;
  const uint16_t scroll = uint16_t(clock.clockX(0) - 48);
  clock.setTime(12,34,true);
  assert(!clock.overlay(0,128,255,135,source.data(),target,2047,scroll));
  assert(clock.overlay(0,128,255,135,source.data(),target,2048,scroll));
  assert(source == original);
  for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 256; ++x) {
    if (x >= 56 && x < 64) assert(target[y * 256 + x] == source[y * 256 + 16 + (x - 48) % 8]);
    else if (x < 48 || x >= 184) assert(target[y * 256 + x] == source[y * 256 + x]);
  }
  const std::vector<uint16_t> twelve(target,target+2048);
  clock.setTime(14,34,true);
  assert(clock.overlay(0,128,255,135,source.data(),target,2048,scroll));
  assert(twelve != std::vector<uint16_t>(target,target+2048));
  clock.setTime(0,34,false);
  assert(clock.overlay(0,128,255,135,source.data(),target,2048,scroll));
  assert(twelve == std::vector<uint16_t>(target,target+2048));
  for (unsigned i=0;i<8;++i) assert(scratch[i]==0x5aa5 && scratch[2048+8+i]==0x5aa5);
  assert(!clock.overlay(0,112,255,119,source.data(),target,2048,scroll));
  assert(!clock.overlay(0,128,255,135,source.data(),target,2048,60000));

  MarioViewport viewport; viewport.configure(240,135);
  std::vector<uint16_t> pixels(240*135,0);
  const auto frame = [&](uint16_t scrollX) {
    for(int y=0;y<240;y+=8) {
      const uint16_t* p=source.data();
      if(clock.overlay(0,y,255,y+7,p,target,2048,scrollX))p=target;
      viewport.consume(0,y,255,y+7,p,pixels.data());
    }
  };
  frame(scroll); const auto withClock=pixels;
  frame(60000); assert(pixels != withClock);
  assert(viewport.rowChanged(128-105+16));
  for(int y=0;y<135;++y) for(int x=0;x<240;++x)
    assert(pixels[y*240+x] == source[(viewport.sourceY(y)%8)*256+x+8]);
}

int main() {
  courses(); nativeClockPixelsAndDamage();
  std::cout << "Mario clock course, native tile, time and scrolling-damage checks passed\n";
}
