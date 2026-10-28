#include "../firmware/flappy/jetpack_rocket_time.h"

#include <assert.h>
#include <cmath>
#include <iostream>

using Rocket = flappy::JetpackRocketTime;

static void configurationAndEveryEvent() {
  const unsigned choices[] = {1,2,2,3,3,4,4,5,5,6};
  const unsigned trails[] = {6,8,10,10,14,14,18,18,22,22};
  for (uint8_t level = 0; level < 10; ++level) {
    Rocket rocket;
    const auto config = Rocket::config(level);
    assert(config.durationTicks == 420 + 150 * level);
    assert(config.waveCount == level + 1 && config.choiceCount == choices[level]);
    assert(config.trailChancePercent == trails[level]);
    rocket.start(level);
    unsigned waves = 0, choiceEvents = 0, releaseEvents = 0, endEvents = 0;
    for (unsigned tick = 1; tick <= unsigned(config.durationTicks) + 240; ++tick) {
      const auto event = rocket.advance();
      for (unsigned wave = 0; wave < 10; ++wave) if (event.waveMask & (1u << wave)) {
        assert(wave <= level && tick == 90 + 150 * wave); ++waves;
      }
      if (event.choiceCount) {
        assert(tick == unsigned(config.durationTicks) - 120 && event.choiceCount == choices[level]);
        ++choiceEvents;
      }
      if (event.release) { assert(tick == unsigned(config.durationTicks) + 150); ++releaseEvents; }
      if (event.ended) { assert(tick == unsigned(config.durationTicks) + 240); ++endEvents; }
    }
    assert(waves == level + 1 && choiceEvents == 1 && releaseEvents == 1 && endEvents == 1);
    assert(!rocket.active() && rocket.phase() == Rocket::Phase::Inactive);
    const auto again = rocket.advance(UINT16_MAX);
    assert(!again.waveMask && !again.choiceCount && !again.release && !again.ended);
  }
}

static void catchupCancelAndRestart() {
  Rocket rocket;
  rocket.start(9);
  const auto none = rocket.advance(0);
  assert(!none.waveMask && !none.choiceCount && rocket.elapsedTicks() == 0);
  const auto all = rocket.advance(UINT16_MAX);
  assert(all.waveMask == 0x3ff && all.choiceCount == 6 && all.release && all.ended);
  assert(rocket.elapsedTicks() == 2010 && !rocket.active());
  rocket.start(0);
  assert(rocket.advance(89).waveMask == 0);
  assert(rocket.advance().waveMask == 1);
  rocket.collectVehicle();
  const auto cancelled = rocket.advance(UINT16_MAX);
  assert(!cancelled.waveMask && !cancelled.choiceCount && !cancelled.ended);
  assert(rocket.timeScaleQ16(480 * Rocket::One) == Rocket::One);
  rocket.start(255);
  assert(rocket.level() == 9 && rocket.elapsedTicks() == 0);
  assert(rocket.advance(90).waveMask == 1);
  rocket.cancel();
  assert(!rocket.active() && rocket.phase() == Rocket::Phase::Inactive);
}

static void globalScaleEnvelopeAgainstIndependentCurve() {
  assert(Rocket::targetScaleQ16(0) == 32768);
  assert(Rocket::targetScaleQ16(175 * Rocket::One) == 32768);
  assert(Rocket::targetScaleQ16(380 * Rocket::One) == 32768);
  assert(Rocket::targetScaleQ16(480 * Rocket::One) == 25941);
  for (uint32_t speed : {175u, 480u}) {
    Rocket rocket; rocket.start(0);
    uint32_t previous = Rocket::One;
    const double target = 0.5 * std::fmin(380.0 / speed, 1.0);
    for (unsigned tick = 0; tick <= 660; ++tick) {
      const uint32_t actual = rocket.timeScaleQ16(speed * Rocket::One);
      double expected = target;
      if (tick < 90) expected += (1.0 - target) * std::cos(tick * std::acos(-1.0) / 180.0);
      else if (tick >= 570) expected += (1.0 - target) * std::sin((tick - 570) * std::acos(-1.0) / 180.0);
      assert(std::fabs(double(actual) / Rocket::One - expected) < 0.001);
      if (tick <= 90) assert(actual <= previous);
      if (tick >= 570) assert(actual >= previous);
      previous = actual;
      rocket.advance();
    }
    assert(rocket.timeScaleQ16(speed * Rocket::One) == Rocket::One);
  }
  static_assert(sizeof(Rocket) <= 8, "Rocket Time sequence must stay compact");
}

int main() {
  configurationAndEveryEvent();
  catchupCancelAndRestart();
  globalScaleEnvelopeAgainstIndependentCurve();
  std::cout << "Jetpack Rocket Time tests passed\n";
}
