#pragma once

#include <stdint.h>
#include <stddef.h>
#include "jetpack_missions.h"

namespace flappy {

// Coins and earned items persist separately from the current run.
class JetpackProfile {
 public:
  enum Upgrade : uint8_t { Shield, Boost, CashDash, RocketTime, UpgradeCount };
  enum { StorageSize = 128 };
  struct State {
    uint32_t coins = 0, lifetimeCoins = 0, lifetimeMeters = 0, runs = 0, samDistance = 0;
    uint16_t headstarts = 0, doubleRuns = 0;
    uint8_t sam = 0, levels[UpgradeCount] = {};
  };
  static uint8_t maxLevel(uint8_t kind) { return kind == RocketTime ? 9 : kind < UpgradeCount ? 4 : 0; }
  static uint32_t cost(uint8_t kind, uint8_t level) {
    static const uint32_t shield[] = {7500,45000,185000,625000,1200000};
    static const uint32_t boost[] = {1500,3000,17500,85000,425000};
    static const uint32_t cash[] = {0,12500,65000,225000,650000};
    static const uint32_t rocket[] = {1000,3500,12000,30000,75000,150000,250000,350000,450000,550000};
    if (kind >= UpgradeCount || level > maxLevel(kind)) return 0;
    return kind == Shield ? shield[level] : kind == Boost ? boost[level] : kind == CashDash ? cash[level] : rocket[level];
  }
  static uint16_t boostMeters(uint8_t level) { return uint16_t(100 * (boundedLevel(Boost, level) + 1)); }
  static uint16_t boostNativeSpeed(uint8_t level) {
    const uint16_t speeds[] = {2000,2000,3000,3500,3500}; return speeds[boundedLevel(Boost, level)];
  }
  static uint16_t boostEndThreshold(uint8_t level) {
    const uint16_t thresholds[] = {30,30,40,50,50}; return thresholds[boundedLevel(Boost, level)];
  }
  static uint16_t cashMeters(uint8_t level) {
    const uint16_t values[] = {60,80,100,120,150}; return values[boundedLevel(CashDash, level)];
  }
  static uint16_t cashPayload(uint8_t level) { return uint16_t(300 + 100 * boundedLevel(CashDash, level)); }
  static uint8_t cashGemPercent(uint8_t level) { return uint8_t(5 + 5 * boundedLevel(CashDash, level)); }
  static uint16_t rocketTicks(uint8_t level) { return uint16_t(420 + 150 * boundedLevel(RocketTime, level)); }
  static uint8_t shieldHits(uint8_t level) { return uint8_t(boundedLevel(Shield, level) + 1); }

  const State& state() const { return state_; }
  void restore(State saved) {
    saved.coins = saved.coins > MaxCoins ? MaxCoins : saved.coins;
    saved.sam &= 7;
    for (uint8_t i = 0; i < UpgradeCount; ++i) saved.levels[i] = boundedLevel(i, saved.levels[i]);
    state_ = saved; active_ = false; creditedCoins_ = 0; dirty_ = false;
  }
  uint32_t coins() const { return state_.coins; }
  uint8_t upgradeLevel(uint8_t kind) const { return kind < UpgradeCount ? state_.levels[kind] : 0; }
  uint32_t upgradeCost(uint8_t kind) const {
    if (kind >= UpgradeCount || state_.levels[kind] >= maxLevel(kind)) return 0;
    return cost(kind, uint8_t(state_.levels[kind] + 1));
  }
  bool upgradeUnlocked(uint8_t kind) const {
    return kind < UpgradeCount && (kind != RocketTime || missions_.rank() >= 5);
  }
  bool buyUpgrade(uint8_t kind) {
    if (!upgradeUnlocked(kind)) return false;
    const uint32_t price = upgradeCost(kind);
    if (!price || state_.coins < price) return false;
    state_.coins -= price; ++state_.levels[kind]; dirty_ = true;
    missions_.recordPurchase(); absorbMissionCoins(); return true;
  }
  void beginRun(bool eligible) {
    active_ = eligible; creditedCoins_ = 0; missions_.beginRun(eligible); absorbMissionCoins();
  }
  void observeRunStats(const JetpackMissions::Stats& stats) {
    if (!active_) return;
    missions_.observe(stats); absorbMissionCoins();
  }
  const JetpackMissions::Tracker& missions() const { return missions_; }
  void checkpointCoins(uint32_t runCoins) {
    if (!active_ || runCoins <= creditedCoins_) return;
    const uint32_t earned = runCoins - creditedCoins_;
    creditedCoins_ = runCoins;
    state_.coins = addLimited(state_.coins, earned, MaxCoins);
    state_.lifetimeCoins = addLimited(state_.lifetimeCoins, earned, UINT32_MAX);
    dirty_ = true;
  }
  void settleRun(uint32_t runCoins, uint32_t meters) {
    if (!active_) return;
    checkpointCoins(runCoins);
    active_ = false;
    state_.lifetimeMeters = addLimited(state_.lifetimeMeters, meters, UINT32_MAX);
    state_.runs = addLimited(state_.runs, 1, UINT32_MAX);
    dirty_ = true;
  }
  uint8_t samLetters() const { return state_.sam; }
  uint32_t samCompensation() const { return state_.samDistance; }
  void setSamCompensation(uint32_t meters) {
    if (active_ && state_.samDistance != meters) { state_.samDistance = meters; dirty_ = true; }
  }
  void setSamLetters(uint8_t bits) {
    bits &= 7;
    if (active_ && bits != state_.sam) { state_.sam = bits; dirty_ = true; }
  }
  bool takeHeadstart() {
    if (!active_ || !state_.headstarts) return false;
    --state_.headstarts; dirty_ = true; return true;
  }
  bool takeDoubleCoins() {
    if (!active_ || !state_.doubleRuns) return false;
    --state_.doubleRuns; dirty_ = true; return true;
  }
  void grantHeadstart() {
    if (active_ && state_.headstarts != UINT16_MAX) { ++state_.headstarts; dirty_ = true; }
  }
  void grantDoubleCoins() {
    if (active_ && state_.doubleRuns != UINT16_MAX) { ++state_.doubleRuns; dirty_ = true; }
  }
  bool dirty() const { return dirty_ || missions_.dirty(); }
  void clearDirty() { dirty_ = false; missions_.clearDirty(); }
  bool active() const { return active_; }

  void encode(uint8_t* output) const {
    if (!output) return;
    for (size_t i = 0; i < StorageSize; ++i) output[i] = 0;
    output[0] = 'M'; output[1] = '5'; output[2] = 'J'; output[3] = 'P'; output[4] = 1;
    size_t at = 6;
    put(output, at, state_.coins, 4); put(output, at, state_.lifetimeCoins, 4);
    put(output, at, state_.lifetimeMeters, 4); put(output, at, state_.runs, 4);
    put(output, at, state_.samDistance, 4);
    put(output, at, state_.headstarts, 2); put(output, at, state_.doubleRuns, 2);
    put(output, at, state_.sam, 1);
    for (uint8_t level : state_.levels) put(output, at, level, 1);
    const auto& m = missions_.state();
    put(output, at, m.version, 1); put(output, at, m.next, 1); put(output, at, m.completedRanks, 1);
    put(output, at, m.stars, 2); put(output, at, m.totalStars, 4); put(output, at, m.pendingCoins, 4);
    for (const auto& slot : m.slots) {
      put(output, at, slot.id, 1); put(output, at, slot.done, 1); put(output, at, slot.progress, 4);
    }
    at = StorageSize - 4;
    put(output, at, checksum(output, StorageSize - 4), 4);
  }
  bool decode(const uint8_t* input, size_t size) {
    if (!input || size != StorageSize || input[0] != 'M' || input[1] != '5' || input[2] != 'J' || input[3] != 'P' || input[4] != 1) return false;
    size_t at = StorageSize - 4;
    if (get(input, at, 4) != checksum(input, StorageSize - 4)) return false;
    at = 6; State saved;
    saved.coins = get(input, at, 4); saved.lifetimeCoins = get(input, at, 4);
    saved.lifetimeMeters = get(input, at, 4); saved.runs = get(input, at, 4);
    saved.samDistance = get(input, at, 4);
    saved.headstarts = uint16_t(get(input, at, 2)); saved.doubleRuns = uint16_t(get(input, at, 2));
    saved.sam = uint8_t(get(input, at, 1));
    for (uint8_t& level : saved.levels) level = uint8_t(get(input, at, 1));
    JetpackMissions::State m;
    m.version = uint8_t(get(input, at, 1)); m.next = uint8_t(get(input, at, 1)); m.completedRanks = uint8_t(get(input, at, 1));
    m.stars = uint16_t(get(input, at, 2)); m.totalStars = get(input, at, 4); m.pendingCoins = get(input, at, 4);
    for (auto& slot : m.slots) {
      slot.id = uint8_t(get(input, at, 1)); slot.done = uint8_t(get(input, at, 1)); slot.progress = get(input, at, 4);
    }
    restore(saved); missions_.restore(m); absorbMissionCoins(); return true;
  }

 private:
  static constexpr uint32_t MaxCoins = 999999999;
  State state_;
  JetpackMissions::Tracker missions_;
  uint32_t creditedCoins_ = 0;
  bool active_ = false, dirty_ = false;
  void absorbMissionCoins() {
    const uint32_t earned = missions_.takeCoinReward();
    if (earned) { state_.coins = addLimited(state_.coins, earned, MaxCoins); dirty_ = true; }
  }
  static uint8_t boundedLevel(uint8_t kind, uint8_t level) { return level > maxLevel(kind) ? maxLevel(kind) : level; }
  static uint32_t addLimited(uint32_t a, uint32_t b, uint32_t limit) { return b > limit - a ? limit : a + b; }
  static void put(uint8_t* data, size_t& at, uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) { data[at++] = uint8_t(value); value >>= 8; }
  }
  static uint32_t get(const uint8_t* data, size_t& at, unsigned bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= uint32_t(data[at++]) << (8 * i);
    return value;
  }
  static uint32_t checksum(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
      crc ^= data[i];
      for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
  }
};

}  // namespace flappy
