#include "../firmware/flappy/hockey_cpu.h"
#include <assert.h>
#include <iostream>
using namespace flappy;
namespace flappy {
struct HockeyTestAccess {
  static void incoming(HockeyGame& game,unsigned who,float x) {
    game.phase_=hockey::Phase::Rally;
    auto& puck=game.pucks_[0];
    game.players_[who].x=24;
    puck.active=true; puck.x=x; puck.y=who ? 166.f : 78.f;
    puck.vx=0; puck.vy=who ? 210.f : -210.f;
  }
};
}
int main() {
  for(int i=-1024;i<=1024;++i) {
    const int result=hockey::paddleInput(int16_t(i));
    assert(result>=-1024 && result<=1024);
    assert(result==-hockey::paddleInput(int16_t(-i)));
    if(i>=0)assert(result>=i);
  }
  unsigned humanGoals=0,cpuGoals=0;
  for(unsigned level=0;level<4;++level) for(unsigned side=0;side<2;++side) {
    HockeyGame a,b;
    hockey::Options options; options.map=1; options.powers=hockey::Powers::Off;
    a.reset(19,options); b.reset(19,options);
    HockeyCpu cpu,copy;
    cpu.reset(91);copy.reset(91);cpu.setDifficulty(uint8_t(level));copy.setDifficulty(uint8_t(level));
    HockeyTestAccess::incoming(a,side,84);HockeyTestAccess::incoming(b,side,84);
    for(unsigned tick=0;tick<90;++tick) {
      const int16_t first=cpu.input(a,side),second=copy.input(b,side);
      assert(first==second && first>=-1024 && first<=1024);
      a.tick(side ? 0 : first,side ? first : 0);b.tick(side ? 0 : second,side ? second : 0);
      assert(hockey::valid(a.frame()));
    }
    if(level==1) assert(a.frame().scores[1-side]>0);
    for(unsigned map=0;map<hockey::MapCount;++map) {
      options.map=uint8_t(map+1);options.powers=hockey::Powers::Both;
      a.reset(31+map,options);cpu.reset(121+map);
      for(unsigned tick=0;tick<12000 && a.phase()!=hockey::Phase::Finished;++tick) {
        const int human=(int((tick/27)%3)-1)*900;
        const int16_t move=cpu.input(a,1-side);
        a.tick(side ? move : human,side ? human : move);
        assert(hockey::valid(a.frame()));
        assert(a.player(0).x>=12 && a.player(0).x<=122);
        assert(a.player(1).x>=12 && a.player(1).x<=122);
      }
      humanGoals+=a.frame().scores[side];cpuGoals+=a.frame().scores[1-side];
    }
  }
  assert(humanGoals>0 && cpuGoals>0);
  std::cout<<"Hockey CPU: four skills, both roles, bounded inputs, deterministic reactions, misses and eight-map matches passed\n";
}
