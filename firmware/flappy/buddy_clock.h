#pragma once

#include "buddy_renderer.h"
#include "clock_scene.h"

namespace flappy {

// The pocket clock has its own animation state and never touches a game profile.
class BuddyClock {
 public:
  void reset(uint32_t seed) { frame_ = 0; seed_ = seed ? seed : 1; }
  void step(const ClockTime&) { ++frame_; }
  void draw(uint16_t* pixels, const ClockTime& time) const {
    if (!pixels) return;
    ClockScene::clear(pixels,BuddyRenderer::rgb(37,28,45));
    BuddyRenderer::Scene scene;
    scene.hud = false; scene.bottom = 225; scene.frame = frame_;
    scene.theme = BuddyGame::Theme((seed_ / 7) % 5);
    const unsigned beat = frame_ % 600;
    scene.lane = beat < 300 ? 1 + float(beat) / 600 : 2 - float(beat) / 600;
    scene.rolling = beat > 410 && beat < 465;
    scene.jump = beat >= 190 && beat <= 250 ? float((beat - 190) * (250 - beat)) / 900 : 0;
    scene.board = beat > 260 && beat < 390;
    scene.mask = 1;
    BuddyRenderer::drawScene(pixels,scene);
    const auto ink = BuddyRenderer::rgb(37,28,45), cream = BuddyRenderer::rgb(255,239,201);
    ClockScene::rect(pixels,13,27,110,137,ink);
    ClockScene::rect(pixels,15,29,106,133,BuddyRenderer::rgb(70,48,78));
    ClockScene::rect(pixels,17,31,102,129,ink);
    BuddyRenderer::drawText(pixels,"BITTY BUDDY",25,11,cream,2);
    BuddyRenderer::drawText(pixels,"POCKET TIME",46,37,BuddyRenderer::rgb(106,242,204));
    for (unsigned digit = 0; digit < 4; ++digit) {
      const int ox = digit % 2 ? 82 : 39, oy = digit < 2 ? 58 : 111;
      for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 3; ++x) {
        if (ClockScene::digitPixel(time.digit(digit),x,y))
          BuddyRenderer::drawCoin(pixels,ox + int(x * 8),oy + int(y * 8),0);
        else ClockScene::rect(pixels,ox + int(x * 8) - 1,oy + int(y * 8) - 1,2,2,BuddyRenderer::rgb(81,56,84));
      }
    }
    BuddyRenderer::drawText(pixels,"HR",22,74,cream);
    BuddyRenderer::drawText(pixels,"MIN",20,127,cream);
    ClockScene::rect(pixels,34,101,68,1,BuddyRenderer::rgb(99,70,105));
    if (!time.twentyFourHour) BuddyRenderer::drawText(pixels,time.hour % 24 < 12 ? "AM" : "PM",107,127,cream);
    if ((time.second & 1) == 0) {
      BuddyRenderer::drawCoin(pixels,125,193,frame_);
      BuddyRenderer::drawCoin(pixels,10,179,frame_ + 14);
    }
    BuddyRenderer::drawText(pixels,"LITTLE FEET. BIG PLANS.",25,218,cream);
  }
 private:
  uint32_t frame_ = 0, seed_ = 1;
};

}  // namespace flappy
