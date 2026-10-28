#include "../firmware/flappy/jetpack_rewards.h"

#include <array>
#include <assert.h>
#include <iostream>

namespace R = flappy::JetpackRewards;

static unsigned prizeIndex(const R::Award& award) {
  switch (award.kind) {
    case R::Kind::Coins: return award.amount == 200 ? 0 : award.amount == 1000 ? 1 : 2;
    case R::Kind::Blast: return 2 + award.tier;
    case R::Kind::Revive: return 6;
    case R::Kind::Tokens: return 7;
    case R::Kind::DoubleCoins: return 8;
    case R::Kind::Headstart: return 9;
  }
  assert(false);
  return 0;
}

static R::Award win(R::FinalSpin& spin, unsigned index) {
  uint32_t ticket = 0;
  for (unsigned i = 0; i < index; ++i) ticket += spin.weight(i);
  assert(spin.weight(index));
  const R::Award award = spin.draw(ticket);
  assert(prizeIndex(award) == index);
  return award;
}

static void weightedSelectionAndPayouts() {
  const std::array<unsigned, 10> expected{{120, 90, 50, 120, 75, 40, 100, 70, 120, 200}};
  std::array<unsigned, 10> counts{};
  for (unsigned ticket = 0; ticket < 985; ++ticket) {
    R::FinalSpin spin;
    assert(spin.totalWeight() == 985);
    ++counts[prizeIndex(spin.draw(ticket))];
  }
  assert(counts == expected);
  R::FinalSpin spin;
  assert(win(spin, 0).amount == 200);
  assert(win(spin, 1).amount == 1000);
  assert(win(spin, 2).amount == 2000);
  for (unsigned index = 3; index <= 5; ++index) {
    const auto award = win(spin, index);
    assert(award.kind == R::Kind::Blast && award.tier == index - 2 && award.amount == 1);
  }
  assert(win(spin, 6).amount == 1);
  assert(win(spin, 7).amount == 3);
  assert(win(spin, 8).amount == 1);
  assert(win(spin, 9).amount == 500);
}

static void fixedRepeatSuppressionAndRunReset() {
  R::FinalSpin spin;
  const std::array<unsigned, 10> after{{120, 90, 50, 60, 18, 4, 2, 70, 0, 0}};
  for (unsigned index = 0; index < 10; ++index) {
    win(spin, index);
    assert(spin.weight(index) == after[index]);
    if (after[index]) {
      for (unsigned repeat = 0; repeat < 5; ++repeat) win(spin, index);
      assert(spin.weight(index) == after[index]);
    }
  }
  assert(spin.totalWeight() == 414);
  std::array<unsigned, 10> counts{};
  for (unsigned ticket = 0; ticket < 414; ++ticket) {
    auto copy = spin;
    ++counts[prizeIndex(copy.draw(ticket))];
  }
  assert(counts == after);
  spin.reset();
  assert(spin.totalWeight() == 985 && spin.weight(R::Prize::Revive) == 100);
  assert(spin.weight(R::Prize::DoubleCoins) == 120 && spin.weight(R::Prize::Headstart) == 200);
}

static void eligibilityAndUnsignedBoundaries() {
  R::Eligibility eligibility;
  eligibility.canRevive = false;
  eligibility.canDoubleCoins = false;
  eligibility.canHeadstart = false;
  R::FinalSpin spin;
  assert(spin.totalWeight(false) == 885);
  assert(spin.totalWeight(eligibility) == 565);
  std::array<unsigned, 10> counts{};
  for (unsigned ticket = 0; ticket < 565; ++ticket) {
    auto copy = spin;
    ++counts[prizeIndex(copy.draw(ticket, eligibility))];
  }
  assert(counts[6] == 0 && counts[8] == 0 && counts[9] == 0);
  assert(counts[0] == 120 && counts[7] == 70);
  assert(spin.weight(6) == 100 && spin.weight(8) == 120 && spin.weight(9) == 200);
  assert(spin.weight(10) == 0 && spin.weight(UINT32_MAX) == 0);
  const auto wrapped = spin.draw(UINT32_MAX, eligibility);
  assert(wrapped.kind != R::Kind::Revive && wrapped.kind != R::Kind::DoubleCoins
      && wrapped.kind != R::Kind::Headstart);
  assert(R::cashInValue(0) == 0 && R::cashInValue(3) == 300);
  assert(R::cashInValue(UINT16_MAX) == 6553500);
  assert(R::awardFor(R::Prize::Count).amount == 0);
  assert(sizeof(R::FinalSpin) <= 24);
}

static void reelStopsLossAndForcedWinCadence() {
  unsigned outcomes[4] = {};
  for (unsigned ticket = 0; ticket < 10000; ++ticket) {
    R::FinalSpin spin;
    spin.reset(1);
    const auto result = spin.spin(0, ticket, 0);
    assert(!result.forced && result.matches >= 1 && result.matches <= 3);
    ++outcomes[result.matches];
    assert(result.hasPrize == (result.matches == 3));
    assert(result.hasPrize ? result.award.amount == 200 : result.award.amount == 0);
  }
  assert(outcomes[1] == 2500 && outcomes[2] == 5000 && outcomes[3] == 2500);

  R::FinalSpin spin;
  spin.reset(0);
  assert(spin.spinsUntilForcedWin() == 3);
  const auto first = spin.spin(665, 9999, 0);
  assert(!first.hasPrize && first.matches == 1 && spin.weight(R::Prize::DoubleCoins) == 120);
  const auto second = spin.spin(665, 0, 0);
  assert(second.hasPrize && !second.forced && spin.weight(R::Prize::DoubleCoins) == 0);
  assert(spin.spinsUntilForcedWin() == 1);
  spin.resetWeights();
  assert(spin.spinsUntilForcedWin() == 1 && spin.weight(R::Prize::DoubleCoins) == 120);
  const auto third = spin.spin(0, 9999, 1);
  assert(third.hasPrize && third.forced && third.matches == 3 && spin.spinsUntilForcedWin() == 4);
  for (unsigned i = 0; i < 3; ++i) assert(!spin.spin(0, 9999, 0).hasPrize);
  const auto seventh = spin.spin(0, 9999, 0);
  assert(seventh.hasPrize && seventh.forced && spin.spinsUntilForcedWin() == 3);

  R::Eligibility eligibility;
  eligibility.canRevive = eligibility.canDoubleCoins = eligibility.canHeadstart = false;
  for (unsigned i = 0; i < 30; ++i) {
    const auto result = spin.spin(UINT32_MAX, UINT32_MAX, UINT32_MAX, eligibility);
    if (result.hasPrize) assert(result.award.kind != R::Kind::Revive
        && result.award.kind != R::Kind::DoubleCoins && result.award.kind != R::Kind::Headstart);
  }
  spin.reset(UINT32_MAX);
  assert(spin.totalWeight() == 985 && spin.spinsUntilForcedWin() == 4);
}

int main() {
  weightedSelectionAndPayouts();
  fixedRepeatSuppressionAndRunReset();
  eligibilityAndUnsignedBoundaries();
  reelStopsLossAndForcedWinCadence();
  std::cout << "Jetpack reward tests passed\n";
}
