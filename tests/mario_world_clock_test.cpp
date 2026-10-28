#include "../firmware/flappy/mario_world_clock.h"
#include <assert.h>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

namespace flappy {
struct MarioWorldClockTestAccess {
  static void bindFixture(MarioWorldClock& scene, const std::vector<uint8_t>& bytes) {
    scene.rom_=bytes.data(); scene.loadPalette();
  }
  static void tile(const MarioWorldClock& scene, uint16_t* pixels, int x, int y,
                   uint8_t number, bool sprite, bool flipX, bool flipY) {
    scene.tile(pixels,x,y,number,1,sprite,flipX,flipY);
  }
  static uint16_t color(const MarioWorldClock& scene, unsigned value) { return scene.palette_[4+value]; }
  static uint16_t paletteColor(const MarioWorldClock& scene, unsigned pal, unsigned value) {
    return scene.palette_[pal*4+value];
  }
  static void meta(const MarioWorldClock& scene, uint16_t* pixels, int x, int y) {
    scene.meta(pixels,x,y,0x40,false);
  }
  static uint64_t terrainHash(const MarioWorldClock& scene) {
    uint64_t hash=1469598103934665603ull;
    for(unsigned i=0;i<scene.objectCount_;++i) {
      const auto& o=scene.objects_[i];
      const unsigned fields[]={o.x,o.y,o.width,o.height,unsigned(o.kind),o.variation};
      for(unsigned n:fields) {hash^=n; hash*=1099511628211ull;}
    }
    return hash;
  }
  static void validate(const MarioWorldClock& scene) {
    assert(scene.objectCount_<=MarioWorldClock::MaxObjects);
    assert(scene.enemyCount_<=MarioWorldClock::MaxEnemies);
    assert(scene.playerX()>=0 && scene.playerX()<=1600);
    assert(scene.playerFeet()>=0 && scene.playerFeet()<=224);
    assert(scene.phaseTicks_<160);
    for(unsigned i=0;i<scene.objectCount_;++i) {
      const auto& o=scene.objects_[i];
      assert(o.x<1660 && o.width && o.height);
    }
    for(unsigned i=0;i<MarioWorldClock::SignCount;++i) {
      assert(scene.signs_[i]>=160 && scene.signs_[i]+MarioWorldClock::ClockWidth<scene.exitX_);
      for(unsigned k=0;k<scene.objectCount_;++k) {
        const auto& o=scene.objects_[k];
        if(o.y<MarioWorldClock::ClockBottom)
          assert(o.x+o.width<scene.signs_[i] || o.x>scene.signs_[i]+MarioWorldClock::ClockWidth);
      }
    }
  }
  static void coinReset(MarioWorldClock& scene, const ClockTime& time) {
    scene.minuteKey_=uint16_t(time.minuteKey() | (time.twentyFourHour ? 0x8000 : 0));
    scene.taken_[0][0]=0xffffffff;
  }
  static uint32_t taken(const MarioWorldClock& scene) { return scene.taken_[0][0]; }
  static bool signCell(const ClockTime& time,unsigned cell,int& x,int& y) {
    return MarioWorldClock::signCell(time,cell,x,y);
  }
  static void signs(MarioWorldClock& scene,uint16_t* pixels,const ClockTime& time,int left) {
    scene.signs_[0]=200;scene.signs_[1]=1000;scene.signs_[2]=1400;
    scene.camera_=int16_t(200-left);scene.drawSigns(pixels,time);
  }
  static void clearTaken(MarioWorldClock& scene) {std::memset(scene.taken_,0,sizeof(scene.taken_));}
  static void collectAt(MarioWorldClock& scene,const ClockTime& time,unsigned cell) {
    int x,y;assert(MarioWorldClock::signCell(time,cell,x,y));
    scene.xq_=int32_t(scene.signs_[0]+x)*256;scene.yq_=int32_t(y+16)*256;
    scene.collectClockCoins(time);
  }
  static unsigned collected(const MarioWorldClock& scene) {return scene.collectedCells(0);}
  static void away(MarioWorldClock& scene) {scene.xq_=32*256;scene.yq_=216*256;}
  static void allBranches(MarioWorldClock& scene) {
    scene.objectCount_=scene.enemyCount_=0;
    for(unsigned k=0;k<=unsigned(MarioWorldClock::Kind::Bowser);++k)
      scene.add(MarioWorldClock::Kind(k),unsigned(k*4),80+(k%3)*24,32,32,k%4);
    for(unsigned k=0;k<=unsigned(MarioWorldClock::Foe::Lakitu);++k)
      scene.foe(MarioWorldClock::Foe(k),unsigned(k*8),160,k%3);
  }
};
}
using namespace flappy;

static std::vector<uint8_t> fixture() {
  std::vector<uint8_t> bytes(40976,0);
  bytes[0]='N';bytes[1]='E';bytes[2]='S';bytes[3]=0x1a;bytes[4]=2;bytes[5]=1;bytes[6]=1;
  const auto set=[&](unsigned address,uint8_t value){bytes[16+address-0x8000]=value;};
  for(unsigned p=0;p<4;++p) {
    const unsigned table=0xa000+p*256;
    set(0x8b08+p,uint8_t(table));set(0x8b0c+p,uint8_t(table>>8));
    for(unsigned m=0;m<64;++m)
      for(unsigned tile=0;tile<4;++tile) set(table+m*4+tile,uint8_t(tile+1));
    for(unsigned i=0;i<32;++i) set(0x8ca4+p*36+3+i,uint8_t(1+i%3));
    set(0x85cf+p,0x0f);
  }
  for(unsigned n=0;n<4;++n) {
    set(0xe682+n,uint8_t(n+1));
    for(unsigned y=0;y<8;++y) {
      bytes[16+32768+n*16+y]=uint8_t(0x80>>y);
      bytes[16+32768+n*16+y+8]=uint8_t((n&1)?0xff:0);
      bytes[16+32768+4096+n*16+y]=uint8_t((n&1)?0xff:0);
      bytes[16+32768+4096+n*16+y+8]=uint8_t((n&2)?0xff:0);
    }
  }
  // Every sprite table references authored synthetic pattern tiles.
  for(unsigned i=0;i<256;++i) {set(0xe73e + i,uint8_t(i%3+1));set(0xee17+i,uint8_t(i%3+1));}
  for(unsigned i=0;i<16;++i)set(0xe6be + i,uint8_t(i%3+1));
  return bytes;
}

static void bindingAndNativePixels() {
  MarioWorldClock scene; scene.reset(1);
  const auto bytes=fixture();
  assert(!scene.bindRom(nullptr,40976));
  assert(!scene.bindRom(bytes.data(),40975));
  assert(!scene.bindRom(bytes.data(),bytes.size()));
  assert(!scene.ready());
  std::vector<uint16_t> storage(135*240+32,0x5aa5);
  uint16_t* pixels=storage.data()+16;
  ClockTime time;
  scene.draw(pixels,time);
  for(unsigned i=0;i<135*240;++i)assert(pixels[i]==0);
  MarioWorldClockTestAccess::bindFixture(scene,bytes);
  assert(scene.ready());
  for(bool sprite:{false,true})for(bool flipX:{false,true})for(bool flipY:{false,true})
    for(int x:{-9,-7,0,131,135})for(int y:{-9,-7,0,236,240}) {
      std::fill(pixels,pixels+135*240,0x5aa5);
      MarioWorldClockTestAccess::tile(scene,pixels,x,y,1,sprite,flipX,flipY);
      for(int py=0;py<240;++py)for(int px=0;px<135;++px) {
        uint16_t expected=0x5aa5;
        if(px>=x&&px<x+8&&py>=y&&py<y+8) {
          const int sy=flipY?7-(py-y):py-y, sx=flipX?7-(px-x):px-x;
          const unsigned value=sprite ? unsigned((sx==sy)?3:2):1;
          expected=MarioWorldClockTestAccess::color(scene,value);
        }
        assert(pixels[py*135+px]==expected);
      }
      for(unsigned i=0;i<16;++i)assert(storage[i]==0x5aa5&&storage[135*240+16+i]==0x5aa5);
    }
  std::fill(pixels,pixels+135*240,0x5aa5);
  MarioWorldClockTestAccess::meta(scene,pixels,3,5);
  assert(pixels[5*135+3]==MarioWorldClockTestAccess::color(scene,1));
  assert(pixels[13*135+3]==MarioWorldClockTestAccess::color(scene,2));
  assert(pixels[5*135+11]==MarioWorldClockTestAccess::color(scene,3));
  assert(pixels[13*135+11]==MarioWorldClockTestAccess::color(scene,0));
  scene.draw(nullptr,time);
}

static void stackedClockGeometryAndCollection() {
  static_assert(MarioWorldClock::ClockCoinSize==16,"Clock coins retain their native square cell");
  static_assert(MarioWorldClock::ClockWidth==112,"Two three-column digits share a seven-cell row");
  static_assert(MarioWorldClock::ClockBottom<209,"Clock art must leave the shared date line clear");
  const auto bytes=fixture();
  MarioWorldClock scene;scene.reset(31);MarioWorldClockTestAccess::bindFixture(scene,bytes);
  ClockTime time;time.hour=12;time.minute=34;
  std::vector<uint16_t> storage(135*240+32,0x5aa5),expected(135*240,0x5aa5);
  auto* pixels=storage.data()+16;
  // This independent fixture spells 12 above 34, with no colon or center marks.
  const char* rows[4][5]={{"010","110","010","010","111"},
    {"111","001","111","100","111"},{"111","001","111","001","111"},
    {"101","101","111","001","001"}};
  for(unsigned digit=0;digit<4;++digit)for(unsigned row=0;row<5;++row)for(unsigned col=0;col<3;++col) {
    const unsigned cell=digit*15+row*3+col;int x=0,y=0;
    const bool ink=MarioWorldClockTestAccess::signCell(time,cell,x,y);
    assert(ink==(rows[digit][row][col]=='1'));
    assert(x==int(digit%2)*64+int(col)*16);
    assert(y==(digit<2?16:112)+int(row)*16);
    assert(x>=0&&x+16<=112&&y>=16&&y+16<=192);
    if(!ink)continue;
    for(int py=0;py<16;++py)for(int px=0;px<16;++px) {
      const unsigned value=px<8?(py<8?1:2):(py<8?3:0);
      if(value)expected[(y+py)*135+MarioWorldClock::ClockLeft+x+px]=
        MarioWorldClockTestAccess::paletteColor(scene,3,value);
    }
  }
  int x=0,y=0;assert(!MarioWorldClockTestAccess::signCell(time,60,x,y));
  MarioWorldClockTestAccess::signs(scene,pixels,time,MarioWorldClock::ClockLeft);
  assert(std::equal(expected.begin(),expected.end(),pixels));
  for(int py=16;py<192;++py)for(int px=59;px<75;++px)assert(pixels[py*135+px]==0x5aa5);
  for(int py=209;py<240;++py)for(int px=0;px<135;++px)assert(pixels[py*135+px]==0x5aa5);
  for(int left:{-113,-112,-111,-15,-1,0,11,134,135}) {
    std::fill(pixels,pixels+135*240,0x5aa5);
    MarioWorldClockTestAccess::signs(scene,pixels,time,left);
    for(unsigned i=0;i<16;++i)assert(storage[i]==0x5aa5&&storage[135*240+16+i]==0x5aa5);
  }
  MarioWorldClockTestAccess::clearTaken(scene);
  const unsigned before=scene.coinsCollected();
  for(unsigned cell:{30u,31u,32u,35u,36u})MarioWorldClockTestAccess::collectAt(scene,time,cell);
  assert(MarioWorldClockTestAccess::collected(scene)==3&&scene.coinsCollected()==before+3);
  MarioWorldClockTestAccess::away(scene);
  scene.step(time);assert(MarioWorldClockTestAccess::collected(scene)==0);
  MarioWorldClockTestAccess::collectAt(scene,time,30);
  assert(MarioWorldClockTestAccess::collected(scene)==1);
  MarioWorldClockTestAccess::away(scene);time.twentyFourHour=false;
  scene.step(time);assert(MarioWorldClockTestAccess::collected(scene)==0);
  std::cout<<"Mario clock: stacked native coin cells, empty separator, collection limit and date/footer clearance passed\n";
}

static void lifetimeAndVariety() {
  const auto bytes=fixture(), original=bytes;
  std::vector<uint16_t> guarded(135*240+32,0x5aa5);
  std::set<uint64_t> terrain;
  uint32_t allEnemies=0; unsigned minTransitions=100000, totalCoins=0, totalStomps=0;
  ClockTime time;time.hour=12;time.minute=34;
  for(unsigned seed=1;seed<=64;++seed) {
    MarioWorldClock scene, repeat;
    scene.reset(seed);repeat.reset(seed);
    assert(MarioWorldClockTestAccess::terrainHash(scene)==MarioWorldClockTestAccess::terrainHash(repeat));
    terrain.insert(MarioWorldClockTestAccess::terrainHash(scene));
    MarioWorldClockTestAccess::bindFixture(scene,bytes);
    for(unsigned frame=0;frame<36000;++frame) {
      time.ms=frame*1000/60; time.second=uint8_t(frame/60%60);
      scene.step(time);MarioWorldClockTestAccess::validate(scene);
      if(seed==1&&frame%19==0) {
        const auto before=scene;
        scene.draw(guarded.data()+16,time);
        assert(std::memcmp(&before,&scene,sizeof(scene))==0);
        for(unsigned i=0;i<16;++i)assert(guarded[i]==0x5aa5&&guarded[135*240+16+i]==0x5aa5);
      }
    }
    assert(scene.themesSeen()==15);
    assert(scene.travelledPixels()>15000);
    assert(scene.transitions()>10);
    assert(scene.safetyRebounds()==0);
    minTransitions=scene.transitions()<minTransitions?scene.transitions():minTransitions;
    allEnemies|=scene.enemiesSeen();totalCoins+=scene.coinsCollected();totalStomps+=scene.enemiesStomped();
  }
  assert(terrain.size()>60);
  assert(allEnemies==0xfff);
  assert(totalCoins>64&&totalStomps>16);
  assert(bytes==original);
  for (uint32_t seed:{0u,0x7fffffffu,0x80000000u,0xffffffffu}) {
    MarioWorldClock edge;edge.reset(seed);MarioWorldClockTestAccess::bindFixture(edge,bytes);
    for(unsigned f=0;f<24000;++f) {
      edge.step(time);MarioWorldClockTestAccess::validate(edge);
      if(f%211==0)edge.draw(guarded.data()+16,time);
    }
    assert(edge.themesSeen()==15 && edge.transitions()>8 && edge.safetyRebounds()==0);
  }
  std::cout<<"Mario world clocks: 64 seeds, 2,304,000 frames, minimum "<<minTransitions
           <<" continuous pipe transitions, "<<totalCoins<<" coins and "<<totalStomps<<" stomps\n";

  MarioWorldClock scene;scene.reset(99);MarioWorldClockTestAccess::bindFixture(scene,bytes);
  MarioWorldClockTestAccess::coinReset(scene,time);
  scene.step(time);assert(MarioWorldClockTestAccess::taken(scene)==0xffffffff);
  time.minute=35;scene.step(time);assert(MarioWorldClockTestAccess::taken(scene)==0);
  MarioWorldClockTestAccess::allBranches(scene);
  for(unsigned f=0;f<128;++f) {
    scene.draw(guarded.data()+16,time);scene.step(time);
    for(unsigned i=0;i<16;++i)assert(guarded[i]==0x5aa5&&guarded[135*240+16+i]==0x5aa5);
  }
  assert(sizeof(scene)<2048);
  std::cout<<"Mario world clock state: "<<sizeof(scene)<<" bytes; native pixel and draw-boundary checks passed\n";
}

int main() {bindingAndNativePixels();stackedClockGeometryAndCollection();lifetimeAndVariety();}
