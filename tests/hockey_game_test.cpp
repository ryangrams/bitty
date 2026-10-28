#include "../firmware/flappy/hockey_clock.h"
#include "../firmware/flappy/app_model.h"
#include <array>
#include <assert.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace flappy {
struct HockeyTestAccess {
  static void power(HockeyGame& g,unsigned who,hockey::Power p) { g.pickupOwner_=uint8_t(who); g.pickup_=p; g.collect(); }
  static void puck(HockeyGame& g,float x,float y,float vx,float vy) {
    g.phase_=hockey::Phase::Rally; g.pucks_[0].x=x; g.pucks_[0].y=y; g.pucks_[0].vx=vx; g.pucks_[0].vy=vy; g.pucks_[0].active=true;
  }
  static void last(HockeyGame& g,int who) { g.pucks_[0].last=int8_t(who); }
  static void goal(HockeyGame& g,unsigned who) { g.score(who); }
  static void expire(HockeyGame& g) { g.players_[0].remaining=3; }
};
}
using namespace flappy;

static void powersAndGoals() {
  hockey::Options options; options.map=1;
  for(unsigned power=1;power<hockey::PowerCount;++power) {
    HockeyGame g; g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power(power));
    assert(g.player(0).power==hockey::Power(power)); assert(g.takeCue()==ArcadeCue::Power);
    assert(hockey::valid(g.frame()));
    if(power==unsigned(hockey::Power::Mega)) assert(HockeyGame::radius(g.frame(),0)==14);
    if(power==unsigned(hockey::Power::Shrink)) assert(HockeyGame::radius(g.frame(),1)==7);
    if(power==unsigned(hockey::Power::Party)) assert(g.frame().puckMask==7);
  }
  HockeyGame g; g.reset(1,options);
  HockeyTestAccess::puck(g,67,217,0,245); g.tick(0,0);
  assert(g.frame().scores[0]==1 && g.phase()==hockey::Phase::Goal);
  g.reset(1,options); HockeyTestAccess::power(g,1,hockey::Power::Wall);
  HockeyTestAccess::puck(g,67,219,0,245); g.tick(0,0);
  assert(g.frame().scores[0]==0 && g.puck().vy<0);
  g.reset(1,options); HockeyTestAccess::puck(g,67,56,0,-245); g.tick(0,0);
  assert(g.puck().vy>0 && g.frame().scores[1]==0);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Backwards); g.tick(0,1024);
  assert(g.player(1).x<67);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Slime); g.tick(0,1024);
  assert(g.player(1).x>67 && g.player(1).x<68);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Freeze);
  HockeyTestAccess::puck(g,67,189,0,245); g.tick(0,0);
  assert(g.frame().stunMask&2);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Beam);
  HockeyTestAccess::puck(g,67,122,0,0); g.tick(0,0); assert(g.puck().vy>0);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Rocket);
  HockeyTestAccess::puck(g,67,53,0,-100); g.tick(0,0); assert(g.puck().vy>220);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Lasers);
  HockeyTestAccess::puck(g,30,122,0,0); for(unsigned i=0;i<4;++i) g.tick(0,0); assert(g.frame().stunMask&2);
  g.reset(1,options); HockeyTestAccess::power(g,0,hockey::Power::Mega); HockeyTestAccess::expire(g);
  g.tick(0,0); g.tick(0,0); assert(g.player(0).power==hockey::Power::Mega);
  g.tick(0,0); assert(g.player(0).power==hockey::Power::None);
  for(unsigned score=0;score<hockey::PlayTo;++score) HockeyTestAccess::goal(g,0);
  assert(g.phase()==hockey::Phase::Finished);
  const auto before=g.frame(); for(unsigned i=0;i<600;++i) g.tick(1024,-1024);
  assert(g.frame().scores[0]==before.scores[0] && g.phase()==hockey::Phase::Finished);
}

static uint64_t hash(const uint16_t* p) { uint64_t h=1469598103934665603ULL; for(unsigned i=0;i<135*240;++i) h=(h^p[i])*1099511628211ULL; return h; }
static void write(const uint16_t* p,const std::string& path) {
  std::ofstream f(path,std::ios::binary); assert(f); f<<"P6\n135 240\n255\n";
  for(unsigned i=0;i<135*240;++i) { const char rgb[]={char((p[i]>>11)*255/31),char(((p[i]>>5)&63)*255/63),char((p[i]&31)*255/31)}; f.write(rgb,3); }
}

static void mapsAndLongPlay() {
  std::set<unsigned> seen;
  std::array<uint16_t,135*240+64> canvas; canvas.fill(0xa55a);
  for(unsigned seed=1;seed<=128;++seed) { HockeyGame g; g.reset(seed); seen.insert(g.map()); }
  assert(seen.size()==hockey::MapCount);
  for(unsigned map=0;map<hockey::MapCount;++map) for(unsigned style=0;style<4;++style) {
    hockey::Options options; options.map=uint8_t(map+1); options.palette=uint8_t(style); options.powers=hockey::Powers::Both;
    HockeyGame a,b; a.reset(711,options); b.reset(711,options);
    unsigned powerFrames=0;
    for(unsigned tick=0;tick<18000;++tick) {
      if(a.phase()==hockey::Phase::Finished) { a.reset(tick+1,options); b.reset(tick+1,options); }
      const auto f=a.frame();
      int16_t moves[2];
      for(unsigned i=0;i<2;++i) {
        const float target=f.pickup!=hockey::Power::None && f.pickupOwner==i ? float(f.pickupX) : a.puck().x;
        const float d=target-a.player(i).x; moves[i]=int16_t(d<-2 ? -850 : d>2 ? 850 : 0);
      }
      a.tick(moves[0],moves[1]); b.tick(moves[0],moves[1]);
      const auto after=a.frame(); assert(hockey::valid(after)); assert(after.map==map && after.palette==style);
      assert(a.puck().x==b.puck().x && a.puck().y==b.puck().y);
      assert(std::isfinite(a.puck().x) && std::isfinite(a.puck().y));
      assert(a.player(0).x>=12 && a.player(0).x<=122 && a.player(1).x>=12 && a.player(1).x<=122);
      powerFrames+=after.powers[0]!=hockey::Power::None || after.powers[1]!=hockey::Power::None;
      if(tick%89==0) {
        HockeyRenderer::draw(canvas.data()+32,after,tick%2); const uint64_t before=hash(canvas.data()+32);
        HockeyRenderer::draw(canvas.data()+32,after,tick%2); assert(hash(canvas.data()+32)==before);
        for(unsigned i=0;i<32;++i) assert(canvas[i]==0xa55a && canvas[135*240+32+i]==0xa55a);
      }
    }
    assert(powerFrames>0);
  }
}

static void menusAndClock(const char* destination) {
  AppModel app; assert(app.setCurrentGame(GameTitle::HockeyQueen)); assert(app.setBootGame(GameTitle::HockeyQueen));
  assert(visibleGameIndex(GameTitle::HockeyQueen)==7);
  const auto originalOptions=app.settings.hockeyOptions.encode();
  auto invalid=app.settings.hockeyOptions; invalid.map=17; app.setHockeyOptions(invalid);
  assert(app.settings.hockeyOptions.encode()==originalOptions);
  for(auto page:{MenuPage::GameSettings,MenuPage::HockeyMap,MenuPage::HockeyColors,MenuPage::HockeyLink}) {
    app.openMenu(page); const uint8_t count=app.menuItemCount(); assert(count>=4);
    for(uint8_t i=0;i<count;++i) assert(app.menuLabel(i)[0]);
    assert(std::string(app.menuLabel(count-1))=="Back"); assert(!app.menuLabel(count)[0]);
    app.menu.selection=count-1; app.step(true,false,17,false);
    assert(app.menu.page==(page==MenuPage::GameSettings ? MenuPage::Home : MenuPage::GameSettings));
  }
  app.openMenu(MenuPage::HockeyMap); app.menu.selection=0; app.step(true,false,34,false); assert(app.settings.hockeyOptions.map==0);
  app.openMenu(MenuPage::HockeyLink); app.menu.selection=1; app.step(true,false,51,false); assert(app.hockeyLinkRole==2 && app.hockeyLinkRequest==1 && !app.menuOpen());
  std::array<uint16_t,135*240> canvas;
  HockeyClock clock; clock.reset(33); ClockTime time; time.hour=12; time.minute=34;
  uint64_t previous=0; unsigned changed=0;
  for(unsigned tick=0;tick<18000;++tick) {
    time.minute=uint8_t((34+tick/3600)%60); clock.step(time);
    if(tick%31==0) { clock.draw(canvas.data(),time); const uint64_t h=hash(canvas.data()); changed+=h!=previous; previous=h; }
    if(destination && tick==280) { clock.draw(canvas.data(),time); write(canvas.data(),std::string(destination)+"-clock.ppm"); }
  }
  assert(changed>400);
  if(destination) for(unsigned map=0;map<hockey::MapCount;++map) {
    HockeyGame g; hockey::Options options; options.map=uint8_t(map+1); g.reset(map+20,options);
    for(unsigned tick=0;tick<220;++tick) g.tick(int16_t((tick/30)%2 ? 700 : -700),int16_t((tick/40)%2 ? -700 : 700));
    HockeyTestAccess::power(g,0,hockey::Power(1+map));
    HockeyRenderer::draw(canvas.data(),g.frame(),0); write(canvas.data(),std::string(destination)+"-map-"+std::to_string(map)+".ppm");
    HockeyRenderer::draw(canvas.data(),g.frame(),1); write(canvas.data(),std::string(destination)+"-guest-"+std::to_string(map)+".ppm");
  }
}
int main(int argc,char** argv) {
  static_assert(sizeof(HockeyClock)<3072,"Clock state stays small");
  powersAndGoals(); mapsAndLongPlay(); menusAndClock(argc>1 ? argv[1] : nullptr);
  std::cout<<"Hockey Queen: scoring, 12 powers, eight maps, four palettes, deterministic long play, guarded rendering, menus and clock passed\n";
}
