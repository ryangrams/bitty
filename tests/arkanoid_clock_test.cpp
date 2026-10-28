#include "../firmware/flappy/arkanoid_clock.h"
#include <assert.h>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

namespace flappy {
struct ArkanoidClockTestAccess {
  static void oneBrick(ArkanoidClock& c,unsigned index,unsigned strength) {
    c.initialized_=true;c.requestedKey_=uint16_t(12*60+34)|0x8000;
    c.phase_=ArkanoidClock::Phase::Play;c.phaseFrame_=0;
    c.remaining_=1;c.target_=uint16_t(index);
    for(auto& b:c.bricks_)b=ArkanoidClock::Brick();
    for(auto& b:c.balls_)b=ArkanoidClock::Ball();
    c.bricks_[index]={uint8_t(strength),128,0,uint8_t(strength)};
    c.balls_[0].active=true;
  }
  static void approach(ArkanoidClock& c,unsigned index,int horizontal=0) {
    c.balls_[0].x=(c.brickX(index)+ArkanoidClock::BrickWidth/2)*ArkanoidClock::Unit;
    c.balls_[0].y=(c.brickY(index)+ArkanoidClock::BrickHeight+3)*ArkanoidClock::Unit;
    c.balls_[0].vx=int16_t(horizontal);c.balls_[0].vy=-790;
  }
  static void ball(ArkanoidClock& c,int x,int y,int vx,int vy) {
    c.balls_[0].x=x*ArkanoidClock::Unit;c.balls_[0].y=y*ArkanoidClock::Unit;
    c.balls_[0].vx=int16_t(vx);c.balls_[0].vy=int16_t(vy);
  }
  static void velocity(ArkanoidClock& c,int vx,int vy) {
    c.balls_[0].vx=int16_t(vx);c.balls_[0].vy=int16_t(vy);
  }
  static void move(ArkanoidClock& c) { c.moveBall(c.balls_[0]); }
  static unsigned digit(const ArkanoidClock& c,unsigned d) {return c.digits_[d];}
  static int x(unsigned index) {return ArkanoidClock::brickX(index);}
  static int y(unsigned index) {return ArkanoidClock::brickY(index);}
  static void late(ArkanoidClock& c) { c.phaseFrame_=539; }
};
}
using namespace flappy;

static ClockTime noon() {ClockTime t;t.hour=12;t.minute=34;return t;}
static uint64_t hash(const ArkanoidClock& c) {
  uint64_t value=1469598103934665603ull;
  for(unsigned i=0;i<ArkanoidClock::BrickCount;++i) {
    value^=c.brick(i).hits;value*=1099511628211ull;
    value^=c.brick(i).color;value*=1099511628211ull;
  }
  return value;
}
static void buildsAndMinuteRollover() {
  std::set<uint64_t> boards;
  auto t=noon();
  for(unsigned seed=0;seed<100;++seed) {
    ArkanoidClock a,b;a.reset(seed);b.reset(seed);a.step(t);b.step(t);
    assert(hash(a)==hash(b));boards.insert(hash(a));
    assert(a.originalClockBricks()>=30&&a.originalClockBricks()<=52);
    unsigned ink=0,extra=0;
    for(unsigned i=0;i<ArkanoidClock::BrickCount;++i)if(a.brick(i).hits) {
      if(a.clockBrick(i)){++ink;assert(a.brick(i).hits>=2&&a.brick(i).hits<=3);}
      else {++extra;assert(a.brick(i).hits==1);}
    }
    assert(ink==a.originalClockBricks()&&extra>=4);
    assert(ink+extra==a.remainingBricks());
  }
  assert(boards.size()>=98);
  ArkanoidClock c;c.reset(5);c.step(t);
  assert(c.phase()==ArkanoidClock::Phase::Build);
  for(unsigned f=1;f<96;++f)c.step(t);
  assert(c.phase()==ArkanoidClock::Phase::Read);
  for(unsigned f=0;f<240;++f)c.step(t);
  assert(c.phase()==ArkanoidClock::Phase::Play);
  assert(c.brickContacts()==0);
  const auto before=c.boards();t.minute=35;c.step(t);
  assert(c.phase()==ArkanoidClock::Phase::Wipe&&c.boards()==before);
  for(unsigned f=0;f<47;++f)c.step(t);
  assert(c.phase()==ArkanoidClock::Phase::Build&&c.boards()==before+1);
  assert(ArkanoidClockTestAccess::digit(c,3)==5);
  t.hour=0;t.minute=0;t.twentyFourHour=false;c.step(t);
  for(unsigned f=0;f<47;++f)c.step(t);
  assert(ArkanoidClockTestAccess::digit(c,0)==1&&ArkanoidClockTestAccess::digit(c,1)==2);
  assert(ArkanoidClockTestAccess::digit(c,2)==0&&ArkanoidClockTestAccess::digit(c,3)==0);
}

static void actualCollisionAndDurability() {
  const unsigned index=9*8+4;
  ArkanoidClock c;c.reset(1);
  ArkanoidClockTestAccess::oneBrick(c,index,3);
  auto time=noon();
  for(unsigned hit=0;hit<3;++hit) {
    ArkanoidClockTestAccess::approach(c,index);
    c.step(time);
    assert(c.brick(index).hits==2-hit);
    assert(c.ball(0).vy>0);
    assert(c.brickContacts()==hit+1);
    assert(c.bricksDestroyed()==(hit==2?1:0));
    assert(c.phase()==(hit==2?ArkanoidClock::Phase::Wipe:ArkanoidClock::Phase::Play));
  }
  assert(c.clearedBoards()==1);
  for(unsigned f=0;f<48;++f)c.step(time);
  assert(c.phase()==ArkanoidClock::Phase::Build&&c.remainingBricks()>30);

  ArkanoidClockTestAccess::oneBrick(c,0,1);
  ArkanoidClockTestAccess::ball(c,7,200,-790,0);ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vx>0&&c.ball(0).x>=7*256);
  ArkanoidClockTestAccess::ball(c,127,200,790,0);ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vx<0&&c.ball(0).x<=127*256);
  ArkanoidClockTestAccess::ball(c,67,19,0,-790);ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vy>0&&c.ball(0).y>=19*256);
  ArkanoidClockTestAccess::ball(c,c.paddleX()+10,213,100,780);ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vy<0&&c.paddleBounces()>0);
  const auto paddleHits=c.paddleBounces();
  ArkanoidClockTestAccess::ball(c,7,213,0,790);ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vy<0&&c.paddleBounces()==paddleHits);

  ArkanoidClockTestAccess::oneBrick(c,index,2);
  ArkanoidClockTestAccess::ball(c,ArkanoidClockTestAccess::x(index)-3,ArkanoidClockTestAccess::y(index)+3,790,0);
  ArkanoidClockTestAccess::move(c);
  assert(c.ball(0).vx<0&&c.brick(index).hits==1);
  ArkanoidClockTestAccess::late(c);c.step(time);
  assert(c.ball(1).active&&c.ball(2).active);
}

static void deterministicLongRunAndGuards() {
  std::vector<uint16_t> storage(135*240+32,0x5aa5);uint16_t* pixels=storage.data()+16;
  uint32_t contacts=0,destroyed=0,clears=0,bounces=0,boards=0;
  for(unsigned seed=1;seed<=64;++seed) {
    ArkanoidClock c;c.reset(seed);
    ClockTime t=noon();
    for(unsigned f=0;f<36000;++f) {
      t.minute=uint8_t((34+f/3600)%60);t.second=uint8_t(f/60%60);t.ms=f*1000/60;
      c.step(t);
      assert(c.paddleX()>=22&&c.paddleX()<=112);
      assert(c.remainingBricks()<=ArkanoidClock::BrickCount);
      if(c.phase()==ArkanoidClock::Phase::Play) {
        unsigned remaining=0;
        for(unsigned i=0;i<ArkanoidClock::BrickCount;++i)remaining+=c.brick(i).hits!=0;
        assert(c.remainingBricks()==remaining);
      }
      for(unsigned b=0;b<ArkanoidClock::MaxBalls;++b)if(c.ball(b).active) {
        assert(c.ball(b).x>=7*256&&c.ball(b).x<=127*256);
        assert(c.ball(b).y>=19*256&&c.ball(b).y<=214*256);
      }
      if(seed==1&&f%11==0) {
        const auto before=c;c.draw(pixels,t);assert(std::memcmp(&before,&c,sizeof(c))==0);
        for(unsigned i=0;i<16;++i)assert(storage[i]==0x5aa5&&storage[135*240+16+i]==0x5aa5);
      }
    }
    assert(c.boards()>=10&&c.brickContacts()>300&&c.paddleBounces()>20);
    contacts+=c.brickContacts();destroyed+=c.bricksDestroyed();clears+=c.clearedBoards();
    bounces+=c.paddleBounces();boards+=c.boards();
  }
  ArkanoidClock c;c.reset(0xffffffff);auto t=noon();
  for(unsigned f=0;f<72000;++f){c.step(t);if(f%31==0)c.draw(pixels,t);}
  assert(c.clearedBoards()>0);
  assert(sizeof(c)<2048);c.draw(nullptr,t);
  std::cout<<"Arkanoid clocks: 64 seeds, 2,304,000 steps, "<<boards<<" boards, "<<contacts
           <<" contacts, "<<destroyed<<" destroyed, "<<clears<<" natural clears and "<<bounces
           <<" paddle bounces; state "<<sizeof(c)<<" bytes\n";
}

int main(){buildsAndMinuteRollover();actualCollisionAndDurability();deterministicLongRunAndGuards();}
