#pragma once
#include "hockey_state.h"
#include <stddef.h>

namespace flappy { namespace hockey {
constexpr uint32_t PacketIntervalMs = 33;
constexpr uint32_t StaleMs = 250;
enum class Part : uint8_t { Input, Motion, Board };
struct Input { uint16_t sequence=0; int16_t move=0; bool finished=false, viewReady=true; };
inline bool valid(const Input& input) { return input.move>=-1024 && input.move<=1024; }
inline void write16(uint8_t* p,uint16_t v) { p[0]=uint8_t(v); p[1]=uint8_t(v>>8); }
inline uint16_t read16(const uint8_t* p) { return uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8); }
inline size_t payloadSize(Part part) { return part==Part::Input ? 5 : 11; }

inline bool encodePayload(Part part,const Frame& f,const Input& in,uint8_t* b) {
  if(!b || (part==Part::Input ? !valid(in) : !valid(f))) return false;
  if(part==Part::Input) {
    write16(b,in.sequence); write16(b+2,uint16_t(in.move)); b[4]=uint8_t((in.finished ? 1 : 0)|(in.viewReady ? 2 : 0));
  } else if(part==Part::Motion) {
    write16(b,f.tick);
    for(unsigned i=0;i<3;++i) { b[2+i*2]=f.x[i]; b[3+i*2]=f.y[i]; }
    b[8]=f.paddles[0]; b[9]=f.paddles[1]; b[10]=uint8_t(f.puckMask|(uint8_t(f.phase)<<3)|(f.stunMask<<6));
  } else {
    write16(b,f.tick); b[2]=uint8_t(f.scores[0]|(f.scores[1]<<4));
    b[3]=uint8_t(f.map|(f.palette<<3)|(uint8_t(f.rules)<<5));
    b[4]=uint8_t(uint8_t(f.powers[0])|(uint8_t(f.powers[1])<<4));
    b[5]=f.remaining[0]; b[6]=f.remaining[1]; b[7]=uint8_t(uint8_t(f.pickup)|(f.pickupOwner<<4));
    b[8]=f.pickupX; b[9]=f.pickupLife; b[10]=f.event;
  }
  return true;
}
inline bool decodePayload(Part part,const uint8_t* b,size_t n,Frame& f,Input& in) {
  if(!b || n!=payloadSize(part)) return false;
  f=Frame(); in=Input();
  if(part==Part::Input) {
    in.sequence=read16(b); const uint16_t raw=read16(b+2);
    in.move=raw<=INT16_MAX ? int16_t(raw) : int16_t(-1-int32_t(UINT16_MAX-raw));
    if(b[4]>3) return false;
    in.finished=(b[4]&1)!=0; in.viewReady=(b[4]&2)!=0; return valid(in);
  }
  f.tick=read16(b);
  if(part==Part::Motion) {
    for(unsigned i=0;i<3;++i) { f.x[i]=b[2+i*2]; f.y[i]=b[3+i*2]; }
    f.paddles[0]=b[8]; f.paddles[1]=b[9]; f.puckMask=b[10]&7; f.phase=Phase((b[10]>>3)&7); f.stunMask=b[10]>>6;
  } else {
    if((b[3]&128) || (b[7]&224)) return false;
    f.scores[0]=b[2]&15; f.scores[1]=b[2]>>4; f.map=b[3]&7; f.palette=(b[3]>>3)&3; f.rules=Powers((b[3]>>5)&3);
    f.powers[0]=Power(b[4]&15); f.powers[1]=Power(b[4]>>4); f.remaining[0]=b[5]; f.remaining[1]=b[6];
    f.pickup=Power(b[7]&15); f.pickupOwner=(b[7]>>4)&1; f.pickupX=b[8]; f.pickupLife=b[9]; f.event=b[10];
  }
  return valid(f);
}

// A frame is committed only after matching motion and board packets arrive.
class Channel {
 public:
  void reset() { *this=Channel(); }
  void setLocal(const Frame& frame) { if(valid(frame)) local_=frame; }
  void setInput(int16_t move,bool finished,bool viewReady=true) { input_.move=move < -1024 ? -1024 : move>1024 ? 1024 : move; input_.finished=finished; input_.viewReady=viewReady; }
  bool next(bool host,uint32_t now,Part& part,Frame& frame,Input& input) {
    if(host && boardPending_) { part=Part::Board; frame=outgoing_; boardPending_=false; return true; }
    if(sent_ && uint32_t(now-sentMs_)<PacketIntervalMs) return false;
    sent_=true; sentMs_=now;
    if(host) { part=Part::Motion; frame=outgoing_=local_; boardPending_=true; }
    else { part=Part::Input; input=input_; input.sequence=++inputSequence_; }
    return true;
  }
  bool receiveInput(const Input& input,uint32_t now) {
    if(!valid(input) || (haveInput_ && !newer(input.sequence,remoteInput_.sequence))) return false;
    remoteInput_=input; inputMs_=now; haveInput_=true; return true;
  }
  bool receiveFrame(Part part,const Frame& frame,uint32_t now) {
    if(part==Part::Input || !valid(frame) || (haveCurrent_ && !newer(frame.tick,current_.tick))) return false;
    if(!parts_ || newer(frame.tick,pending_.tick)) { pending_=Frame(); pending_.tick=frame.tick; parts_=0; }
    if(frame.tick!=pending_.tick) return false;
    if(part==Part::Motion) {
      for(unsigned i=0;i<3;++i) { pending_.x[i]=frame.x[i]; pending_.y[i]=frame.y[i]; }
      for(unsigned i=0;i<2;++i) pending_.paddles[i]=frame.paddles[i];
      pending_.puckMask=frame.puckMask; pending_.phase=frame.phase; pending_.stunMask=frame.stunMask; parts_|=1;
    } else {
      for(unsigned i=0;i<2;++i) { pending_.scores[i]=frame.scores[i]; pending_.powers[i]=frame.powers[i]; pending_.remaining[i]=frame.remaining[i]; }
      pending_.map=frame.map; pending_.palette=frame.palette; pending_.rules=frame.rules;
      pending_.pickup=frame.pickup; pending_.pickupOwner=frame.pickupOwner; pending_.pickupX=frame.pickupX; pending_.pickupLife=frame.pickupLife;
      pending_.event=frame.event; parts_|=2;
    }
    if(parts_==3) {
      if(!valid(pending_)) { parts_=0; return false; }
      previous_=current_; previousMs_=currentMs_; havePrevious_=haveCurrent_;
      current_=pending_; currentMs_=now; haveCurrent_=true; parts_=0;
    }
    return true;
  }
  bool inputFresh(uint32_t now) const { return haveInput_ && remoteInput_.viewReady && uint32_t(now-inputMs_)<StaleMs; }
  int16_t move(uint32_t now) const { return inputFresh(now) ? remoteInput_.move : 0; }
  bool finished(bool host) const { return host ? local_.phase==Phase::Finished : haveCurrent_ && current_.phase==Phase::Finished; }
  bool peerFinished(bool host) const { return host ? haveInput_ && remoteInput_.finished : finished(false); }
  bool received() const { return haveCurrent_; }
  bool fresh(uint32_t now) const { return haveCurrent_ && uint32_t(now-currentMs_)<StaleMs; }
  const Frame& latest() const { return current_; }
  bool display(uint32_t now,Frame& result) const {
    if(!haveCurrent_) return false;
    result=current_;
    if(!fresh(now)) { result.phase=Phase::LinkWait; return true; }
    const uint32_t span=uint32_t(currentMs_-previousMs_);
    if(!havePrevious_ || !span || span>100 || current_.phase!=previous_.phase || current_.phase!=Phase::Rally) return true;
    const uint32_t age=uint32_t(now-currentMs_);
    const float alpha=age>=PacketIntervalMs ? 1.f : float(span+age>PacketIntervalMs ? span+age-PacketIntervalMs : 0)/span;
    for(unsigned i=0;i<3;++i) if((current_.puckMask&previous_.puckMask)&(1<<i)) {
      const int dx=int(current_.x[i])-previous_.x[i],dy=int(current_.y[i])-previous_.y[i];
      if(dx*dx+dy*dy<1600) { result.x[i]=mix(previous_.x[i],current_.x[i],alpha); result.y[i]=mix(previous_.y[i],current_.y[i],alpha); }
    }
    for(unsigned i=0;i<2;++i) result.paddles[i]=mix(previous_.paddles[i],current_.paddles[i],alpha);
    return true;
  }
 private:
  Frame local_,outgoing_,pending_,previous_,current_;
  Input input_,remoteInput_;
  uint32_t sentMs_=0,inputMs_=0,currentMs_=0,previousMs_=0;
  uint16_t inputSequence_=0;
  uint8_t parts_=0;
  bool boardPending_=false,sent_=false,haveInput_=false,haveCurrent_=false,havePrevious_=false;
  static uint8_t mix(uint8_t a,uint8_t b,float amount) { return uint8_t(float(a)+(int(b)-int(a))*amount+.5f); }
};
} }
