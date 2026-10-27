#include <M5Unified.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <new>
#include "app_model.h"
#include "ble_link.h"
#include "ble_controller.h"
#include "nes_runtime.h"
#include "nes_viewport.h"
#include "generated_rom.h"
#include "build_clock.h"
#include "render.h"
#include "generated_sounds.h"
#include "stick_power.h"

flappy::AppModel app;
flappy::coop::BleLink coopLink;
flappy::controller::BleController* bluetoothPad = nullptr;
flappy::NesRuntime nes;
flappy::NesViewport nesViewport;
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
uint32_t handledControllerRequest = 0;
uint32_t handledGameRequest = UINT32_MAX;
uint32_t nextNesTick = 0;
uint16_t previousPadButtons = 0;
uint16_t padButtons = 0;
uint8_t pendingNesButtons = 0;
uint8_t controllerMenuPulse = 0;
bool pendingPadAction = false;
bool nesLandscape = false;
int16_t (*nesAudioBuffers)[512] = nullptr;
uint8_t nesAudioIndex = 0;
const char* runtimeNotice = "";
bool pendingAction = false;
bool coopRoundPrepared = false;
bool coopRoundLaunched = false;
uint8_t appliedBrightness = UINT8_MAX;
uint8_t appliedVolume = UINT8_MAX;
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
  app.setVolume(preferences.getUChar("volume", 2));
  app.setSkin(preferences.getUChar("skin", 0));
  app.settings.clock24Hour = preferences.getBool("clock24", true);
  const uint8_t boot = preferences.getUChar("bootgame", 0);
  app.settings.bootGame = boot <= uint8_t(flappy::GameTitle::Nes) ? flappy::GameTitle(boot) : flappy::GameTitle::FlappyBird;
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
  preferences.putUChar("volume", app.settings.volumeIndex);
  preferences.putUChar("skin", app.settings.skinIndex);
  preferences.putBool("clock24", app.settings.clock24Hour);
  preferences.putUChar("bootgame", uint8_t(app.settings.bootGame));
  app.settingsDirty = false;
}

void saveScores() {
  if (!app.highScoresDirty) return;
  preferences.putUShort("best0", app.bestScore(flappy::Mode::Original));
  preferences.putUShort("best3", app.bestScore(flappy::Mode::Arcade));
  preferences.putUShort("best4", app.bestScore(flappy::Mode::Big));
  app.highScoresDirty = false;
}

void playSound(const FlappySounds::Sound& sound) {
  if (app.volume() && sound.length && sound.samples && M5.Speaker.isEnabled()) {
    M5.Speaker.playRaw(sound.samples, sound.length, sound.sampleRate, false, 1, 0, true);
  }
}

void applyOutputSettings() {
  if (appliedVolume != app.settings.volumeIndex) {
    const bool preview = appliedVolume != UINT8_MAX && app.menu.page == flappy::MenuPage::Volume;
    M5.Speaker.setVolume(app.volume());
    appliedVolume = app.settings.volumeIndex;
    if (!app.volume()) M5.Speaker.stop();
    else if (preview) playSound(FlappySounds::wing);
  }
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

void playEvents(uint8_t flags) {
  if (app.currentGame != flappy::GameTitle::FlappyBird || app.autoPlaying()) return;
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
  if (app.settings.coopRole) {
    app.setCurrentGame(flappy::GameTitle::FlappyBird);
    if (bluetoothPad) bluetoothPad->stop();
    previousPadButtons = padButtons = 0;
    pendingPadAction = false;
  }
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

void setNesDisplay(bool landscape) {
  if (nesLandscape == landscape) return;
  nesLandscape = landscape;
  M5.Display.setRotation(landscape ? 1 : 0);
  M5.Display.fillScreen(TFT_BLACK);
  if (landscape) nesViewport.configure(M5.Display.width(), M5.Display.height());
  else viewport.configure(M5.Display.width(), M5.Display.height());
}

void nesVideo(void*, int x1, int y1, int x2, int y2, const uint16_t* source) {
  if (!nesLandscape || app.menuOpen() || !source || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return;
  const int first = nesViewport.firstRow(y1);
  int end = nesViewport.firstRow(y2 + 1);
  if (end > nesViewport.height) end = nesViewport.height;
  const int sourceWidth = x2 - x1 + 1;
  M5.Display.startWrite();
  for (int row = first; row < end; row += StripRows) {
    const int rows = end - row < StripRows ? end - row : StripRows;
    for (int dy = 0; dy < rows; ++dy) {
      const int sy = nesViewport.sourceY(row + dy) - y1;
      for (int x = 0; x < nesViewport.width; ++x) {
        const int sx = nesViewport.sourceX(x) - x1;
        canvasStorage[dy * nesViewport.width + x] = sx >= 0 && sx < sourceWidth ? source[sy * sourceWidth + sx] : 0;
      }
    }
    M5.Display.pushImage(nesViewport.x, nesViewport.y + row, nesViewport.width, rows, canvasStorage);
  }
  M5.Display.endWrite();
}

void nesAudio(void*, const int16_t* samples, size_t count, uint32_t rate) {
  if (!nesAudioBuffers || !app.volume() || app.menuOpen() || !M5.Speaker.isEnabled() || count > 512 || M5.Speaker.isPlaying(1) >= 2) return;
  int16_t* target = nesAudioBuffers[nesAudioIndex];
  memcpy(target, samples, count * sizeof(int16_t));
  if (M5.Speaker.playRaw(target, count, rate, false, 1, 1, false)) nesAudioIndex = uint8_t((nesAudioIndex + 1) % 3);
}

uint8_t nesButtons(uint16_t buttons) {
  using namespace flappy::controller;
  if ((buttons & (Down | A | B)) == (Down | A | B)) return 0;
  if ((buttons & (Up | A)) == (Up | A)) return flappy::NesRuntime::Start;
  if ((buttons & (Down | B)) == (Down | B)) return flappy::NesRuntime::Select;
  uint8_t result = 0;
  if (buttons & A) result |= flappy::NesRuntime::A;
  if (buttons & B) result |= flappy::NesRuntime::B;
  if (buttons & Start) result |= flappy::NesRuntime::Start;
  if (buttons & Select) result |= flappy::NesRuntime::Select;
  if (buttons & Up) result |= flappy::NesRuntime::Up;
  if (buttons & Down) result |= flappy::NesRuntime::Down;
  if (buttons & Left) result |= flappy::NesRuntime::Left;
  if (buttons & Right) result |= flappy::NesRuntime::Right;
  return result;
}

bool controllerMenuDown() {
  using namespace flappy::controller;
  if (padButtons & Menu) return true;
  if (app.menuOpen()) return (padButtons & B) != 0;
  if (app.currentGame == flappy::GameTitle::Nes) return (padButtons & (Down | A | B)) == (Down | A | B);
  return (padButtons & B) != 0;
}

void updateController(uint32_t nowMs) {
  using namespace flappy::controller;
  if (!bluetoothPad) { padButtons = previousPadButtons = 0; return; }
  bluetoothPad->update(nowMs);
  const Snapshot snapshot = bluetoothPad->consumeSnapshot();
  padButtons = snapshot.connected ? snapshot.buttons : 0;
  const uint16_t pressed = snapshot.pressed | (padButtons & ~previousPadButtons);
  previousPadButtons = padButtons;
  app.setControllerStatus(bluetoothPad->statusText());
  const bool releasedMenuTap = (pressed & Menu) && !(padButtons & Menu);
  const bool releasedBTap = (pressed & B) && !(padButtons & B) &&
                            (app.menuOpen() || app.currentGame == flappy::GameTitle::FlappyBird);
  if (releasedMenuTap || releasedBTap) controllerMenuPulse = 2;
  if (app.menuOpen()) {
    if (pressed & (Down | Right)) controllerMenuPulse = 2;
    if (pressed & (Up | Left)) {
      const uint8_t count = app.menuItemCount();
      if (count) app.menu.selection = uint8_t((app.menu.selection + count - 1) % count);
    }
    pendingPadAction |= (pressed & (A | Start)) != 0;
    pendingNesButtons = 0;
  } else if (app.currentGame == flappy::GameTitle::FlappyBird) {
    pendingPadAction |= (pressed & (A | Start)) != 0;
  } else {
    pendingNesButtons |= nesButtons(padButtons | pressed) & uint8_t(~nesButtons(padButtons & ~pressed));
  }
}

void handleControllerRequest(uint32_t nowMs) {
  if (handledControllerRequest == app.controllerRequest) return;
  handledControllerRequest = app.controllerRequest;
  if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  if (!bluetoothPad) bluetoothPad = new (std::nothrow) flappy::controller::BleController;
  if (!bluetoothPad) { app.setControllerStatus("Controller memory unavailable"); return; }
  if (app.controllerAction == 1) bluetoothPad->beginPairing(nowMs);
  else if (app.controllerAction == 3) bluetoothPad->forget();
  else if (bluetoothPad) bluetoothPad->stop();
  padButtons = previousPadButtons = 0;
  pendingNesButtons = 0;
  pendingPadAction = false;
  app.setControllerStatus(bluetoothPad->statusText());
  nextTick = micros() + TickMicros;
}

void handleGameRequest() {
  if (handledGameRequest == app.gameRequest) return;
  handledGameRequest = app.gameRequest;
  M5.Speaker.stop();
  pendingNesButtons = 0;
  pendingPadAction = pendingAction = false;
  runtimeNotice = "";
  if (app.currentGame == flappy::GameTitle::Nes) {
    if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
    if (!nesAudioBuffers) nesAudioBuffers = new (std::nothrow) int16_t[3][512];
    nes.setCallbacks(nesVideo, nesAudio, nullptr);
    if (!nes.ready() && !nes.load(FlappyRom::data, FlappyRom::length)) {
      runtimeNotice = nes.error();
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      handledGameRequest = app.gameRequest;
      app.openMenu(flappy::MenuPage::ChangeGame);
    }
    nextNesTick = micros();
  } else {
    nes.unload();
    // Keep the three audio buffers alive while the speaker drains its stop marker.
  }
  nextTick = micros() + TickMicros;
}

void showFrame(uint32_t nowMs) {
  const bool landscape = app.currentGame == flappy::GameTitle::Nes && !app.menuOpen();
  setNesDisplay(landscape);
  if (landscape) return;
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
  options.notice = runtimeNotice;
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
  const bool inCoop = app.currentGame == flappy::GameTitle::FlappyBird && app.settings.mode == flappy::Mode::Coop;
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
  handleControllerRequest(nowMs);
  handleGameRequest();
  applyOutputSettings();
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
  viewport.configure(M5.Display.width(), M5.Display.height());
  if (viewport.width <= 0 || viewport.height <= 0) return;
  renderer = new flappy::Renderer(canvasStorage);
  uint8_t count = renderer->skinCount();
  if (count > MaxSkins) count = MaxSkins;
  for (uint8_t i = 0; i < count; ++i) skinNames[i] = renderer->skinName(i);
  app.setSkinNames(skinNames, count);
  app.setNesAvailable(FlappyRom::length > 0);
  app.setNesTitle(FlappyRom::title);
  loadSettings();
  app.restoreBootGame();
  handleGameRequest();
  initializeClock(millis());
  applyOutputSettings();
  showFrame(millis());
  nextTick = micros() + TickMicros;
}

void loop() {
  M5.update();
  if (!renderer) { delay(10); return; }
  pendingAction |= M5.BtnA.wasPressed();
  const uint32_t nowMs = millis();
  readClock(nowMs);
  updateController(nowMs);
  updateCoop(nowMs);
  const uint32_t now = micros();
  if (int32_t(now - nextTick) >= 0) {
    int catchUp = 0;
    do {
      bool action = pendingAction || pendingPadAction;
      if (app.currentGame == flappy::GameTitle::Nes && !app.menuOpen()) {
        if (pendingAction) pendingNesButtons |= flappy::NesRuntime::Start;
        action = false;
      }
      const bool menuDown = M5.BtnB.isPressed() || controllerMenuDown() || controllerMenuPulse == 2;
      stepGame(action, menuDown, millis());
      if (controllerMenuPulse) --controllerMenuPulse;
      pendingAction = pendingPadAction = false;
      nextTick += TickMicros;
      ++catchUp;
    } while (int32_t(micros() - nextTick) >= 0 && catchUp < 4);
    if (catchUp == 4) nextTick = micros() + TickMicros;
    showFrame(millis());
  }
  if (app.currentGame == flappy::GameTitle::Nes && !app.menuOpen() && nes.ready()) {
    const uint32_t frameNow = micros();
    if (int32_t(frameNow - nextNesTick) >= 0) {
      setNesDisplay(true);
      nes.step(uint8_t(nesButtons(padButtons) | pendingNesButtons));
      pendingNesButtons = 0;
      const uint32_t duration = 1000000U / nes.frameRate();
      nextNesTick += duration;
      if (int32_t(micros() - nextNesTick) >= int32_t(duration)) nextNesTick = micros();
    }
  }
  delay(1);
}
