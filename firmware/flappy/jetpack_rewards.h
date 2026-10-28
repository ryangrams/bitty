#pragma once

#include <stdint.h>

namespace flappy {
namespace JetpackRewards {

enum class Kind : uint8_t { Coins, Blast, Revive, Tokens, DoubleCoins, Headstart };

enum class Prize : uint8_t {
  CoinsSmall, CoinsMedium, CoinsLarge,
  BlastSmall, BlastMedium, BlastLarge,
  Revive, Tokens, DoubleCoins, Headstart, Count
};

struct Award {
  Kind kind;
  uint16_t amount;
  uint8_t tier;
};

struct Outcome {
  bool hasPrize;
  Award award;
  uint8_t matches;
  bool forced;
};

struct Eligibility {
  bool canRevive = true;
  bool canDoubleCoins = true;
  bool canHeadstart = true;
};

constexpr uint16_t CashInPerToken = 100;
constexpr unsigned PrizeCount = unsigned(Prize::Count);

constexpr uint32_t cashInValue(uint16_t tokens) {
  return uint32_t(tokens) * CashInPerToken;
}

// Amount is coins, tokens, headstart meters, or one consumable. Blast tiers are 1..3.
constexpr Award awardFor(Prize prize) {
  return prize == Prize::CoinsSmall ? Award{Kind::Coins, 200, 0}
       : prize == Prize::CoinsMedium ? Award{Kind::Coins, 1000, 0}
       : prize == Prize::CoinsLarge ? Award{Kind::Coins, 2000, 0}
       : prize == Prize::BlastSmall ? Award{Kind::Blast, 1, 1}
       : prize == Prize::BlastMedium ? Award{Kind::Blast, 1, 2}
       : prize == Prize::BlastLarge ? Award{Kind::Blast, 1, 3}
       : prize == Prize::Revive ? Award{Kind::Revive, 1, 0}
       : prize == Prize::Tokens ? Award{Kind::Tokens, 3, 0}
       : prize == Prize::DoubleCoins ? Award{Kind::DoubleCoins, 1, 0}
       : prize == Prize::Headstart ? Award{Kind::Headstart, 500, 0}
       : Award{Kind::Coins, 0, 0};
}

class FinalSpin {
 public:
  FinalSpin() { reset(); }

  void reset(uint32_t randomWord = 0) {
    resetWeights();
    spinsUntilForcedWin_ = uint8_t(3 + randomWord % 2);
  }

  void resetWeights() {
    for (unsigned i = 0; i < PrizeCount; ++i) weights_[i] = initialWeight(i);
  }

  uint8_t spinsUntilForcedWin() const { return spinsUntilForcedWin_; }

  Outcome spin(uint32_t prizeWord, uint32_t stopWord, uint32_t reseedWord, bool canRevive = true) {
    Eligibility eligibility;
    eligibility.canRevive = canRevive;
    return spin(prizeWord, stopWord, reseedWord, eligibility);
  }

  Outcome spin(uint32_t prizeWord, uint32_t stopWord, uint32_t reseedWord, const Eligibility& eligibility) {
    const bool forced = --spinsUntilForcedWin_ == 0;
    // The later reels each stop on their target half the time.
    const uint8_t matches = forced ? 3 : uint8_t(1 + (stopWord % 100 < 50) + (stopWord / 100 % 100 < 50));
    if (forced) spinsUntilForcedWin_ = uint8_t(3 + reseedWord % 2);
    const bool won = matches == 3;
    return Outcome{won, won ? draw(prizeWord, eligibility) : Award{Kind::Coins, 0, 0}, matches, forced};
  }

  uint16_t weight(unsigned prizeIndex) const {
    return prizeIndex < PrizeCount ? weights_[prizeIndex] : 0;
  }

  uint16_t weight(Prize prize) const { return weight(unsigned(prize)); }

  uint16_t totalWeight(bool canRevive = true) const {
    Eligibility eligibility;
    eligibility.canRevive = canRevive;
    return totalWeight(eligibility);
  }

  uint16_t totalWeight(const Eligibility& eligibility) const {
    uint16_t total = 0;
    for (unsigned i = 0; i < PrizeCount; ++i)
      if (eligible(i, eligibility)) total = uint16_t(total + weights_[i]);
    return total;
  }

  Award draw(uint32_t randomWord, bool canRevive = true) {
    Eligibility eligibility;
    eligibility.canRevive = canRevive;
    return draw(randomWord, eligibility);
  }

  Award draw(uint32_t randomWord, const Eligibility& eligibility) {
    uint32_t ticket = randomWord % totalWeight(eligibility);
    for (unsigned i = 0; i < PrizeCount; ++i) {
      if (!eligible(i, eligibility)) continue;
      if (ticket < weights_[i]) {
        // Each win applies its fixed fraction of the initial weight.
        weights_[i] = afterWinWeight(i);
        return awardFor(Prize(i));
      }
      ticket -= weights_[i];
    }
    return awardFor(Prize::CoinsSmall);
  }

  static constexpr uint16_t initialWeight(unsigned i) {
    return i == unsigned(Prize::CoinsSmall) ? 120
         : i == unsigned(Prize::CoinsMedium) ? 90
         : i == unsigned(Prize::CoinsLarge) ? 50
         : i == unsigned(Prize::BlastSmall) ? 120
         : i == unsigned(Prize::BlastMedium) ? 75
         : i == unsigned(Prize::BlastLarge) ? 40
         : i == unsigned(Prize::Revive) ? 100
         : i == unsigned(Prize::Tokens) ? 70
         : i == unsigned(Prize::DoubleCoins) ? 120
         : i == unsigned(Prize::Headstart) ? 200 : 0;
  }

 private:
  uint16_t weights_[PrizeCount];
  uint8_t spinsUntilForcedWin_ = 3;

  static bool eligible(unsigned i, const Eligibility& eligibility) {
    return (i != unsigned(Prize::Revive) || eligibility.canRevive)
        && (i != unsigned(Prize::DoubleCoins) || eligibility.canDoubleCoins)
        && (i != unsigned(Prize::Headstart) || eligibility.canHeadstart);
  }

  static constexpr uint16_t afterWinWeight(unsigned i) {
    return i == unsigned(Prize::BlastSmall) ? 60
         : i == unsigned(Prize::BlastMedium) ? 18
         : i == unsigned(Prize::BlastLarge) ? 4
         : i == unsigned(Prize::Revive) ? 2
         : i == unsigned(Prize::DoubleCoins) || i == unsigned(Prize::Headstart) ? 0
         : initialWeight(i);
  }
};

}  // namespace JetpackRewards
}  // namespace flappy
