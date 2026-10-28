#include "../firmware/flappy/flappy_clock.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

namespace flappy {
struct FlappyClockTestAccess {
  static int originX(const FlappyClock& clock,unsigned i) { return clock.origins_[i].x; }
  static int originCenter(const FlappyClock& clock,unsigned i) { return clock.origins_[i].center; }
  static int birdX(const FlappyClock& clock) { return clock.BirdX-int(clock.morphAmount())*20/255; }
  static int birdYQ8(const FlappyClock& clock) { return clock.y_; }
  static int velocity(const FlappyClock& clock) { return clock.velocity_; }
  static void sprite(uint16_t* pixels,const Skins& skins,const FlappyAssets::Sprite& sprite,int x,int y,unsigned alpha) {
    FlappyClock::sprite(pixels,skins,sprite,x,y,alpha);
  }
};
}

using flappy::ClockTime;
using flappy::FlappyClock;
using flappy::Skins;
constexpr unsigned Pixels = 135 * 240;
using Frame = std::array<uint16_t,Pixels>;

static ClockTime timeAt(unsigned hour = 12,unsigned minute = 34) {
  ClockTime time; time.hour = uint8_t(hour); time.minute = uint8_t(minute); return time;
}

static uint16_t background(const Skins& skins,int x,int y) {
  const auto& sprite = skins.background();
  const int sy = y / 2 + 60 < sprite.height ? y / 2 + 60 : sprite.height - 1;
  uint16_t value = sprite.pixels[sy * sprite.width + x / 2];
  if (value == FlappyAssets::kTransparent)
    value = FlappyAssets::background.pixels[sy * FlappyAssets::background.width + x / 2];
  return skins.palette(value);
}

static void doubleSizeBirdAndGround() {
  assert(FlappyClock::GroundY == 224 && FlappyClock::Gap == 100);
  for (unsigned skin = 0; skin < Skins::BuiltinCount; ++skin) {
    Skins skins; skins.select(uint8_t(skin));
    FlappyClock clock; clock.reset(77);
    Frame frame; clock.draw(frame.data(),timeAt(),skins);
    const int birdY = clock.birdY();
    unsigned ink = 0;
    for (int y = 0; y < 12; ++y)
      for (int x = 0; x < 17; ++x) {
        const uint16_t color = skins.birdPixel(0,x,y);
        for (int sy = 0; sy < 2; ++sy)
          for (int sx = 0; sx < 2; ++sx) {
            const int px = FlappyClock::BirdX + x * 2 + sx, py = birdY + y * 2 + sy;
            assert(frame[py * 135 + px] ==
              (color == FlappyAssets::kTransparent ? background(skins,px,py) : color));
          }
        ink += color != FlappyAssets::kTransparent;
      }
    assert(ink > 0);
    const auto& ground = skins.ground();
    for (int y = FlappyClock::GroundY; y < 240; ++y)
      for (int x = 0; x < 135; ++x) {
        const int sx = (x / 2) % ground.width, sy = ((y-FlappyClock::GroundY) / 2) % ground.height;
        assert(frame[y * 135 + x] == skins.palette(ground.pixels[sy * ground.width + sx]));
      }
  }
}

static uint16_t blend(uint16_t foreground,uint16_t background,unsigned opacity) {
  const unsigned inverse = 255-opacity;
  const unsigned red = ((foreground >> 11)*opacity+(background >> 11)*inverse)/255;
  const unsigned green = (((foreground >> 5)&63)*opacity+((background >> 5)&63)*inverse)/255;
  const unsigned blue = ((foreground&31)*opacity+(background&31)*inverse)/255;
  return uint16_t(red << 11 | green << 5 | blue);
}

static void doubleSizePipesAndAlpha() {
  for (unsigned skin = 0; skin < Skins::BuiltinCount; ++skin) {
    Skins skins; skins.select(uint8_t(skin));
    for (bool top : {false,true}) {
      const auto& sprite = skins.pipe(top);
      Frame frame; frame.fill(0x1234);
      flappy::FlappyClockTestAccess::sprite(frame.data(),skins,sprite,13,15,255);
      for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 135; ++x) {
          uint16_t expected = 0x1234;
          if (x >= 13 && x < 13+sprite.width*2 && y >= 15 && y < 15+sprite.height*2 && y < FlappyClock::GroundY) {
            const int index = ((y-15)/2)*sprite.width+(x-13)/2;
            const uint16_t source = sprite.pixels[index];
            if (source != FlappyAssets::kTransparent) {
              const unsigned alpha = sprite.alpha ? sprite.alpha[index] : 255;
              expected = alpha == 255 ? skins.palette(source) : blend(skins.palette(source),0x1234,alpha);
            }
          }
          assert(frame[y*135+x] == expected);
        }
    }
  }
  Skins skins; skins.select(0);
  const uint16_t colors[] = {0xf800,FlappyAssets::kTransparent,0x07e0,0x001f};
  const uint8_t alpha[] = {255,255,128,0};
  const FlappyAssets::Sprite sprite = {colors,2,2,4,4,alpha};
  Frame frame; frame.fill(0x1234);
  flappy::FlappyClockTestAccess::sprite(frame.data(),skins,sprite,1,1,128);
  assert(frame[1*135+1] == blend(0xf800,0x1234,128));
  assert(frame[1*135+3] == 0x1234);
  assert(frame[3*135+1] == blend(0x07e0,0x1234,128*128/255));
  assert(frame[3*135+3] == 0x1234);
}

static void exactMorphCadence() {
  FlappyClock clock; clock.reset(1);
  unsigned previous = 0, appearances = 0;
  for (unsigned frame = 1; frame <= 7200; ++frame) {
    clock.step(timeAt());
    const unsigned phase = frame % 800, morph = clock.morphAmount();
    assert(clock.frames() == frame && morph <= 255);
    if (phase < 190 || phase >= 600) assert(morph == 0);
    if (phase >= 250 && phase <= 540) assert(morph == 255);
    if (phase > 190 && phase < 250) assert(morph >= previous);
    if (phase > 540 && phase < 600) assert(morph <= previous);
    if (!previous && morph) ++appearances;
    previous = morph;
  }
  assert(appearances == 9);
}

static void longFlightAndDeterminism() {
  int largestJump = 0;
  unsigned corrections = 0;
  for (unsigned seed = 1; seed <= 32; ++seed) {
    FlappyClock a,b; a.reset(seed * 719); b.reset(seed * 719);
    int previous = a.birdY();
    for (unsigned frame = 1; frame <= 100000; ++frame) {
      const int previousQ8 = flappy::FlappyClockTestAccess::birdYQ8(a);
      a.step(timeAt()); b.step(timeAt(frame % 24,frame % 60));
      assert(a.birdY() == b.birdY() && a.morphAmount() == b.morphAmount());
      assert(a.birdY() >= 22 && a.birdY() + 24 < FlappyClock::GroundY);
      const int delta = abs(a.birdY()-previous);
      largestJump = delta > largestJump ? delta : largestJump;
      if (delta > 3) std::cerr << "bird jump seed=" << seed * 719 << " frame=" << frame
          << " previous=" << previous << " current=" << a.birdY() << " delta=" << delta << '\n';
      assert(delta <= 3);
      assert(flappy::FlappyClockTestAccess::velocity(a) >= -590 && flappy::FlappyClockTestAccess::velocity(a) <= 640);
      if (flappy::FlappyClockTestAccess::birdYQ8(a)-previousQ8 != flappy::FlappyClockTestAccess::velocity(a)) ++corrections;
      if (!a.morphAmount()) {
        for (unsigned i = 0; i < 3; ++i) {
          const int x = a.pipeX(i), center = a.pipeCenter(i);
          assert(center >= 72 && center <= 145);
          if (x < FlappyClock::BirdX+34 && x+52 > FlappyClock::BirdX) {
            assert(a.birdY() >= center-FlappyClock::Gap/2);
            assert(a.birdY()+24 <= center+FlappyClock::Gap/2);
          }
        }
      }
      previous = a.birdY();
    }
  }
  std::cout << "Largest bird movement: " << largestJump << " pixels per step.\n";
  std::cout << "Safety corrections: " << corrections << ".\n";
  assert(corrections == 0);
}

static void morphOriginsStayContinuous() {
  FlappyClock clock; clock.reset(1);
  std::array<int,6> previous = {};
  unsigned largestCellMove = 0;
  int previousX = 0, previousY = 0;
  for (unsigned frame = 0; frame < 25600; ++frame) {
    const unsigned phase = clock.frames() % 800;
    std::array<int,6> current;
    for (unsigned i = 0; i < 3; ++i) {
      current[i*2] = flappy::FlappyClockTestAccess::originX(clock,i);
      current[i*2+1] = flappy::FlappyClockTestAccess::originCenter(clock,i);
    }
    if (phase > 190 && phase < 600 && phase != 540) assert(current == previous);
    const unsigned morph = clock.morphAmount();
    const unsigned eased = morph*morph*(765u-2u*morph)/65025u;
    const int originX = current[2], originY = current[3];
    const int cellX = originX+(107-originX)*int(eased)/255;
    const int cellY = originY+(142-originY)*int(eased)/255;
    if (phase > 191 && phase < 600) {
      const int dx = abs(cellX-previousX), dy = abs(cellY-previousY);
      const unsigned moved = unsigned(dx > dy ? dx : dy);
      largestCellMove = moved > largestCellMove ? moved : largestCellMove;
      assert(moved <= 5);
    }
    previousX = cellX; previousY = cellY; previous = current;
    clock.step(timeAt());
  }
  std::cout << "Largest morph-cell movement: " << largestCellMove << " pixels per step.\n";
}

static void glyphsAndClockChanges() {
  FlappyClock clock; clock.reset(1);
  for (unsigned frame = 0; frame < 300; ++frame) clock.step(timeAt());
  assert(clock.morphAmount() == 255);
  Frame first,second;
  for (unsigned skin = 0; skin < Skins::BuiltinCount; ++skin) {
    Skins skins; skins.select(uint8_t(skin));
    const ClockTime time = timeAt();
    clock.draw(first.data(),time,skins);
    const auto& pipe = skins.pipe(false);
    unsigned checked = 0;
    for (unsigned digit = 0; digit < 4; ++digit)
      for (unsigned gy = 0; gy < 5; ++gy)
        for (unsigned gx = 0; gx < 3; ++gx) {
          if (!flappy::ClockScene::digitPixel(time.digit(digit),gx,gy)) continue;
          const int x = 47 + int((digit % 2) * 4 + gx) * 12 + 6;
          const int y = 30 + int(digit / 2) * 88 + int(gy) * 12 + 6;
          assert(x < 135 && y < 224);
          assert(flappy::FlappyClockTestAccess::birdX(clock)+34 < 47);
          const uint16_t expected = skins.palette(pipe.pixels[(5 % pipe.height) * pipe.width + 5 % pipe.width]);
          assert(first[y * 135 + x] == expected);
          ++checked;
        }
    assert(checked >= 20);
    ClockTime midnight = timeAt(0,8);
    clock.draw(first.data(),midnight,skins);
    midnight.twentyFourHour = false;
    clock.draw(second.data(),midnight,skins);
    // A geometric source fixture can map pipe and sky to the same palette
    // value. Require a visible change whenever its actual colors differ.
    const uint16_t ink = skins.palette(pipe.pixels[(5 % pipe.height)*pipe.width+5 % pipe.width]);
    if (ink != background(skins,53,36)) assert(first != second);
    ClockTime noon = timeAt(12,8); noon.twentyFourHour = false;
    clock.draw(first.data(),noon,skins);
    assert(first == second);
    clock.draw(second.data(),timeAt(23,59),skins);
    if (ink != background(skins,53,36)) assert(first != second);
  }
}

static void guardsAndPurity() {
  std::array<uint16_t,Pixels + 32> guarded;
  guarded.fill(0x4a69);
  for (unsigned skin = 0; skin < Skins::BuiltinCount; ++skin) {
    Skins skins; skins.select(uint8_t(skin));
    FlappyClock clock; clock.reset(skin + 47);
    for (unsigned frame = 0; frame < 1600; ++frame) {
      const ClockTime time = timeAt(frame / 60 % 24,frame % 60);
      clock.step(time);
      if (frame % 7) continue;
      const unsigned before = clock.frames();
      const int birdY = clock.birdY();
      clock.draw(guarded.data()+16,time,skins);
      assert(clock.frames() == before && clock.birdY() == birdY);
      for (unsigned i = 0; i < 16; ++i)
        assert(guarded[i] == 0x4a69 && guarded[Pixels + 16 + i] == 0x4a69);
    }
    clock.draw(nullptr,timeAt(),skins);
  }
}

static void writePreview(const std::string& path,const Frame& frame) {
  std::ofstream file(path,std::ios::binary); file << "P6\n135 240\n255\n";
  for (uint16_t value : frame) {
    const char rgb[] = {char(((value >> 11) & 31) * 255 / 31),
      char(((value >> 5) & 63) * 255 / 63),char((value & 31) * 255 / 31)};
    file.write(rgb,3);
  }
}

int main(int argc,char** argv) {
  doubleSizeBirdAndGround(); doubleSizePipesAndAlpha(); exactMorphCadence(); longFlightAndDeterminism();
  morphOriginsStayContinuous();
  glyphsAndClockChanges(); guardsAndPurity();
  if (argc > 1) {
    Skins skins; skins.select(0);
    FlappyClock clock; clock.reset(1);
    Frame frame;
    const unsigned lastFrame = argc > 2 ? 800 : 3200;
    for (unsigned i = 0; i <= lastFrame; ++i) {
      clock.draw(frame.data(),timeAt(),skins);
      if (i == 0 || i == 220 || i == 300 || i == 560 || i == 2960 || i == 2961)
        writePreview(std::string(argv[1]) + "/frame-" + std::to_string(i) + ".ppm",frame);
      if (argc > 2 && i % 4 == 0)
        writePreview(std::string(argv[1]) + "/anim-" + std::to_string(i) + ".ppm",frame);
      clock.step(timeAt());
    }
  }
  std::cout << "Flappy clock tests passed. State: " << sizeof(FlappyClock) << " bytes.\n";
}
