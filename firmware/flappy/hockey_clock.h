#pragma once
#include "hockey_renderer.h"

namespace flappy {
class HockeyClock {
 public:
  void reset(uint32_t seed) { seed_=seed ? seed : 1; age_=0; minute_=255; game_.reset(seed_); }
  void step(const ClockTime& time) {
    if(minute_!=time.minute || game_.phase()==hockey::Phase::Finished) {
      minute_=time.minute; seed_^=seed_<<13; seed_^=seed_>>17; seed_^=seed_<<5; game_.reset(seed_);
    }
    const auto frame=game_.frame();
    int16_t moves[2]={};
    for(unsigned i=0;i<2;++i) {
      float target=game_.puck().x;
      if(frame.pickup!=hockey::Power::None && frame.pickupOwner==i) target=frame.pickupX;
      const float difference=target-game_.player(i).x;
      moves[i]=int16_t(difference<-2 ? -560 : difference>2 ? 560 : 0);
    }
    game_.tick(moves[0],moves[1]); ++age_;
  }
  void draw(uint16_t* pixels,const ClockTime& time) const { HockeyRenderer::draw(pixels,game_.frame(),1,&time); }
 private:
  HockeyGame game_; uint32_t seed_=1,age_=0; uint8_t minute_=255;
};
}  // namespace flappy
