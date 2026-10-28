#pragma once

#include "clock_scene.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {

// The home screen is an endless stage simulation. It reads the supplied
// cartridge's CHR, metatiles, animation tables and palettes without changing it.
// The ordinary NES game keeps its own engine and controller input.
class MarioWorldClock {
 public:
  enum class Theme : uint8_t { Overworld, Underground, Water, Castle };
  enum class Phase : uint8_t { Run, EnterPipe, Dark, ExitPipe };
  static constexpr int Width = ClockScene::Width, Height = ClockScene::Height;
  static constexpr unsigned MaxObjects = 40, MaxEnemies = 12, SignCount = 3;
  static constexpr unsigned ClockCoinSize = 16, ClockCells = 4 * 3 * 5;
  static constexpr int ClockWidth = 7 * ClockCoinSize;
  static constexpr int ClockHourY = 16, ClockMinuteY = 112;
  static constexpr int ClockBottom = ClockMinuteY + 5 * ClockCoinSize;
  static constexpr int ClockLeft = (Width - ClockWidth) / 2;
  static constexpr int ClockReadOffset = 34 - ClockLeft;

  void reset(uint32_t seed) {
    seed_ = rng_ = seed ? seed : 1;
    ticks_ = segment_ = distance_ = transitions_ = collectedCoins_ = stomps_ = rescues_ = 0;
    themeMask_ = enemyMask_ = 0;
    order_[0] = 0; order_[1] = 1; order_[2] = 2; order_[3] = 3;
    // Every group visits all four themes. Only the first stage is fixed.
    if (next() & 1u) { const uint8_t t = order_[1]; order_[1] = order_[2]; order_[2] = t; }
    if (next() & 1u) { const uint8_t t = order_[2]; order_[2] = order_[3]; order_[3] = t; }
    generate(false);
  }

  bool bindRom(const uint8_t* bytes, size_t length) {
    rom_ = nullptr;
    if (!bytes || length != 40976 || bytes[0] != 'N' || bytes[1] != 'E' ||
        bytes[2] != 'S' || bytes[3] != 0x1a || bytes[4] != 2 || bytes[5] != 1 ||
        bytes[6] != 1 || bytes[7] != 0) return false;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
      crc ^= bytes[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    if ((crc ^ 0xffffffffu) != 0x3337ec46u) return false;
    rom_ = bytes;
    loadPalette();
    return true;
  }

  void step(const ClockTime& time) {
    ++ticks_;
    if (!rom_) return;
    const uint16_t key=uint16_t(time.minuteKey() | (time.twentyFourHour ? 0x8000 : 0));
    if (key != minuteKey_) {
      minuteKey_ = key;
      memset(taken_, 0, sizeof(taken_));
    }
    updateEnemies();
    for (unsigned i=0;i<objectCount_;++i)
      if (objects_[i].kind == Kind::Question && objects_[i].variation && objects_[i].variation < 64)
        ++objects_[i].variation;
    if (phase_ != Phase::Run) { transition(); return; }

    const int oldX = playerX(), oldFeet = playerFeet();
    if (hold_) {
      --hold_;
      if (hold_ == 126 && theme_ != Theme::Water) vy_ = -1640;
      moveVertical(oldFeet);
      collectClockCoins(time);
      if (!hold_) activeSign_ = -1;
      return;
    }

    for (unsigned i = 0; i < SignCount; ++i) {
      if (!(seenSigns_ & (1u << i)) && oldX >= int(signs_[i]) + ClockReadOffset) {
        seenSigns_ |= uint8_t(1u << i);
        activeSign_ = int8_t(i); hold_ = 210;
        xq_ = int32_t(signs_[i] + ClockReadOffset) * 256;
        camera_ = int16_t(signs_[i] - ClockLeft);
        break;
      }
    }
    if (hold_) return;

    if (theme_ == Theme::Water) {
      // The original swimming frames accompany a gentle fixed-step stroke.
      const int target = 156 + wave(int(ticks_ / 3u), 28);
      const int delta = target * 256 - yq_;
      vy_ = int16_t(clamp(int(vy_) + clamp(delta / 30, -24, 24), -260, 260));
      yq_ += vy_;
      if (yq_ < 82 * 256) yq_ = 82 * 256;
      if (yq_ > 201 * 256) yq_ = 201 * 256;
    } else {
      if (grounded_) {
        bool jump = false;
        for (unsigned i = 0; i < objectCount_; ++i) {
          const Object& o = objects_[i];
          if (!solid(o) && o.kind != Kind::Gap) continue;
          if (o.y >= oldFeet && o.kind != Kind::Gap) continue;
          const int lead = o.kind == Kind::Gap ? 14 : 27;
          const int d = int(o.x) - oldX - 16;
          if (d >= -8 && d <= lead && o.y >= oldFeet - 52) { jump = true; break; }
        }
        for (unsigned i = 0; i < enemyCount_; ++i) {
          const Enemy& e = enemies_[i];
          if (e.state || !walking(e.kind)) continue;
          const int d = (e.xq >> 8) - oldX - 16;
          if (d >= 0 && d < 35) { jump = true; break; }
        }
        if (jump) { vy_ = -1640; grounded_ = false; }
      }
      moveVertical(oldFeet);
    }
    xq_ += theme_ == Theme::Water ? 224 : 320;
    if (playerX() != oldX) distance_ += unsigned(playerX() - oldX);
    camera_ = int16_t(playerX() - 34);
    if (camera_ < 0) camera_ = 0;
    interactEnemies(oldFeet);
    collectClockCoins(time);
    if (playerX() >= int(exitX_) + 8) {
      xq_ = int32_t(exitX_ + 8) * 256;
      camera_ = int16_t(playerX() - 34);
      if (playerFeet() >= ExitTop && (grounded_ || theme_ == Theme::Water)) {
        yq_ = int32_t(ExitTop) * 256;
        vy_ = 0; phase_ = Phase::EnterPipe; phaseTicks_ = 0;
      }
    }
  }

  void draw(uint16_t* pixels, const ClockTime& time) const {
    if (!pixels) return;
    ClockScene::clear(pixels, rom_ ? palette_[0] : 0);
    if (!rom_ || phase_ == Phase::Dark) return;
    background(pixels);
    drawObjects(pixels, false);
    drawSigns(pixels, time);
    drawEnemies(pixels);
    drawPlayer(pixels);
    // A native pipe lip masks the body during entry and emergence.
    drawObjects(pixels, true);
    if (theme_ == Theme::Water) {
      for (int i = 0; i < 4; ++i) {
        const int x = (91 + i * 43 - camera_ / 3) % (Width + 16) - 8;
        const int y = 198 - int((ticks_ + unsigned(i * 57)) % 176);
        tile(pixels, x, y, 0x74, 6, true);
      }
    }
    if (phase_ == Phase::EnterPipe && phaseTicks_ > 44)
      shutter(pixels, (int(phaseTicks_) - 44) * 8);
    if (phase_ == Phase::ExitPipe && phaseTicks_ < 18)
      shutter(pixels, (18 - int(phaseTicks_)) * 8);
  }

  bool ready() const { return rom_ != nullptr; }
  Theme theme() const { return theme_; }
  Phase phase() const { return phase_; }
  uint32_t segment() const { return segment_; }
  uint32_t transitions() const { return transitions_; }
  uint32_t travelledPixels() const { return distance_; }
  uint32_t coinsCollected() const { return collectedCoins_; }
  uint32_t enemiesStomped() const { return stomps_; }
  uint32_t safetyRebounds() const { return rescues_; }
  uint8_t themesSeen() const { return themeMask_; }
  uint32_t enemiesSeen() const { return enemyMask_; }
  bool clockReadable() const { return hold_ != 0; }
  int playerX() const { return int(xq_ >> 8); }
  int playerFeet() const { return int(yq_ >> 8); }

 private:
  friend struct MarioWorldClockTestAccess;
  enum class Kind : uint8_t { Pipe, Brick, Question, Solid, Platform, Mushroom,
    Gap, Bridge, Firebar, Cannon, Vine, CoinArc, Castle, Flag, Spring, Powerup, Toad, Bowser };
  enum class Foe : uint8_t { Goomba, Koopa, Buzzy, Paratroopa, Spiny, Blooper,
    Cheep, Piranha, Podoboo, Bullet, Hammer, Lakitu };
  struct Object { uint16_t x; uint8_t y, width, height; Kind kind; uint8_t variation; };
  struct Enemy { int32_t xq; int16_t yq, vx; uint16_t origin, age;
    Foe kind; uint8_t state, homeY, variation; };
  static constexpr int Floor = 216, ExitTop = 184;
  static constexpr uint16_t MetatileLow = 0x8b08, MetatileHigh = 0x8b0c;
  static constexpr uint16_t EnemyGraphics = 0xe73e, PlayerGraphics = 0xee17;
  static constexpr uint16_t PowerupGraphics = 0xe6be;

  const uint8_t* rom_ = nullptr;
  uint32_t seed_ = 1, rng_ = 1, ticks_ = 0, segment_ = 0, distance_ = 0;
  uint32_t transitions_ = 0, collectedCoins_ = 0, stomps_ = 0, enemyMask_ = 0, rescues_ = 0;
  int32_t xq_ = 32 * 256, yq_ = Floor * 256;
  int16_t vy_ = 0, camera_ = 0;
  uint16_t phaseTicks_ = 0, hold_ = 0, exitX_ = 1440, minuteKey_ = 0xffff;
  uint16_t signs_[SignCount] = {}, palette_[32] = {};
  uint32_t taken_[SignCount][2] = {};
  Object objects_[MaxObjects] = {};
  Enemy enemies_[MaxEnemies] = {};
  uint8_t objectCount_ = 0, enemyCount_ = 0, seenSigns_ = 0, themeMask_ = 0;
  uint8_t order_[4] = {0,1,2,3};
  int8_t activeSign_ = -1;
  Theme theme_ = Theme::Overworld;
  Phase phase_ = Phase::Run;
  bool grounded_ = true, night_ = false;

  uint32_t next() { return ClockScene::random(rng_); }
  static int clamp(int n, int low, int high) { return n < low ? low : n > high ? high : n; }
  static int wave(int frame, int extent) {
    const int p = frame & 127;
    return ((p < 64 ? p : 127 - p) * extent * 2 / 63) - extent;
  }
  uint8_t prg(unsigned address) const { return rom_[16 + address - 0x8000]; }

  // The six-bit 2C02 color indices use the same RGB565 conversion as the NES
  // renderer. These are hardware colors, not cartridge artwork.
  static uint16_t nesColor(unsigned n) {
    // Four luminance rows, each with sixteen hardware hue indices.
    // Converted from PeakRacing/nes (Apache-2.0, Copyright PeakRacing).
    // See LICENSES/PeakNES-Apache-2.0.txt and THIRD_PARTY_NOTICES.md.
    static const uint16_t rgb[4][16] = {
      {0x73ae,0x20d1,0x0015,0x4013,0x880e,0xa802,0xa000,0x7840,0x4160,0x0220,0x0280,0x01e2,0x19eb,0x0000,0x0000,0x0000},
      {0xbdf7,0x039d,0x21dd,0x801e,0xb817,0xe00b,0xd940,0xca61,0x8b80,0x04a0,0x0540,0x0487,0x0411,0x0000,0x0000,0x0000},
      {0xffff,0x3dff,0x5cbf,0xa45f,0xf3df,0xfbb6,0xfbac,0xfcc7,0xf5e7,0x8682,0x4ee9,0x5fd3,0x075b,0x0000,0x0000,0x0000},
      {0xffff,0xaf3f,0xc6bf,0xd65f,0xfe3f,0xfe3b,0xfdf6,0xfed5,0xff34,0xe7f4,0xaf97,0xb7f9,0x9ffe,0x0000,0x0000,0x0000}
    };
    return rgb[(n >> 4) & 3][n & 15];
  }

  void loadPalette() {
    if (!rom_) return;
    const unsigned nativeArea[] = {1,2,0,3};
    const unsigned area = nativeArea[unsigned(theme_)];
    const unsigned data = 0x8ca4 + area * 36 + 3;
    for (unsigned i = 0; i < 32; ++i) palette_[i] = nesColor(prg(data + i));
    palette_[0] = nesColor(night_ ? 0x0f : prg(0x85cf + area));
    for (unsigned p = 1; p < 8; ++p) palette_[p * 4] = palette_[0];
    for (unsigned o=0;o<objectCount_;++o) if (objects_[o].kind == Kind::Bowser)
      for (unsigned i=1;i<4;++i) palette_[20+i]=nesColor(prg(0x8d4f+i));
  }

  void add(Kind kind, unsigned x, unsigned y, unsigned w = 16, unsigned h = 16, unsigned v = 0) {
    if (objectCount_ < MaxObjects)
      objects_[objectCount_++] = {uint16_t(x),uint8_t(y),uint8_t(w),uint8_t(h),kind,uint8_t(v)};
  }
  void foe(Foe kind, unsigned x, unsigned y, unsigned variation = 0) {
    if (enemyCount_ >= MaxEnemies) return;
    if (walking(kind)) y=Floor-16;
    if (kind==Foe::Piranha) y+=8;
    enemies_[enemyCount_++] = {int32_t(x * 256),int16_t(y * 128),
      int16_t(kind == Foe::Bullet ? -420 : -88),uint16_t(x),uint16_t(next() % 120),
      kind,0,uint8_t(y),uint8_t(variation)};
    enemyMask_ |= uint32_t(1u << unsigned(kind));
  }

  void generate(bool pipeExit) {
    theme_ = Theme(order_[segment_ & 3]);
    themeMask_ |= uint8_t(1u << unsigned(theme_));
    night_ = theme_ == Theme::Overworld && segment_ && (next() % 5 == 0);
    objectCount_ = enemyCount_ = seenSigns_ = 0;
    hold_ = 0; activeSign_ = -1; vy_ = 0; camera_ = 0;
    xq_ = 32 * 256; yq_ = Floor * 256; grounded_ = true;
    memset(taken_, 0, sizeof(taken_));
    signs_[0] = 176; signs_[1] = 592; signs_[2] = 1008;
    exitX_ = uint16_t(1424 + (next() % 3) * 64);
    phaseTicks_ = 0;
    phase_ = pipeExit ? Phase::ExitPipe : Phase::Run;
    if (pipeExit) { add(Kind::Pipe,24,ExitTop,32,32); xq_ = 32 * 256; yq_ = 224 * 256; }

    // Clearings fit a complete coin clock inside the narrow portrait view.
    // The vignettes between them carry the procedural terrain and encounters.
    for (unsigned vignette = 0; vignette < 3; ++vignette) {
      const unsigned x = 344 + vignette * 416 + (next() % 3) * 16;
      const unsigned variant = next() % 5;
      if (theme_ == Theme::Water) {
        add(Kind::Solid,x,184,32,32,1);
        add(Kind::Solid,x+80,152 + 16 * (vignette & 1),32,64,1);
        add(Kind::CoinArc,x+8,112,64,8);
        foe(vignette & 1 ? Foe::Blooper : Foe::Cheep,x+28,120 + vignette * 16,vignette);
        foe(Foe::Cheep,x+124,172,vignette+1);
      } else if (theme_ == Theme::Castle) {
        add(Kind::Gap,x+32,Floor,48,24);
        add(Kind::Bridge,x+16,192,80,8);
        add(Kind::Firebar,x+104,168,8,8,2+vignette);
        add(Kind::Brick,x+96,184,32,16);
        foe(vignette == 2 ? Foe::Hammer : Foe::Podoboo,x+52,194,variant);
        if (variant == 0) add(Kind::Bowser,x+136,Floor-24,32,24);
      } else if (variant == 0) {
        add(Kind::Pipe,x,184,32,32);
        add(Kind::Pipe,x+96,168,32,48);
        foe(Foe::Piranha,x+8,160);
        foe(Foe::Goomba,x+65,Floor-16);
        add(Kind::CoinArc,x+32,136,64,16);
      } else if (variant == 1) {
        for (unsigned i = 0; i < 3; ++i) add(Kind::Solid,x+i*16,Floor-16-i*16,16,16+i*16);
        add(Kind::Platform,x+72,160,48,8,variant+vignette);
        add(Kind::Question,x+80,112,16,16);
        foe(Foe::Paratroopa,x+128,164);
      } else if (variant == 2) {
        add(Kind::Gap,x+32,Floor,24,24);
        add(Kind::Mushroom,x+72,160,48,56);
        add(Kind::CoinArc,x+8,142,64,16);
        foe(theme_ == Theme::Underground ? Foe::Buzzy : Foe::Koopa,x+120,Floor-24,vignette);
      } else if (variant == 3) {
        add(Kind::Brick,x+8,152,48,16);
        add(Kind::Question,x+56,152,16,16);
        add(Kind::Cannon,x+112,184,16,32);
        foe(Foe::Bullet,x+152,180);
        foe(Foe::Goomba,x+78,Floor-16);
      } else {
        add(Kind::Spring,x+8,192,16,24);
        add(Kind::Platform,x+72,160,48,8,vignette+1);
        add(Kind::Vine,x+128,88,16,128);
        foe(theme_ == Theme::Underground ? Foe::Spiny : Foe::Koopa,x+148,Floor-24,vignette);
      }
    }
    add(Kind::Pipe,exitX_,ExitTop,32,32);
    if (theme_ == Theme::Castle) add(Kind::Toad,exitX_-36,Floor-24,16,24,next()%12 == 0);
    else if (theme_ == Theme::Overworld && next()%3 == 0) {
      add(Kind::Castle,exitX_-112,136,80,80);
      add(Kind::Flag,exitX_-128,104,16,112);
    }
    if (next() % 11 == 0 && theme_ != Theme::Water)
      foe(Foe::Lakitu,64,48,1);
    if (next() % 5 == 0) add(Kind::Powerup,128,Floor-16,16,16,next()%4);
    loadPalette();
  }

  static bool solid(const Object& o) {
    return o.kind == Kind::Pipe || o.kind == Kind::Brick || o.kind == Kind::Question ||
        o.kind == Kind::Solid || o.kind == Kind::Platform || o.kind == Kind::Mushroom ||
        o.kind == Kind::Bridge || o.kind == Kind::Cannon || o.kind == Kind::Spring;
  }
  static bool walking(Foe kind) {
    return kind == Foe::Goomba || kind == Foe::Koopa || kind == Foe::Buzzy ||
        kind == Foe::Spiny || kind == Foe::Hammer;
  }
  int objectY(const Object& o) const {
    return o.kind == Kind::Platform ? int(o.y) + wave(int(ticks_/2u + o.variation * 19),12) : o.y;
  }
  bool gapAt(int x) const {
    for (unsigned i=0;i<objectCount_;++i) {
      const Object& o = objects_[i];
      if (o.kind == Kind::Gap && x >= o.x && x < int(o.x + o.width)) return true;
    }
    return false;
  }
  void moveVertical(int oldFeet) {
    if (theme_ == Theme::Water) return;
    vy_ = int16_t(clamp(int(vy_) + 82, -2000, 1700));
    yq_ += vy_;
    const int x = playerX();
    if (vy_ < 0) for (unsigned i=0;i<objectCount_;++i) {
      Object& o=objects_[i];
      if ((o.kind==Kind::Question || o.kind==Kind::Brick) && x+13>o.x && x+3<int(o.x+o.width) &&
          oldFeet-16>=int(o.y+o.height) && playerFeet()-16<int(o.y+o.height)) {
        yq_=int32_t(o.y+o.height+16)*256; vy_=0;
        if (o.kind==Kind::Question && !o.variation) { o.variation=1; ++collectedCoins_; }
      }
    }
    int landing = gapAt(x+8) ? 248 : Floor;
    for (unsigned i=0;i<objectCount_;++i) {
      const Object& o = objects_[i];
      const int y = objectY(o);
      if (solid(o) && x+13 > o.x && x+3 < int(o.x+o.width) && oldFeet <= y+3 && y < landing)
        landing = y;
    }
    grounded_ = false;
    if (vy_ >= 0 && playerFeet() >= landing) {
      yq_ = landing * 256; vy_ = 0; grounded_ = true;
    }
    // Clock actors are invulnerable. A missed edge gets an immediate rebound
    // before leaving the visible playfield, with the normal jumping sprite.
    if (playerFeet() > Floor+4) { yq_ = Floor*256; vy_ = -1640; grounded_ = false; ++rescues_; }
  }

  void updateEnemies() {
    for (unsigned i=0;i<enemyCount_;++i) {
      Enemy& e = enemies_[i]; ++e.age;
      if (e.state) { if (e.state < 120) ++e.state; continue; }
      if (walking(e.kind)) {
        e.xq += e.vx;
        if ((e.xq >> 8) < int(e.origin)-27 || (e.xq >> 8) > int(e.origin)+27) e.vx = int16_t(-e.vx);
        e.yq = int16_t((int(e.homeY) + (e.kind == Foe::Hammer ? -clamp(wave(e.age,22),0,22) : 0)) * 128);
      } else if (e.kind == Foe::Piranha) {
        e.yq = int16_t((int(e.homeY) + clamp(wave(e.age,28),0,28)) * 128);
      } else if (e.kind == Foe::Podoboo) {
        e.yq = int16_t((int(e.homeY) - clamp(wave(e.age/2,60),0,60)) * 128);
      } else if (e.kind == Foe::Bullet) {
        e.xq += e.vx;
        if ((e.xq >> 8) < int(e.origin)-190) e.xq = int32_t(e.origin)*256;
      } else if (e.kind == Foe::Lakitu) {
        e.xq = xq_ + 38*256;
        e.yq = int16_t((int(e.homeY)+wave(e.age/2,8))*128);
      } else {
        e.xq = int32_t((int(e.origin) + wave(e.age/2,20))*256);
        e.yq = int16_t((int(e.homeY) + wave(e.age/2+24,20))*128);
      }
    }
  }

  void interactEnemies(int oldFeet) {
    if (theme_ == Theme::Water || vy_ < 0) return;
    for (unsigned i=0;i<enemyCount_;++i) {
      Enemy& e = enemies_[i];
      if (e.state || !walking(e.kind) || e.kind == Foe::Spiny) continue;
      const int x=e.xq>>8, y=e.yq>>7;
      if (playerX()+13 > x && playerX()+3 < x+16 && oldFeet <= y+5 && playerFeet() >= y) {
        e.state=1; ++stomps_; vy_=-1110; yq_=y*256; grounded_=false;
      }
    }
  }

  void transition() {
    ++phaseTicks_;
    if (phase_ == Phase::EnterPipe) {
      if (phaseTicks_ <= 44) yq_ += 192;
      if (phaseTicks_ == 62) { phase_ = Phase::Dark; phaseTicks_=0; }
    } else if (phase_ == Phase::Dark) {
      if (phaseTicks_ == 12) {
        ++segment_; ++transitions_;
        if ((segment_ & 3u) == 0) {
          for (unsigned i=3;i>0;--i) { const unsigned j=next()%(i+1); const uint8_t t=order_[i]; order_[i]=order_[j]; order_[j]=t; }
          if (order_[0] == uint8_t(theme_)) { const uint8_t t=order_[0]; order_[0]=order_[1]; order_[1]=t; }
        }
        generate(true);
      }
    } else if (phase_ == Phase::ExitPipe) {
      if (phaseTicks_ > 18 && playerFeet() > ExitTop) yq_ -= 192;
      if (phaseTicks_ >= 74) {
        yq_=ExitTop*256; vy_=0; grounded_=true; phase_=Phase::Run; phaseTicks_=0;
      }
    }
  }

  // A square native coin metatile forms each cell. Two digits sit above two
  // digits, with one empty cell between each pair and between the two rows.
  static bool signCell(const ClockTime& time, unsigned cell, int& x, int& y) {
    if (cell >= ClockCells) return false;
    const unsigned digit = cell / 15, column = cell % 3, row = (cell % 15) / 3;
    x = int((digit % 2) * 4 + column) * int(ClockCoinSize);
    y = (digit < 2 ? ClockHourY : ClockMinuteY) + int(row * ClockCoinSize);
    return ClockScene::digitPixel(time.digit(digit), column, row);
  }
  unsigned collectedCells(unsigned sign) const {
    unsigned count = 0;
    for (unsigned word = 0; word < 2; ++word)
      for (uint32_t bits = taken_[sign][word]; bits; bits &= bits - 1) ++count;
    return count;
  }
  void collectClockCoins(const ClockTime& time) {
    for (unsigned s=0;s<SignCount;++s) {
      if (playerX()+16 < signs_[s] || playerX() > int(signs_[s])+ClockWidth) continue;
      unsigned collected = collectedCells(s);
      // Occasional collected coins leave the time readable during its pause.
      for (unsigned cell=0;cell<ClockCells && collected<3;++cell) {
        int localX, y;
        if (!signCell(time,cell,localX,y) || (taken_[s][cell/32] & (1u<<(cell%32)))) continue;
        const int x=int(signs_[s])+localX;
        if (playerX()+13>x && playerX()+3<x+int(ClockCoinSize) &&
            playerFeet()>y && playerFeet()-16<y+int(ClockCoinSize)) {
          taken_[s][cell/32] |= 1u<<(cell%32); ++collectedCoins_; ++collected;
        }
      }
    }
  }

  void tile(uint16_t* pixels, int x, int y, uint8_t number, unsigned pal,
            bool sprite=false, bool flipX=false, bool flipY=false, bool transparent=false) const {
    if (x<=-8 || y<=-8 || x>=Width || y>=Height) return;
    const uint8_t* chr=rom_+16+32768+(sprite?0:4096)+unsigned(number)*16;
    const int left=x<0?-x:0, top=y<0?-y:0;
    const int right=x+8>Width?Width-x:8, bottom=y+8>Height?Height-y:8;
    const uint16_t* colors=palette_+(pal&7)*4;
    for (int py=top;py<bottom;++py) {
      const unsigned sy=flipY?7u-unsigned(py):unsigned(py);
      const uint8_t low=chr[sy], high=chr[sy+8];
      uint16_t* out=pixels+(y+py)*Width;
      for (int px=left;px<right;++px) {
        const unsigned shift=flipX?unsigned(px):7u-unsigned(px);
        const unsigned value=((low>>shift)&1u)|(((high>>shift)&1u)<<1);
        if (value || (!sprite && !transparent)) out[x+px]=colors[value];
      }
    }
  }

  void meta(uint16_t* pixels, int x, int y, uint8_t number, bool transparent=true) const {
    if (x<=-16 || y<=-16 || x>=Width || y>=Height) return;
    const unsigned pal=number>>6, offset=(number&63)*4;
    const unsigned table=prg(MetatileLow+pal)|(unsigned(prg(MetatileHigh+pal))<<8);
    if (table<0x8000 || table+offset+3>=0x10000) return;
    tile(pixels,x,y,prg(table+offset),pal,false,false,false,transparent);
    tile(pixels,x,y+8,prg(table+offset+1),pal,false,false,false,transparent);
    tile(pixels,x+8,y,prg(table+offset+2),pal,false,false,false,transparent);
    tile(pixels,x+8,y+8,prg(table+offset+3),pal,false,false,false,transparent);
  }

  void spriteFrame(uint16_t* pixels, int x, int y, unsigned table, unsigned rows,
                   unsigned pal, bool flip=false, bool symmetric=false) const {
    if (x<=-16 || x>=Width || y<=-int(rows*8) || y>=Height) return;
    for (unsigned row=0;row<rows;++row) {
      const uint8_t l=prg(table+row*2), r=prg(table+row*2+1);
      tile(pixels,x,y+int(row*8),flip?r:l,pal,true,flip);
      tile(pixels,x+8,y+int(row*8),flip?l:r,pal,true,symmetric&&l==r ? !flip : flip);
    }
  }

  void coin(uint16_t* pixels, int x, int y, unsigned phase) const {
    // Native jumping coins consist of a top tile and its vertical mirror.
    const uint8_t number=prg(0xe682+(phase&3));
    tile(pixels,x,y,number,6,true);
    tile(pixels,x,y+8,number,6,true,false,true);
  }

  void background(uint16_t* pixels) const {
    const int first=camera_/16;
    for (int column=first;column<=first+9;++column) {
      const int x=column*16-camera_;
      if (theme_ == Theme::Water) {
        meta(pixels,x,24,0x86);
        for(int y=40;y<Height;y+=16) meta(pixels,x,y,0x87);
        if ((column+int(seed_&0xffff))%5==0) meta(pixels,x,200,0x22);
      } else if (theme_ == Theme::Underground) {
        meta(pixels,x,0,0x51); meta(pixels,x,16,0x51);
        if ((column+int(seed_&255))%9<2) {
          meta(pixels,x,32,0x51); meta(pixels,x,48,0x51);
        }
      } else if (theme_ == Theme::Castle) {
        meta(pixels,x,0,0x62); meta(pixels,x,16,0x62);
        if (column%8<2) {
          for(int y=40;y<184;y+=16) meta(pixels,x,y,0x47);
        }
      }
      if (!gapAt(column*16+8)) {
        const uint8_t ground=theme_==Theme::Water?0x69:theme_==Theme::Underground?0x52:theme_==Theme::Castle?0x62:0x54;
        meta(pixels,x,Floor,ground); meta(pixels,x,Floor+16,ground);
      } else {
        meta(pixels,x,Floor+8,0x86);
        meta(pixels,x,Floor+24,0x87);
      }
    }
    if (theme_ != Theme::Overworld) return;
    for (int group=camera_/128-1;group<=camera_/128+2;++group) {
      if (group<0) continue;
      const int cloudX=group*128+16-camera_, cloudY=30+((group*37+int(seed_&31))%3)*24;
      for(int i=0;i<3;++i) {
        meta(pixels,cloudX+i*16,cloudY,uint8_t(0x80+i));
        meta(pixels,cloudX+i*16,cloudY+16,uint8_t(0x83+i));
      }
      const int hill=group*128+64-camera_;
      meta(pixels,hill,184,5); meta(pixels,hill+16,168,7); meta(pixels,hill+16,184,6);
      meta(pixels,hill+32,184,8); meta(pixels,hill+32,200,9);
      meta(pixels,hill,200,6); meta(pixels,hill+16,200,10);
      for(int i=0;i<3;++i) meta(pixels,group*128+8+i*16-camera_,200,uint8_t(i+2));
    }
  }

  void drawObjects(uint16_t* pixels, bool pipeLips) const {
    for(unsigned i=0;i<objectCount_;++i) {
      const Object& o=objects_[i]; const int x=int(o.x)-camera_, y=objectY(o);
      if(x<=-int(o.width)-40 || x>=Width+40) continue;
      if(pipeLips) {
        if(o.kind==Kind::Pipe) {
          meta(pixels,x,y,0x10); meta(pixels,x+16,y,0x11);
          for(int yy=y+16;yy<Floor;yy+=16) { meta(pixels,x,yy,0x14);meta(pixels,x+16,yy,0x15); }
        }
        continue;
      }
      switch(o.kind) {
        case Kind::Pipe:
          meta(pixels,x,y,0x10);meta(pixels,x+16,y,0x11);
          for(int yy=y+16;yy<y+o.height;yy+=16) {meta(pixels,x,yy,0x14);meta(pixels,x+16,yy,0x15);} break;
        case Kind::Brick: case Kind::Question: case Kind::Solid:
          for(int yy=0;yy<o.height;yy+=16) for(int xx=0;xx<o.width;xx+=16)
            meta(pixels,x+xx,y+yy,o.kind==Kind::Question?(o.variation?0xc4:0xc0):o.kind==Kind::Brick?0x51:o.variation?0x69:0x61);
          if(o.kind==Kind::Question&&o.variation&&o.variation<48)
            coin(pixels,x+4,y-16-clamp(int(o.variation),0,20),unsigned(ticks_/5));
          break;
        case Kind::Platform:
          for(int xx=0;xx<o.width;xx+=8) tile(pixels,x+xx,y,0x80,5,true);
          break;
        case Kind::Mushroom:
          for(int xx=0;xx<o.width;xx+=16) meta(pixels,x+xx,y,xx==0?0x19:xx+16>=o.width?0x1b:0x1a);
          for(int yy=y+16;yy<Floor;yy+=16) meta(pixels,x+o.width/2-8,yy,0x50);
          break;
        case Kind::Bridge:
          for(int xx=0;xx<o.width;xx+=16) meta(pixels,x+xx,y,theme_==Theme::Castle?0x89:0x63);
          break;
        case Kind::Firebar: {
          meta(pixels,x-4,y-4,0xc4);
          const unsigned phase=(ticks_/2+o.variation*17)&127;
          for(unsigned n=1;n<=4;++n) {
            const int dx=wave(int(phase),int(n*7)), dy=wave(int(phase)+32,int(n*7));
            tile(pixels,x+dx,y+dy,uint8_t(0x64+((ticks_/4)&1)),6,true);
          }
          break;
        }
        case Kind::Cannon:
          meta(pixels,x,y,0x64);
          for(int yy=y+16;yy<y+o.height;yy+=16) meta(pixels,x,yy,yy==y+16?0x65:0x66);
          break;
        case Kind::Vine:
          for(int yy=y;yy<y+o.height;yy+=8) {tile(pixels,x,yy,uint8_t(yy==y?0xe1:0xe0),5,true);}
          break;
        case Kind::CoinArc:
          for(int c=0;c<6;++c) coin(pixels,x+c*12,y-((c<3?c:5-c)*7),unsigned(ticks_/8)+unsigned(c));
          break;
        case Kind::Castle:
          for(int row=0;row<5;++row) for(int col=0;col<5;++col) {
            uint8_t m=0x47;
            if(row==0) m=0x45;
            else if(row==1&&(col==1||col==3))m=col==1?0x46:0x48;
            else if(col==2&&row>=3)m=row==3?0x4a:0x4b;
            meta(pixels,x+col*16,y+row*16,m);
          }
          break;
        case Kind::Flag:
          meta(pixels,x,y,0x24);
          for(int yy=y+16;yy<Floor;yy+=16)meta(pixels,x,yy,0x25);
          tile(pixels,x+8,y+18,0x7e,5,true);
          break;
        case Kind::Spring:
          spriteFrame(pixels,x,y,EnemyGraphics+0xf0+((ticks_/14)%3)*6,3,6,false,true); break;
        case Kind::Powerup:
          spriteFrame(pixels,x+wave(int(ticks_/3),10),y,PowerupGraphics+(o.variation&3)*4,2,
                      o.variation==2?4+((ticks_/6)&3):4+(prg(0xe6ce + (o.variation&3))&3),false,true);
          break;
        case Kind::Toad:
          spriteFrame(pixels,x,y,EnemyGraphics+(o.variation?0x9c:0xa2),3,6,false,true); break;
        case Kind::Bowser: {
          const unsigned offset=0xd2+((ticks_/16)&1)*12;
          spriteFrame(pixels,x,y,EnemyGraphics+offset,3,5);
          spriteFrame(pixels,x+16,y,EnemyGraphics+offset+6,3,5); break;
        }
        case Kind::Gap: break;
      }
    }
  }

  void drawSigns(uint16_t* pixels, const ClockTime& time) const {
    for(unsigned s=0;s<SignCount;++s) {
      const int x=int(signs_[s])-camera_;
      if(x>=Width || x+ClockWidth<=0)continue;
      for(unsigned cell=0;cell<ClockCells;++cell) {
        int localX, y;
        if(signCell(time,cell,localX,y)&&!(taken_[s][cell/32]&(1u<<(cell%32))))
          meta(pixels,x+localX,y,theme_==Theme::Water?0xc3:0xc2);
      }
    }
  }

  void drawPlayer(uint16_t* pixels) const {
    unsigned frame=0x60+((ticks_/7)%3)*8;
    if(phase_!=Phase::Run || hold_) frame=0xb8;
    if(theme_==Theme::Water&&phase_==Phase::Run) frame=0x88+((ticks_/9)%3)*8;
    else if(!grounded_ && phase_==Phase::Run) frame=0x80;
    spriteFrame(pixels,playerX()-camera_,playerFeet()-16,PlayerGraphics+frame+4,2,4,false,true);
  }

  void drawEnemies(uint16_t* pixels) const {
    for(unsigned i=0;i<enemyCount_;++i) {
      const Enemy& e=enemies_[i]; const int x=(e.xq>>8)-camera_, y=e.yq>>7;
      if(x<-20||x>=Width+4||e.state>=32) continue;
      unsigned offset=0x54, palette=7; bool symmetrical=false, flip=e.vx>0;
      if(e.state) {offset=e.kind==Foe::Goomba?0x8a:0x72;}
      else switch(e.kind) {
        case Foe::Goomba: offset=0x54; flip=((ticks_/8)&1)!=0;break;
        case Foe::Koopa:offset=0x0c+((ticks_/12)&1)*6;palette=e.variation&1?6:5;break;
        case Foe::Buzzy:offset=((ticks_/10)&1)*6;palette=7;break;
        case Foe::Paratroopa:offset=0x18+((ticks_/9)&1)*6;palette=5;break;
        case Foe::Spiny:offset=0x24+((ticks_/10)&1)*6;palette=6;break;
        case Foe::Blooper:offset=0x3c+((ticks_/16)&1)*6;palette=7;symmetrical=true;break;
        case Foe::Cheep:offset=0x48+((ticks_/11)&1)*6;palette=e.variation&1?6:5;break;
        case Foe::Piranha:offset=0xc0+((ticks_/9)&1)*6;palette=5;symmetrical=true;break;
        case Foe::Podoboo:offset=0xcc;palette=6;symmetrical=true;break;
        case Foe::Bullet:offset=0xea;palette=7;break;
        case Foe::Hammer:offset=0xa8+((ticks_/10)&3)*6;palette=5;break;
        case Foe::Lakitu:offset=0x90+((ticks_/18)&1)*6;palette=5;symmetrical=true;break;
      }
      spriteFrame(pixels,x,y-8,EnemyGraphics+offset,3,palette,flip,symmetrical);
    }
  }

  static void shutter(uint16_t* pixels, int closed) {
    const int n=clamp(closed,0,Height/2);
    ClockScene::rect(pixels,0,0,Width,n,0);
    ClockScene::rect(pixels,0,Height-n,Width,n,0);
  }
};

}  // namespace flappy
