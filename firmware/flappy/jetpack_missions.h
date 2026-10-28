#pragma once

#include <stdint.h>

namespace flappy {
namespace JetpackMissions {

constexpr uint8_t MissionCount = 17;
constexpr uint8_t ActiveCount = 3;
constexpr uint8_t RankCount = 15;

struct Stats {
  uint32_t meters = 0, coinsCollected = 0, vehiclesCollected = 0, mechsCollected = 0;
  uint32_t highFives = 0, tokensCollected = 0, footMeters = 0, hogMeters = 0;
  uint32_t gravityFlips = 0, cashInTokens = 0, prizesWon = 0;
};

enum class Metric : uint8_t {
  Coins, Meters, Vehicles, HighFives, Tokens, FootMeters, Mechs,
  CashInTokens, Games, Purchases, NoVehicleMeters, HogMeters, GravityFlips, Prizes
};

struct Definition {
  Metric metric;
  uint16_t target;
  uint8_t stars;
  bool perRun;
};

inline Definition definition(uint8_t id) {
  static const Definition definitions[MissionCount] = {
    {Metric::Coins, 150, 1, false},
    {Metric::Meters, 1000, 2, true},
    {Metric::Vehicles, 2, 1, false},
    {Metric::HighFives, 10, 1, true},
    {Metric::Tokens, 2, 1, true},
    {Metric::FootMeters, 300, 2, true},
    {Metric::Mechs, 3, 1, false},
    {Metric::CashInTokens, 3, 1, false},
    {Metric::Games, 5, 1, false},
    {Metric::Purchases, 1, 2, false},
    {Metric::Coins, 200, 1, true},
    {Metric::NoVehicleMeters, 500, 2, true},
    {Metric::Meters, 3000, 1, false},
    {Metric::HogMeters, 750, 2, false},
    {Metric::Tokens, 5, 1, false},
    {Metric::GravityFlips, 30, 1, true},
    {Metric::Prizes, 5, 3, false}
  };
  return id < MissionCount ? definitions[id] : Definition{Metric::Coins, 0, 0, false};
}

inline const char* label(uint8_t id) {
  static const char* const labels[MissionCount] = {
    "Gather 150 coins", "Reach 1000m in a run", "Find 2 vehicles",
    "High-five 10 in a run", "2 tokens in one run", "Run 300m on foot",
    "Find 3 Mechs", "Cash in 3 tokens", "Start 5 runs", "Buy an upgrade",
    "200 coins in one run", "500m before a vehicle", "Travel 3000m total",
    "Ride Hog for 750m", "Gather 5 tokens total", "30 gravity flips/run",
    "Win 5 Spin prizes"
  };
  return id < MissionCount ? labels[id] : "";
}

constexpr uint16_t rankStarTarget(uint8_t completedRanks) {
  return completedRanks < 3 ? 3 : completedRanks < 5 ? 5 : completedRanks < 8 ? 7
       : completedRanks < 10 ? 9 : completedRanks < 13 ? 11
       : completedRanks < RankCount ? 13 : 0;
}

constexpr uint16_t rankCoinReward(uint8_t completedRanks) {
  return completedRanks < RankCount ? uint16_t((completedRanks + 1) * 400) : 0;
}

struct Slot {
  uint8_t id = 0;
  uint8_t done = 0;
  uint32_t progress = 0;
};

struct State {
  uint8_t version = 1;
  uint8_t next = ActiveCount;
  uint8_t completedRanks = 0;
  uint16_t stars = 0;
  uint32_t totalStars = 0;
  uint32_t pendingCoins = 0;
  Slot slots[ActiveCount];
};

class Tracker {
 public:
  Tracker() { reset(); }

  void reset() {
    state_ = State{};
    for (uint8_t i = 0; i < ActiveCount; ++i) state_.slots[i].id = i;
    previous_ = Stats{};
    eligible_ = false;
    dirty_ = false;
  }

  const State& state() const { return state_; }

  void restore(State saved) {
    reset();
    if (saved.version != 1) return;
    saved.next = saved.next < MissionCount ? saved.next : 0;
    saved.completedRanks = saved.completedRanks <= RankCount ? saved.completedRanks : RankCount;
    const uint16_t threshold = rankStarTarget(saved.completedRanks);
    saved.stars = threshold && saved.stars >= threshold ? uint16_t(threshold - 1) : saved.stars;
    if (!threshold) saved.stars = 0;
    for (uint8_t i = 0; i < ActiveCount; ++i) {
      bool invalid = saved.slots[i].id >= MissionCount;
      for (uint8_t j = 0; j < i; ++j) invalid |= saved.slots[i].id == saved.slots[j].id;
      if (invalid) {
        uint8_t candidate = saved.next;
        while (usedBefore(saved, i, candidate)) candidate = uint8_t((candidate + 1) % MissionCount);
        saved.slots[i] = Slot{};
        saved.slots[i].id = candidate;
        saved.next = uint8_t((candidate + 1) % MissionCount);
      }
      const uint16_t goal = definition(saved.slots[i].id).target;
      saved.slots[i].done = saved.slots[i].done == 1 && saved.slots[i].progress >= goal;
      if (saved.slots[i].progress > goal) saved.slots[i].progress = goal;
      // A partial single-run mission starts fresh after a reboot.
      if (definition(saved.slots[i].id).perRun && !saved.slots[i].done)
        saved.slots[i].progress = 0;
    }
    state_ = saved;
  }

  void beginRun(bool eligible = true) {
    previous_ = Stats{};
    eligible_ = eligible;
    if (!eligible_) return;
    for (uint8_t i = 0; i < ActiveCount; ++i) {
      Slot& slot = state_.slots[i];
      if (slot.done) {
        uint8_t candidate = state_.next;
        while (usedOutside(i, candidate)) candidate = uint8_t((candidate + 1) % MissionCount);
        slot = Slot{};
        slot.id = candidate;
        state_.next = uint8_t((candidate + 1) % MissionCount);
        dirty_ = true;
      } else if (definition(slot.id).perRun && slot.progress) {
        slot.progress = 0;
        dirty_ = true;
      }
    }
    addEvent(Metric::Games, 1);
  }

  void observe(const Stats& stats) {
    if (!eligible_) return;
    Stats delta;
    delta.meters = consume(stats.meters, previous_.meters);
    delta.coinsCollected = consume(stats.coinsCollected, previous_.coinsCollected);
    delta.vehiclesCollected = consume(stats.vehiclesCollected, previous_.vehiclesCollected);
    delta.mechsCollected = consume(stats.mechsCollected, previous_.mechsCollected);
    delta.highFives = consume(stats.highFives, previous_.highFives);
    delta.tokensCollected = consume(stats.tokensCollected, previous_.tokensCollected);
    delta.footMeters = consume(stats.footMeters, previous_.footMeters);
    delta.hogMeters = consume(stats.hogMeters, previous_.hogMeters);
    delta.gravityFlips = consume(stats.gravityFlips, previous_.gravityFlips);
    delta.cashInTokens = consume(stats.cashInTokens, previous_.cashInTokens);
    delta.prizesWon = consume(stats.prizesWon, previous_.prizesWon);
    for (uint8_t i = 0; i < ActiveCount; ++i) {
      const Definition def = definition(state_.slots[i].id);
      if (def.metric == Metric::Games || def.metric == Metric::Purchases) continue;
      const uint32_t value = metricValue(def.metric, def.perRun ? previous_ : delta);
      if (def.perRun) setProgress(i, value);
      else addProgress(i, value);
    }
  }

  void recordPurchase(bool eligible = true) { if (eligible) addEvent(Metric::Purchases, 1); }

  uint32_t takeCoinReward() {
    const uint32_t coins = state_.pendingCoins;
    if (coins) { state_.pendingCoins = 0; dirty_ = true; }
    return coins;
  }

  uint8_t missionId(uint8_t slot) const { return slot < ActiveCount ? state_.slots[slot].id : MissionCount; }
  const char* label(uint8_t slot) const { return JetpackMissions::label(missionId(slot)); }
  bool complete(uint8_t slot) const { return slot < ActiveCount && state_.slots[slot].done; }
  uint32_t progress(uint8_t slot) const { return slot < ActiveCount ? state_.slots[slot].progress : 0; }
  uint16_t target(uint8_t slot) const { return definition(missionId(slot)).target; }
  uint8_t difficulty(uint8_t slot) const { return definition(missionId(slot)).stars; }
  uint8_t rank() const { return state_.completedRanks < RankCount ? uint8_t(state_.completedRanks + 1) : RankCount; }
  uint16_t stars() const { return state_.stars; }
  uint16_t starTarget() const { return rankStarTarget(state_.completedRanks); }
  bool rankComplete() const { return state_.completedRanks == RankCount; }
  bool dirty() const { return dirty_; }
  void clearDirty() { dirty_ = false; }

 private:
  State state_;
  Stats previous_;
  bool eligible_ = false, dirty_ = false;

  static uint32_t consume(uint32_t current, uint32_t& previous) {
    if (current <= previous) return 0;
    const uint32_t amount = current - previous;
    previous = current;
    return amount;
  }

  static bool usedBefore(const State& state, uint8_t count, uint8_t id) {
    for (uint8_t i = 0; i < count; ++i) if (state.slots[i].id == id) return true;
    return false;
  }

  bool usedOutside(uint8_t except, uint8_t id) const {
    for (uint8_t i = 0; i < ActiveCount; ++i) if (i != except && state_.slots[i].id == id) return true;
    return false;
  }

  static uint32_t metricValue(Metric metric, const Stats& stats) {
    switch (metric) {
      case Metric::Coins: return stats.coinsCollected;
      case Metric::Meters: return stats.meters;
      case Metric::Vehicles: return stats.vehiclesCollected;
      case Metric::HighFives: return stats.highFives;
      case Metric::Tokens: return stats.tokensCollected;
      case Metric::FootMeters: return stats.footMeters;
      case Metric::Mechs: return stats.mechsCollected;
      case Metric::CashInTokens: return stats.cashInTokens;
      case Metric::NoVehicleMeters: return stats.vehiclesCollected ? 0 : stats.meters;
      case Metric::HogMeters: return stats.hogMeters;
      case Metric::GravityFlips: return stats.gravityFlips;
      case Metric::Prizes: return stats.prizesWon;
      default: return 0;
    }
  }

  void addEvent(Metric metric, uint32_t amount) {
    for (uint8_t i = 0; i < ActiveCount; ++i)
      if (definition(state_.slots[i].id).metric == metric) addProgress(i, amount);
  }

  void addProgress(uint8_t index, uint32_t amount) {
    const uint32_t remaining = target(index) - state_.slots[index].progress;
    setProgress(index, amount >= remaining ? target(index) : state_.slots[index].progress + amount);
  }

  void setProgress(uint8_t index, uint32_t progress) {
    Slot& slot = state_.slots[index];
    if (slot.done) return;
    const Definition def = definition(slot.id);
    if (progress > def.target) progress = def.target;
    if (progress != slot.progress) { slot.progress = progress; dirty_ = true; }
    if (progress < def.target) return;
    slot.done = 1;
    state_.totalStars = state_.totalStars > UINT32_MAX - def.stars ? UINT32_MAX : state_.totalStars + def.stars;
    state_.stars = uint16_t(state_.stars + def.stars);
    while (state_.completedRanks < RankCount && state_.stars >= starTarget()) {
      state_.stars = uint16_t(state_.stars - starTarget());
      const uint16_t reward = rankCoinReward(state_.completedRanks);
      state_.pendingCoins = state_.pendingCoins > UINT32_MAX - reward ? UINT32_MAX : state_.pendingCoins + reward;
      ++state_.completedRanks;
    }
    if (rankComplete()) state_.stars = 0;
    dirty_ = true;
  }
};

}  // namespace JetpackMissions
}  // namespace flappy
