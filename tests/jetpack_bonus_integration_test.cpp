#define M5FLAP_STARTUP_DIAGNOSTICS 1
#include "../firmware/flappy/jetpack_game.h"
#include <array>
#include <assert.h>
#include <cmath>
#include <iostream>

namespace flappy {
struct JetpackTestAccess {
  static void isolate(JetpackGame& g) {
    g.progression_.setEnabled(false);
    for (auto& z : g.zaps_) z = JetpackGame::Zap{};
    for (auto& c : g.coinField_) c = JetpackGame::Coin{};
    for (auto& m : g.missiles_) m = JetpackGame::Missile{};
    for (auto& a : g.attractedCoins_) a = JetpackGame::AttractedCoin{};
    for (auto& a : g.importedGroups_) a = JetpackGame::ImportedGroup{};
  }
  static void player(JetpackGame& g,int y) { g.y_ = y * JetpackGame::FlightUnit; g.velocity_ = 0; }
  static void meters(JetpackGame& g,uint32_t m) { g.world_ = uint32_t(uint64_t(m)*69*JetpackGame::Unit/10); }
  static double y(const JetpackGame& g) { return double(g.y_) / JetpackGame::FlightUnit; }
  static void clearChoices(JetpackGame& g) { for (auto& p : g.rocketChoices_) p.active = false; }
  static void choices(JetpackGame& g) { g.spawnRocketChoices(6); }
  static uint32_t choiceMask(const JetpackGame& g) {
    uint32_t mask = 0;
    for (auto& p : g.rocketChoices_) if (p.active) {
      assert(p.kind >= JetpackGame::Vehicle::Gravity && p.kind <= JetpackGame::Vehicle::Dragon);
      assert(p.y >= JetpackGame::Ceiling && p.y <= JetpackGame::Floor);
      assert(!(mask & (1u << unsigned(p.kind)))); mask |= 1u << unsigned(p.kind);
    }
    return mask;
  }
  static void catchChoice(JetpackGame& g) {
    g.rocketChoices_[0].x = JetpackGame::PlayerX * JetpackGame::Unit;
    g.rocketChoices_[0].y = int16_t(g.playerY()); g.collectExtras();
  }
  static void shower(JetpackGame& g) { g.startCashShower(300,5,JetpackGame::PlayerX*JetpackGame::Unit,70*JetpackGame::Unit); }
  static bool swept(JetpackGame& g,int x,int y,int span) { g.frameAdvance_ = span*JetpackGame::Unit; return g.sweptCoinContact(x,y); }
  static void legacyMissile(JetpackGame& g) {
    g.missiles_[0] = JetpackGame::Missile{}; g.missiles_[0].active = true;
    g.missiles_[0].x = 230*JetpackGame::Unit; g.missiles_[0].y = 35;
  }
  static JetpackGame::Missile& pendingShot(JetpackGame& g,uint8_t flags,uint16_t chase) {
    auto& m = g.missiles_[0]; m = JetpackGame::Missile{};
    m.active = true; m.rocketFlags = uint8_t(128 | flags); m.warning = 255;
    m.x = 254 * JetpackGame::Unit; m.y = 30; m.yQ8 = 30 * JetpackGame::Unit;
    m.chaseQ8 = chase; m.warningStart = 30; m.launchTicks = 60;
    return m;
  }
  static void updateShot(JetpackGame& g,JetpackGame::Missile& m) { g.updateRocketMissile(m); }
  static void updateTime(JetpackGame& g,unsigned ticks) { while(ticks--) g.updateRocketTime(); }
  static void blast(JetpackGame& g,uint8_t tier) {
    g.phase_ = JetpackGame::Phase::Spin; g.spinResult_ = true; g.spinTicks_ = 0; g.spinTokens_ = 1;
    g.lastAward_ = JetpackRewards::Award{JetpackRewards::Kind::Blast,0,tier};
    g.spinPresentation_.reset(); g.tickSpin(false);
  }
  static int32_t crashSpeed(const JetpackGame& g) { return g.deathSpeed_; }
};
}
using G = flappy::JetpackGame;
using A = flappy::JetpackTestAccess;
using Input = flappy::ArcadeInput;
static Input press() { Input i; i.action = i.pressed = true; return i; }
static Input hold() { Input i; i.action = true; return i; }
static void start(G& g,uint32_t seed=1) { g.reset(seed); g.setProfileEnabled(false); g.setDiagnosticProtection(true); g.tick(Input{}); g.tick(press()); A::isolate(g); }

static void scaledFlightAndWaves() {
  G g; start(g,541); A::player(g,70); assert(g.diagnosticPower(unsigned(G::Power::RocketTime),9));
  double y=70,v=0,maximum=0;
  for (unsigned tick=0;tick<600;++tick) {
    const double native=175+305*std::min(g.distance(),2250u)/2250.0;
    const double factor=1+(native-175)/350;
    const bool held=tick%120<55;
    g.tick(held?hold():Input{});
    const double dt=g.movementScale()/65536.0;
    v=(v+(held?-.21:.15)*(92.0/240.0)*factor*dt)*std::pow(.98,dt);
    y+=v*dt;
    if(y<22){y=22;v=0;} if(y>114){y=114;v=0;}
    maximum=std::max(maximum,std::abs(y-A::y(g)));
  }
  assert(maximum<.06 && g.rocketWaveCount()==4 && g.rocketShotCount()>=12 && !g.rocketDroppedShots());
  assert(g.rocketTime().active() && g.movementScale()==32768 && g.phase()==G::Phase::Running);
  std::cout<<"Rocket flight maximum reference error: "<<maximum<<"px\n";
  for(uint32_t seed=1;seed<=32;++seed) {
    start(g,seed); A::meters(g,2250); g.diagnosticPower(unsigned(G::Power::RocketTime),9);
    for(unsigned t=0;t<2020;++t) { g.tick(hold()); A::clearChoices(g); }
    assert(g.rocketWaveCount()==10 && !g.rocketDroppedShots() && !g.rocketTime().active());
    assert(g.movementScale()==65536 && !g.powerTicks(G::Power::RocketTime));
    for(unsigned i=0;i<G::MissileCount;++i) assert(!(g.missile(i).rocketFlags&128));
  }
}
static void choicesRestoreNormal() {
  G g; start(g); g.diagnosticPower(unsigned(G::Power::RocketTime),9);
  for(unsigned i=0;i<100;++i)g.tick(hold());
  A::choices(g); assert(g.rocketChoiceCount()==6 && A::choiceMask(g)==252);
  A::catchChoice(g); assert(!g.rocketTime().active() && !g.rocketChoiceCount() && g.vehicle()!=G::Vehicle::None);
  assert(!g.powerTicks(G::Power::RocketTime) && g.movementScale()==65536);
  start(g); g.diagnosticPower(unsigned(G::Power::RocketTime));
  g.diagnosticPower(unsigned(G::Power::Boost)); assert(!g.rocketTime().active() && g.powerTicks(G::Power::Boost));
}
static void rocketSourceRegressions() {
  for(uint8_t flags : {uint8_t(flappy::JetpackRocketCourse::Ranged),uint8_t(flappy::JetpackRocketCourse::RandomLane),uint8_t(0)}) {
    G g; start(g); A::player(g,100);
    auto& m = A::pendingShot(g,flags,20);
    for(unsigned i=0;i<10;++i) A::updateShot(g,m);
    assert(m.yQ8 == 30*G::Unit+200);
    m.clockQ8 = (m.warningStart-1)*G::Unit;
    const auto lockedY = m.yQ8; A::updateShot(g,m);
    assert(m.yQ8 == lockedY);
    auto& still = A::pendingShot(g,flags,0);
    for(unsigned i=0;i<10;++i) A::updateShot(g,still);
    assert(still.yQ8 == 30*G::Unit);
  }
  G g; start(g);
  auto& m = A::pendingShot(g,flappy::JetpackRocketCourse::Ricochet,0);
  m.rocketFlags |= 64; m.warning = 0; m.yQ8 = 23*G::Unit; m.vyQ8 = -10*G::Unit;
  A::updateShot(g,m);
  const double nativeBoundary = 68-136*(92.0/240.0);
  assert(std::abs(m.yQ8/double(G::Unit)-(2*nativeBoundary-13))<.01 && m.vyQ8>0);
  start(g); A::meters(g,1500); g.diagnosticPower(unsigned(G::Power::RocketTime),9);
  A::meters(g,2250); A::updateTime(g,120);
  assert(g.movementScale()==32768);
  start(g); A::meters(g,2250); g.diagnosticPower(unsigned(G::Power::RocketTime),9);
  A::meters(g,0); A::updateTime(g,120);
  assert(g.movementScale()==flappy::JetpackRocketTime::targetScaleQ16(480u*G::FlightUnit));
}
static void blastFinishesHorizontalTravel() {
  for(uint8_t tier=1;tier<=3;++tier) {
    G g; start(g,330+tier); A::blast(g,tier);
    unsigned ticks=0;
    while(g.phase()==G::Phase::Crashing && ticks<1000) { g.tick(Input{}); ++ticks; }
    assert(ticks<1000 && g.phase()==G::Phase::Spin && A::crashSpeed(g)==0);
    assert(g.spinTokens()==1);
  }
}
static void ballisticAndSweptCoins() {
  G normal,magnet; start(normal,9881); start(magnet,9881);
  A::player(normal,70); A::player(magnet,70); magnet.diagnosticPower(unsigned(G::Power::Magnet));
  A::shower(normal); A::shower(magnet);
  for(unsigned i=0;i<500;++i) {
    A::player(normal,70); A::player(magnet,70); normal.tick(Input{}); magnet.tick(Input{});
    if(i<18)assert(!normal.coins()&&!magnet.coins());
  }
  assert(!normal.cashShowerPending()&&!magnet.cashShowerPending());
  assert(!normal.attractedCoinCount()&&!magnet.attractedCoinCount());
  assert(normal.coins()<magnet.coins() && magnet.coins()<=1500);
  std::cout<<"Cash shower collected without/with magnet: "<<normal.coins()<<"/"<<magnet.coins()<<"\n";
  G g; start(g); A::player(g,70);
  assert(!A::swept(g,18,70,0)); assert(A::swept(g,18,70,40)); assert(!A::swept(g,18,90,40));
}
static void spinWaitAndSkip() {
  G g; start(g,41); assert(g.diagnosticSpin(3)); g.tick(Input{}); g.tick(press());
  const unsigned expected = g.lastSpinWon() && g.lastAward().kind == flappy::JetpackRewards::Kind::Tokens ? 5 : 2;
  assert(g.spinPresentation().running()&&g.spinTokens()==expected);
  for(unsigned i=0;i<60;++i) { assert(g.phase()==G::Phase::Spin); g.tick(Input{}); }
  assert(!g.spinPresentation().running()&&g.phase()==G::Phase::Spin);
  assert(!g.cashInSpinTokens()); g.tick(press());
  if(g.phase()==G::Phase::Spin) {
    const auto tokens=g.spinTokens(); for(unsigned i=0;i<500;++i)g.tick(Input{});
    assert(g.spinTokens()==tokens && !g.spinPresentation().running());
  }
}
static void compatibleLoadouts() {
  for (uint8_t bonus = 4; bonus <= 6; ++bonus) {
    G g; g.setNextLoadout(unsigned(G::Vehicle::Hog),bonus,false); start(g);
    assert(g.vehicle()==G::Vehicle::None);
    g.setNextLoadout(unsigned(G::Vehicle::Hog),bonus,true); start(g);
    assert(g.vehicle()==G::Vehicle::None && g.powerTicks(G::Power::Boost));
    assert(!g.powerTicks(G::Power::CashDash) && !g.rocketTime().active());
  }
  G ride; ride.setNextLoadout(unsigned(G::Vehicle::Hog),1,false); start(ride);
  assert(ride.vehicle()==G::Vehicle::Hog && ride.powerTicks(G::Power::Magnet));
  for (G::Power power : {G::Power::Boost,G::Power::CashDash,G::Power::RocketTime}) {
    assert(!ride.diagnosticPower(unsigned(power)) && !ride.powerTicks(power));
  }
}
int main() {
  scaledFlightAndWaves(); choicesRestoreNormal(); rocketSourceRegressions(); blastFinishesHorizontalTravel();
  ballisticAndSweptCoins(); spinWaitAndSkip(); compatibleLoadouts();
  std::cout<<"Jetpack bonus integration checks passed. State "<<sizeof(G)<<"bytes\n";
}
