#include "../firmware/flappy/jetpack_missions.h"

#include <assert.h>
#include <cstring>
#include <iostream>
#include <type_traits>

namespace M = flappy::JetpackMissions;

static M::Tracker withMission(uint8_t id) {
  M::Tracker tracker;
  auto saved = tracker.state();
  for (uint8_t i = 0; i < 3; ++i) saved.slots[i].id = uint8_t((id + i) % M::MissionCount);
  saved.next = uint8_t((id + 3) % M::MissionCount);
  tracker.restore(saved);
  tracker.beginRun();
  return tracker;
}

static void threeSlotsRunBoundariesAndRankCoins() {
  M::Tracker tracker;
  tracker.beginRun();
  M::Stats stats;
  stats.coinsCollected = 100; stats.meters = 700; stats.vehiclesCollected = 1;
  tracker.observe(stats);
  assert(tracker.progress(0) == 100 && tracker.progress(1) == 700 && tracker.progress(2) == 1);
  tracker.clearDirty();
  tracker.observe(stats);
  assert(!tracker.dirty());
  tracker.beginRun();
  assert(tracker.progress(0) == 100 && tracker.progress(1) == 0 && tracker.progress(2) == 1);
  stats = M::Stats{};
  stats.coinsCollected = 60; stats.meters = 400; stats.vehiclesCollected = 1;
  tracker.observe(stats);
  assert(tracker.complete(0) && tracker.complete(2) && !tracker.complete(1));
  assert(tracker.stars() == 2 && tracker.rank() == 1);
  tracker.observe(stats);
  assert(tracker.stars() == 2);
  stats.meters = 1000;
  tracker.observe(stats);
  assert(tracker.complete(1) && tracker.rank() == 2 && tracker.stars() == 1);
  assert(tracker.takeCoinReward() == 400 && tracker.takeCoinReward() == 0);
  assert(tracker.missionId(0) == 0 && tracker.missionId(1) == 1 && tracker.missionId(2) == 2);
  tracker.beginRun();
  assert(tracker.missionId(0) == 3 && tracker.missionId(1) == 4 && tracker.missionId(2) == 5);
  stats = M::Stats{}; stats.highFives = 10; stats.tokensCollected = 2; stats.footMeters = 300;
  tracker.observe(stats);
  assert(tracker.complete(0) && tracker.complete(1) && tracker.complete(2));
  assert(tracker.takeCoinReward() == 800 && tracker.rank() == 3 && tracker.stars() == 2);
}

static void sourceCriteriaAndCompactLabels() {
  const uint16_t targets[] = {150,1000,2,10,2,300,3,3,5,1,200,500,3000,750,5,30,5};
  const uint8_t stars[] = {1,2,1,1,1,2,1,1,1,2,1,2,1,2,1,1,3};
  for (uint8_t id = 0; id < M::MissionCount; ++id) {
    auto tracker = withMission(id);
    assert(tracker.target(0) == targets[id] && tracker.difficulty(0) == stars[id]);
    assert(std::strlen(M::label(id)) <= 21 && std::strlen(tracker.label(0)) > 5);
    M::Stats stats;
    switch (id) {
      case 0: case 10: stats.coinsCollected = targets[id]; break;
      case 1: case 11: case 12: stats.meters = targets[id]; break;
      case 2: stats.vehiclesCollected = 2; break;
      case 3: stats.highFives = 10; break;
      case 4: case 14: stats.tokensCollected = targets[id]; break;
      case 5: stats.footMeters = 300; break;
      case 6: stats.mechsCollected = 3; break;
      case 7: stats.cashInTokens = 3; break;
      case 8:
        for (unsigned run = 1; run < 5; ++run) tracker.beginRun();
        break;
      case 9: tracker.recordPurchase(); break;
      case 13: stats.hogMeters = 750; break;
      case 15: stats.gravityFlips = 30; break;
      case 16: stats.prizesWon = 5; break;
    }
    tracker.observe(stats);
    assert(tracker.complete(0) && tracker.progress(0) == targets[id]);
  }
  assert(M::definition(M::MissionCount).target == 0 && M::label(255)[0] == '\0');
}

static void totalDeltasCoinExclusionsAndNoVehicleRule() {
  auto coins = withMission(0);
  M::Stats stats;
  stats.cashInTokens = 10; stats.prizesWon = 3;
  coins.observe(stats);
  assert(coins.progress(0) == 0);
  stats.coinsCollected = 100;
  coins.observe(stats);
  stats.coinsCollected = 50;
  coins.observe(stats);
  stats.coinsCollected = 100;
  coins.observe(stats);
  assert(coins.progress(0) == 100);
  stats.coinsCollected = 150;
  coins.observe(stats);
  assert(coins.complete(0));

  auto avoid = withMission(11);
  stats = M::Stats{}; stats.meters = 400;
  avoid.observe(stats);
  assert(avoid.progress(0) == 400);
  stats.vehiclesCollected = 1; stats.meters = 600;
  avoid.observe(stats);
  assert(avoid.progress(0) == 0 && !avoid.complete(0));
  avoid.beginRun();
  stats = M::Stats{}; stats.meters = 500;
  avoid.observe(stats);
  assert(avoid.complete(0));
  stats.vehiclesCollected = 1;
  avoid.observe(stats);
  assert(avoid.complete(0));
}

static void diagnosticsAndRestoreSanitization() {
  auto tracker = withMission(0);
  M::Stats stats; stats.coinsCollected = 100;
  tracker.observe(stats);
  const auto saved = tracker.state();
  tracker.clearDirty();
  tracker.beginRun(false);
  stats.coinsCollected = UINT32_MAX; stats.meters = UINT32_MAX; stats.vehiclesCollected = UINT32_MAX;
  tracker.observe(stats); tracker.recordPurchase(false);
  assert(tracker.progress(0) == 100 && !tracker.dirty());
  tracker.restore(saved);
  assert(tracker.progress(0) == 100 && tracker.progress(1) == 0);
  tracker.beginRun(); stats = M::Stats{}; stats.coinsCollected = 50;
  tracker.observe(stats);
  assert(tracker.complete(0));

  auto purchase = withMission(9);
  const auto purchaseSaved = purchase.state();
  purchase.restore(purchaseSaved);
  purchase.recordPurchase();
  assert(purchase.complete(0));

  auto corrupt = saved;
  corrupt.next = 255; corrupt.completedRanks = 255; corrupt.stars = UINT16_MAX;
  for (auto& slot : corrupt.slots) { slot.id = 255; slot.done = 255; slot.progress = UINT32_MAX; }
  tracker.restore(corrupt);
  assert(tracker.rankComplete() && tracker.stars() == 0);
  for (uint8_t i = 0; i < 3; ++i) {
    assert(tracker.missionId(i) < M::MissionCount && tracker.progress(i) <= tracker.target(i));
    for (uint8_t j = 0; j < i; ++j) assert(tracker.missionId(i) != tracker.missionId(j));
  }
  corrupt.version = 255;
  tracker.restore(corrupt);
  assert(tracker.rank() == 1 && tracker.progress(0) == 0 && tracker.missionId(0) == 0);
  assert(!tracker.complete(3) && tracker.progress(3) == 0 && tracker.target(3) == 0);
  assert(tracker.label(3)[0] == '\0');
  static_assert(std::is_trivially_copyable<M::State>::value, "Mission state must be persistable");
}

static void allRanksPayOnceAndSequenceWraps() {
  M::Tracker tracker;
  unsigned rewards = 0;
  for (unsigned run = 0; run < 200; ++run) {
    tracker.beginRun();
    M::Stats stats;
    stats.meters = stats.coinsCollected = stats.highFives = stats.tokensCollected = UINT16_MAX;
    stats.footMeters = stats.hogMeters = stats.gravityFlips = stats.cashInTokens = stats.prizesWon = UINT16_MAX;
    stats.mechsCollected = 3;
    // The vehicle-free objective needs a run without a vehicle pickup.
    stats.vehiclesCollected = (run % 2) ? 0 : 2;
    tracker.observe(stats); tracker.recordPurchase();
    rewards += tracker.takeCoinReward();
    for (uint8_t i = 0; i < 3; ++i)
      for (uint8_t j = 0; j < i; ++j) assert(tracker.missionId(i) != tracker.missionId(j));
  }
  assert(tracker.rankComplete() && tracker.rank() == 15 && tracker.starTarget() == 0);
  assert(rewards == 48000 && tracker.takeCoinReward() == 0);
  assert(tracker.state().totalStars > 117);
}

int main() {
  threeSlotsRunBoundariesAndRankCoins();
  sourceCriteriaAndCompactLabels();
  totalDeltasCoinExclusionsAndNoVehicleRule();
  diagnosticsAndRestoreSanitization();
  allRanksPayOnceAndSequenceWraps();
  std::cout << "Jetpack mission tests passed\n";
}
