#include <M5Unified.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include "app_model.h"
#include "ble_link.h"
#include "build_clock.h"
#include "render.h"
#include "generated_sounds.h"
#include "stick_power.h"

flappy::AppModel app;
flappy::coop::BleLink coopLink;
flappy::Viewport viewport;
Preferences preferences;
constexpr int StripRows = 16;
constexpr uint32_t TickMicros = 16667;
constexpr uint8_t MaxSkins = 22;
alignas(4) uint16_t canvasStorage[flappy::PixelWidth * flappy::PixelHeight];
alignas(4) uint16_t stripStorage[flappy::PixelWidth * StripRows];
const char* skinNames[MaxSkins] = {};
flappy::Renderer* renderer = nullptr;
uint32_t nextTick = 0;
uint32_t lastClockReadMs = 0;
uint32_t handledCoopRequest = 0;
bool pendingAction = false;
bool coopRoundPrepared = false;
bool coopRoundLaunched = false;
uint8_t appliedBrightness = UINT8_MAX;
uint8_t appliedSkin = UINT8_MAX;

uint32_t nonzeroRandom() {
  const uint32_t value = esp_random();
  return value ? value : 1;
}

bool validRtc(const m5::rtc_datetime_t& value) {
  return value.date.year >= 2024 && value.date.year <= 2099 &&
         value.date.month >= 1 && value.date.month <= 12 &&
         value.date.date >= 1 && value.date.date <= 31 &&
         value.time.hours >= 0 && value.time.hours < 24 &&
         value.time.minutes >= 0 && value.time.minutes < 60 &&
         value.time.seconds >= 0 && value.time.seconds < 60;
}

int weekDay(int year, int month, int day) {
  constexpr int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) --year;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

void applyClock(const m5::rtc_time_t& time, uint32_t nowMs) {
  const bool dirty = app.settingsDirty;
  app.setClock(uint8_t(time.hours), uint8_t(time.minutes), nowMs - uint32_t(time.seconds) * 1000U);
  app.settingsDirty = dirty;
}

void initializeClock(uint32_t nowMs) {
  m5::rtc_datetime_t dateTime;
  const bool rtcValid = M5.Rtc.isEnabled() && M5.Rtc.getDateTime(&dateTime) &&
                        !M5.Rtc.getVoltLow() && validRtc(dateTime);
  if (!rtcValid) {
    dateTime.date = m5::rtc_date_t(FlappyBuild::year, FlappyBuild::month, FlappyBuild::day,
        weekDay(FlappyBuild::year, FlappyBuild::month, FlappyBuild::day));
    dateTime.time = m5::rtc_time_t(FlappyBuild::hour, FlappyBuild::minute, FlappyBuild::second);
    if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(dateTime);
  }
  applyClock(dateTime.time, nowMs);
  lastClockReadMs = nowMs;
}

void readClock(uint32_t nowMs) {
  if (uint32_t(nowMs - lastClockReadMs) < 1000U) return;
  lastClockReadMs = nowMs;
  if (!M5.Rtc.isEnabled()) return;
  m5::rtc_datetime_t dateTime;
  if (M5.Rtc.getDateTime(&dateTime) && validRtc(dateTime)) applyClock(dateTime.time, nowMs);
}

void writeClock(uint32_t nowMs) {
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setTime(m5::rtc_time_t(app.clockHour(nowMs), app.clockMinute(nowMs), 0));
  }
  lastClockReadMs = nowMs;
}

void loadSettings() {
  preferences.begin("flappy", false);
  const uint8_t mode = preferences.getUChar("mode", 0);
  app.settings.mode = mode < uint8_t(flappy::Mode::Count) ? flappy::Mode(mode) : flappy::Mode::Original;
  app.setBrightness(preferences.getUChar("brightness", 3));
  app.setSkin(preferences.getUChar("skin", 0));
  app.settings.clock24Hour = preferences.getBool("clock24", true);
  app.settings.coopRole = 0;
  app.setBestScore(flappy::Mode::Original, preferences.getUShort("best0", preferences.getUShort("best", 0)));
  app.setBestScore(flappy::Mode::Arcade, preferences.getUShort("best3", 0));
  app.setBestScore(flappy::Mode::Big, preferences.getUShort("best4", 0));
  app.restart(nonzeroRandom());
  app.settingsDirty = false;
  app.highScoresDirty = false;
}

void saveSettings() {
  if (!app.settingsDirty) return;
  preferences.putUChar("mode", uint8_t(app.settings.mode));
  preferences.putUChar("brightness", app.settings.brightnessIndex);
  preferences.putUChar("skin", app.settings.skinIndex);
  preferences.putBool("clock24", app.settings.clock24Hour);
  app.settingsDirty = false;
}

void saveScores() {
  if (!app.highScoresDirty) return;
  preferences.putUShort("best0", app.bestScore(flappy::Mode::Original));
  preferences.putUShort("best3", app.bestScore(flappy::Mode::Arcade));
  preferences.putUShort("best4", app.bestScore(flappy::Mode::Big));
  app.highScoresDirty = false;
}

void applyDisplaySettings() {
  if (appliedBrightness != app.settings.brightnessIndex) {
    M5.Display.setBrightness(app.brightness());
    appliedBrightness = app.settings.brightnessIndex;
  }
  if (appliedSkin != app.settings.skinIndex) {
    renderer->setSkin(app.settings.skinIndex);
    app.game.setBirdMasks(renderer->birdMasks());
    appliedSkin = app.settings.skinIndex;
  }
}

void playSound(const FlappySounds::Sound& sound) {
  if (sound.length && sound.samples && M5.Speaker.isEnabled()) {
    M5.Speaker.playRaw(sound.samples, sound.length, sound.sampleRate, false, 1, 0, true);
  }
}

void playEvents(uint8_t flags) {
  if (app.autoPlaying()) return;
  if (flags & flappy::Hit) playSound(FlappySounds::hit);
  else if (flags & flappy::Die) playSound(FlappySounds::die);
  else if (flags & flappy::Point) playSound(FlappySounds::point);
  else if (flags & flappy::Flap) playSound(FlappySounds::wing);
}

flappy::coop::PlayerSnapshot localSnapshot() {
  flappy::coop::PlayerSnapshot player;
  player.tick = app.game.worldTick();
  player.y = app.game.birdY;
  player.rotation = int8_t(app.game.visibleRotation());
  player.frame = app.game.birdFrame;
  player.score = app.game.score;
  switch (app.game.state) {
    case flappy::State::Ready: player.life = flappy::coop::Life::Ready; break;
    case flappy::State::Playing: player.life = flappy::coop::Life::Alive; break;
    case flappy::State::Dying: player.life = flappy::coop::Life::Dying; break;
    case flappy::State::GameOver: player.life = flappy::coop::Life::GameOver; break;
  }
  return player;
}

void stopCoop() {
  coopLink.stop();
  coopRoundPrepared = false;
  coopRoundLaunched = false;
  app.settings.coopRole = 0;
  app.settingsDirty = true;
  if (app.settings.mode == flappy::Mode::Coop) app.restart(nonzeroRandom());
}

void handleCoopRequest(uint32_t nowMs) {
  if (handledCoopRequest == app.coopRequest) return;
  handledCoopRequest = app.coopRequest;
  coopRoundPrepared = false;
  coopRoundLaunched = false;
  if (app.settings.coopRole == 1) {
    coopLink.beginHost(nowMs, nonzeroRandom(), nonzeroRandom(), uint16_t(esp_random() % 360));
  } else if (app.settings.coopRole == 2) {
    coopLink.beginJoin(nowMs);
  } else {
    coopLink.stop();
  }
  nextTick = micros() + TickMicros;
}

void updateCoop(uint32_t nowMs) {
  coopLink.setLocal(localSnapshot());
  coopLink.update(nowMs, app.game.worldTick());
  flappy::coop::StartInfo start;
  if (coopLink.consumeStart(start)) {
    app.startCoopRound(start.seed);
    app.game.enableSharedMap(start.seed);
    coopRoundPrepared = true;
    coopRoundLaunched = false;
  }
  const flappy::coop::Phase phase = coopLink.session().phase();
  if (phase == flappy::coop::Phase::Playing && coopRoundPrepared && !coopRoundLaunched) {
    const uint32_t seed = coopLink.session().seed();
    app.startCoopRound(seed);
    app.game.enableSharedMap(seed);
    playEvents(app.game.step(true));
    coopRoundLaunched = true;
    nextTick = micros() + TickMicros;
  }
  if (phase == flappy::coop::Phase::Disconnected && (coopRoundPrepared || coopRoundLaunched)) {
    app.restart(nonzeroRandom());
    app.settings.coopRole = 0;
    app.settingsDirty = true;
    coopRoundPrepared = coopRoundLaunched = false;
  }
}

void showFrame(uint32_t nowMs) {
  flappy::RenderOptions options;
  options.uiScale = viewport.native ? 2 : 3;
  options.nowMs = nowMs;
  options.coopStatus = coopLink.statusText();
  options.peerName = coopLink.session().role() == flappy::coop::Role::Host ?
                     coopLink.deviceName() : coopLink.peerName();
  options.localReady = coopLink.session().localReady();
  options.remoteReady = coopLink.session().remoteReady();
  options.multipleHosts = coopLink.multipleHostsFound();
  const bool linkedMode = app.settings.mode == flappy::Mode::Coop &&
                          coopLink.session().role() != flappy::coop::Role::Off;
  const bool hostTint = coopLink.session().baseHue() >= 180;
  options.localTint = linkedMode && (coopLink.session().role() == flappy::coop::Role::Host ? hostTint : !hostTint);
  options.remoteTint = linkedMode && !options.localTint;
  const flappy::coop::RemoteView remote = coopLink.ghost(nowMs);
  options.remoteVisible = linkedMode && remote.visible;
  options.remoteY = remote.y;
  options.remoteRotation = remote.rotation;
  options.remoteFrame = remote.frame;
  options.remoteScore = remote.score;
  renderer->draw(app, options);
  M5.Display.startWrite();
  for (int y = 0; y < viewport.height; y += StripRows) {
    const int rows = viewport.height - y < StripRows ? viewport.height - y : StripRows;
    viewport.copyRows(canvasStorage, stripStorage, y, rows);
    M5.Display.pushImage(viewport.outputX, viewport.outputY + y, viewport.width, rows, stripStorage);
  }
  M5.Display.endWrite();
}

void stepGame(bool actionEdge, bool menuDown, uint32_t nowMs) {
  const bool menuWasOpen = app.menuOpen();
  const uint8_t previousHour = app.settings.clockHour;
  const uint8_t previousMinute = app.settings.clockMinute;
  const bool inCoop = app.settings.mode == flappy::Mode::Coop;
  const flappy::coop::Phase phase = coopLink.session().phase();
  bool actionForApp = actionEdge;
  if (inCoop && !menuWasOpen) {
    if (actionEdge && (app.game.state == flappy::State::Ready || app.game.state == flappy::State::GameOver)) {
      coopLink.setLocal(localSnapshot());
      coopLink.requestReady(nowMs);
    }
    actionForApp = actionEdge && phase == flappy::coop::Phase::Playing &&
                   app.game.state == flappy::State::Playing;
  }
  uint32_t sharedTick = app.game.worldTick();
  const bool liveCoop = inCoop && coopRoundLaunched && phase == flappy::coop::Phase::Playing && !menuWasOpen;
  if (liveCoop) {
    const uint32_t linkTick = coopLink.worldTick(nowMs);
    if (linkTick > sharedTick) sharedTick = linkTick;
    if (app.game.state == flappy::State::Playing) {
      app.game.syncWorldTick(sharedTick ? sharedTick - 1 : 0);
    }
  }
  const uint8_t events = app.step(actionForApp, menuDown, nowMs);
  if (liveCoop && app.game.state != flappy::State::Playing) app.game.syncWorldTick(sharedTick);
  playEvents(events);
  if (previousHour != app.settings.clockHour || previousMinute != app.settings.clockMinute) writeClock(nowMs);
  if (!menuWasOpen && app.menuOpen() && coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  if (app.settings.mode != flappy::Mode::Coop && coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  handleCoopRequest(nowMs);
  applyDisplaySettings();
  if ((events & flappy::GameOver) || app.menuOpen() || app.settingsDirty) saveScores();
  saveSettings();
}

void setup() {
  prepareStickDisplayPower();
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  cfg.internal_imu = false;
  cfg.internal_rtc = true;
  cfg.internal_mic = false;
  cfg.internal_spk = true;
  cfg.external_display_value = 0;
  cfg.output_power = false;
  M5.begin(cfg);
  rtc_gpio_pullup_dis(GPIO_NUM_2);
  rtc_gpio_pulldown_en(GPIO_NUM_2);
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setSwapBytes(true);
  M5.Speaker.setVolume(80);
  viewport.configure(M5.Display.width(), M5.Display.height());
  if (viewport.width <= 0 || viewport.height <= 0) return;
  renderer = new flappy::Renderer(canvasStorage);
  uint8_t count = renderer->skinCount();
  if (count > MaxSkins) count = MaxSkins;
  for (uint8_t i = 0; i < count; ++i) skinNames[i] = renderer->skinName(i);
  app.setSkinNames(skinNames, count);
  loadSettings();
  initializeClock(millis());
  applyDisplaySettings();
  showFrame(millis());
  nextTick = micros() + TickMicros;
}

void loop() {
  M5.update();
  if (!renderer) { delay(10); return; }
  pendingAction |= M5.BtnA.wasPressed();
  const uint32_t nowMs = millis();
  readClock(nowMs);
  updateCoop(nowMs);
  const uint32_t now = micros();
  if (int32_t(now - nextTick) >= 0) {
    int catchUp = 0;
    do {
      stepGame(pendingAction, M5.BtnB.isPressed(), millis());
      pendingAction = false;
      nextTick += TickMicros;
      ++catchUp;
    } while (int32_t(micros() - nextTick) >= 0 && catchUp < 4);
    if (catchUp == 4) nextTick = micros() + TickMicros;
    showFrame(millis());
  } else {
    delay(1);
  }
}
