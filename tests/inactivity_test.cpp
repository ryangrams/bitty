#include "../firmware/flappy/inactivity.h"
#include <cassert>
#include <cstdint>
#include <iostream>

using flappy::InactivityPolicy;
using flappy::PowerSource;

static void powerDetectionUsesPresenceAndReadValidity() {
  for (unsigned charge : {0u, 4u}) {
    assert(flappy::axp192PowerSource(true, uint8_t(0x20 | charge), 0x20) == PowerSource::External);
    assert(flappy::axp192PowerSource(true, uint8_t(0x80 | charge), 0x20) == PowerSource::External);
  }
  assert(flappy::axp192PowerSource(false, 0, 0x20) == PowerSource::Unknown);
  assert(flappy::axp192PowerSource(true, 0, 0) == PowerSource::Unknown);
  assert(flappy::axp192PowerSource(true, 0, 0x20) == PowerSource::Battery);
}

static void freshBatteryWindowAndHeldActivity() {
  InactivityPolicy policy;
  assert(!policy.shouldSleep(1000, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(30999, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(31000, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(31000, 30000, PowerSource::Battery, true));
  assert(!policy.shouldSleep(90000, 30000, PowerSource::Battery, true));
  assert(!policy.shouldSleep(119999, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(120000, 30000, PowerSource::Battery));
  policy.activity(120000);
  assert(policy.idleMs(120123) == 123);
  assert(!policy.shouldSleep(149999, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(150000, 30000, PowerSource::Battery));
}

static void unplugAndUnknownPowerRestartTheWindow() {
  InactivityPolicy policy;
  assert(!policy.shouldSleep(0, 30000, PowerSource::External));
  assert(!policy.shouldSleep(500000, 30000, PowerSource::External));
  assert(!policy.shouldSleep(500001, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(530000, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(530001, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(530002, 30000, PowerSource::Unknown));
  assert(!policy.shouldSleep(900000, 30000, PowerSource::Unknown));
  assert(!policy.shouldSleep(900001, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(930000, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(930001, 30000, PowerSource::Battery));
}

static void settingsAndInhibitionHaveFreshWindows() {
  InactivityPolicy policy;
  assert(!policy.shouldSleep(0, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(29000, 0, PowerSource::Battery));
  assert(!policy.shouldSleep(900000, 0, PowerSource::Battery));
  assert(!policy.shouldSleep(900001, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(930000, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(930001, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(930002, 600000, PowerSource::Battery));
  assert(!policy.shouldSleep(1530001, 600000, PowerSource::Battery));
  assert(policy.shouldSleep(1530002, 600000, PowerSource::Battery));
  assert(!policy.shouldSleep(1530003, 600000, PowerSource::Battery, false, true));
  assert(!policy.shouldSleep(2130002, 600000, PowerSource::Battery));
  assert(policy.shouldSleep(2130003, 600000, PowerSource::Battery));
}

static void wrapAndWakeReleaseAreSafe() {
  InactivityPolicy policy;
  const uint32_t start = UINT32_MAX - 10000;
  assert(!policy.shouldSleep(start, 30000, PowerSource::Battery));
  assert(!policy.shouldSleep(start + 29999u, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(start + 30000u, 30000, PowerSource::Battery));
  policy.resumed(70000);
  assert(policy.wakeInputBlocked());
  assert(policy.consumeWakeInput(true, 140000));
  assert(!policy.shouldSleep(140000, 30000, PowerSource::Battery));
  assert(policy.consumeWakeInput(false, 140000));
  assert(!policy.wakeInputBlocked());
  assert(policy.idleMs(140000) == 0);
  assert(!policy.shouldSleep(140001, 30000, PowerSource::Battery));
  assert(!policy.consumeWakeInput(false, 140001));
  assert(!policy.shouldSleep(169999, 30000, PowerSource::Battery));
  assert(policy.shouldSleep(170000, 30000, PowerSource::Battery));
}

int main() {
  powerDetectionUsesPresenceAndReadValidity();
  freshBatteryWindowAndHeldActivity();
  unplugAndUnknownPowerRestartTheWindow();
  settingsAndInhibitionHaveFreshWindows();
  wrapAndWakeReleaseAreSafe();
  std::cout << "Battery-only inactivity and wake-input checks passed\n";
}
