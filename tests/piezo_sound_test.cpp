#include "../firmware/flappy/piezo_sound.h"

#include <assert.h>
#include <iostream>

using flappy::PiezoCue;
using flappy::PiezoSound;
using flappy::PiezoTone;

static void patternsAndPriority() {
  PiezoSound sound;
  PiezoTone tone;
  assert(!sound.update(0, tone));
  assert(sound.play(PiezoCue::Flap, 100));
  assert(sound.update(100, tone) && tone.frequency == 1900 && tone.durationMs == 24);
  assert(tone.volume == 102);
  assert(!sound.update(123, tone));
  assert(sound.update(124, tone) && tone.frequency == 2700 && tone.durationMs == 30);
  assert(!sound.play(PiezoCue::Move, 125));
  assert(sound.play(PiezoCue::Crash, 126));
  assert(!sound.play(PiezoCue(255), 127));
  assert(sound.update(126, tone) && tone.frequency == 1700);
  sound.setVolumeIndex(5);
  assert(sound.update(127, tone) && tone.frequency == 1700 && tone.volume == 255);
  assert(!sound.play(PiezoCue::Point, 150));
  assert(sound.update(192, tone) && tone.frequency == 1100 && tone.durationMs == 99);
  assert(sound.update(292, tone) && tone.frequency == 700);
  assert(sound.update(411, tone) && tone.frequency == 0);
  assert(!sound.update(412, tone));
  assert(!sound.active());
  assert(sound.play(PiezoCue::Move, 420));
  assert(!sound.play(PiezoCue::Move, 421));
  assert(sound.update(420, tone) && tone.frequency == 1700);
  assert(sound.update(434, tone) && !tone.frequency);
}

static void volumeQuietAndWrap() {
  PiezoSound sound;
  PiezoTone tone;
  for (unsigned i = 0; i < 6; ++i) {
    sound.setVolumeIndex(i);
    assert(PiezoSound::volume(i) == i * 51);
    assert(sound.play(PiezoCue::Preview, 1000 + i * 1000) == (i != 0));
    if (i) {
      assert(sound.update(1000 + i * 1000, tone));
      assert(tone.frequency == 2000 && tone.volume == i * 51);
    }
  }
  sound.setVolumeIndex(255);
  assert(sound.volumeIndex() == 5);
  sound.setQuiet(true);
  assert(sound.update(7000, tone) && !tone.frequency);
  assert(!sound.play(PiezoCue::Start, 7001));
  sound.setNesTone(2500, 15, 7001);
  assert(!sound.update(7002, tone));
  sound.setQuiet(false);
  assert(!sound.update(7003, tone));
  assert(sound.play(PiezoCue::Preview, UINT32_MAX - 30));
  assert(sound.update(UINT32_MAX - 30, tone) && tone.frequency == 2000);
  assert(sound.update(39, tone) && !tone.frequency && tone.durationMs == 25);
  assert(sound.update(64, tone) && tone.frequency == 3000 && tone.durationMs == 100);
  assert(sound.update(164, tone) && !tone.frequency);
  assert(sound.play(PiezoCue::Start, 200));
  assert(sound.update(200, tone) && tone.frequency);
  sound.setVolumeIndex(0);
  assert(sound.update(201, tone) && !tone.frequency);
  assert(!sound.play(PiezoCue::Coin, 202));
}

static void lateUpdatesAndAllCues() {
  PiezoSound sound;
  PiezoTone tone;
  for (unsigned i = 0; i <= unsigned(PiezoCue::Start); ++i) {
    sound.stop();
    sound.update(0, tone);
    assert(sound.play(PiezoCue(i), 100));
    assert(sound.update(100, tone));
    assert(tone.frequency >= 700 && tone.frequency <= 8000 && tone.durationMs <= 180);
    assert(sound.update(1100, tone) && !tone.frequency);
    assert(!sound.active());
  }
  assert(sound.play(PiezoCue::Flap, 2000));
  assert(!sound.update(3000, tone));
}

static void nesToneFollow() {
  PiezoSound sound;
  PiezoTone tone;
  assert(PiezoSound::nesPulseFrequency(1789773, 111) == 999);
  assert(!PiezoSound::nesPulseFrequency(1789773, 7));
  assert(!PiezoSound::nesPulseFrequency(1789773, 0x800));
  assert(!PiezoSound::nesPulseFrequency(0, 111));
  sound.setNesTone(2000, 15, 100);
  assert(sound.update(100, tone) && tone.frequency == 2000 && tone.volume == 102);
  assert(!sound.update(120, tone));
  sound.setNesTone(2000, 15, 125);
  assert(sound.update(130, tone) && tone.frequency == 2000);
  assert(sound.play(PiezoCue::Rotate, 135));
  sound.setNesTone(3000, 8, 140);
  assert(sound.update(140, tone) && tone.frequency == 2900);
  assert(sound.update(157, tone) && tone.frequency == 3000 && tone.volume == 55);
  assert(sound.update(221, tone) && !tone.frequency);
  sound.setNesTone(9000, 15, 230);
  assert(!sound.update(230, tone));
  sound.setNesTone(2500, 0, 231);
  assert(!sound.update(231, tone));
  sound.setNesTone(2500, 15, UINT32_MAX - 5);
  assert(sound.update(UINT32_MAX - 5, tone) && tone.frequency == 2500);
  assert(sound.update(75, tone) && !tone.frequency);
}

int main() {
  static_assert(sizeof(PiezoSound) <= 80, "Piezo scheduler must remain bounded");
  patternsAndPriority();
  volumeQuietAndWrap();
  lateUpdatesAndAllCues();
  nesToneFollow();
  std::cout << "Piezo patterns, priority, volume, silence and NES pulse output passed\n";
}
