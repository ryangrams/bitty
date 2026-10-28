#pragma once
#include <math.h>
#include <stdint.h>
#include "arcade_support.h"
#include "hockey_state.h"

namespace flappy {

class HockeyGame {
 public:
  using Power = hockey::Power;
  using Phase = hockey::Phase;
  struct Puck { float x=67, y=122, vx=0, vy=0; uint16_t life=0; int8_t owner=-1, last=-1; bool active=false; };
  struct Player { float x=67, velocity=0; Power power=Power::None; uint16_t remaining=0, stun=0; };
  void reset(uint32_t seed, hockey::Options options = hockey::Options()) {
    *this = HockeyGame(); rng_=seed ? seed : 1;
    options_=options; map_=options.map && options.map<=hockey::MapCount ? uint8_t(options.map-1) : uint8_t(random()%hockey::MapCount);
    options_.palette &= 3;
    if (uint8_t(options_.powers)>2) options_.powers=hockey::Powers::Queen;
    pickupTimer_=132; phase_=Phase::Serve; wait_=36; serve(0);
  }
  void tick(int16_t queen, int16_t peasant) {
    ++ticks_;
    if (phase_==Phase::Finished) return;
    move(0,queen); move(1,peasant);
    for (unsigned i=0;i<2;++i) {
      if (players_[i].stun) --players_[i].stun;
      if (players_[i].remaining && !--players_[i].remaining) players_[i].power=Power::None;
    }
    if (phase_!=Phase::Rally) {
      if (wait_ && !--wait_) {
        if (phase_==Phase::Goal) { serve(serveTo_); phase_=Phase::Serve; wait_=36; }
        else { phase_=Phase::Rally; pucks_[0].vy=serveTo_ ? 105.f : -105.f; pucks_[0].vx=float(int(random()%71)-35); }
      }
      return;
    }
    updatePickup();
    for (unsigned i=0;i<2;++i) laser(i);
    for (unsigned i=0;i<3;++i) {
      Puck& p=pucks_[i]; if (!p.active) continue;
      if (i && p.life && !--p.life) { p.active=false; continue; }
      for (unsigned sub=0;sub<4 && p.active && phase_==Phase::Rally;++sub) {
        p.x+=p.vx/240.f; p.y+=p.vy/240.f;
        if (p.x<9) { p.x=18-p.x; p.vx=fabsf(p.vx); signal(ArcadeCue::Hit); }
        if (p.x>125) { p.x=250-p.x; p.vx=-fabsf(p.vx); signal(ArcadeCue::Hit); }
        for (unsigned who=0;who<2;++who) paddle(p,who);
        bumpers(p);
        for (unsigned who=0;who<2;++who) {
          const float direction=who ? -1.f : 1.f;
          if (players_[who].power==Power::Beam && fabsf(p.x-players_[who].x)<7) {
            p.vy+=direction*1200.f/240.f; p.vx*=.98f; cap(p,200.f);
          }
        }
        if (map_==1 && p.x>83 && p.x<101) { p.vy*=1.0015f; cap(p,210.f); }
        if (map_==4 && (ticks_%480)>=360) p.vx+=(ticks_/480%2 ? -100.f : 100.f)/240.f;
        if (map_==6 && p.x<31 && fabsf(p.y-122)<17) { p.vx*=.992f; p.vy*=.992f; }
        if (p.y<24 || p.y>220) {
          const unsigned goal=p.y<24 ? 0 : 1;
          const float half=players_[goal].power==Power::TinyGoal ? 11.f : options_.powers==hockey::Powers::Queen && goal==1 ? 28.f : 22.f;
          if (fabsf(p.x-67)<half-4 && players_[goal].power!=Power::Wall && (p.owner<0 || p.owner!=int(goal))) {
            score(1-goal); break;
          }
          p.y=goal==0 ? 48-p.y : 440-p.y; p.vy=goal==0 ? fabsf(p.vy) : -fabsf(p.vy); signal(ArcadeCue::Hit);
        }
        if (pickup_!=Power::None && p.last==pickupOwner_ && distance(p.x,p.y,float(pickupX_),paddleY(pickupOwner_))<9*9) collect();
      }
      if (p.active && phase_==Phase::Rally) {
        if (fabsf(p.vy)<18.f) { if (++stalled_[i]>120) { p.vy=p.y<122 ? 90.f : -90.f; stalled_[i]=0; } }
        else stalled_[i]=0;
        cap(p,players_[p.last==1 ? 1 : 0].power==Power::Rocket ? 250.f : 210.f);
      }
    }
  }
  void hold() { ++ticks_; }
  hockey::Frame frame() const {
    hockey::Frame f; f.tick=uint16_t(ticks_); f.map=map_; f.palette=options_.palette; f.rules=options_.powers; f.phase=phase_; f.puckMask=0;
    for (unsigned i=0;i<3;++i) { f.x[i]=pixel(pucks_[i].x,134); f.y[i]=pixel(pucks_[i].y,239); if(pucks_[i].active) f.puckMask|=uint8_t(1<<i); }
    for (unsigned i=0;i<2;++i) {
      f.paddles[i]=pixel(players_[i].x,134); f.scores[i]=scores_[i]; f.powers[i]=players_[i].power;
      f.remaining[i]=uint8_t((players_[i].remaining+14)/15); if(players_[i].stun) f.stunMask|=uint8_t(1<<i);
    }
    f.pickup=pickup_; f.pickupX=pickupX_; f.pickupOwner=pickupOwner_; f.pickupLife=uint8_t((pickupLife_+14)/15); f.event=event_;
    return f;
  }
  const Puck& puck(unsigned i=0) const { return pucks_[i<3 ? i : 0]; }
  const Player& player(unsigned i) const { return players_[i&1]; }
  Phase phase() const { return phase_; }
  uint8_t map() const { return map_; }
  ArcadeCue takeCue() { const ArcadeCue c=cue_; cue_=ArcadeCue::None; return c; }
  static float paddleY(unsigned who) { return who ? 204.f : 40.f; }
  static int radius(const hockey::Frame& f,unsigned who) {
    return f.powers[1-who]==Power::Shrink ? 7 : f.powers[who]==Power::Mega ? 14 : 10;
  }
  static unsigned bumperCount(uint8_t map) { return map==3 ? 2 : map==5 || map==6 ? 4 : map==2 || map==7 ? 2 : 0; }
  static void bumper(uint8_t map,uint32_t tick,unsigned i,float& x,float& y,float& r) {
    r=5; x=(i&1) ? 104.f : 30.f; y=122.f;
    if (map==3) { x=67.f+sinf(float(tick%720)*.008726646f+(i ? 3.14159f : 0.f))*32.f; y=i ? 150.f : 94.f; r=6; }
    else if (map==2) { y=i ? 151.f : 93.f; r=(tick%420)>330 ? 8.f : 0.f; }
    else if (map==5 || map==6) { y=i<2 ? 94.f : 150.f; r=map==6 ? 6.f : 5.f; }
  }
 private:
  friend struct HockeyTestAccess;
  hockey::Options options_;
  uint32_t rng_=1,ticks_=0;
  Puck pucks_[3]; Player players_[2]; uint8_t scores_[2]={};
  uint8_t map_=0,serveTo_=0,pickupOwner_=0,pickupX_=67;
  Power pickup_=Power::None,lastPower_=Power::None;
  Phase phase_=Phase::Serve;
  uint16_t wait_=36,pickupTimer_=132,pickupLife_=0,stalled_[3]={};
  ArcadeCue cue_=ArcadeCue::None;
  uint8_t event_=0;
  void signal(ArcadeCue cue) { cue_=cue; event_=uint8_t(((event_+8)&248)|uint8_t(cue)); }
  uint32_t random() { rng_^=rng_<<13; rng_^=rng_>>17; rng_^=rng_<<5; return rng_; }
  static uint8_t pixel(float f,int max) { return uint8_t(f<0 ? 0 : f>max ? max : int(f+.5f)); }
  static float distance(float x,float y,float a,float b) { return (x-a)*(x-a)+(y-b)*(y-b); }
  static void cap(Puck& p,float maximum) { const float s=p.vx*p.vx+p.vy*p.vy; if (s>maximum*maximum) { const float f=maximum/sqrtf(s); p.vx*=f; p.vy*=f; } }
  int radius(unsigned who) const { return players_[1-who].power==Power::Shrink ? 7 : players_[who].power==Power::Mega ? 14 : 10; }
  void move(unsigned who,int16_t amount) {
    float a=amount < -1024 ? -1.f : amount>1024 ? 1.f : float(amount)/1024.f;
    const Power attack=players_[1-who].power;
    if (attack==Power::Backwards) a=-a;
    if (attack==Power::Butterflies) a+=sinf(float(ticks_%360)*.0783f)*.45f;
    float velocity=a*(attack==Power::Slime ? 55.f : 190.f);
    if(players_[who].stun) velocity=0;
    players_[who].velocity=velocity; players_[who].x+=velocity/60.f;
    const int r=radius(who);
    if (players_[who].x<5+r) players_[who].x=float(5+r);
    if (players_[who].x>129-r) players_[who].x=float(129-r);
  }
  void serve(unsigned to) {
    serveTo_=uint8_t(to);
    for(unsigned i=0;i<3;++i) { pucks_[i]=Puck(); stalled_[i]=0; }
    pucks_[0].active=true; pucks_[0].y=to ? 144.f : 100.f;
  }
  void score(unsigned who) {
    ++scores_[who]; signal(ArcadeCue::Win); serveTo_=uint8_t(1-who);
    phase_=scores_[who]>=hockey::PlayTo ? Phase::Finished : Phase::Goal; wait_=90;
    for(unsigned i=0;i<3;++i) { pucks_[i].active=false; pucks_[i].vx=pucks_[i].vy=0; }
    for(unsigned i=0;i<2;++i) { players_[i].power=Power::None; players_[i].remaining=players_[i].stun=0; }
    pickup_=Power::None; pickupLife_=0; pickupTimer_=132;
  }
  void paddle(Puck& p,unsigned who) {
    const float y=paddleY(who), dx=p.x-players_[who].x,dy=p.y-y;
    const float r=float(radius(who)+4);
    if (dx*dx+dy*dy>=r*r || (who==0 ? p.vy>=0 : p.vy<=0)) return;
    const float offset=dx/r;
    const float speed=players_[who].power==Power::Rocket ? 245.f : who==0 ? 165.f : 155.f;
    p.vx=offset*speed*.85f+players_[who].velocity*.22f;
    p.vy=(who ? -1.f : 1.f)*sqrtf(speed*speed-fminf(p.vx*p.vx,speed*speed*.8f));
    p.y=y+(who ? -1.f : 1.f)*sqrtf(fmaxf(0.f,r*r-dx*dx)); p.last=int8_t(who);
    if (players_[1-who].power==Power::Freeze) { players_[who].stun=90; players_[1-who].power=Power::None; players_[1-who].remaining=0; }
    signal(ArcadeCue::Hit);
  }
  void bumpers(Puck& p) {
    for(unsigned i=0;i<bumperCount(map_);++i) {
      float x,y,r; bumper(map_,ticks_,i,x,y,r); if(r<=0) continue;
      float dx=p.x-x,dy=p.y-y,d2=dx*dx+dy*dy;
      if(d2>=(r+4)*(r+4)) continue;
      if(d2<.0001f) { dx=1; dy=0; d2=1; }
      const float d=sqrtf(d2),nx=dx/d,ny=dy/d,along=p.vx*nx+p.vy*ny;
      p.x=x+nx*(r+4+.01f); p.y=y+ny*(r+4+.01f);
      if(along<0) { p.vx-=2*along*nx; p.vy-=2*along*ny; }
      signal(ArcadeCue::Hit);
    }
  }
  void updatePickup() {
    if(options_.powers==hockey::Powers::Off) return;
    if(pickup_==Power::None) {
      if(pickupTimer_ && --pickupTimer_) return;
      do { pickup_=Power(1+random()%(hockey::PowerCount-1)); } while(pickup_==lastPower_);
      lastPower_=pickup_; pickupOwner_=options_.powers==hockey::Powers::Both ? uint8_t(random()&1) : 0;
      // The single-axis paddle must be able to reach every pickup.
      pickupX_=uint8_t(22+random()%91); pickupLife_=720;
    } else {
      if(pickupLife_ && !--pickupLife_) { pickup_=Power::None; pickupTimer_=uint16_t(270+random()%151); return; }
      if(fabsf(players_[pickupOwner_].x-pickupX_)<radius(pickupOwner_)+6) collect();
    }
  }
  void collect() {
    const unsigned who=pickupOwner_; const Power kind=pickup_;
    players_[who].power=kind; players_[who].remaining=kind==Power::Wall ? 540 : kind==Power::TinyGoal ? 600 : kind==Power::Beam ? 216 : kind==Power::Lasers ? 360 : 480;
    if(kind==Power::Party) for(unsigned i=1;i<3;++i) {
      pucks_[i]=pucks_[0]; pucks_[i].active=true; pucks_[i].owner=int8_t(who); pucks_[i].last=int8_t(who);
      pucks_[i].x=players_[who].x; pucks_[i].y=paddleY(who)+(who ? -18.f : 18.f);
      pucks_[i].vx=i==1 ? -75.f : 75.f; pucks_[i].vy=who ? -140.f : 140.f; pucks_[i].life=720;
    }
    pickup_=Power::None; pickupLife_=0; pickupTimer_=uint16_t(210+random()%151); signal(ArcadeCue::Power);
  }
  void laser(unsigned who) {
    if(players_[who].power!=Power::Lasers || ticks_%16!=4) return;
    if(fabsf(players_[who].x-players_[1-who].x)<12) players_[1-who].stun=51;
    for(unsigned i=0;i<3;++i) if(pucks_[i].active && fabsf(pucks_[i].x-players_[who].x)<6) {
      pucks_[i].vy=who ? -170.f : 170.f; pucks_[i].last=int8_t(who);
    }
    signal(ArcadeCue::Fire);
  }
};
}  // namespace flappy
