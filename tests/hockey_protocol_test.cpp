#include "../firmware/flappy/coop_protocol.h"
#include "../firmware/flappy/hockey_game.h"
#include <assert.h>
#include <array>
#include <iostream>
#include <vector>
using namespace flappy;
namespace net=flappy::coop;

static net::Packet packet(const net::Message& message) { net::Packet p; assert(net::encode(message,p)); return p; }
static bool deliver(net::Session& target,net::Message m,uint32_t now) { const auto p=packet(m); return target.receive(p.bytes,p.length,now); }
static net::Message message(net::MessageType type,uint32_t session=11,uint8_t round=1) {
  net::Message m; m.type=type; m.session=session; m.round=round; return m;
}
static void codec() {
  HockeyGame g; g.reset(123); auto f=g.frame(); f.tick=65535; f.puckMask=7; f.phase=hockey::Phase::Rally;
  f.scores[0]=7; f.scores[1]=6; f.map=7; f.palette=3; f.rules=hockey::Powers::Both;
  f.powers[0]=hockey::Power::Freeze; f.powers[1]=hockey::Power::Beam; f.remaining[0]=40; f.remaining[1]=15;
  f.pickup=hockey::Power::Rocket; f.pickupX=99; f.pickupOwner=1; f.pickupLife=48; f.stunMask=3; f.event=253;
  for(auto type:{net::MessageType::HockeyMotion,net::MessageType::HockeyBoard,net::MessageType::HockeyInput}) {
    auto m=message(type); m.hockeyFrame=f; m.hockeyInput.move=-1024; m.hockeyInput.sequence=65535; m.hockeyInput.finished=true;
    const auto p=packet(m); assert(p.length<=20); net::Message decoded;
    for(size_t n=0;n<p.length;++n) assert(!net::decode(p.bytes,n,decoded));
    assert(net::decode(p.bytes,p.length,decoded));
    if(type==net::MessageType::HockeyInput) assert(decoded.hockeyInput.move==-1024 && decoded.hockeyInput.finished);
    else assert(decoded.hockeyFrame.tick==65535);
    if(type==net::MessageType::HockeyBoard) assert(decoded.hockeyFrame.powers[0]==hockey::Power::Freeze && decoded.hockeyFrame.event==253 && decoded.hockeyFrame.pickupX==99);
    auto bad=p; bad.bytes[2]=99; assert(!net::decode(bad.bytes,bad.length,decoded));
  }
  auto m=message(net::MessageType::HockeyMotion); m.hockeyFrame=f; auto p=packet(m);
  p.bytes[11]=200; net::Message decoded; assert(!net::decode(p.bytes,p.length,decoded));
  for(unsigned value=0;value<512;++value) { hockey::Options o; const bool okay=hockey::Options::decode(uint16_t(value),o); if(okay) assert(o.encode()==value && o.map<=8 && o.palette<4 && uint8_t(o.powers)<3); }
  assert(strcmp(net::serviceUuid(net::GameProfile::Hockey),net::serviceUuid(net::GameProfile::Flappy))!=0);
}

static void pairedFramesAndStaleness() {
  hockey::Channel c; hockey::Frame f; f.tick=65534; f.x[0]=35; f.y[0]=80; f.phase=hockey::Phase::Rally;
  assert(c.receiveFrame(hockey::Part::Board,f,0)); assert(!c.received());
  auto next=f; next.tick=65535;
  assert(c.receiveFrame(hockey::Part::Motion,next,1)); assert(!c.receiveFrame(hockey::Part::Board,f,2)); assert(!c.received());
  assert(c.receiveFrame(hockey::Part::Board,next,3)); assert(c.received());
  assert(!c.receiveFrame(hockey::Part::Motion,f,4));
  next.tick=0; next.x[0]=45;
  assert(c.receiveFrame(hockey::Part::Board,next,34)); assert(c.latest().tick==65535);
  assert(c.receiveFrame(hockey::Part::Motion,next,36)); assert(c.latest().tick==65536);
  float hx,hy,hr,gx,gy,gr;
  HockeyGame::bumper(3,65536,0,hx,hy,hr); HockeyGame::bumper(3,c.latest().tick,0,gx,gy,gr);
  assert(hx==gx && hy==gy && hr==gr);
  hockey::Frame display; assert(c.display(45,display)); assert(display.x[0]>35 && display.x[0]<45);
  assert(c.display(400,display) && display.phase==hockey::Phase::LinkWait);
  hockey::Input input; input.sequence=65535; input.move=-1024;
  assert(c.receiveInput(input,UINT32_MAX-10)); assert(c.inputFresh(20) && c.move(20)==-1024);
  assert(!c.receiveInput(input,21)); input.sequence=0; assert(c.receiveInput(input,22));
  assert(!c.inputFresh(300) && c.move(300)==0);
  input.sequence=1; input.viewReady=false; assert(c.receiveInput(input,301)); assert(!c.inputFresh(302));
  input.sequence=2; input.viewReady=true; assert(c.receiveInput(input,303)); assert(c.inputFresh(304));
  c.reset(); assert(!c.received() && !c.inputFresh(0));
}

struct Match {
  net::Session host,guest;
  HockeyGame game;
  unsigned frames=0;
  bool drop=false;
  bool retainLocal=false;
  Match(uint32_t now) {
    assert(host.setGameProfile(net::GameProfile::Hockey)); assert(guest.setGameProfile(net::GameProfile::Hockey));
    hockey::Options o; o.map=6; o.palette=2; o.powers=hockey::Powers::Both;
    host.beginHost(now,11,988,o.encode()); guest.beginGuest(now); host.transportConnected(now); guest.transportConnected(now);
    game.reset(988,o);
  }
  void pump(uint32_t now) {
    if(!retainLocal) host.hockeyChannel().setLocal(game.frame());
    guest.hockeyChannel().setInput(600,guest.hockeyChannel().finished(false));
    for(unsigned i=0;i<3;++i) {
      net::Packet p;
      if(host.nextPacket(now,p) && (!drop || ++frames%7!=0)) guest.receive(p.bytes,p.length,now);
      if(guest.nextPacket(now,p) && (!drop || ++frames%11!=0)) host.receive(p.bytes,p.length,now);
    }
    host.advance(now); guest.advance(now);
  }
  void start(uint32_t now) {
    for(unsigned t=0;t<400;t+=5) pump(now+t);
    assert(host.phase()==net::Phase::Lobby && guest.phase()==net::Phase::Lobby);
    host.requestReady(now+400); guest.requestReady(now+400);
    for(unsigned t=400;t<3500;t+=5) pump(now+t);
    assert(host.phase()==net::Phase::Playing && guest.phase()==net::Phase::Playing);
    net::StartInfo a,b; assert(host.consumeStart(a) && guest.consumeStart(b)); assert(a.seed==b.seed && a.baseHue==b.baseHue);
  }
};
static void longMatchWorldPhase() {
  hockey::Channel channel; HockeyGame game; game.reset(11);
  for(unsigned tick=0;tick<270000;tick+=100) {
    auto frame=game.frame(); frame.tick=tick; frame.map=3;
    for(auto type:{net::MessageType::HockeyMotion,net::MessageType::HockeyBoard}) {
      auto m=message(type); m.hockeyFrame=frame; const auto wire=packet(m); net::Message decoded;
      assert(net::decode(wire.bytes,wire.length,decoded));
      assert(channel.receiveFrame(net::hockeyPart(type),decoded.hockeyFrame,tick));
    }
    assert(channel.latest().tick==tick);
    for(unsigned map=0;map<hockey::MapCount;++map) for(unsigned i=0;i<HockeyGame::bumperCount(uint8_t(map));++i) {
      float ax,ay,ar,bx,by,br;
      HockeyGame::bumper(uint8_t(map),tick,i,ax,ay,ar);
      HockeyGame::bumper(uint8_t(map),channel.latest().tick,i,bx,by,br);
      assert(ax==bx && ay==by && ar==br);
    }
  }
}
static void sessions() {
  for(uint32_t begin:{0u,UINT32_MAX-2000}) {
    Match m(begin); m.start(begin); m.drop=true;
    for(unsigned t=3500;t<60000;t+=5) {
      if(t%15==0) { if(m.host.hockeyChannel().inputFresh(begin+t)) m.game.tick(-500,m.host.hockeyChannel().move(begin+t)); else m.game.hold(); }
      m.pump(begin+t);
      assert(m.host.phase()==net::Phase::Playing && m.guest.phase()==net::Phase::Playing);
    }
    assert(m.guest.hockeyChannel().fresh(begin+59995)); assert(m.host.hockeyChannel().inputFresh(begin+59995));
    auto bad=message(net::MessageType::HockeyInput,12); assert(!deliver(m.host,bad,begin+60000));
    bad.session=11; bad.round=2; assert(!deliver(m.host,bad,begin+60000));
    bad.round=1; assert(!deliver(m.guest,bad,begin+60000));
    bad=message(net::MessageType::HockeyMotion); bad.hockeyFrame=m.game.frame(); assert(!deliver(m.host,bad,begin+60000));
    auto finished=m.game.frame(); finished.phase=hockey::Phase::Finished; finished.scores[0]=7;
    for(unsigned t=60000;t<61000;t+=5) { ++finished.tick; m.host.hockeyChannel().setLocal(finished);
      m.guest.hockeyChannel().setInput(0,m.guest.hockeyChannel().finished(false));
      net::Packet p; for(unsigned i=0;i<3;++i) { if(m.host.nextPacket(begin+t,p)) m.guest.receive(p.bytes,p.length,begin+t); if(m.guest.nextPacket(begin+t,p)) m.host.receive(p.bytes,p.length,begin+t); }
    }
    assert(m.host.hockeyChannel().peerFinished(true));
    m.retainLocal=true; m.host.requestReady(begin+61000); m.guest.requestReady(begin+61000);
    for(unsigned t=61000;t<61500;t+=5) m.pump(begin+t);
    assert(m.host.phase()==net::Phase::Countdown && m.guest.phase()==net::Phase::Countdown && m.host.round()==2 && m.guest.round()==2);
    m.host.advance(begin+66000); assert(m.host.phase()==net::Phase::Disconnected);
  }
}
int main() { codec(); pairedFramesAndStaleness(); longMatchWorldPhase(); sessions(); std::cout<<"Hockey link: 20-byte paired snapshots, truncation rejection, ordering, 75-minute world phase, wrap, interpolation, stale inputs, dropped packets, role/session checks and rematches passed\n"; }
