#pragma once

#include "arkanoid_game.h"
#include "clock_scene.h"
#include <stdint.h>

namespace flappy {

// The clock uses the game's native brick, ball and paddle artwork. Its automatic
// player aims from the paddle; bricks lose strength only on ball contact.
class ArkanoidClock {
 public:
  enum { Columns = ArkanoidGame::Columns, Rows = 18, BrickCount = Columns * Rows,
         Unit = 256, Left = ArkanoidGame::Left, Right = ArkanoidGame::Right,
         Ceiling = 17, PaddleY = ArkanoidGame::PaddleY,
         BrickLeft = ArkanoidGame::BrickLeft, BrickTop = 29,
         PitchX = ArkanoidGame::BrickPitchX, PitchY = ArkanoidGame::BrickPitchY,
         BrickWidth = ArkanoidGame::BrickWidth, BrickHeight = ArkanoidGame::BrickHeight,
         MaxBalls = 3, HalfPaddle = 17 };
  enum class Phase : uint8_t { Build, Read, Play, Wipe };
  struct Brick {
    uint8_t hits, color, flash, strength;
    Brick(uint8_t h=0,uint8_t c=0,uint8_t f=0,uint8_t s=0) : hits(h),color(c),flash(f),strength(s) {}
  };
  struct Ball { int32_t x = 0, y = 0; int16_t vx = 0, vy = 0; bool active = false; };

  void reset(uint32_t seed) {
    random_ = seed ? seed : 1;
    frames_ = boards_ = contacts_ = destroyed_ = clears_ = paddleBounces_ = 0;
    phaseFrame_ = remaining_ = clockBricks_ = 0;
    initialized_ = false; multi_ = false;
    phase_ = Phase::Build;
    paddleX_ = 67 * Unit;
    target_ = 0;
    for (Brick& b : bricks_) b = Brick();
    for (Ball& b : balls_) b = Ball();
  }

  void step(const ClockTime& time) {
    const uint16_t key = uint16_t(time.minuteKey() | (time.twentyFourHour ? 0x8000 : 0));
    if (!initialized_) begin(time, key);
    else if (key != requestedKey_) { requestedKey_ = key; wipe(); }
    ++frames_; ++phaseFrame_;
    for (Brick& brick : bricks_) if (brick.flash) --brick.flash;
    if (phase_ == Phase::Wipe) {
      const unsigned boundary = unsigned(phaseFrame_) * Rows / 48;
      for (unsigned r = 0; r < boundary && r < Rows; ++r)
        for (unsigned c = 0; c < Columns; ++c) bricks_[(Rows - 1 - r) * Columns + c].hits = 0;
      if (phaseFrame_ >= 48) begin(time,key);
      return;
    }
    if (phase_ == Phase::Build) {
      if (phaseFrame_ >= 96) { phase_ = Phase::Read; phaseFrame_ = 0; }
      attached(); return;
    }
    if (phase_ == Phase::Read) {
      attached();
      if (phaseFrame_ >= 240) {
        phase_ = Phase::Play; phaseFrame_ = 0;
        balls_[0].vx = int16_t(165 + ClockScene::random(random_) % 151);
        if (ClockScene::random(random_) & 1) balls_[0].vx = int16_t(-balls_[0].vx);
        balls_[0].vy = int16_t(-int(root(Speed * Speed - balls_[0].vx * balls_[0].vx)));
        chooseTarget();
      }
      return;
    }
    movePaddle();
    for (Ball& ball : balls_) if (ball.active) moveBall(ball);
    if (!multi_ && phaseFrame_ >= 540) {
      multi_ = true;
      for (unsigned i = 1; i < MaxBalls; ++i) {
        balls_[i] = balls_[0];
        balls_[i].vx = int16_t(i == 1 ? -Speed / 2 : Speed / 2);
        balls_[i].vy = int16_t(-int(root(Speed * Speed - balls_[i].vx * balls_[i].vx)));
      }
    }
    if (!remaining_) { ++clears_; wipe(); }
  }

  void draw(uint16_t* pixels, const ClockTime&) const {
    if (!pixels) return;
    ArcadeCanvas c(pixels);
    const uint16_t white=c.rgb(236,248,255), cyan=c.rgb(91,229,244), dim=c.rgb(23,39,61);
    c.clear(c.rgb(7,12,28));
    for (int y=24; y<228; y+=16)
      for (int x=11+((y/16)&1)*8; x<130; x+=16) c.pixel(x,y,dim);
    c.rect(2,Ceiling-3,131,2,c.rgb(88,125,150));
    c.rect(2,Ceiling-1,3,214,c.rgb(38,65,91));
    c.rect(130,Ceiling-1,3,214,c.rgb(38,65,91));
    for(int y=26;y<225;y+=18) {
      c.rect(2,y,3,4,c.rgb(111,153,176));c.rect(130,y,3,4,c.rgb(111,153,176));
    }
    for(unsigned i=0;i<BrickCount;++i) if(bricks_[i].hits) {
      int y=brickY(i);
      if(phase_==Phase::Build) {
        const int delay=int((Rows-1-i/Columns)*2+(i%3)*7);
        const int falling=int(phaseFrame_)-delay;
        if(falling<0) continue;
        const int fall=falling*falling/6;
        if(fall<y+12)y=-12+fall;
      }
      drawBrick(c,i,y);
    }
    if (phase_==Phase::Build || phase_==Phase::Read) {
      c.rect(65,105,4,2,c.rgb(190,202,215));
      c.rect(65,112,4,2,c.rgb(190,202,215));
    }
    if(phase_==Phase::Wipe) {
      const int line=218-int(phaseFrame_)*204/48;
      c.rect(6,line,123,2,white);c.rect(6,line+2,123,3,c.rgb(56,111,143));
      for(unsigned i=0;i<18;++i) {
        const int x=8+int((i*31+boards_*7)%116);
        const int y=line+int((i*13+frames_)%23);
        c.rect(x,y,2,2,i&1?cyan:c.rgb(252,173,85));
      }
    }
    const int x=paddleX_/Unit, left=x-HalfPaddle;
    c.line(5,PaddleY,129,PaddleY,c.rgb(38,65,91));
    c.rect(left,PaddleY,HalfPaddle*2,5,c.rgb(49,143,180));
    c.rect(left+3,PaddleY,HalfPaddle*2-6,2,white);
    c.rect(left+3,PaddleY+2,HalfPaddle*2-6,2,cyan);
    c.rect(left,PaddleY,3,4,c.rgb(244,87,119));
    c.rect(left+HalfPaddle*2-3,PaddleY,3,4,c.rgb(244,87,119));
    if(phase_!=Phase::Wipe) for(const Ball& ball:balls_) if(ball.active) {
      const int bx=ball.x/Unit, by=ball.y/Unit;
      c.rect(bx-1,by-1,3,3,white);
      c.pixel(bx-2,by,cyan);c.pixel(bx+2,by,cyan);c.pixel(bx,by-2,cyan);c.pixel(bx,by+2,cyan);
    }
    c.line(6,230,128,230,dim);
  }

  Phase phase() const { return phase_; }
  uint16_t phaseFrame() const { return phaseFrame_; }
  uint32_t boards() const { return boards_; }
  uint32_t brickContacts() const { return contacts_; }
  uint32_t bricksDestroyed() const { return destroyed_; }
  uint32_t clearedBoards() const { return clears_; }
  uint32_t paddleBounces() const { return paddleBounces_; }
  uint16_t remainingBricks() const { return remaining_; }
  uint16_t originalClockBricks() const { return clockBricks_; }
  int paddleX() const { return paddleX_/Unit; }
  const Brick& brick(unsigned i) const { return bricks_[i<BrickCount?i:0]; }
  const Ball& ball(unsigned i) const { return balls_[i<MaxBalls?i:0]; }
  bool clockBrick(unsigned i) const { return i<BrickCount && (bricks_[i].color & 128)!=0; }

 private:
  friend struct ArkanoidClockTestAccess;
  static constexpr int Speed=790;
  Brick bricks_[BrickCount];
  Ball balls_[MaxBalls];
  uint32_t random_=1, frames_=0, boards_=0, contacts_=0, destroyed_=0, clears_=0, paddleBounces_=0;
  int32_t paddleX_=67*Unit;
  uint16_t phaseFrame_=0, remaining_=0, clockBricks_=0, requestedKey_=0, target_=0;
  uint8_t digits_[4]={1,2,0,0}, color_=0;
  Phase phase_=Phase::Build;
  bool initialized_=false, multi_=false;

  static int clamp(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
  static int absolute(int n) { return n<0?-n:n; }
  static uint32_t root(uint32_t n) {
    uint32_t value=0,bit=uint32_t(1)<<30;
    while(bit>n)bit>>=2;
    while(bit) {if(n>=value+bit){n-=value+bit;value=(value>>1)+bit;}else value>>=1;bit>>=2;}
    return value;
  }
  static int brickX(unsigned i) { return BrickLeft+int(i%Columns)*PitchX; }
  static int brickY(unsigned i) { return BrickTop+int(i/Columns)*PitchY; }
  void wipe() {
    if(phase_==Phase::Wipe)return;
    phase_=Phase::Wipe;phaseFrame_=0;
  }
  void begin(const ClockTime& time,uint16_t key) {
    initialized_=true;requestedKey_=key;++boards_;
    phase_=Phase::Build;phaseFrame_=remaining_=clockBricks_=0;multi_=false;
    color_=uint8_t(ClockScene::random(random_)%6);
    for(unsigned d=0;d<4;++d)digits_[d]=time.digit(d);
    for(Brick& brick:bricks_)brick=Brick();
    const unsigned shiftH=ClockScene::random(random_)&1, shiftM=ClockScene::random(random_)&1;
    const unsigned baseH=1+ClockScene::random(random_)%2, baseM=10+ClockScene::random(random_)%2;
    bool reserved[BrickCount]={};
    for(unsigned d=0;d<4;++d) {
      const unsigned left=(d<2?shiftH:shiftM)+(d%2)*4;
      const unsigned top=d<2?baseH:baseM;
      for(int row=-1;row<6;++row)for(int col=-1;col<4;++col) {
        const int r=int(top)+row,c=int(left)+col;
        if(r>=0&&r<Rows&&c>=0&&c<Columns)reserved[unsigned(r)*Columns+unsigned(c)]=true;
      }
      for(unsigned row=0;row<5;++row)for(unsigned col=0;col<3;++col)
        if(ClockScene::digitPixel(digits_[d],col,row)) {
          const unsigned i=(top+row)*Columns+left+col;
          const uint8_t hits=uint8_t(2+ClockScene::random(random_)%2);
          bricks_[i]={hits,uint8_t(128+(color_+d)%6),0,hits};
          ++remaining_;++clockBricks_;
        }
    }
    const unsigned pattern=ClockScene::random(random_)%4;
    for(unsigned row=0;row<Rows;++row)for(unsigned col=0;col<Columns;++col) {
      const unsigned i=row*Columns+col;
      if(reserved[i])continue;
      const bool occupied=pattern==0?(row+col)%3==0:pattern==1?(col==0||col==8||row==17):
        pattern==2?(row%4==0):((row*3+col)%5<2);
      if(occupied) {bricks_[i]={1,uint8_t((color_+row+col/3)%6),0,1};++remaining_;}
    }
    for(Ball& b:balls_)b=Ball();
    balls_[0].active=true;attached();chooseTarget();
  }
  void attached() {
    const int target=67*Unit;
    paddleX_+=clamp(target-paddleX_,-2*Unit,2*Unit);
    balls_[0].x=paddleX_;balls_[0].y=(PaddleY-4)*Unit;
  }
  void chooseTarget() {
    if(!remaining_)return;
    const unsigned start=ClockScene::random(random_)%BrickCount;
    // Favor exposed lower faces so aiming remains useful as the field erodes.
    int chosen=-1;
    for(unsigned row=Rows;row--;) {
      for(unsigned c=0;c<Columns;++c) {
        const unsigned i=row*Columns+(c+start)%Columns;
        if(bricks_[i].hits) {chosen=int(i);break;}
      }
      if(chosen>=0)break;
    }
    if(chosen>=0)target_=uint16_t(chosen);
  }
  static int landingX(const Ball& ball) {
    if(ball.vy<=0)return ball.x;
    const int64_t dx=int64_t(ball.vx)*((PaddleY-3)*Unit-ball.y)/ball.vy;
    const int lo=(Left+2)*Unit, width=(Right-Left-4)*Unit;
    int folded=int((int64_t(ball.x)-lo+dx)%(2*width));
    if(folded<0)folded+=2*width;
    return lo+(folded<=width?folded:2*width-folded);
  }
  void movePaddle() {
    const Ball* next=nullptr;int earliest=0x7fffffff;
    for(const Ball& b:balls_)if(b.active&&b.vy>0) {
      const int t=((PaddleY-3)*Unit-b.y)/b.vy;
      if(t<earliest){earliest=t;next=&b;}
    }
    if(!next) {paddleX_+=clamp(balls_[0].x-paddleX_,-360,360);}
    else {
      const int landing=landingX(*next);
      const int dx=(brickX(target_)+BrickWidth/2)*Unit-landing;
      const int dy=(PaddleY-brickY(target_))*Unit;
      const int offset=clamp(dx*HalfPaddle*Unit/(dy+absolute(dx)),-11*Unit,11*Unit);
      const int target=landing-offset;
      paddleX_+=clamp(target-paddleX_,-1152,1152);
    }
    paddleX_=clamp(paddleX_,(Left+HalfPaddle)*Unit,(Right-HalfPaddle)*Unit);
  }
  void paddleBounce(Ball& ball) {
    const int relative=clamp(ball.x-paddleX_,-HalfPaddle*Unit,HalfPaddle*Unit);
    ball.vx=int16_t(relative*Speed*4/(HalfPaddle*Unit*5));
    if(absolute(ball.vx)<95)ball.vx=int16_t(ball.vx<0?-95:95);
    ball.vy=int16_t(-int(root(uint32_t(Speed*Speed-ball.vx*ball.vx))));
    ball.y=(PaddleY-2)*Unit-1;++paddleBounces_;chooseTarget();
  }
  void moveBall(Ball& ball) {
    const int fastest=absolute(ball.vx)>absolute(ball.vy)?absolute(ball.vx):absolute(ball.vy);
    const int steps=(fastest+127)/128+1;
    for(int sub=0;sub<steps;++sub) {
      const int oldX=ball.x,oldY=ball.y;
      ball.x+=ball.vx/steps;ball.y+=ball.vy/steps;
      if(ball.x<(Left+2)*Unit){ball.x=(Left+2)*Unit;ball.vx=int16_t(absolute(ball.vx));}
      else if(ball.x>(Right-2)*Unit){ball.x=(Right-2)*Unit;ball.vx=int16_t(-absolute(ball.vx));}
      if(ball.y<(Ceiling+2)*Unit){ball.y=(Ceiling+2)*Unit;ball.vy=int16_t(absolute(ball.vy));}
      if(ball.vy>0&&oldY<=(PaddleY-2)*Unit&&ball.y>=(PaddleY-2)*Unit) {
        // The clock has a permanent safety rail. The paddle still performs the
        // ordinary offset bounce whenever it reaches a ball's landing point.
        if(absolute(ball.x-paddleX_)<=HalfPaddle*Unit+2*Unit)paddleBounce(ball);
        else {ball.y=(PaddleY-2)*Unit-1;ball.vy=int16_t(-absolute(ball.vy));}
      }
      collideBrick(ball,oldX,oldY);
    }
  }
  void collideBrick(Ball& ball,int oldX,int oldY) {
    if(ball.y<(BrickTop-2)*Unit||ball.y>(BrickTop+Rows*PitchY+2)*Unit)return;
    // Each step travels under half a pixel, below the narrowest brick gap.
    const int row=clamp((ball.y/Unit-BrickTop)/PitchY,0,Rows-1);
    const int col=clamp((ball.x/Unit-BrickLeft)/PitchX,0,Columns-1);
    for(int r=clamp(row-1,0,Rows-1);r<=clamp(row+1,0,Rows-1);++r)
      for(int c=clamp(col-1,0,Columns-1);c<=clamp(col+1,0,Columns-1);++c) {
        const unsigned i=unsigned(r*Columns+c);Brick& brick=bricks_[i];if(!brick.hits)continue;
        const int left=(brickX(i)-2)*Unit,right=left+(BrickWidth+4)*Unit;
        const int top=(brickY(i)-2)*Unit,bottom=top+(BrickHeight+4)*Unit;
        if(ball.x<=left||ball.x>=right||ball.y<=top||ball.y>=bottom)continue;
        if(oldY<=top){ball.y=top;ball.vy=int16_t(-absolute(ball.vy));}
        else if(oldY>=bottom){ball.y=bottom;ball.vy=int16_t(absolute(ball.vy));}
        else if(oldX<=left){ball.x=left;ball.vx=int16_t(-absolute(ball.vx));}
        else if(oldX>=right){ball.x=right;ball.vx=int16_t(absolute(ball.vx));}
        else {
          const int xd=ball.vx>0?ball.x-left:right-ball.x,yd=ball.vy>0?ball.y-top:bottom-ball.y;
          if(xd<yd){ball.x=ball.vx>0?left:right;ball.vx=int16_t(-ball.vx);}
          else{ball.y=ball.vy>0?top:bottom;ball.vy=int16_t(-ball.vy);}
        }
        --brick.hits;brick.flash=7;++contacts_;
        if(!brick.hits){--remaining_;++destroyed_;if(i==target_)chooseTarget();}
        return;
      }
  }
  void drawBrick(ArcadeCanvas& c,unsigned i,int y) const {
    static const uint8_t colors[6][3]={{245,102,128},{252,173,85},{238,219,116},
      {111,216,154},{90,180,240},{177,137,232}};
    const Brick& brick=bricks_[i];const uint8_t* rgb=colors[(brick.color&127)%6];
    const bool ink=(brick.color&128)!=0;
    const unsigned divisor=ink?1:3;
    uint16_t face=c.rgb(rgb[0]/divisor,rgb[1]/divisor,rgb[2]/divisor);
    if(brick.flash)face=c.rgb(244,249,255);
    const int x=brickX(i);
    c.rect(x,y,BrickWidth,BrickHeight,c.rgb(21,32,47));
    c.rect(x,y,BrickWidth-1,BrickHeight-1,face);
    c.line(x,y,x+BrickWidth-2,y,ink?c.rgb(248,248,227):c.rgb(76,93,108));
    c.line(x,y,x,y+BrickHeight-2,ink?c.rgb(231,239,231):c.rgb(71,87,101));
    if(brick.hits>1)for(unsigned mark=0;mark<brick.hits;++mark)
      c.pixel(x+3+int(mark)*2,y+3,c.rgb(53,80,105));
    else if(ink&&brick.strength>1)c.pixel(x+5,y+3,c.rgb(222,232,242));
  }
};

}  // namespace flappy
