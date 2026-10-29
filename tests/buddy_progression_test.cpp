#include "../firmware/flappy/buddy_game.h"
#include <assert.h>
#include <iostream>
namespace flappy {
struct BuddyGameTestAccess {
 static void clear(BuddyGame& g) { for(auto& o:g.objects_) o.active=false;g.spawnDelay_=60000; }
 static void collect(BuddyGame& g,BuddyGame::ObjectKind k) { g.collect(k); }
 static void obstacle(BuddyGame& g,BuddyGame::ObjectKind k) {
  g.addObject(k,1,0,BuddyGame::DepthUnit-g.speed());
 }
 static void coin(BuddyGame& g,int lane,int progress) { g.addObject(BuddyGame::ObjectKind::Coin,lane,0,progress); }
 static void protect(BuddyGame& g) { g.invulnerable_=120; }
 static void goals(BuddyGame& g) {g.goalCoins_=g.goalTarget(0);g.goalJumps_=g.goalTarget(1);g.goalRolls_=g.goalTarget(2);g.checkGoals();}
 static void pose(BuddyGame& g,bool roll) {g.lane_=1;g.laneQ_=BuddyGame::LaneUnit;g.jumpTick_=roll?0:BuddyGame::JumpTicks/2;g.rollTicks_=roll?24:0;}
};
}
using G=flappy::BuddyGame;using A=flappy::BuddyGameTestAccess;using K=G::ObjectKind;
int main() {
 G g;A::clear(g);A::coin(g,0,G::DepthUnit/2);A::coin(g,2,G::DepthUnit/2);
 A::collect(g,K::Magnet);const auto coins=g.coins();
 unsigned animated=0;
 for(unsigned i=0;i<100;++i){g.tick({});for(unsigned n=0;n<G::ObjectCount;++n)if(g.objects()[n].active && g.objects()[n].pull)++animated;}
 assert(animated>5 && g.coins()==coins+2 && g.magnetTicks()>0);
 for(unsigned i=0;i<500;++i)g.tick({});assert(g.coins()==coins+2 && !g.magnetTicks());
 g.reset();A::clear(g);auto score=g.score();A::collect(g,K::Multiplier);
 const auto afterPower=g.score();A::collect(g,K::Coin);assert(g.score()-afterPower==50 && afterPower>score);
 g.reset();A::clear(g);A::goals(g);assert(g.missionSets()==1 && g.scoreMultiplier()==2 && g.coins()==10);
 assert(g.goal(0)==0 && g.goal(1)==0 && g.goal(2)==0 && g.rewardTicks()>0 && g.boardTicks()>0);
 g.reset();A::clear(g);A::collect(g,K::Mystery);assert(g.coins()==15 && (g.boardTicks() || g.magnetTicks()));
 g.reset();A::clear(g);A::pose(g,false);A::obstacle(g,K::LowObstacle);g.tick({});
 assert(g.tricks()==1 && g.goal(1)==1 && g.combo()==1 && g.phase()==G::Phase::Playing);
 A::pose(g,true);A::obstacle(g,K::Straddle);g.tick({});assert(g.tricks()==2 && g.goal(2)==1 && g.combo()==2);
 g.reset();A::clear(g);A::collect(g,K::Hoverboard);A::collect(g,K::Balloon);
 const auto shield=g.boardTicks();
 for(unsigned i=0;i<40;++i){A::obstacle(g,K::Person);g.tick({});}
 assert(g.phase()==G::Phase::Playing && g.flightTicks()>0 && g.flightHeight()==100 && g.boardTicks()==shield && !g.tricks());
 A::coin(g,1,G::DepthUnit/2);
 for(unsigned i=0;i<100;++i)g.tick({});assert(g.coins()>0);
 for(unsigned i=0;i<G::FlightTicks;++i)g.tick({});assert(g.flightHeight()==0 && !g.flightTicks());
 g.reset();A::clear(g);
 flappy::BuddyInput jump; jump.jump=true;g.tick(jump);
 for(unsigned i=0;i<20;++i)g.tick({});
 flappy::BuddyInput roll;roll.roll=true;g.tick(roll);
 for(unsigned i=0;i<16;++i)g.tick({});
 assert(g.jumpHeight()==0 && g.rolling());
 g.tick(jump);assert(!g.rolling() && g.jumpTick()==1);
 for(unsigned i=0;i<G::JumpTicks-3;++i)g.tick({});
 g.tick(jump); // Queue another jump just before landing and its brief recovery.
 for(unsigned i=0;i<8;++i)g.tick({});
 assert(g.jumpTick()>0);
 // Trace scheduling independently of object types: every themed course has
 // bonus and recovery sections, and active objects stay inside the fixed pool.
 unsigned sections=0;
 for(unsigned seed=1;seed<=64;++seed){
  g.reset(seed);
  for(unsigned tick=0;tick<20000;++tick){
   // Only this trace grants protection; collision tests exercise normal play.
   if(tick%100==0)A::protect(g);
   flappy::BuddyInput input;input.lane=1;g.tick(input);
   assert(g.phase()==G::Phase::Playing);sections|=1u<<unsigned(g.section());
   assert(g.objectCount()<=G::ObjectCount);
  }
 }
 assert(sections==255);
 g.reset();assert(g.missionSets()==0 && g.coins()==0 && !g.magnetTicks() && g.scoreMultiplier()==1);
 std::cout<<"Buddy progression: course variety, magnet animation, single credit, boosts, goals, prizes and trick rewards passed\n";
}
