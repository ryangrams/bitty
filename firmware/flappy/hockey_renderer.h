#pragma once
#include "hockey_game.h"
#include "clock_scene.h"

namespace flappy {
class HockeyRenderer {
 public:
  static void draw(uint16_t* pixels,const hockey::Frame& f,unsigned local=1,const ClockTime* clock=nullptr) {
    if(!pixels) return;
    ArcadeCanvas c(pixels); const Palette p=palette(f.map,f.palette);
    c.clear(p.outside); c.rect(5,20,125,205,p.floor);
    for(int y=26;y<221;y+=8) for(int x=10;x<128;x+=8) c.pixel(x,y,p.holes);
    c.rect(3,18,129,2,p.rail); c.rect(3,224,129,2,p.rail);
    c.rect(3,20,2,204,p.rail); c.rect(130,20,2,204,p.rail);
    World w(c,local==0);
    for(int x=6;x<129;x+=6) w.rect(x,122,3,1,p.line);
    w.circle(67,122,21,p.line,false);
    if(f.map==1) for(unsigned stripe=0;stripe<5;++stripe) w.rect(83+int(stripe)*3,65,2,114,rainbow(stripe));
    if(f.map==4 && f.tick%480>=300) for(unsigned i=0;i<4;++i) {
      const int x=20+int(i)*28,y=110+(i%2)*24;
      w.rect(x,y,3,1,p.rail); w.pixel(x+(f.tick/480%2 ? -1 : 3),y-1,p.rail);
    }
    if(f.map==6) w.circle(24,122,14,0x72a4,true);
    for(unsigned i=0;i<HockeyGame::bumperCount(f.map);++i) {
      float x,y,r; HockeyGame::bumper(f.map,f.tick,i,x,y,r);
      if(r<=0) {
        w.circle(int(x),int(y),7,p.holes,false);
        if(f.tick%420>285) w.circle(int(x),int(y),3,p.rail,false);
      } else {
        w.circle(int(x),int(y),int(r)+1,p.outside,true); w.circle(int(x),int(y),int(r),f.map==6 ? 0xab67 : rainbow(i),true);
        w.pixel(int(x)-2,int(y)-1,0xffff); w.pixel(int(x)+2,int(y)-1,0xffff);
        if(f.map==3) for(int j=-3;j<=3;j+=3) w.rect(int(x)+j,int(y)+int(r),1,3,p.rail);
        if(f.map==2) w.rect(int(x),int(y)-int(r)-4,1,5,0xffff);
      }
    }
    for(unsigned who=0;who<2;++who) {
      const int y=who ? 222 : 22;
      const int half=f.powers[who]==hockey::Power::TinyGoal ? 11 : f.rules==hockey::Powers::Queen && who==1 ? 28 : 22;
      w.rect(67-half,y-2,half*2,4,p.outside);
      if(f.powers[who]==hockey::Power::Wall) for(int j=0;j<4;++j) w.rect(67-half,y-2+j,half*2,1,rainbow(unsigned(j)));
      const auto kind=f.powers[who];
      if(kind==hockey::Power::Beam || (kind==hockey::Power::Lasers && f.tick%16<5)) {
        const int x=f.paddles[who],from=who ? 51 : 53;
        for(int j=-2;j<=2;++j) w.rect(x+j,from,1,140,kind==hockey::Power::Beam ? rainbow(unsigned(j+2+f.tick/4)) : 0xfaf7);
      }
      const int r=HockeyGame::radius(f,who),x=f.paddles[who],py=int(HockeyGame::paddleY(who));
      const uint16_t body=who ? 0xff32 : 0xfaf7;
      w.circle(x,py,r+1,p.outside,true); w.circle(x,py,r,body,true); w.circle(x,py,r-3,who ? 0xbbe9 : 0xb9b5,true);
      const int faceX=local==0 ? 134-x : x,faceY=local==0 ? 244-py : py;
      c.rect(faceX-3,faceY-1,2,2,0xffff); c.rect(faceX+2,faceY-1,2,2,0xffff);
      c.pixel(faceX-2,faceY,0x0842); c.pixel(faceX+3,faceY,0x0842);
      c.rect(faceX-2,faceY+4,5,1,0x0842);
      if(who==0) { c.rect(faceX-5,faceY-r-3,11,3,0xfe46); c.rect(faceX-5,faceY-r-6,2,3,0xfe46); c.rect(faceX-1,faceY-r-7,3,4,0xfe46); c.rect(faceX+4,faceY-r-6,2,3,0xfe46); }
      else { c.rect(faceX-7,faceY-r-1,15,2,0x7b26); c.rect(faceX-4,faceY-r-4,9,3,0x7b26); }
      if(f.stunMask&(1<<who)) { w.circle(x,py,r+2,0xc7ff,false); w.rect(x-2,py-r-5,5,1,0xffff); }
      if(f.powers[1-who]==hockey::Power::Slime) w.rect(x-r,py+r-2,r*2,3,0x5e6b);
      if(f.powers[1-who]==hockey::Power::Backwards) { w.pixel(x-r-3,py,0xfd04); w.pixel(x+r+3,py,0xfd04); }
      if(f.powers[1-who]==hockey::Power::Butterflies) for(int b=0;b<3;++b) {
        const int bx=x-12+b*10,by=py-16+(f.tick/5+b)%4;
        w.rect(bx-2,by,2,3,0xc53f); w.rect(bx+1,by,2,3,0xc53f); w.rect(bx,by,1,4,0xffff);
      }
    }
    if(f.pickup!=hockey::Power::None) {
      const int x=f.pickupX,y=int(HockeyGame::paddleY(f.pickupOwner));
      w.circle(x,y,7,0xffff,false); w.circle(x,y,6,p.outside,true);
      const uint16_t color=rainbow(uint8_t(f.pickup));
      w.rect(x-2,y-4,5,2,color); w.rect(x-4,y-2,9,5,color); w.rect(x-2,y+3,5,2,color);
      // Each pickup has a distinct native bitmap mark.
      const uint8_t mark=uint8_t(f.pickup);
      for(unsigned i=0;i<4;++i) if(mark&(1<<i)) w.pixel(x-2+int(i%2)*3,y-1+int(i/2)*3,p.outside);
    }
    for(unsigned i=0;i<3;++i) if(f.puckMask&(1<<i)) {
      w.circle(f.x[i],f.y[i],5,p.outside,true); w.circle(f.x[i],f.y[i],4,i ? 0xa7ee : 0xffff,true);
      w.rect(int(f.x[i])-2,int(f.y[i])-2,3,1,i ? 0x57a8 : 0xc7ff);
    }
    c.rect(0,0,135,17,p.outside);
    char score[24];
    if(clock) c.text("HOCKEY QUEEN",31,2,0xffff);
    else { snprintf(score,sizeof(score),"Q %u   HOCKEY   P %u",unsigned(f.scores[0]),unsigned(f.scores[1])); c.text(score,10,2,0xffff); }
    if(clock) {
      for(unsigned d=0;d<4;++d) for(unsigned row=0;row<5;++row) for(unsigned col=0;col<3;++col)
        if(ClockScene::digitPixel(clock->digit(d),col,row))
          c.rect(42+int(d%2)*30+int(col)*8,72+int(d/2)*50+int(row)*8,6,6,p.outside);
    } else {
      if(f.phase==hockey::Phase::Goal) banner(c,"GOAL!",p.outside);
      if(f.phase==hockey::Phase::Finished) banner(c,f.scores[local]>=hockey::PlayTo ? "YOU WIN!" : "ROYAL REVENGE?",p.outside);
      if(f.phase==hockey::Phase::LinkWait) banner(c,"LINK WAIT",p.outside);
      const auto power=f.powers[local]!=hockey::Power::None ? f.powers[local] : f.powers[1-local];
      c.rect(0,226,135,14,p.outside);
      c.text(power!=hockey::Power::None ? hockey::powerName(power) : local ? "PEASANT  TILT TO MOVE" : "QUEEN  TILT TO MOVE",5,227,0xffff);
    }
  }
  static void banner(ArcadeCanvas& c,const char* text,uint16_t background=0) {
    c.rect(7,108,121,27,background); c.text(text,(135-MenuFont::textWidth(text))/2,115,0xffff);
  }
 private:
  struct Palette { uint16_t floor,holes,line,rail,outside; };
  static uint16_t rainbow(unsigned n) { static const uint16_t colors[]={0xfaeb,0xfd47,0xff29,0x67ad,0x3d9f,0xa33f}; return colors[n%6]; }
  static Palette palette(uint8_t map,uint8_t style) {
    static const Palette natural[]={
      {0x96bf,0x7e1c,0xffff,0xfb77,0x20ca},{0xd5df,0xc4fe,0xffff,0xfe27,0x2087},
      {0x337b,0x2b19,0xb77f,0xeeb4,0x08c9},{0x20ca,0x316d,0x77bf,0x4a34,0x0843},
      {0x976f,0x86ed,0xffff,0xcc4a,0x1923},{0xf5bc,0xecfa,0xffff,0xea70,0x2087},
      {0x9c69,0x8be8,0xeef1,0x9b66,0x20c2},{0x308b,0x410e,0xfed2,0xc529,0x1003}};
    if(style==1) return Palette{0x10c5,0x2128,0x7414,0xb456,0x0842};
    if(style==2) return Palette{0x1006,0x204a,0x17df,0xfa5b,0x0803};
    if(style==3) return Palette{0xd71f,0xbedb,0x6b95,0xc539,0x394d};
    return natural[map<hockey::MapCount ? map : 0];
  }
  class World {
   public:
    World(ArcadeCanvas& c,bool rotate):c_(c),rotate_(rotate){}
    void pixel(int x,int y,uint16_t color) { if(y<18||y>225) return; c_.pixel(rotate_ ? 134-x : x,rotate_ ? 244-y : y,color); }
    void rect(int x,int y,int width,int height,uint16_t color) { for(int row=0;row<height;++row) for(int col=0;col<width;++col) pixel(x+col,y+row,color); }
    void circle(int x,int y,int r,uint16_t color,bool fill) {
      for(int dy=-r;dy<=r;++dy) for(int dx=-r;dx<=r;++dx) {
        const int d=dx*dx+dy*dy; if(d<=r*r && (fill || d>=(r-1)*(r-1))) pixel(x+dx,y+dy,color);
      }
    }
   private: ArcadeCanvas& c_; bool rotate_;
  };
};
}  // namespace flappy
