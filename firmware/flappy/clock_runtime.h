#pragma once
#include <stddef.h>
#include <new>
#include "app_model.h"
#include "clock_scene.h"
#include "flappy_clock.h"
#include "mario_world_clock.h"
#include "tetris_clock.h"
#include "jetpack_clock.h"
#include "geometry_clock.h"
#include "galaga_clock.h"
#include "arkanoid_clock.h"
#include "hockey_clock.h"
#include "menu_font.h"

namespace flappy {

class NesShelfClock {
 public:
  void reset(uint32_t seed) { frames_ = seed % 240; }
  void step(const ClockTime&) { ++frames_; }
  void draw(uint16_t* p,const ClockTime& t) const {
    ClockScene::clear(p,0x0842);
    for (unsigned d=0;d<4;++d) for(unsigned y=0;y<5;++y) for(unsigned x=0;x<3;++x)
      if(ClockScene::digitPixel(t.digit(d),x,y))
        ClockScene::rect(p,28+int(d%2)*48+int(x)*12,38+int(d/2)*80+int(y)*12,10,10,0xa51f);
    MenuFont::draw(p,135,240,"NES",58,203,0xbdf7);
    ClockScene::rect(p,int(frames_%151)-16,190,16,3,0xe8e8);
  }
 private:
  uint32_t frames_=0;
};

// A selected clock owns only its scene state. The caller supplies the existing
// display canvas, and releases the previous game before creating a new scene.
class ClockRuntime {
 public:
  ClockRuntime() = default;
  ClockRuntime(const ClockRuntime&) = delete;
  ClockRuntime& operator=(const ClockRuntime&) = delete;
  ClockRuntime(ClockRuntime&&) = delete;
  ClockRuntime& operator=(ClockRuntime&&) = delete;
  ~ClockRuntime() { release(); }
  bool reset(GameTitle title,uint32_t seed,const uint8_t* rom=nullptr,size_t length=0) {
    release(); title_=title; fallback_=false;
    switch(title_) {
      case GameTitle::FlappyBird: state_=new(std::nothrow) FlappyClock; break;
      case GameTitle::Nes: {
        auto* scene=new(std::nothrow) MarioWorldClock;
        if(scene && scene->bindRom(rom,length)) state_=scene;
        else { delete scene; fallback_=true; state_=new(std::nothrow) NesShelfClock; }
        break;
      }
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: state_=new(std::nothrow) TetrisClock; break;
      case GameTitle::GeometryDash: state_=new(std::nothrow) GeometryClock; break;
      case GameTitle::Galaga: state_=new(std::nothrow) GalagaClock; break;
      case GameTitle::Arkanoid: state_=new(std::nothrow) ArkanoidClock; break;
      case GameTitle::JetpackJoyride: state_=new(std::nothrow) JetpackClock; break;
      case GameTitle::HockeyQueen: state_=new(std::nothrow) HockeyClock; break;
    }
    if(!state_) return false;
    dispatchReset(seed);
    return true;
  }
  void step(const ClockTime& time) {
    if(!state_) return;
    switch(title_) {
      case GameTitle::FlappyBird: cast<FlappyClock>()->step(time); break;
      case GameTitle::Nes:
        if(fallback_) cast<NesShelfClock>()->step(time); else cast<MarioWorldClock>()->step(time);
        break;
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: cast<TetrisClock>()->step(time); break;
      case GameTitle::GeometryDash: cast<GeometryClock>()->step(time); break;
      case GameTitle::Galaga: cast<GalagaClock>()->step(time); break;
      case GameTitle::Arkanoid: cast<ArkanoidClock>()->step(time); break;
      case GameTitle::JetpackJoyride: cast<JetpackClock>()->step(time); break;
      case GameTitle::HockeyQueen: cast<HockeyClock>()->step(time); break;
    }
  }
  void draw(uint16_t* pixels,const ClockTime& time,const Skins& skin) const {
    if(!state_) { ClockScene::clear(pixels,0); return; }
    switch(title_) {
      case GameTitle::FlappyBird: cast<FlappyClock>()->draw(pixels,time,skin); break;
      case GameTitle::Nes:
        if(fallback_) cast<NesShelfClock>()->draw(pixels,time); else cast<MarioWorldClock>()->draw(pixels,time);
        break;
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: cast<TetrisClock>()->draw(pixels,time); break;
      case GameTitle::GeometryDash: cast<GeometryClock>()->draw(pixels,time); break;
      case GameTitle::Galaga: cast<GalagaClock>()->draw(pixels,time); break;
      case GameTitle::Arkanoid: cast<ArkanoidClock>()->draw(pixels,time); break;
      case GameTitle::JetpackJoyride: cast<JetpackClock>()->draw(pixels,time); break;
      case GameTitle::HockeyQueen: cast<HockeyClock>()->draw(pixels,time); break;
    }
  }
  void release() {
    switch(title_) {
      case GameTitle::FlappyBird: delete cast<FlappyClock>(); break;
      case GameTitle::Nes:
        if(fallback_) delete cast<NesShelfClock>(); else delete cast<MarioWorldClock>();
        break;
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: delete cast<TetrisClock>(); break;
      case GameTitle::GeometryDash: delete cast<GeometryClock>(); break;
      case GameTitle::Galaga: delete cast<GalagaClock>(); break;
      case GameTitle::Arkanoid: delete cast<ArkanoidClock>(); break;
      case GameTitle::JetpackJoyride: delete cast<JetpackClock>(); break;
      case GameTitle::HockeyQueen: delete cast<HockeyClock>(); break;
    }
    state_=nullptr;
  }
  bool ready() const { return state_!=nullptr; }
  bool fallback() const { return fallback_; }
  size_t stateBytes() const {
    if(!state_) return 0;
    switch(title_) {
      case GameTitle::FlappyBird: return sizeof(FlappyClock);
      case GameTitle::Nes: return fallback_?sizeof(NesShelfClock):sizeof(MarioWorldClock);
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: return sizeof(TetrisClock);
      case GameTitle::GeometryDash: return sizeof(GeometryClock);
      case GameTitle::Galaga: return sizeof(GalagaClock);
      case GameTitle::Arkanoid: return sizeof(ArkanoidClock);
      case GameTitle::JetpackJoyride: return sizeof(JetpackClock);
      case GameTitle::HockeyQueen: return sizeof(HockeyClock);
    }
    return 0;
  }

 private:
  void* state_=nullptr;
  GameTitle title_=GameTitle::FlappyBird;
  bool fallback_=false;
  template<class T> T* cast() const { return static_cast<T*>(state_); }
  void dispatchReset(uint32_t seed) {
    switch(title_) {
      case GameTitle::FlappyBird: cast<FlappyClock>()->reset(seed); break;
      case GameTitle::Nes:
        if(fallback_) cast<NesShelfClock>()->reset(seed); else cast<MarioWorldClock>()->reset(seed);
        break;
      case GameTitle::Tetris:
      case GameTitle::TetrisZero: cast<TetrisClock>()->reset(seed); break;
      case GameTitle::GeometryDash: cast<GeometryClock>()->reset(seed); break;
      case GameTitle::Galaga: cast<GalagaClock>()->reset(seed); break;
      case GameTitle::Arkanoid: cast<ArkanoidClock>()->reset(seed); break;
      case GameTitle::JetpackJoyride: cast<JetpackClock>()->reset(seed); break;
      case GameTitle::HockeyQueen: cast<HockeyClock>()->reset(seed); break;
    }
  }
};
}  // namespace flappy
