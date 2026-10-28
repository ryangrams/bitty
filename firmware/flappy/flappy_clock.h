#pragma once
#include "clock_scene.h"
#include "skins.h"

namespace flappy {

// The home scene uses double-size source pixels and has no score or death state.
class FlappyClock {
 public:
  enum { CycleFrames = 800, GroundY = 224, BirdX = 25, Gap = 100, Cell = 12 };
  void reset(uint32_t seed) {
    random_ = seed ? seed : 1; frames_ = 0; y_ = 100 * 256; velocity_ = 0;
    for (unsigned i = 0; i < 3; ++i) pipes_[i] = {int32_t(140 + i * 156) * 256, uint8_t(72 + ClockScene::random(random_) % 74)};
    captureOrigins();
  }
  void step(const ClockTime&) {
    ++frames_;
    const Pipe* target = nullptr;
    for (unsigned i = 0; i < 3; ++i) {
      pipes_[i].x -= 192;
      if (pipes_[i].x < -52 * 256) {
        int32_t right = 0;
        for (unsigned j = 0; j < 3; ++j) if (pipes_[j].x > right) right = pipes_[j].x;
        pipes_[i] = {right + 156 * 256, uint8_t(72 + ClockScene::random(random_) % 74)};
      }
      if (pipes_[i].x + 52 * 256 > BirdX * 256 && (!target || pipes_[i].x < target->x)) target = &pipes_[i];
    }
    if (frames_ % CycleFrames == 190 || frames_ % CycleFrames == 540) captureOrigins();
    const int center = target ? target->center : 112;
    velocity_ += 40;
    if (velocity_ > 640) velocity_ = 640;
    if (y_ + 12 * 256 + velocity_ * 6 > center * 256 && velocity_ >= -64) velocity_ = -590;
    y_ += velocity_;
    // Preserve the safe gap during a late change of target. There is no crash,
    // score event or record path in this scene.
    int top = 22, bottom = GroundY - 26;
    if (target && target->x < (BirdX + 34) * 256) {
      top = center - Gap / 2 + 3;
      bottom = center + Gap / 2 - 27;
    }
    if (y_ < top * 256) { y_ = top * 256; velocity_ = 0; }
    if (y_ > bottom * 256) { y_ = bottom * 256; velocity_ = -500; }
  }

  void draw(uint16_t* pixels, const ClockTime& time, const Skins& skins) const {
    if (!pixels) return;
    const auto& bg = skins.background();
    for (int y = 0; y < ClockScene::Height; ++y) {
      int sy = y / 2 + 60; if (sy >= bg.height) sy = bg.height - 1;
      for (int x = 0; x < ClockScene::Width; ++x) {
        uint16_t c = bg.pixels[sy * bg.width + x / 2];
        if (c == FlappyAssets::kTransparent) c = FlappyAssets::background.pixels[sy * FlappyAssets::background.width + x / 2];
        pixels[y * ClockScene::Width + x] = skins.palette(c);
      }
    }
    const unsigned morph = morphAmount();
    for (unsigned i = 0; i < 3; ++i) {
      const int x = pipes_[i].x / 256, gapTop = int(pipes_[i].center) - Gap / 2;
      sprite(pixels,skins,skins.pipe(true),x,gapTop-int(skins.pipe(true).height)*2,255-morph);
      sprite(pixels,skins,skins.pipe(false),x,gapTop+Gap,255-morph);
    }
    if (morph) {
      for (unsigned digit = 0; digit < 4; ++digit) for (unsigned gy = 0; gy < 5; ++gy)
        for (unsigned gx = 0; gx < 3; ++gx) {
          if (!ClockScene::digitPixel(time.digit(digit),gx,gy)) continue;
          const int tx = 47 + int((digit % 2) * 4 + gx) * Cell;
          const int ty = 30 + int(digit / 2) * 88 + int(gy) * Cell;
          const unsigned slot = (digit * 3 + gx) % 3;
          const int originX = origins_[slot].x;
          const int originY = origins_[slot].center + (int(gy) - 2) * 18;
          const unsigned eased = morph * morph * (765u - 2u * morph) / 65025u;
          const int x = originX + (tx - originX) * int(eased) / 255;
          const int y = originY + (ty - originY) * int(eased) / 255;
          pipeCell(pixels,skins,x,y,morph);
        }
    }
    const int birdY = y_ / 256, birdX = BirdX-int(morph)*20/255, frame = int((frames_ / 6) % 3);
    for (int y = 0; y < 24; ++y) for (int x = 0; x < 34; ++x) {
      const uint16_t color = skins.birdPixel(frame,x/2,y/2);
      if (color != FlappyAssets::kTransparent)
        ClockScene::rect(pixels,birdX+x,birdY+y,1,1,color);
    }
    const auto& ground = skins.ground();
    for (int y = GroundY; y < ClockScene::Height; ++y) for (int x = 0; x < ClockScene::Width; ++x) {
      const int sy = (y-GroundY)/2 % ground.height;
      const int sx = ((x + int(frames_ * 3 / 4)) / 2) % ground.width;
      pixels[y * ClockScene::Width + x] = skins.palette(ground.pixels[sy * ground.width + sx]);
    }
  }

  uint32_t frames() const { return frames_; }
  int birdY() const { return y_ / 256; }
  int pipeX(unsigned i) const { return i < 3 ? int(pipes_[i].x / 256) : 0; }
  int pipeCenter(unsigned i) const { return i < 3 ? pipes_[i].center : 0; }
  unsigned morphAmount() const {
    const unsigned phase = frames_ % CycleFrames;
    if (phase < 190 || phase >= 600) return 0;
    if (phase < 250) return (phase - 190) * 255 / 60;
    if (phase < 540) return 255;
    return (600 - phase) * 255 / 60;
  }

 private:
  friend struct FlappyClockTestAccess;
  struct Pipe { int32_t x; uint8_t center; } pipes_[3] = {};
  struct Origin { int16_t x; uint8_t center; } origins_[3] = {};
  int32_t y_ = 100 * 256;
  int16_t velocity_ = 0;
  uint32_t frames_ = 0, random_ = 1;
  void captureOrigins() {
    for(unsigned i=0;i<3;++i) {
      int x=int(pipes_[i].x/256)+20;
      if(x < -24) x=-24;
      if(x > 148) x=148;
      origins_[i]={int16_t(x),pipes_[i].center};
    }
  }
  static uint16_t blend(uint16_t foreground, uint16_t background, unsigned opacity) {
    const unsigned inv = 255-opacity;
    return uint16_t(((((foreground >> 11) * opacity + (background >> 11) * inv)/255) << 11) |
        (((((foreground >> 5)&63) * opacity + ((background >> 5)&63) * inv)/255) << 5) |
        (((foreground&31) * opacity + (background&31) * inv)/255));
  }
  static void sprite(uint16_t* p,const Skins& skin,const FlappyAssets::Sprite& s,int x,int y,unsigned opacity) {
    if (!opacity) return;
    const int x0 = x < 0 ? -x : 0, x1 = x + int(s.width)*2 > ClockScene::Width ? ClockScene::Width-x : int(s.width)*2;
    const int y0 = y < 0 ? -y : 0, y1 = y + int(s.height)*2 > GroundY ? GroundY-y : int(s.height)*2;
    for (int py = y0; py < y1; ++py) for (int px = x0; px < x1; ++px) {
      const int at = (py/2)*s.width+px/2;
      const uint16_t c = s.pixels[at];
      if (c == FlappyAssets::kTransparent) continue;
      const unsigned a = s.alpha ? opacity * s.alpha[at]/255 : opacity;
      uint16_t& dest = p[(y+py)*ClockScene::Width+x+px];
      dest = a == 255 ? skin.palette(c) : blend(skin.palette(c),dest,a);
    }
  }
  static void pipeCell(uint16_t* p,const Skins& skin,int x,int y,unsigned opacity,int size=Cell) {
    const auto& s = skin.pipe(false);
    for (int py = 0; py < size; ++py) for (int px = 0; px < size; ++px) {
      if (x+px<0 || x+px>=ClockScene::Width || y+py<0 || y+py>=GroundY) continue;
      const int sx = 2 + px/2, sy = 2 + py/2;
      const int at=(sy % s.height)*s.width + sx % s.width;
      const uint16_t source=s.pixels[at];
      if(source == FlappyAssets::kTransparent) continue;
      const unsigned alpha=s.alpha ? opacity*s.alpha[at]/255 : opacity;
      if(!alpha) continue;
      const uint16_t color = skin.palette(source);
      uint16_t& dest = p[(y+py)*ClockScene::Width+x+px];
      dest = alpha == 255 ? color : blend(color,dest,alpha);
    }
  }
};
}  // namespace flappy
