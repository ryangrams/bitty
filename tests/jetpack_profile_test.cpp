#include "../firmware/flappy/jetpack_profile.h"
#include "../firmware/flappy/app_model.h"

#include <array>
#include <assert.h>
#include <cstring>
#include <iostream>

using Profile = flappy::JetpackProfile;
namespace M = flappy::JetpackMissions;

static void sameState(const Profile::State& a, const Profile::State& b) {
  assert(a.coins == b.coins && a.lifetimeCoins == b.lifetimeCoins);
  assert(a.lifetimeMeters == b.lifetimeMeters && a.runs == b.runs && a.samDistance == b.samDistance);
  assert(a.headstarts == b.headstarts && a.doubleRuns == b.doubleRuns && a.sam == b.sam);
  for (unsigned i = 0; i < Profile::UpgradeCount; ++i) assert(a.levels[i] == b.levels[i]);
}

static void sameMissions(const M::State& a, const M::State& b) {
  assert(a.version == b.version && a.next == b.next && a.completedRanks == b.completedRanks);
  assert(a.stars == b.stars && a.totalStars == b.totalStars && a.pendingCoins == b.pendingCoins);
  for (unsigned i = 0; i < M::ActiveCount; ++i) {
    assert(a.slots[i].id == b.slots[i].id && a.slots[i].done == b.slots[i].done);
    assert(a.slots[i].progress == b.slots[i].progress);
  }
}

static void codecRoundtripCorruptionAndNull() {
  Profile profile;
  Profile::State saved;
  saved.coins = 12345678; saved.lifetimeCoins = 0x76543210;
  saved.lifetimeMeters = 0x89abcdef; saved.runs = 0x13579bdf;
  saved.samDistance = 0x10203040; saved.headstarts = 1234; saved.doubleRuns = 5678; saved.sam = 5;
  saved.levels[0] = 3; saved.levels[1] = 4; saved.levels[2] = 2; saved.levels[3] = 9;
  profile.restore(saved); profile.beginRun(true);
  M::Stats stats; stats.coinsCollected = 100; stats.vehiclesCollected = 1;
  profile.observeRunStats(stats);
  std::array<uint8_t, Profile::StorageSize + 2> guarded;
  guarded.fill(0xa5);
  profile.encode(guarded.data() + 1);
  assert(Profile::StorageSize == 128 && guarded.front() == 0xa5 && guarded.back() == 0xa5);
  const auto* bytes = guarded.data() + 1;
  assert(bytes[0] == 'M' && bytes[1] == '5' && bytes[2] == 'J' && bytes[3] == 'P');
  Profile restored;
  assert(restored.decode(bytes, Profile::StorageSize));
  sameState(profile.state(), restored.state());
  sameMissions(profile.missions().state(), restored.missions().state());
  assert(!restored.active());
  std::array<uint8_t, Profile::StorageSize> again;
  restored.encode(again.data());
  assert(std::memcmp(bytes, again.data(), again.size()) == 0);

  Profile destination;
  Profile::State unchanged; unchanged.coins = 321; destination.restore(unchanged);
  const auto missionBefore = destination.missions().state();
  for (size_t offset = 0; offset < Profile::StorageSize; ++offset) {
    auto corrupt = again;
    corrupt[offset] ^= 1;
    assert(!destination.decode(corrupt.data(), corrupt.size()));
    sameState(unchanged, destination.state());
    sameMissions(missionBefore, destination.missions().state());
  }
  assert(!destination.decode(bytes, 127) && !destination.decode(bytes, 129));
  assert(!destination.decode(nullptr, 0) && !destination.decode(nullptr, Profile::StorageSize));
  destination.encode(nullptr);
}

static void checkpointsSettlementAndSaturation() {
  Profile profile;
  profile.beginRun(true);
  profile.checkpointCoins(100); profile.checkpointCoins(100); profile.checkpointCoins(50);
  assert(profile.coins() == 100 && profile.state().lifetimeCoins == 100);
  profile.checkpointCoins(130);
  assert(profile.coins() == 130 && profile.state().lifetimeCoins == 130);
  profile.settleRun(150, 1234);
  assert(profile.coins() == 150 && profile.state().lifetimeCoins == 150);
  assert(profile.state().runs == 1 && profile.state().lifetimeMeters == 1234 && !profile.active());
  profile.settleRun(150, 1234); profile.checkpointCoins(999);
  assert(profile.coins() == 150 && profile.state().runs == 1);
  profile.beginRun(true); profile.settleRun(10, 100);
  assert(profile.coins() == 160 && profile.state().runs == 2 && profile.state().lifetimeMeters == 1334);

  Profile::State large;
  large.coins = 999999998; large.lifetimeCoins = UINT32_MAX - 1;
  large.lifetimeMeters = UINT32_MAX - 2; large.runs = UINT32_MAX;
  profile.restore(large); profile.beginRun(true); profile.settleRun(100, 100);
  assert(profile.coins() == 999999999 && profile.state().lifetimeCoins == UINT32_MAX);
  assert(profile.state().lifetimeMeters == UINT32_MAX && profile.state().runs == UINT32_MAX);
  large.coins = UINT32_MAX; large.sam = 0xff;
  for (auto& level : large.levels) level = 255;
  profile.restore(large);
  assert(profile.coins() == 999999999 && profile.samLetters() == 7);
  for (unsigned i = 0; i < Profile::UpgradeCount; ++i) assert(profile.upgradeLevel(i) == Profile::maxLevel(i));
}

static void practiceGatesRunRewards() {
  Profile profile;
  Profile::State saved; saved.coins = 100000; saved.headstarts = 2; saved.doubleRuns = 3;
  saved.sam = 3; saved.samDistance = 777;
  profile.restore(saved); profile.clearDirty();
  const auto missions = profile.missions().state();
  profile.beginRun(false);
  profile.checkpointCoins(1000); profile.setSamLetters(7); profile.setSamCompensation(9999);
  profile.grantHeadstart(); profile.grantDoubleCoins();
  assert(!profile.takeHeadstart() && !profile.takeDoubleCoins());
  M::Stats stats; stats.coinsCollected = 150; stats.meters = 1000; stats.vehiclesCollected = 2;
  profile.observeRunStats(stats); profile.settleRun(1000, 1000);
  sameState(saved, profile.state()); sameMissions(missions, profile.missions().state());
  assert(!profile.dirty() && !profile.active());
  // Explicit purchases remain available from the menu between runs.
  assert(profile.buyUpgrade(Profile::Boost));
  assert(profile.coins() == 97000 && profile.upgradeLevel(Profile::Boost) == 1);
}

static void reachRankFive(Profile& profile) {
  for (unsigned run = 0; run < 20 && profile.missions().rank() < 5; ++run) {
    profile.beginRun(true);
    M::Stats stats;
    stats.coinsCollected = 2000; stats.meters = 3000;
    stats.vehiclesCollected = run % 2 ? 0 : 3;
    stats.mechsCollected = 3; stats.highFives = 10; stats.tokensCollected = 5;
    stats.footMeters = 300; stats.hogMeters = 750; stats.gravityFlips = 30;
    stats.cashInTokens = 3; stats.prizesWon = 5;
    profile.observeRunStats(stats);
    for (uint8_t slot = 0; slot < M::ActiveCount; ++slot)
      if (profile.missions().missionId(slot) == 9 && !profile.missions().complete(slot))
        assert(profile.buyUpgrade(Profile::Boost));
    profile.settleRun(0, stats.meters);
  }
  assert(profile.missions().rank() >= 5);
}

static void upgradePricesLimitsAndEffects() {
  Profile profile;
  assert(profile.upgradeCost(Profile::Shield) == 45000);
  assert(profile.upgradeCost(Profile::Boost) == 3000);
  assert(profile.upgradeCost(Profile::CashDash) == 12500);
  assert(profile.upgradeCost(Profile::RocketTime) == 3500);
  assert(!profile.buyUpgrade(Profile::Shield));
  assert(Profile::cost(Profile::Shield, 0) == 7500 && Profile::cost(Profile::Shield, 4) == 1200000);
  assert(Profile::cost(Profile::Boost, 4) == 425000 && Profile::cost(Profile::CashDash, 4) == 650000);
  assert(Profile::cost(Profile::RocketTime, 9) == 550000);
  Profile::State saved; saved.coins = 10000000; profile.restore(saved);
  assert(!profile.upgradeUnlocked(Profile::RocketTime));
  assert(!profile.buyUpgrade(Profile::RocketTime) && profile.coins() == saved.coins);
  reachRankFive(profile);
  assert(profile.upgradeUnlocked(Profile::RocketTime));
  const uint32_t beforeUpgrades = profile.coins();
  uint32_t spent = 0;
  for (uint8_t kind = 0; kind < Profile::UpgradeCount; ++kind) {
    for (uint8_t level = uint8_t(profile.upgradeLevel(kind) + 1); level <= Profile::maxLevel(kind); ++level) {
      const uint32_t price = profile.upgradeCost(kind);
      assert(price && profile.buyUpgrade(kind)); spent += price;
      assert(profile.upgradeLevel(kind) == level);
    }
    assert(profile.upgradeCost(kind) == 0 && !profile.buyUpgrade(kind));
  }
  assert(profile.coins() == beforeUpgrades - spent);
  assert(!profile.buyUpgrade(255) && profile.upgradeCost(255) == 0 && profile.upgradeLevel(255) == 0);
  assert(Profile::cost(255, 0) == 0 && Profile::cost(Profile::Shield, 255) == 0);
  assert(Profile::shieldHits(0) == 1 && Profile::shieldHits(255) == 5);
  assert(Profile::boostMeters(0) == 100 && Profile::boostMeters(255) == 500);
  assert(Profile::boostNativeSpeed(2) == 3000 && Profile::boostNativeSpeed(255) == 3500);
  assert(Profile::boostEndThreshold(0) == 30 && Profile::boostEndThreshold(255) == 50);
  assert(Profile::cashMeters(0) == 60 && Profile::cashMeters(255) == 150);
  assert(Profile::cashPayload(255) == 700 && Profile::cashGemPercent(255) == 25);
  assert(Profile::rocketTicks(0) == 420 && Profile::rocketTicks(255) == 1770);

  flappy::AppModel menu;
  menu.setCurrentGame(flappy::GameTitle::JetpackJoyride);
  menu.openMenu(flappy::MenuPage::JetpackUpgrades);
  const uint8_t baseline[4] = {};
  menu.setJetpackProgress(1000000, baseline);
  menu.setJetpackRank(1, 0, 3);
  assert(std::strcmp(menu.menuValue(4), "Requires rank 5") == 0);
  menu.setJetpackRank(5, 0, 5);
  assert(std::strcmp(menu.menuValue(4), "Lv1 Buy 3500") == 0);
}

static void inventoryAndSamSurviveReboot() {
  Profile profile;
  assert(!profile.takeHeadstart() && !profile.takeDoubleCoins());
  profile.beginRun(true);
  profile.grantHeadstart(); profile.grantHeadstart(); profile.grantDoubleCoins();
  profile.setSamLetters(0xfd); profile.setSamCompensation(123456);
  std::array<uint8_t, Profile::StorageSize> bytes;
  profile.encode(bytes.data());
  Profile restored;
  assert(restored.decode(bytes.data(), bytes.size()));
  assert(restored.samLetters() == 5 && restored.samCompensation() == 123456);
  assert(restored.state().headstarts == 2 && restored.state().doubleRuns == 1);
  assert(!restored.takeHeadstart());
  restored.beginRun(true);
  assert(restored.takeHeadstart() && restored.takeDoubleCoins() && !restored.takeDoubleCoins());
  assert(restored.state().headstarts == 1);
  restored.settleRun(0, 0);
  assert(!restored.takeHeadstart());
  auto saved = restored.state(); saved.headstarts = saved.doubleRuns = UINT16_MAX;
  restored.restore(saved); restored.beginRun(true);
  restored.grantHeadstart(); restored.grantDoubleCoins();
  assert(restored.state().headstarts == UINT16_MAX && restored.state().doubleRuns == UINT16_MAX);
}

static void missionPayoutOnceAndPurchaseAtReady() {
  Profile profile; profile.beginRun(true);
  M::Stats stats; stats.coinsCollected = 150; stats.meters = 1000; stats.vehiclesCollected = 2;
  profile.observeRunStats(stats); profile.checkpointCoins(150);
  assert(profile.coins() == 550 && profile.missions().rank() == 2);
  assert(profile.state().lifetimeCoins == 150);
  profile.observeRunStats(stats); profile.checkpointCoins(150); profile.settleRun(150, 1000);
  assert(profile.coins() == 550);
  std::array<uint8_t, Profile::StorageSize> bytes; profile.encode(bytes.data());
  Profile restored; assert(restored.decode(bytes.data(), bytes.size()));
  assert(restored.coins() == 550 && restored.missions().complete(0));
  restored.beginRun(true); restored.observeRunStats(M::Stats{});
  assert(restored.coins() == 550);

  Profile shop;
  Profile::State funded; funded.coins = 1000000; shop.restore(funded);
  shop.beginRun(true); shop.observeRunStats(stats); shop.settleRun(0, 1000);
  shop.beginRun(true); stats = M::Stats{};
  stats.highFives = 10; stats.tokensCollected = 2; stats.footMeters = 300;
  shop.observeRunStats(stats); shop.settleRun(0, 300);
  shop.beginRun(true); stats = M::Stats{}; stats.mechsCollected = 3; stats.cashInTokens = 3;
  shop.observeRunStats(stats); shop.settleRun(0, 0);
  shop.beginRun(true); shop.settleRun(0, 0);
  assert(shop.missions().missionId(0) == 9 && !shop.missions().complete(0));
  shop.encode(bytes.data());
  Profile ready; assert(ready.decode(bytes.data(), bytes.size()));
  assert(!ready.active());
  const uint32_t before = ready.coins();
  assert(ready.buyUpgrade(Profile::Boost));
  assert(ready.missions().complete(0) && ready.coins() == before - 3000);
  ready.encode(bytes.data());
  Profile afterPurchase; assert(afterPurchase.decode(bytes.data(), bytes.size()));
  assert(afterPurchase.missions().complete(0) && afterPurchase.upgradeLevel(Profile::Boost) == 1);
}

int main() {
  codecRoundtripCorruptionAndNull();
  checkpointsSettlementAndSaturation();
  practiceGatesRunRewards();
  upgradePricesLimitsAndEffects();
  inventoryAndSamSurviveReboot();
  missionPayoutOnceAndPurchaseAtReady();
  std::cout << "Jetpack profile tests passed\n";
}
