#pragma once
#include "mario_clock.h"
#include "mario_viewport.h"
#include "nes_palette.h"
#include "clock_scene.h"

namespace flappy {

// Ordinary NES frames run the home course. Only private cached level-data
// windows change. Starting manual play unloads them and reloads the user's ROM.
class MarioAttract {
 public:
  enum { Width = 135, Height = 240, CourseCount = 4 };
  template<class Runtime>
  bool begin(Runtime& runtime, const uint8_t* rom, size_t length, uint32_t seed) {
    rom_ = nullptr;
    MarioCartridge cartridge;
    if (!cartridge.configure(rom,length) || !cartridge.clockSupported()) return false;
    rom_ = rom;
    course_ = uint8_t(seed % CourseCount);
    courses_ = deaths_ = pipes_ = goals_ = recoveries_ = 0; failures_ = 0;
    return runtime.load(rom,length) && prepare(runtime);
  }

  template<class Runtime>
  bool step(Runtime& runtime) {
    if (!rom_ || !runtime.ready()) return false;
    ++ticks_;
    const uint8_t mode = runtime.readRam(0x770), task = runtime.readRam(0x772);
    const uint8_t engine = runtime.readRam(0x0e);
    if (restart_) {
      if (--restart_ == 0) {
        if (advance_) { course_ = uint8_t((course_ + 1) % CourseCount); ++courses_; failures_ = 0; }
        return prepare(runtime);
      }
      return runtime.step(0);
    }
    uint8_t buttons = 0;
    if (!playing_) {
      // Title and world introduction retain their native engine transitions.
      if (ticks_ > 8 && mode == 0 && task == 3) {
        titleSeen_ = true;
        if (ticks_ % 24 == 0) buttons = 8;
      }
      if (titleSeen_ && mode == 1 && task == 3 && engine == 8) playing_ = true;
      if (ticks_ > 900) { ++recoveries_; queueRestart(true); }
    } else {
      ++playTicks_;
      if (engine == 4 || engine == 5) {
        if (!goalTicks_++) ++goals_;
        if (goalTicks_ >= 240) queueRestart(true);
      }
      if (engine == 3 && route_.pipeExit()) {
        if (++pipeTicks_ == 1) ++pipes_;
        queueRestart(true);
      } else if (engine == 6 || engine == 11) {
        ++deaths_; ++failures_; queueRestart(failures_ >= 2);
      } else if (runtime.readRam(0x75c) || runtime.readRam(0x75f) || mode == 0 || mode == 2 || mode == 3 || playTicks_ >= 3000) {
        if (playTicks_ >= 3000) ++recoveries_;
        queueRestart(true);
      } else if (mode == 1 && task == 3) {
        buttons = route_.input(runtime);
        const int x = int(runtime.readRam(0x6d)) * 256 + runtime.readRam(0x86);
        const int y = runtime.readRam(0xce);
        if (x == lastX_) ++stalled_; else stalled_ = 0;
        lastX_ = x;
        if (theme() == 0) {
          // Swimming taps have a release frame between strokes. Keep the body
          // in the water corridor, using the cartridge's own water physics.
          if (!route_.pauseFrames()) buttons = uint8_t(128 | 2);
          if (y > 115 && (playTicks_ % 20) < 12) buttons |= 1;
        } else {
          for (unsigned i = 0; i < 6; ++i) {
            if (!runtime.readRam(0x0f + i) || runtime.readRam(0xb6 + i) != 1) continue;
            const int id = runtime.readRam(0x16 + i);
            if (id >= 0x15 || runtime.readRam(0x1e + i) & 0x20) continue;
            const int ex = int(runtime.readRam(0x6e + i)) * 256 + runtime.readRam(0x87 + i);
            const int ey = runtime.readRam(0xcf + i);
            if (ex - x > -4 && ex - x < 56 && ey - y > -32 && ey - y < 40 && runtime.readRam(0x1d) == 0)
              jump_ = 24;
          }
          if (jump_) { buttons |= 1; --jump_; }
        }
        if (route_.pipeExit() && x >= int(route_.finishX()) - 130) {
          const int delta = int(route_.finishX()) + 8 - x;
          const int velocity = int8_t(runtime.readRam(0x57));
          const bool ground = runtime.readRam(0x1d) == 0;
          if (ground && y + 32 > 176 && !jump_) jump_ = 26;
          const int projected = x + velocity / 2;
          buttons = projected < int(route_.finishX()) + 7 ? 128 : projected > int(route_.finishX()) + 9 ? 64 : 0;
          if (jump_) { buttons |= 1; --jump_; }
          if (ground && y + 32 <= 176 && delta >= -1 && delta <= 1) buttons = 32;
        }
        if (engine == 8 && stalled_ > 360 && !route_.pauseFrames()) { ++recoveries_; queueRestart(true); }
      }
    }
    // All strips belonging to a frame use one stable horizontal crop.
    const int player = runtime.readRam(0x3ad);
    int target = player < 51 ? 0 : player > 172 ? 121 : player - 51;
    scroll_ = uint16_t(runtime.readRam(0x71a) * 256u + runtime.readRam(0x71c));
    if (route_.pauseFrames()) {
      const int worldX = int(runtime.readRam(0x6d)) * 256 + runtime.readRam(0x86);
      for (unsigned sign = 0; sign < route_.clockCount(); ++sign) {
        const int delta = worldX - route_.clockX(sign);
        if (delta >= -16 && delta <= 112) {
          target = int(route_.clockX(sign)) - scroll_ - 11;
          target = target < 0 ? 0 : target > 121 ? 121 : target;
          break;
        }
      }
    }
    // Keep the clearing transition smooth while braking settles native inertia.
    const int delta = target - crop_;
    crop_ = uint8_t(int(crop_) + (delta < -4 ? -4 : delta > 4 ? 4 : delta));
    playerX_ = player - crop_;
    playerY_ = runtime.readRam(0xce) + 17;
    return runtime.step(buttons);
  }

  // Keep the full vertical native frame. There is no stretching or vertical
  // camera, including while native orange platforms carry Mario up and down.
  void consume(int x1,int y1,int x2,int y2,const uint16_t* source,uint16_t* output) const {
    if (!source || !output || x1 < 0 || x2 > 255 || y1 < 0 || y2 > 239 || x2 < x1 || y2 < y1) return;
    const int left = x1 > crop_ ? x1 : crop_, right = x2 < crop_ + Width - 1 ? x2 : crop_ + Width - 1;
    if (right < left) return;
    const int stride = x2 - x1 + 1;
    for (int y = y1; y <= y2; ++y) {
      uint16_t* row = output + y * Width + left - crop_;
      if (y < 32) {
        const uint16_t sky = NesPalette::rgb565(prg(0x85cf + theme()));
        for (int x = left; x <= right; ++x) row[x - left] = sky;
      } else memcpy(row,source + (y - y1) * stride + left - x1,size_t(right - left + 1) * 2);
    }
  }

  void drawTime(uint16_t* pixels,const ClockTime& time) const {
    if (!pixels || !rom_) return;
    if (!playing_) ClockScene::clear(pixels,0);
    for (unsigned clock = 0; clock < route_.clockCount(); ++clock) {
      const int left = playing_ ? int(route_.clockX(clock)) - scroll_ - crop_ : 11;
      if (left >= Width || left + 112 < 0) continue;
      for (unsigned d = 0; d < 4; ++d) for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 3; ++x) {
        if (!ClockScene::digitPixel(time.digit(d),x,y)) continue;
        const int px = left + int((d % 2) * 64 + x * 16), py = 16 + int((d / 2) * 96 + y * 16);
        // Mario can pass through a few clock coins without being painted over.
        if (px < playerX_ + 16 && px + 16 > playerX_ && py < playerY_ + 16 && py + 16 > playerY_) continue;
        coin(pixels,px,py);
      }
      if (!playing_) break;
    }
  }
  uint16_t pauseFrames() const { return route_.pauseFrames(); }
  uint8_t course() const { return course_; }
  unsigned theme() const { const uint8_t types[] = {1,2,0,3}; return types[course_]; }
  uint32_t coursesCompleted() const { return courses_; }
  uint32_t deaths() const { return deaths_; }
  uint32_t pipesEntered() const { return pipes_; }
  uint32_t goalsReached() const { return goals_; }
  uint32_t recoveries() const { return recoveries_; }
  bool playing() const { return playing_; }
  bool restarting() const { return restart_ != 0; }
  unsigned crop() const { return crop_; }
  size_t levelBytes() const { return route_.levelBytes(); }

 private:
  friend struct MarioAttractTestAccess;
  const uint8_t* rom_ = nullptr;
  MarioClock route_;
  uint32_t ticks_ = 0, playTicks_ = 0, courses_ = 0, deaths_ = 0, pipes_ = 0, goals_ = 0, recoveries_ = 0;
  uint16_t restart_ = 0, stalled_ = 0, scroll_ = 0, pipeTicks_ = 0, goalTicks_ = 0;
  int lastX_ = 0, playerX_ = 0, playerY_ = 0;
  uint8_t course_ = 0, crop_ = 0, jump_ = 0, failures_ = 0;
  bool playing_ = false, advance_ = false, titleSeen_ = false;
  void queueRestart(bool advance) { if (!restart_) { restart_ = 90; advance_ = advance; } }
  uint8_t prg(unsigned address) const { return rom_[16 + address - 0x8000]; }
  template<class Runtime>
  static bool patch(Runtime& runtime,unsigned address,const uint8_t* data,size_t bytes) {
    return runtime.patchCachedPrg((address - 0x8000) / 8192,(address - 0x8000) % 8192,data,bytes);
  }
  template<class Runtime>
  bool prepare(Runtime& runtime) {
    route_.buildCourse(course_);
    route_.setPortrait(true);
    if (!route_.install(runtime)) return false;
    const uint8_t area = uint8_t(theme() * 32);
    const unsigned levelIndex = prg(0x9d28 + theme()), enemyIndex = prg(0x9ce0 + theme());
    const uint8_t levelLow = 0x8e, levelHigh = 0xa6, enemyLow = 1, enemyHigh = 0x9f;
    if (!patch(runtime,0x9cbc,&area,1) ||
        !patch(runtime,0x9d2c + levelIndex,&levelLow,1) || !patch(runtime,0x9d4e + levelIndex,&levelHigh,1) ||
        !patch(runtime,0x9ce4 + enemyIndex,&enemyLow,1) || !patch(runtime,0x9d06 + enemyIndex,&enemyHigh,1)) return false;
    // Authored encounters, separate from the imported cartridge's level lists.
    // A page command followed by an enemy uses the native course data format.
    const uint8_t land[] = {0x0f,0x02,0x2b,0x06,0x0f,0x03,0x58,0x25,0x0f,0x05,0x4b,0x00,0xff};
    const uint8_t water[] = {0x0f,0x02,0x27,0x07,0x0f,0x03,0x58,0x0a,0x0f,0x05,0x48,0x0a,0xff};
    const uint8_t castle[] = {0x0f,0x02,0x2b,0x06,0x0f,0x03,0x58,0x25,0x0f,0x05,0x43,0x1b,0xff};
    const uint8_t* enemies = theme() == 0 ? water : theme() == 3 ? castle : land;
    uint8_t encounters[sizeof(land) + 5];
    memcpy(encounters,enemies,sizeof(land) - 1);
    const uint8_t destination[] = {0x0f,6,0x0e,area,0,0xff};
    memcpy(encounters + sizeof(land) - 1,destination,sizeof(destination));
    if (!patch(runtime,0x9f01,encounters,sizeof(encounters))) return false;
    const uint8_t scenery = theme() == 1 ? 0x21 : 0x01;
    if (!patch(runtime,0xa68f,&scenery,1)) return false;
    runtime.setPal(false); runtime.setPcmEnabled(false); runtime.setVideoDivisor(1); runtime.resetVideoRows();
    ticks_ = playTicks_ = 0; restart_ = stalled_ = pipeTicks_ = goalTicks_ = 0; jump_ = crop_ = 0;
    playing_ = advance_ = titleSeen_ = false; lastX_ = 0;
    return runtime.softReset();
  }
  void coin(uint16_t* output,int x,int y) const {
    const unsigned number = theme() == 0 ? 0xc3 : 0xc2;
    const unsigned table = prg(0x8b0b) | (unsigned(prg(0x8b0f)) << 8);
    const unsigned offset = (number & 63) * 4;
    const unsigned palette = 0x8ca4 + theme() * 36 + 3 + 12;
    for (unsigned t = 0; t < 4; ++t) {
      const unsigned tile = prg(table + offset + t);
      const uint8_t* chr = rom_ + 16 + 32768 + 4096 + tile * 16;
      for (unsigned row = 0; row < 8; ++row) for (unsigned col = 0; col < 8; ++col) {
        const int px = x + int(t / 2 * 8 + col), py = y + int(t % 2 * 8 + row);
        if (px < 0 || px >= Width || py < 0 || py >= 225) continue;
        const unsigned ink = ((chr[row] >> (7 - col)) & 1u) | (((chr[row + 8] >> (7 - col)) & 1u) << 1);
        if (ink) output[py * Width + px] = NesPalette::rgb565(prg(palette + ink));
      }
    }
  }
};
} // namespace flappy
