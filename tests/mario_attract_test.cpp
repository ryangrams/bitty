#include "../firmware/flappy/mario_attract.h"
#include <array>
#include <assert.h>
#include <iostream>
namespace flappy {
struct MarioAttractTestAccess {
 template<class R> static void attach(MarioAttract& c,R& r,const uint8_t* rom) {c.rom_=rom;assert(c.prepare(r));}
};
}
struct Runtime {
 std::array<uint8_t,2048> ram{};
 std::array<uint8_t,16384> cache{};
 unsigned frames=0, resets=0, writes=0;
 bool boot=false;
 uint8_t mask=0;
 bool ready() const{return true;}
 bool load(const uint8_t*,size_t){return true;}
 uint8_t readRam(unsigned a) const {assert(a<ram.size());return ram[a];}
 bool setPrgCache(uint8_t m){mask=m;return true;}
 bool patchCachedPrg(unsigned bank,size_t off,const uint8_t* data,size_t size) {
  assert(bank<2 && off+size<=8192 && data && size);
  for(size_t i=0;i<size;++i) cache[bank*8192+off+i]=data[i];
  ++writes;
  return true;
 }
 bool step(uint8_t input){++frames;if(boot){ram.fill(0);ram[0x772]=3;boot=false;return true;}if(input&8){ram[0x770]=1;ram[0x772]=3;ram[0x0e]=8;}return true;}
 bool softReset(){++resets;boot=true;return true;}
 void setPal(bool p){assert(!p);} void setPcmEnabled(bool p){assert(!p);}
 void setVideoDivisor(unsigned n){assert(n==1);} void resetVideoRows(){}
};
int main() {
 using namespace flappy;
 std::array<uint8_t,40976> rom{};
 // Synthetic metadata lets the unit test inspect address and state handling.
 // Production accepts only the fingerprinted user cartridge.
 rom[16+0x1d28]=0;rom[16+0x1d29]=3;rom[16+0x1d2a]=25;rom[16+0x1d2b]=28;
 rom[16+0x1ce0]=31;rom[16+0x1ce1]=6;rom[16+0x1ce2]=28;rom[16+0x1ce3]=0;
 Runtime r;MarioAttract clock;assert(!clock.begin(r,rom.data(),rom.size(),0));
 MarioAttractTestAccess::attach(clock,r,rom.data());
 assert(r.mask==3 && r.writes>5 && clock.levelBytes()>=30 && clock.levelBytes()<MarioClock::MaxLevelBytes);
 const auto input=rom;
 for(unsigned i=0;i<100&&!clock.playing();++i)assert(clock.step(r));
 assert(clock.playing());unsigned frames=r.frames;
 for(unsigned i=0;i<100;++i) assert(clock.step(r));
 assert(r.frames==frames+100);
 r.ram[0xe]=6;assert(clock.step(r));assert(clock.deaths()==1 && clock.restarting());
 for(unsigned i=0;i<90;++i) assert(clock.step(r));
 assert(clock.course()==0 && !clock.playing());
 for(unsigned i=0;i<100&&!clock.playing();++i)assert(clock.step(r));
 r.ram[0xe]=11;assert(clock.step(r));
 for(unsigned i=0;i<90;++i) assert(clock.step(r));
 assert(clock.course()==1 && clock.coursesCompleted()==1 && clock.deaths()==2);
 for(unsigned i=0;i<100&&!clock.playing();++i)assert(clock.step(r));
 r.ram[0x3ad]=200;
 for(unsigned i=0;i<32;++i) assert(clock.step(r));
 assert(clock.crop()==121);
 std::array<uint16_t,135*240+32> output{};output.fill(0x1234);
 std::array<uint16_t,256*8> pixels{};
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<256;++x)pixels[y*256+x]=uint16_t(y*256+x);
 clock.consume(0,64,255,71,pixels.data(),output.data()+16);
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<135;++x)assert(output[16+(64+y)*135+x]==pixels[y*256+x+121]);
 ClockTime time;time.hour=23;time.minute=59;clock.drawTime(output.data()+16,time);
 for(unsigned i=0;i<16;++i)assert(output[i]==0x1234 && output[135*240+16+i]==0x1234);
 assert(rom==input);
 for(unsigned i=0;i<4;++i){MarioClock route;route.buildCourse(i);assert(route.clockCount()==2 && route.obstacleCount()==(i<2 ? 8u : 7u));}
 std::cout<<"Mario attract: authored courses, NTSC every-frame calls, death retry, bounded recovery, crop guards and ROM ownership passed\n";
}
