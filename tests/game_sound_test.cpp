#include "../firmware/flappy/piezo_sound.h"
#include <assert.h>
#include <set>
#include <vector>
#include <iostream>
using namespace flappy;
int main() {
  for(unsigned voice=0;voice<4;++voice) {
    const auto* wave=cueWaveform(CueVoice(voice));unsigned sum=0;
    for(unsigned i=0;i<16;++i)sum+=wave[i];
    assert(sum==2048);
  }
  const PiezoCue cues[]={PiezoCue::Move,PiezoCue::Lock,PiezoCue::Coin,PiezoCue::Crash,PiezoCue::LevelUp};
  std::set<std::vector<unsigned>> signatures;
  for(unsigned theme=0;theme<6;++theme) {
    std::vector<unsigned> signature;
    for(auto cue:cues) {
      PiezoSound sound;sound.setTheme(SoundTheme(theme));
      assert(sound.play(cue,100));PiezoTone tone;
      for(unsigned now=100;now<1000;++now) if(sound.update(now,tone)) {
        assert(tone.frequency<=8000 && tone.durationMs<=1000);
        signature.push_back(tone.frequency);signature.push_back(tone.durationMs);
      }
      assert(!sound.active());
    }
    assert(signatures.insert(signature).second);
    PiezoSound sound;sound.setTheme(SoundTheme(theme));PiezoTone tone;
    assert(sound.play(PiezoCue::MenuMove,0) && sound.update(0,tone) && tone.frequency==2200);
    sound.setVolumeIndex(0);assert(!sound.play(PiezoCue::Coin,100));
  }
  std::cout<<"Game audio: six distinct cue profiles, bounded envelopes, common UI and mute passed\n";
}
