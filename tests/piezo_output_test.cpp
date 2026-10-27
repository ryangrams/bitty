#include "../firmware/flappy/piezo_output.h"
#include "../firmware/flappy/piezo_sound.h"

#include <assert.h>
#include <iostream>

struct FakeDriver {
  unsigned frequencyCalls = 0, dutyCalls = 0;
  unsigned failFrequency = 0, failDuty = 0;
  uint16_t frequency = 2500, duty = 0;
  bool roundFrequency = false;

  uint32_t setFrequency(uint16_t value) {
    ++frequencyCalls;
    if (failFrequency) { --failFrequency; return 0; }
    frequency = value;
    return roundFrequency ? value - 1u : value;
  }

  bool writeDuty(uint16_t value) {
    ++dutyCalls;
    if (failDuty) { --failDuty; return false; }
    duty = value;
    return true;
  }
};

static void cachedPitchDutyAndSilence() {
  flappy::PiezoOutput output;
  FakeDriver driver;
  driver.roundFrequency = true;
  output.reset(2500, 0);
  assert(output.service(driver));
  assert(!driver.frequencyCalls && !driver.dutyCalls);
  output.request(197, 204);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 1 && driver.dutyCalls == 1);
  assert(output.frequency() == 197 && output.duty() == 204);
  for (unsigned frame = 0; frame < 120; ++frame) {
    output.request(197, 204);
    assert(output.service(driver));
    assert(output.service(driver));
  }
  assert(driver.frequencyCalls == 1 && driver.dutyCalls == 1);
  output.request(220, 204);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 1);
  assert(driver.frequency == 220 && driver.duty == 204);
  output.request(220, 350);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 2);
  output.request(220, 0);
  assert(output.service(driver));
  assert(!output.frequency() && !output.duty());
  assert(output.configuredFrequency() == 220);
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 3);
  output.request(100, 0);
  assert(output.service(driver));
  output.request(0, 511);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 3);
  output.request(220, 511);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 4);
  output.request(220, UINT16_MAX);
  assert(output.service(driver));
  assert(output.duty() == 511 && driver.dutyCalls == 4);
}

static void failuresStayPending() {
  flappy::PiezoOutput output;
  FakeDriver driver;
  output.reset(2500, 0);
  driver.failFrequency = 1;
  output.request(100, 200);
  assert(!output.service(driver));
  assert(!driver.dutyCalls && output.configuredFrequency() == 2500);
  assert(!output.frequency() && !output.duty());
  driver.failDuty = 1;
  assert(!output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 1);
  assert(output.configuredFrequency() == 100 && !output.duty());
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 2);
  assert(output.frequency() == 100 && output.duty() == 200);
  driver.failDuty = 1;
  output.request(0, 0);
  assert(!output.service(driver));
  assert(output.duty() == 200);
  assert(output.service(driver));
  assert(!output.duty() && output.configuredFrequency() == 100);
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 4);
  driver.failFrequency = 1;
  output.request(300, 100);
  assert(!output.service(driver));
  output.request(0, 0);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 3 && driver.dutyCalls == 4);
}

static void schedulerRefreshAndDiagnosticOverride() {
  flappy::PiezoOutput output;
  flappy::PiezoSound scheduler;
  flappy::PiezoTone tone;
  FakeDriver driver;
  output.reset(2500, 0);
  unsigned refreshes = 0;
  for (unsigned now = 0; now < 1000; now += 16) {
    scheduler.setNesTone(197, 15, now);
    if (scheduler.update(now, tone)) {
      ++refreshes;
      output.request(tone.frequency, uint16_t(uint32_t(tone.volume) * 511u / 255u));
    }
    assert(output.service(driver));
    assert(output.service(driver));
  }
  assert(refreshes > 20);
  assert(driver.frequencyCalls == 1 && driver.dutyCalls == 1);
  scheduler.stop();
  output.request(2500, 511);
  assert(output.service(driver));
  for (unsigned i = 0; i < 100; ++i) assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 2);
  output.request(0, 0);
  assert(output.service(driver));
  assert(scheduler.update(1200, tone) && !tone.frequency);
  output.request(tone.frequency, 0);
  assert(output.service(driver));
  assert(driver.frequencyCalls == 2 && driver.dutyCalls == 3);
  scheduler.setNesTone(197, 15, 1216);
  assert(scheduler.update(1216, tone));
  output.request(tone.frequency, uint16_t(uint32_t(tone.volume) * 511u / 255u));
  assert(output.service(driver));
  assert(driver.frequencyCalls == 3 && driver.dutyCalls == 4);
}

int main() {
  static_assert(sizeof(flappy::PiezoOutput) <= 8, "Piezo output cache must remain bounded");
  flappy::PiezoOutput fresh;
  FakeDriver driver;
  assert(fresh.service(driver));
  assert(fresh.service(driver));
  assert(driver.dutyCalls == 1 && !driver.frequencyCalls);
  cachedPitchDutyAndSilence();
  failuresStayPending();
  schedulerRefreshAndDiagnosticOverride();
  std::cout << "Piezo output caching, silence, diagnostic override and driver retries passed\n";
}
