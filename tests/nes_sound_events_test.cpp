#include "../firmware/flappy/nes_sound_events.h"

#include <assert.h>
#include <iostream>

using namespace flappy;

static TetrisSoundSnapshot playingPiece() {
  TetrisSoundSnapshot snapshot;
  snapshot.playing = true;
  snapshot.active = true;
  snapshot.pieceY = 3;
  snapshot.lines = 12;
  snapshot.level = 2;
  return snapshot;
}

static void baselinesAndMenusAreSilent() {
  TetrisSoundTracker tracker;
  TetrisSoundSnapshot snapshot;
  snapshot.lines = 30;
  snapshot.level = 9;
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.lines = 5;
  snapshot.level = 10;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  assert(tracker.sample(snapshot) == NesSoundNone);

  snapshot.playing = false;
  snapshot.active = false;
  ++snapshot.lines;
  ++snapshot.level;
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot = playingPiece();
  snapshot.lines = 100;
  snapshot.level = 18;
  assert(tracker.sample(snapshot) == NesSoundNone);
}

static void locksFollowPieceLifecycle() {
  TetrisSoundTracker tracker;
  auto snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  for (uint8_t y : {uint8_t(4), uint8_t(10), uint8_t(3), uint8_t(19)}) {
    snapshot.pieceY = y;
    assert(tracker.sample(snapshot) == NesSoundNone);
  }
  snapshot.active = false;
  assert(tracker.sample(snapshot) == NesSoundLock);
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = true;
  snapshot.pieceY = 0;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  assert(tracker.sample(snapshot) == NesSoundLock);
}

static void clearsAndLevelsTrackChanges() {
  TetrisSoundTracker tracker;
  auto snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  snapshot.lines += 4;
  assert(tracker.sample(snapshot) == NesSoundLineClear);
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.lines -= 1;
  assert(tracker.sample(snapshot) == NesSoundLineClear);
  snapshot.lines = 0;
  assert(tracker.sample(snapshot) == NesSoundLineClear);
  snapshot.level += 1;
  assert(tracker.sample(snapshot) == NesSoundLevelUp);
  snapshot.level -= 1;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = true;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  ++snapshot.lines;
  ++snapshot.level;
  const uint8_t events = tracker.sample(snapshot);
  assert(events == (NesSoundLineClear | NesSoundLevelUp));
  assert(TetrisSoundTracker::priority(events) == NesSoundLineClear);
}

static void crashesEndTheRound() {
  TetrisSoundTracker tracker;
  auto snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundCrash);
  assert(tracker.sample(snapshot) == NesSoundNone);
  ++snapshot.lines;
  ++snapshot.level;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = true;
  snapshot.toppedOut = false;
  ++snapshot.lines;
  ++snapshot.level;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundCrash);

  tracker.reset();
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = true;
  snapshot.toppedOut = false;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundCrash);

  snapshot.playing = false;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  assert(tracker.sample(snapshot) == NesSoundLock);
}

static void resetStartsAFreshBaseline() {
  TetrisSoundTracker tracker;
  auto snapshot = playingPiece();
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.active = false;
  assert(tracker.sample(snapshot) == NesSoundLock);
  tracker.reset();
  snapshot.active = true;
  snapshot.lines = 50;
  snapshot.level = 20;
  assert(tracker.sample(snapshot) == NesSoundNone);
  snapshot.toppedOut = true;
  assert(tracker.sample(snapshot) == NesSoundCrash);
}

static void prioritySelectsOneSound() {
  assert(TetrisSoundTracker::priority(NesSoundNone) == NesSoundNone);
  for (uint8_t mask = 1; mask < 16; ++mask) {
    const uint8_t expected = mask & NesSoundLineClear ? NesSoundLineClear :
        mask & NesSoundLevelUp ? NesSoundLevelUp :
        mask & NesSoundCrash ? NesSoundCrash : NesSoundLock;
    assert(TetrisSoundTracker::priority(mask) == expected);
  }
}

int main() {
  baselinesAndMenusAreSilent();
  locksFollowPieceLifecycle();
  clearsAndLevelsTrackChanges();
  crashesEndTheRound();
  resetStartsAFreshBaseline();
  prioritySelectsOneSound();
  std::cout << "NES sound baselines, piece events, terminal suppression and priority passed.\n";
}
