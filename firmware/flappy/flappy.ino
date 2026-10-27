#include <M5Unified.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <new>
#include "app_model.h"
#include "ble_link.h"
#include "ble_controller.h"
#include "nes_runtime.h"
#include "nes_sound_events.h"
#include "piezo_sound.h"
#include "piezo_output.h"
#include "nes_viewport.h"
#include "mario_viewport.h"
#include "rom_fingerprint.h"
#include "generated_rom.h"
#include "generated_tetris_rom.h"
#include "generated_zero_rom.h"
#include "tetris_gym.h"
#include "tetris_viewport.h"
#include "tetris_zero.h"
#include "tetris_zero_viewport.h"
#include "display_damage.h"
#include "async_display.h"
#include "tetris_input.h"
#include "tetris_actions.h"
#include "build_clock.h"
#include "build_display.h"
#include "stick_display.h"
#include "nes_system_button.h"
#include "render.h"
#include "generated_sounds.h"
#include "stick_power.h"
#include "inactivity.h"

flappy::AppModel app;
flappy::coop::BleLink coopLink;
flappy::controller::BleController* bluetoothPad = nullptr;
flappy::NesRuntime nes;
flappy::PiezoSound piezo;
flappy::PiezoOutput piezoOutput;
flappy::TetrisSoundTracker tetrisSounds[2];
bool piezoDriver = false, soundReady = false;
uint16_t soundFrequency = 0, soundDuty = 0;
uint32_t soundOverrideUntil = 0;
bool soundOverride = false;
uint8_t previousSoundPad[2] = {};
flappy::NesSystemButton nesSystemButton;
flappy::NesViewport nesViewport;
flappy::MarioViewport marioViewport;
flappy::MarioCartridge marioCartridge;
flappy::MarioLauncher marioLauncher;
flappy::MarioController marioController;
flappy::TetrisViewport tetrisViewport;
flappy::TetrisZeroViewport zeroViewport;
flappy::TetrisZero::Options zeroOptions;
flappy::TetrisZero::Launcher zeroLauncher;
flappy::DamageRows displayDamage;
flappy::AsyncDisplay asyncDisplay;
flappy::InactivityPolicy inactivity;
flappy::PowerSource powerSource = flappy::PowerSource::Unknown;
uint32_t lastPowerPollMs = 0;
bool powerPolled = false;
struct PeerTetrisHud {
  bool valid = false, fresh = false, toppedOut = false, large = false;
  uint32_t score = 0;
  uint16_t lines = 0;
} peerTetrisHud;
flappy::TetrisButtons tetrisButtons;
flappy::TetrisTilt tetrisTilt;
flappy::TetrisSoftDrop tetrisSoftDrop;
flappy::TetrisSoftDrop zeroDrop1, zeroDrop2;
flappy::TetrisActions tetrisActions[2];
flappy::TetrisGym::Options tetrisOptions;
flappy::Viewport viewport;
Preferences preferences;
constexpr int StripRows = 8;
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
bool controllerConnected = false;
uint8_t pendingNesButtons = 0;
uint8_t pendingControllerNesButtons = 0;
bool pendingPadAction = false;
bool nesLandscape = false;
int16_t (*nesAudioBuffers)[512] = nullptr;
uint8_t nesAudioIndex = 0;
const char* runtimeNotice = "";
bool pendingAction = false;
bool pendingTetrisDrop = false;
bool pendingControllerTetrisDrop = false;
bool controllerDownTracking = false;
uint32_t controllerDownSince = 0;
bool nativeMario = false, marioLaunching = false;
uint8_t gameMessageShown = 0;
uint32_t tetrisFrontendAt = 0;
bool tetrisLaunching = false;
uint8_t tetrisLaunchStage = 0, tetrisStableFrames = 0;
uint8_t tetrisHeld = 0;
uint32_t handledTetrisRequest = 0, handledTetrisCalibrate = 0, handledTetrisLink = 0, handledRestart = 0;
bool tetrisRoundPrepared = false;
bool zeroLaunching = false, zeroRoundPrepared = false, zeroReadyWhenPrepared = false;
bool zeroFinishedRound = false, zeroSampleSubmitted = false;
bool zeroPadArmed = false;
uint32_t handledZeroRequest = 0, zeroPreparedSeed = 0, zeroSampleFrame = UINT32_MAX;
uint8_t zeroSamplePad = 0;
float tiltX = 0;
uint32_t lastTiltPollMs = 0, lastTiltSampleMs = 0, tiltZeroNoticeMs = 0;
bool haveTiltSample = false;
const char* tiltZeroNotice = "";
bool coopRoundPrepared = false;
bool coopRoundLaunched = false;
uint8_t appliedBrightness = UINT8_MAX;
uint8_t appliedVolume = UINT8_MAX;
uint8_t appliedSkin = UINT8_MAX;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
uint64_t nesWorkUs = 0, nesVideoUs = 0;
uint32_t nesRenderedFrames = 0, lcdFrames = 0, lcdRows = 0, appTicks = 0, appDrawFrames = 0;
uint64_t lcdUs = 0, appRenderUs = 0;
uint16_t marioBenchmarkFrames = 0;
uint8_t benchmarkPrgCache = 0, benchmarkVideoDivisor = 0;
uint8_t benchmarkIdleJump = 2;
bool benchmarkPcm = false;
bool timingProbe = false;
uint16_t timingIntervals = 0;
uint32_t timingStart = 0, timingPrevious = 0, timingMin = UINT32_MAX, timingMax = 0;
uint32_t timingSourceStart = 0, timingPresentStart = 0;
uint64_t timingPresentUs = 0;
#endif

void invalidateDisplay() {
  displayDamage.invalidate();
  marioViewport.invalidate();
  tetrisViewport.invalidate();
  zeroViewport.invalidate();
  peerTetrisHud.valid = false;
}

uint32_t nonzeroRandom() {
  const uint32_t value = esp_random();
  return value ? value : 1;
}

bool emulatedGame() { return app.currentGame != flappy::GameTitle::FlappyBird; }

void configureNesPerformance() {
  nes.setPcmEnabled(!piezoDriver);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  nes.setPcmEnabled(benchmarkPcm);
  nes.setPrgCache(benchmarkPrgCache);
  if (benchmarkIdleJump < 2) nes.setIdleJump(benchmarkIdleJump != 0);
#endif
}

bool tetrisLinked() {
  return coopLink.gameProfile() == flappy::coop::GameProfile::Tetris &&
         coopLink.session().role() != flappy::coop::Role::Off;
}

bool zeroLinked() {
  return coopLink.gameProfile() == flappy::coop::GameProfile::TetrisZero &&
         coopLink.session().role() != flappy::coop::Role::Off;
}

bool tetrisLike() {
  return app.tetrisFrontendActive();
}

bool tetrisLinkActive() { return tetrisLinked() || zeroLinked(); }

void resetSoundTracking() {
  tetrisSounds[0].reset();
  tetrisSounds[1].reset();
  previousSoundPad[0] = previousSoundPad[1] = 0;
}

void stopGameAudio() {
  piezo.stop();
  resetSoundTracking();
  if (!piezoDriver) M5.Speaker.stop();
}

void playCue(flappy::PiezoCue cue, uint32_t nowMs) {
  piezo.setQuiet(app.currentGame == flappy::GameTitle::FlappyBird && !app.menuOpen() && app.autoPlaying());
  piezo.setVolumeIndex(app.settings.volumeIndex);
  piezo.play(cue, nowMs);
}

struct PiezoDriver {
  uint32_t setFrequency(uint16_t frequency) { return ledcChangeFrequency(2, frequency, 10); }
  bool writeDuty(uint16_t duty) { return ledcWrite(2, duty); }
};

void servicePiezoOutput() {
  PiezoDriver driver;
  piezoOutput.service(driver);
  soundFrequency = piezoOutput.frequency();
  soundDuty = piezoOutput.duty();
}

void initializeSound() {
  piezoDriver = M5.getBoard() == m5::board_t::board_M5StickCPlus;
  if (piezoDriver) {
    M5.Speaker.end();
    rtc_gpio_deinit(GPIO_NUM_2);
    gpio_pullup_dis(GPIO_NUM_2);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    gpio_pulldown_en(GPIO_NUM_2);
    soundReady = ledcAttach(2, 2500, 10);
    if (soundReady) {
      soundReady = ledcWrite(2, 0);
      if (soundReady) piezoOutput.reset(2500, 0);
    }
  } else soundReady = M5.Speaker.isEnabled();
}

void serviceSound(uint32_t nowMs) {
  piezo.setQuiet(app.currentGame == flappy::GameTitle::FlappyBird && !app.menuOpen() && app.autoPlaying());
  piezo.setVolumeIndex(app.settings.volumeIndex);
  if (soundOverride) {
    if (int32_t(nowMs - soundOverrideUntil) < 0) {
      if (piezoDriver && soundReady) servicePiezoOutput();
      return;
    }
    soundOverride = false;
    if (piezoDriver && soundReady) piezoOutput.request(0, 0);
    else if (soundReady) M5.Speaker.stop(0);
    soundFrequency = soundDuty = 0;
    piezo.stop();
  }
  flappy::PiezoTone tone;
  if (piezo.update(nowMs, tone) && soundReady) {
    const uint16_t duty = tone.frequency ? uint16_t(uint32_t(tone.volume) * 511u / 255u) : 0;
    if (piezoDriver) piezoOutput.request(tone.frequency, duty);
    else {
      soundFrequency = tone.frequency;
      soundDuty = duty;
      if (tone.frequency && tone.volume) M5.Speaker.tone(tone.frequency, tone.durationMs, 0, true);
      else M5.Speaker.stop(0);
    }
  }
  if (piezoDriver && soundReady) servicePiezoOutput();
}

void menuSound(flappy::MenuPage before, uint8_t selection, bool action, uint32_t nowMs) {
  if (before != app.menu.page) {
    stopGameAudio();
    playCue(app.menu.page == flappy::MenuPage::Closed || app.menu.page == flappy::MenuPage::Home ?
        flappy::PiezoCue::MenuBack : flappy::PiezoCue::MenuSelect, nowMs);
  } else if (app.menuOpen() && action) playCue(flappy::PiezoCue::MenuSelect, nowMs);
  else if (app.menuOpen() && selection != app.menu.selection) playCue(flappy::PiezoCue::MenuMove, nowMs);
}

void updateNesSound(uint8_t input1, uint8_t input2, uint32_t nowMs) {
  if (!nes.ready() || app.menuOpen() || tetrisLaunching || zeroLaunching ||
      (tetrisLinkActive() && coopLink.session().phase() != flappy::coop::Phase::Playing)) {
    piezo.setNesTone(0, 0, nowMs);
    resetSoundTracking();
    return;
  }
  const uint8_t pressed = uint8_t((input1 & ~previousSoundPad[0]) | (input2 & ~previousSoundPad[1]));
  previousSoundPad[0] = input1;
  previousSoundPad[1] = input2;
  if (tetrisLike()) {
    if (pressed & (flappy::NesRuntime::A | flappy::NesRuntime::B)) playCue(flappy::PiezoCue::Rotate, nowMs);
    else if (pressed & flappy::NesRuntime::Down) playCue(flappy::PiezoCue::Drop, nowMs);
    else if (pressed & (flappy::NesRuntime::Left | flappy::NesRuntime::Right)) playCue(flappy::PiezoCue::Move, nowMs);
    uint8_t events = 0;
    const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;
    const uint8_t players = zero ? flappy::TetrisZero::players(nes) : 1;
    for (uint8_t player = 1; player <= players; ++player) {
      flappy::TetrisSoundSnapshot snapshot;
      snapshot.playing = zero ? flappy::TetrisZero::playing(nes) : flappy::TetrisGym::playing(nes);
      snapshot.active = zero ? flappy::TetrisZero::active(nes, player) : nes.readRam(0x48) == 1;
      snapshot.pieceY = zero ? flappy::TetrisZero::pieceY(nes, player) : nes.readRam(0x41);
      snapshot.lines = zero ? flappy::TetrisZero::lines(nes, player) : flappy::TetrisGym::lines(nes);
      snapshot.level = zero ? flappy::TetrisZero::level(nes, player) : flappy::TetrisGym::level(nes);
      snapshot.toppedOut = zero ? flappy::TetrisZero::finished(nes) &&
          !(zeroOptions.type && snapshot.lines == 0) : flappy::TetrisGym::toppedOut(nes);
      events |= tetrisSounds[player - 1].sample(snapshot);
    }
    switch (flappy::TetrisSoundTracker::priority(events)) {
      case flappy::NesSoundLineClear: playCue(flappy::PiezoCue::LineClear, nowMs); break;
      case flappy::NesSoundLevelUp: playCue(flappy::PiezoCue::LevelUp, nowMs); break;
      case flappy::NesSoundCrash: playCue(flappy::PiezoCue::Crash, nowMs); break;
      case flappy::NesSoundLock: playCue(flappy::PiezoCue::Lock, nowMs); break;
      default: break;
    }
  } else {
    if (pressed & flappy::NesRuntime::A) playCue(flappy::PiezoCue::Jump, nowMs);
    else if (pressed & flappy::NesRuntime::B) playCue(flappy::PiezoCue::Action, nowMs);
  }
  if (!piezoDriver) return;
  const nes_t* core = nes.core();
  const pulse_t* channels[] = {&core->nes_apu.pulse1, &core->nes_apu.pulse2};
  uint16_t frequency = 0;
  uint8_t level = 0;
  for (uint8_t channel = 0; channel < 2; ++channel) {
    const pulse_t& pulse = *channels[channel];
    if (!(core->nes_apu.status & (1u << channel)) || !pulse.length_counter) continue;
    const uint8_t volume = pulse.constant_volume ? pulse.envelope_lowers : pulse.envelope_volume;
    const uint16_t candidate = flappy::PiezoSound::nesPulseFrequency(core->timing.cpu_clock, pulse.cur_period);
    if (candidate && volume > level) { level = volume; frequency = candidate; }
  }
  piezo.setNesTone(frequency, level, nowMs);
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
  uint8_t brightness = preferences.getUChar("brightness", 3);
  if (!preferences.getUChar("brightver", 0)) {
    brightness = brightness > 2 ? (brightness > 3 ? 2 : 1) : 0;
    preferences.putUChar("brightness", brightness);
    preferences.putUChar("brightver", 1);
  }
  app.setBrightness(brightness);
  app.setVolume(preferences.getUChar("volume", 2));
  app.setSleep(preferences.getUChar("sleepidx", 0));
  app.setSkin(preferences.getUChar("skin", 0));
  app.settings.clock24Hour = preferences.getBool("clock24", true);
  const uint8_t boot = preferences.getUChar("bootgame", 0);
  app.settings.bootGame = boot < flappy::GameTitleCount ? flappy::GameTitle(boot) : flappy::GameTitle::FlappyBird;
  app.setTetrisLevel(preferences.getUChar("tetlevel", 0));
  app.setTetrisGameType(preferences.getUChar("tettype", 0));
  app.setTetrisControls(preferences.getUChar("tetctrl", 0));
  app.setTetrisTilt(true);
  if (preferences.isKey("tetneutral")) tetrisTilt.calibrate(preferences.getFloat("tetneutral", 0));
  if (!preferences.getUChar("timingver", 0)) {
    preferences.putBool("tetpal", false);
    preferences.putUChar("timingver", 1);
  }
  app.setTetrisPal(preferences.getBool("tetpal", false));
  app.setTetrisLarge(preferences.getBool("tetlarge", true));
  app.setTetrisZeroLevel(preferences.getUChar("zerolevel", 0));
  app.setTetrisZeroGameType(preferences.getUChar("zerotype", 0));
  app.setTetrisZeroPlayers(preferences.getUChar("zeroplayers", 2));
  app.setTetrisPlayers(preferences.getUChar("tetrisplayers", boot == 3 ? app.settings.tetrisZeroPlayers : 1));
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
  preferences.putUChar("sleepidx", app.settings.sleepIndex);
  preferences.putUChar("skin", app.settings.skinIndex);
  preferences.putBool("clock24", app.settings.clock24Hour);
  preferences.putUChar("bootgame", uint8_t(app.settings.bootGame));
  preferences.putUChar("tetlevel", app.settings.tetrisLevel);
  preferences.putUChar("tettype", app.settings.tetrisGameType);
  preferences.putUChar("tetctrl", app.settings.tetrisControls);
  preferences.putBool("tettilt", app.settings.tetrisTilt);
  preferences.putBool("tetpal", app.settings.tetrisPal);
  preferences.putBool("tetlarge", app.settings.tetrisLarge);
  preferences.putUChar("zerolevel", app.settings.tetrisZeroLevel);
  preferences.putUChar("zerotype", app.settings.tetrisZeroGameType);
  preferences.putUChar("zeroplayers", app.settings.tetrisZeroPlayers);
  preferences.putUChar("tetrisplayers", app.settings.tetrisPlayers);
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
  if (!piezoDriver && app.volume() && sound.length && sound.samples && M5.Speaker.isEnabled()) {
    M5.Speaker.playRaw(sound.samples, sound.length, sound.sampleRate, false, 1, 0, true);
  }
}

void applyOutputSettings() {
  if (appliedVolume != app.settings.volumeIndex) {
    const bool preview = appliedVolume != UINT8_MAX && app.menu.page == flappy::MenuPage::Volume;
    piezo.setVolumeIndex(app.settings.volumeIndex);
    if (!piezoDriver) M5.Speaker.setVolume(app.volume());
    appliedVolume = app.settings.volumeIndex;
    if (!app.volume()) stopGameAudio();
    else if (preview) playCue(flappy::PiezoCue::Preview, millis());
  }
  if (appliedBrightness != app.settings.brightnessIndex) {
    asyncDisplay.drain();
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
  if (piezoDriver) {
    if (flags & (flappy::Hit | flappy::Die)) playCue(flappy::PiezoCue::Crash, millis());
    else if (flags & flappy::Point) playCue(flappy::PiezoCue::Point, millis());
    else if (flags & flappy::Flap) playCue(flappy::PiezoCue::Flap, millis());
    return;
  }
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
  invalidateDisplay();
  coopRoundPrepared = false;
  coopRoundLaunched = false;
  tetrisRoundPrepared = false;
  zeroRoundPrepared = zeroReadyWhenPrepared = zeroFinishedRound = false;
  zeroSampleFrame = UINT32_MAX;
  zeroDrop1.reset();
  zeroDrop2.reset();
  zeroPadArmed = false;
  app.tetrisLinkRole = 0;
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
    nes.setPrgCache(0);
    if (bluetoothPad) bluetoothPad->stop();
    previousPadButtons = padButtons = 0;
    pendingPadAction = false;
  }
  if (app.settings.coopRole == 1) {
    coopLink.setGameProfile(flappy::coop::GameProfile::Flappy);
    coopLink.beginHost(nowMs, nonzeroRandom(), nonzeroRandom(), uint16_t(esp_random() % 360));
  } else if (app.settings.coopRole == 2) {
    coopLink.setGameProfile(flappy::coop::GameProfile::Flappy);
    coopLink.beginJoin(nowMs);
  } else {
    coopLink.stop();
  }
  nextTick = micros() + TickMicros;
}

void updateCoop(uint32_t nowMs) {
  if (tetrisLinkActive()) return;
  coopLink.setLocal(localSnapshot());
  coopLink.update(nowMs, app.game.worldTick());
  flappy::coop::StartInfo start;
  if (coopLink.consumeStart(start)) {
    stopGameAudio();
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
  asyncDisplay.drain();
  nesLandscape = landscape;
  invalidateDisplay();
  M5.Display.setRotation(landscape ? 1 : 0);
  M5.Display.fillScreen(TFT_BLACK);
  if (landscape) {
    nesViewport.configure(M5.Display.width(), M5.Display.height());
    marioViewport.configure(M5.Display.width(), M5.Display.height());
  }
  else viewport.configure(M5.Display.width(), M5.Display.height());
}

void submitDisplay(int x, int y, int width, int height, const uint16_t* pixels, int stride = 0) {
  if (!stride) stride = width;
  for (int row = 0; row < height; row += flappy::DisplayStrip::MaxRows) {
    const int rows = height - row < flappy::DisplayStrip::MaxRows ? height - row : flappy::DisplayStrip::MaxRows;
    const uint16_t* source = pixels + row * stride;
    if (asyncDisplay.enabled() && asyncDisplay.submit(x, y + row, width, rows, source, stride)) continue;
    asyncDisplay.drain();
    if (asyncDisplay.enabled()) invalidateDisplay();
    M5.Display.startWrite();
    if (stride == width) M5.Display.pushImage(x, y + row, width, rows, source);
    else for (int dy = 0; dy < rows; ++dy)
      M5.Display.pushImage(x, y + row + dy, width, 1, source + dy * stride);
    M5.Display.endWrite();
  }
}

void recordLcdFrame(unsigned rows, uint32_t start) {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  if (rows) { ++lcdFrames; lcdRows += rows; }
  lcdUs += uint32_t(micros() - start);
#else
  (void)rows; (void)start;
#endif
}

void pushGameFrame(int width, int height, bool exactTetris = false, int peerDirtyFrom = -1) {
  const uint32_t start = micros();
  unsigned pushed = 0;
  displayDamage.beginFrame(width, height, millis());
  int first = -1;
  for (int row = 0; row <= height; ++row) {
    const bool changed = row < height && (exactTetris ?
        (app.currentGame == flappy::GameTitle::TetrisZero ? zeroViewport.rowChanged(row) :
          tetrisViewport.rowChanged(row)) || (peerDirtyFrom >= 0 && row >= peerDirtyFrom) :
        displayDamage.changed(row, canvasStorage + row * width));
    if (changed && first < 0) first = row;
    if (!changed && first >= 0) {
      submitDisplay(0, first, width, row - first, canvasStorage + first * width);
      pushed += unsigned(row - first);
      first = -1;
    }
  }
  displayDamage.endFrame();
  recordLcdFrame(pushed, start);
}

void pushAppCanvas() {
  const uint32_t start = micros();
  unsigned pushed = 0;
  displayDamage.beginFrame(viewport.width, viewport.height, millis());
  for (int y = 0; y < viewport.height; y += StripRows) {
    const int rows = viewport.height - y < StripRows ? viewport.height - y : StripRows;
    viewport.copyRows(canvasStorage, stripStorage, y, rows);
    int first = -1;
    for (int dy = 0; dy <= rows; ++dy) {
      const bool changed = dy < rows && displayDamage.changed(y + dy, stripStorage + dy * viewport.width);
      if (changed && first < 0) first = dy;
      if (!changed && first >= 0) {
        submitDisplay(viewport.outputX, viewport.outputY + y + first, viewport.width,
            dy - first, stripStorage + first * viewport.width);
        pushed += unsigned(dy - first);
        first = -1;
      }
    }
  }
  displayDamage.endFrame();
  recordLcdFrame(pushed, start);
}

void showGameMessage(uint8_t message) {
  if (gameMessageShown == message || !renderer) return;
  gameMessageShown = message;
  invalidateDisplay();
  if (message == 2) {
    renderer->drawMessageLandscape("Mario", "Starting game");
    pushGameFrame(240, 135);
  } else {
    renderer->drawMessage("Tetris", message == 3 ? "Game over" :
        message == 4 ? "Returning to settings" : "Starting game");
    pushAppCanvas();
  }
}

bool tetrisNativePlaying() {
  return app.currentGame == flappy::GameTitle::TetrisZero ?
      flappy::TetrisZero::playing(nes) && !flappy::TetrisZero::finished(nes) :
      flappy::TetrisGym::playing(nes) && !flappy::TetrisGym::finished(nes);
}

void openTetrisFrontend(bool focusRestart) {
  if (!tetrisLike()) return;
  if (tetrisLinkActive()) stopCoop();
  app.openTetrisSettings();
  if (focusRestart && app.menuItemCount() >= 2) app.menu.selection = uint8_t(app.menuItemCount() - 2);
  stopGameAudio();
  invalidateDisplay();
  gameMessageShown = 0;
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = 0;
  pendingTetrisDrop = pendingControllerTetrisDrop = false;
  tetrisButtons.reset();
  tetrisSoftDrop.reset();
  zeroDrop1.reset();
  zeroDrop2.reset();
}

void updateTetrisFrontend(uint32_t nowMs) {
  if (!tetrisLike() || app.menuOpen() || tetrisLinkActive() || tetrisLaunching || zeroLaunching || !nes.ready()) {
    tetrisFrontendAt = 0;
    return;
  }
  if (tetrisNativePlaying()) { tetrisFrontendAt = 0; return; }
  if (!tetrisFrontendAt) { tetrisFrontendAt = nowMs ? nowMs : 1; return; }
  if (uint32_t(nowMs - tetrisFrontendAt) >= 1000) {
    openTetrisFrontend(true);
    tetrisFrontendAt = 0;
  }
}

void prepareTetrisView(bool zero) {
  if (zero) {
    zeroViewport.setLargeCells(app.settings.tetrisLarge);
    for (uint8_t player = 1; player <= flappy::TetrisZero::players(nes); ++player) {
      zeroViewport.setHeld(player, tetrisActions[player - 1].heldOrientation());
      zeroViewport.setNext(player, nes.readRam(player == 2 ? 0x7ff1 : 0xbf));
      zeroViewport.setStats(player, flappy::TetrisZero::score(nes, player),
          flappy::TetrisZero::lines(nes, player), flappy::TetrisZero::level(nes, player));
    }
  } else {
    tetrisViewport.setLargeCells(app.settings.tetrisLarge);
    tetrisViewport.setHeld(tetrisActions[0].heldOrientation());
    tetrisViewport.setNext(nes.readRam(0xbf));
    tetrisViewport.setStats(flappy::TetrisGym::score(nes), flappy::TetrisGym::lines(nes),
        flappy::TetrisGym::level(nes));
  }
}

void nesVideo(void*, int x1, int y1, int x2, int y2, const uint16_t* source) {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  struct VideoTimer {
    uint32_t start = micros();
    ~VideoTimer() { nesVideoUs += uint32_t(micros() - start); }
  } videoTimer;
  if (y2 == 239) ++nesRenderedFrames;
#endif
  if (tetrisLike() && !app.menuOpen()) {
    if (tetrisLaunching || zeroLaunching || !tetrisNativePlaying()) return;
    if (tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || zeroLaunching)) return;
    const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;

    const bool complete = zero ? zeroViewport.consume(x1, y1, x2, y2, source, canvasStorage,
        flappy::TetrisZero::playing(nes), flappy::TetrisZero::players(nes) == 2) :
        tetrisViewport.consume(x1, y1, x2, y2, source, canvasStorage, flappy::TetrisGym::playing(nes));
    if (complete) {
      gameMessageShown = 0;
      int peerDirtyFrom = -1;
      if (tetrisLinked()) {
        const auto peer = coopLink.remoteTetris(millis());
        const int px = app.settings.tetrisLarge ? 111 : 0;
        int py = app.settings.tetrisLarge ? 116 : 70;
        const bool changed = !peerTetrisHud.valid || peerTetrisHud.fresh != peer.fresh ||
            peerTetrisHud.toppedOut != peer.stats.toppedOut ||
            peerTetrisHud.score != peer.stats.score || peerTetrisHud.lines != peer.stats.lines ||
            peerTetrisHud.large != app.settings.tetrisLarge;
        if (changed) {
          peerTetrisHud = {true, peer.fresh, peer.stats.toppedOut, app.settings.tetrisLarge,
              peer.stats.score, peer.stats.lines};
          peerDirtyFrom = py;
          const int clearWidth = app.settings.tetrisLarge ? tetrisViewport.width - px : 18;
          for (int row = py; row < tetrisViewport.height; ++row)
            memset(canvasStorage + row * tetrisViewport.width + px, 0, size_t(clearWidth) * sizeof(uint16_t));
          flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width, tetrisViewport.height,
              tetrisViewport.width, "P2", px, py);
          py += 14;
          char text[16];
          if (peer.fresh) {
            snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(peer.stats.score));
            for (unsigned offset = 0; text[offset]; offset += 3) {
              char group[4] = {};
              for (unsigned i = 0; i < 3 && text[offset + i]; ++i) group[i] = text[offset + i];
              flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width, tetrisViewport.height,
                  tetrisViewport.width, group, px, py);
              py += 12;
              if (strlen(text + offset) <= 3) break;
            }
            py += 4;
            flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width, tetrisViewport.height,
                tetrisViewport.width, "Ln", px, py);
            snprintf(text, sizeof(text), "%u", unsigned(peer.stats.lines));
            flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width, tetrisViewport.height,
                tetrisViewport.width, text, px, py + 12);
            if (peer.stats.toppedOut) flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width,
                tetrisViewport.height, tetrisViewport.width, "END", px, py + 28);
          } else flappy::TetrisViewport::label(canvasStorage, tetrisViewport.width, tetrisViewport.height,
              tetrisViewport.width, "...", px, py);
        }
      }
      pushGameFrame(zero ? zeroViewport.width : tetrisViewport.width,
          zero ? zeroViewport.height : tetrisViewport.height, true, peerDirtyFrom);
    }
    return;
  }
  if (!nesLandscape || app.menuOpen() || !source || x1 < 0 || y1 < 0 || x2 > 255 || y2 > 239 || x2 < x1 || y2 < y1) return;
  if (nativeMario) {
    if (marioLaunching) return;
    static unsigned submittedRows = 0;
    static uint32_t submitUs = 0;
    if (y1 == 0) { submittedRows = 0; submitUs = 0; }
    const uint32_t submittedAt = micros();
    const bool complete = marioViewport.consume(x1, y1, x2, y2, source, canvasStorage);
    int first = -1;
    int previous = -2;
    for (int sy = y1; sy <= y2 + 1; ++sy) {
      const int dy = sy <= y2 ? marioViewport.targetRow(sy) : -1;
      const bool changed = dy >= 0 && marioViewport.rowChanged(dy);
      if (first >= 0 && (!changed || dy != previous + 1)) {
        submitDisplay(0, first, 240, previous + 1 - first, canvasStorage + first * 240);
        submittedRows += unsigned(previous + 1 - first);
        first = -1;
      }
      if (changed && first < 0) first = dy;
      if (changed) previous = dy;
    }
    submitUs += uint32_t(micros() - submittedAt);
    if (complete) {
      gameMessageShown = 0;
      recordLcdFrame(submittedRows, micros() - submitUs);
      if (asyncDisplay.enabled()) asyncDisplay.submitEndFrame();
    }
    return;
  }
  const int first = nesViewport.firstRow(y1);
  int end = nesViewport.firstRow(y2 + 1);
  if (end > nesViewport.height) end = nesViewport.height;
  const int sourceWidth = x2 - x1 + 1;
  if (y1 == 0) displayDamage.beginFrame(nesViewport.width, nesViewport.height, millis());
  for (int row = first; row < end; row += StripRows) {
    const int rows = end - row < StripRows ? end - row : StripRows;
    for (int dy = 0; dy < rows; ++dy) {
      const int sy = nesViewport.sourceY(row + dy) - y1;
      for (int x = 0; x < nesViewport.width; ++x) {
        const int sx = nesViewport.sourceX(x) - x1;
        canvasStorage[dy * nesViewport.width + x] = sx >= 0 && sx < sourceWidth ? source[sy * sourceWidth + sx] : 0;
      }
    }
    int firstChanged = -1;
    for (int dy = 0; dy <= rows; ++dy) {
      const bool changed = dy < rows && displayDamage.changed(row + dy, canvasStorage + dy * nesViewport.width);
      if (changed && firstChanged < 0) firstChanged = dy;
      if (!changed && firstChanged >= 0) {
        submitDisplay(nesViewport.x, nesViewport.y + row + firstChanged, nesViewport.width,
            dy - firstChanged, canvasStorage + firstChanged * nesViewport.width);
        firstChanged = -1;
      }
    }
  }
  if (y2 == 239) displayDamage.endFrame();
}

void nesAudio(void*, const int16_t* samples, size_t count, uint32_t rate) {
  if (piezoDriver) return;
  if (tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || zeroLaunching)) return;
  if (!nesAudioBuffers || !app.volume() || app.menuOpen() || !M5.Speaker.isEnabled() || count > 512 || M5.Speaker.isPlaying(1) >= 2) return;
  int16_t* target = nesAudioBuffers[nesAudioIndex];
  memcpy(target, samples, count * sizeof(int16_t));
  if (M5.Speaker.playRaw(target, count, rate, false, 1, 1, false)) nesAudioIndex = uint8_t((nesAudioIndex + 1) % 3);
}

uint8_t nesButtons(uint16_t buttons) {
  using namespace flappy::controller;
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

bool controllerDownHeld(uint32_t nowMs) {
  if (!controllerConnected || app.menuOpen() || !tetrisLike() ||
      !(padButtons & flappy::controller::Down)) {
    controllerDownTracking = false;
    return false;
  }
  if (!controllerDownTracking) { controllerDownTracking = true; controllerDownSince = nowMs; }
  return uint32_t(nowMs - controllerDownSince) >= 200;
}

uint8_t tetrisControllerButtons(uint32_t nowMs) {
  uint8_t result = uint8_t((nesButtons(padButtons) | pendingControllerNesButtons) &
      ~(flappy::NesRuntime::Start | flappy::NesRuntime::Select | flappy::NesRuntime::Down));
  if (controllerDownHeld(nowMs)) result |= flappy::NesRuntime::Down;
  return result;
}

void updateController(uint32_t nowMs) {
  using namespace flappy::controller;
  if (!bluetoothPad) { padButtons = previousPadButtons = 0; controllerConnected = false; return; }
  bluetoothPad->update(nowMs);
  const Snapshot snapshot = bluetoothPad->consumeSnapshot();
  if (snapshot.buttons || snapshot.pressed) inactivity.activity(nowMs);
  const bool justConnected = snapshot.connected && !controllerConnected;
  const flappy::MenuPage menuBefore = app.menu.page;
  const uint8_t selectionBefore = app.menu.selection;
  controllerConnected = snapshot.connected;
  padButtons = snapshot.connected ? snapshot.buttons : 0;
  uint16_t pressed = snapshot.pressed | (padButtons & ~previousPadButtons);
  previousPadButtons = padButtons;
  app.setControllerStatus(bluetoothPad->statusText());
  if (justConnected && app.menu.page == flappy::MenuPage::Controller) {
    app.closeMenu();
    pendingNesButtons = pendingControllerNesButtons = 0;
    pendingPadAction = pendingAction = false;
    menuSound(menuBefore, selectionBefore, false, nowMs);
    return;
  }
  if (tetrisLike()) {
    if (!app.menuOpen() && (pressed & (Start | Select))) openTetrisFrontend(false);
    pressed &= uint16_t(~(Start | Select));
    padButtons &= uint16_t(~(Start | Select));
    if (!app.menuOpen() && (pressed & Down)) pendingControllerTetrisDrop = true;
  }
  if ((pressed & Menu) || ((pressed & B) && (app.menuOpen() || !emulatedGame()))) {
    app.openMenuFromController();
    pendingNesButtons = pendingControllerNesButtons = 0;
    nesSystemButton.reset(M5.BtnA.isPressed());
    tetrisButtons.reset();
    tetrisSoftDrop.reset();
    pendingTetrisDrop = pendingControllerTetrisDrop = false;
  }
  if (app.menuOpen()) {
    if (pressed & (Down | Right)) app.openMenuFromController();
    if (pressed & (Up | Left)) {
      const uint8_t count = app.menuItemCount();
      if (count) app.menu.selection = uint8_t((app.menu.selection + count - 1) % count);
    }
    pendingPadAction |= (pressed & (A | Start)) != 0;
    pendingNesButtons = pendingControllerNesButtons = 0;
  } else if (app.currentGame == flappy::GameTitle::FlappyBird) {
    pendingPadAction |= (pressed & (A | Start)) != 0;
  } else {
    if (tetrisLinkActive() && (coopLink.session().phase() == flappy::coop::Phase::Lobby ||
        (zeroLinked() ? zeroFinishedRound : flappy::TetrisGym::toppedOut(nes))))
      pendingPadAction |= (pressed & (A | Start)) != 0;
    pendingControllerNesButtons |= nesButtons(padButtons | pressed) & uint8_t(~nesButtons(padButtons & ~pressed));
  }
  menuSound(menuBefore, selectionBefore, false, nowMs);
  controllerDownHeld(nowMs);
}

void handleControllerRequest(uint32_t nowMs) {
  if (handledControllerRequest == app.controllerRequest) return;
  handledControllerRequest = app.controllerRequest;
  if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  nes.setPrgCache(0);
  if (!bluetoothPad) bluetoothPad = new (std::nothrow) flappy::controller::BleController;
  if (!bluetoothPad) { app.setControllerStatus("Controller memory unavailable"); return; }
  if (app.controllerAction == 1) bluetoothPad->beginPairing(nowMs);
  else if (app.controllerAction == 3) bluetoothPad->forget();
  else if (bluetoothPad) bluetoothPad->stop();
  padButtons = previousPadButtons = 0;
  pendingNesButtons = pendingControllerNesButtons = 0;
  pendingPadAction = false;
  app.setControllerStatus(bluetoothPad->statusText());
  nextTick = micros() + TickMicros;
}

bool beginZero(const flappy::TetrisZero::Options& options) {
  stopGameAudio();
  if (!flappy::RomFingerprint::tetris(TetrisZeroRom::data, TetrisZeroRom::length, true)) {
    runtimeNotice = "Unsupported Tetris cartridge";
    return false;
  }
  nes.setCallbacks(nesVideo, piezoDriver ? nullptr : nesAudio, nullptr);
  if (!nes.load(TetrisZeroRom::data, TetrisZeroRom::length)) return false;
  configureNesPerformance();
  nes.setPal(false);
  for (uint16_t address = 0x6000; address < 0x8000; ++address) nes.writeRam(address, 0);
  zeroOptions = options;
  zeroLauncher.reset(zeroOptions);
  zeroLaunching = true;
  tetrisLaunching = false;
  zeroSampleFrame = UINT32_MAX;
  zeroSampleSubmitted = false;
  zeroDrop1.reset();
  zeroDrop2.reset();
  tetrisActions[0].reset();
  tetrisActions[1].reset();
  zeroPadArmed = false;
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = 0;
  pendingTetrisDrop = pendingControllerTetrisDrop = false;
  controllerDownTracking = false;
  tetrisButtons.reset();
  tetrisSoftDrop.reset();
  invalidateDisplay();
  gameMessageShown = 0;
  tetrisFrontendAt = 0;
  nextNesTick = micros();
  return true;
}

flappy::TetrisZero::Options zeroSettings(uint32_t linkedSeed = 0) {
  flappy::TetrisZero::Options options;
  options.level = linkedSeed ? uint8_t(linkedSeed & 15) : app.settings.tetrisZeroLevel;
  options.type = linkedSeed ? uint8_t((linkedSeed >> 4) & 1) : app.settings.tetrisZeroGameType;
  options.players = linkedSeed ? 2 : app.settings.tetrisPlayers;
  return options;
}

void handleGameRequest() {
  if (handledGameRequest == app.gameRequest) return;
  handledGameRequest = app.gameRequest;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  marioBenchmarkFrames = 0;
  timingProbe = false;
#endif
  stopGameAudio();
  invalidateDisplay();
  pendingNesButtons = pendingControllerNesButtons = 0;
  nesSystemButton.reset(M5.BtnA.isPressed());
  tetrisButtons.reset();
  tetrisSoftDrop.reset();
  pendingTetrisDrop = pendingControllerTetrisDrop = false;
  controllerDownTracking = false;
  tetrisActions[0].reset();
  tetrisActions[1].reset();
  pendingPadAction = pendingAction = false;
  tetrisLaunching = zeroLaunching = false;
  marioLaunching = false;
  nativeMario = false;
  gameMessageShown = 0;
  tetrisFrontendAt = 0;
  runtimeNotice = "";
  if (emulatedGame()) {
    renderer->setBackgroundCacheEnabled(false);
    if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
    if (!piezoDriver && !nesAudioBuffers) nesAudioBuffers = new (std::nothrow) int16_t[3][512];
    nes.setCallbacks(nesVideo, piezoDriver ? nullptr : nesAudio, nullptr);
    const bool tetris = app.currentGame == flappy::GameTitle::Tetris;
    const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;
    setNesDisplay(!tetris && !zero);
    const bool displayReady = zero ? zeroViewport.configure(M5.Display.width(), M5.Display.height()) :
        !tetris || tetrisViewport.configure(M5.Display.width(), M5.Display.height());
    const bool validCartridge = tetris ? flappy::RomFingerprint::tetris(TetrisRom::data, TetrisRom::length, false) :
        !zero || flappy::RomFingerprint::tetris(TetrisZeroRom::data, TetrisZeroRom::length, true);
    const bool loaded = displayReady && validCartridge && (zero ? beginZero(zeroSettings()) :
        nes.load(tetris ? TetrisRom::data : FlappyRom::data, tetris ? TetrisRom::length : FlappyRom::length));
    const bool ntscCartridge = tetris || zero || !loaded || nes.frameRate() == 60;
    if (!loaded || !ntscCartridge) {
      runtimeNotice = !ntscCartridge ? "Import an NTSC ROM" : !validCartridge ? "Unsupported Tetris cartridge" :
          displayReady ? nes.error() : "Tetris needs a 135x240 display";
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      handledGameRequest = app.gameRequest;
      app.openMenu(flappy::MenuPage::ChangeGame);
      nes.unload();
      renderer->setBackgroundCacheEnabled(true);
      return;
    }
    nes.setPal(false);
    configureNesPerformance();
    if (tetris) {
      tetrisLaunching = true;
      tetrisLaunchStage = tetrisStableFrames = 0;
      tetrisOptions.level = app.settings.tetrisLevel;
      tetrisOptions.trainer = app.settings.tetrisGameType;
      tetrisOptions.pal = app.settings.tetrisPal;
    } else if (!zero) {
      nativeMario = marioCartridge.supported() && marioViewport.configure(M5.Display.width(), M5.Display.height());
      marioLauncher.reset(nativeMario);
      marioController.reset();
      marioLaunching = nativeMario;
    }
    nextNesTick = micros();
  } else {
    nes.unload();
    renderer->setBackgroundCacheEnabled(true);
    // Keep the three audio buffers alive while the speaker drains its stop marker.
  }
  nextTick = micros() + TickMicros;
}

void restartTetris(bool linked = false, const flappy::coop::StartInfo* start = nullptr) {
  stopGameAudio();
  nes.softReset();
  tetrisLaunching = true;
  gameMessageShown = 0;
  tetrisFrontendAt = 0;
  tetrisActions[0].reset();
  tetrisActions[1].reset();
  tetrisLaunchStage = tetrisStableFrames = 0;
  tetrisOptions.level = app.settings.tetrisLevel;
  tetrisOptions.trainer = app.settings.tetrisGameType;
  tetrisOptions.pal = app.settings.tetrisPal;
  if (linked && start) {
    tetrisOptions.level = uint8_t(start->seed >> 24);
    tetrisOptions.trainer = 2;
    tetrisOptions.pal = start->baseHue != 0;
    tetrisOptions.seed[0] = uint8_t(start->seed >> 16);
    tetrisOptions.seed[1] = uint8_t(start->seed >> 8);
    tetrisOptions.seed[2] = uint8_t(start->seed);
    if (!tetrisOptions.seed[0] && tetrisOptions.seed[1] < 2) tetrisOptions.seed[1] = 2;
  }
  pendingNesButtons = tetrisHeld = 0;
  tetrisButtons.reset();
  tetrisSoftDrop.reset();
  pendingTetrisDrop = pendingControllerTetrisDrop = false;
  nesSystemButton.reset(M5.BtnA.isPressed());
  nextNesTick = micros();
}

uint8_t tetrisLaunchInput() {
  const uint8_t stage = flappy::TetrisGym::stage(nes);
  if (stage != tetrisLaunchStage) { tetrisLaunchStage = stage; tetrisStableFrames = 0; }
  if (flappy::TetrisGym::playing(nes) && nes.readRam(0xa7) == 2 && nes.readRam(0x48) == 1) {
    tetrisLaunching = false;
    return 0;
  }
  if (tetrisStableFrames < 255) ++tetrisStableFrames;
  if ((stage == 2 || stage == 3) && tetrisStableFrames >= 15 && tetrisStableFrames % 15 == 0 &&
      flappy::TetrisGym::configureMenu(nes, tetrisOptions)) return flappy::NesRuntime::Start;
  return 0;
}

void handleTetrisRequests(uint32_t nowMs) {
  if (handledRestart != app.gameRestartRequest) {
    handledRestart = app.gameRestartRequest;
    if (app.currentGame == flappy::GameTitle::Nes) {
      stopGameAudio();
      if (nativeMario) {
        if (!nes.load(FlappyRom::data, FlappyRom::length)) {
          runtimeNotice = nes.error();
          marioLaunching = false;
          app.openMenu(flappy::MenuPage::GameSettings);
          invalidateDisplay();
          return;
        }
        configureNesPerformance();
        nes.setPal(false);
        marioViewport.resetCamera();
        marioLauncher.reset(true);
        marioController.reset();
        marioLaunching = true;
        gameMessageShown = 0;
      } else nes.softReset();
    }
  }
  if (handledTetrisRequest != app.tetrisRequest) {
    handledTetrisRequest = app.tetrisRequest;
    if (app.currentGame == flappy::GameTitle::Tetris) {
      if (tetrisLinkActive()) stopCoop();
      restartTetris();
    }
  }
  if (handledZeroRequest != app.tetrisZeroRequest) {
    handledZeroRequest = app.tetrisZeroRequest;
    if (app.currentGame == flappy::GameTitle::TetrisZero) {
      if (tetrisLinkActive()) stopCoop();
      if (!beginZero(zeroSettings())) runtimeNotice = nes.error();
    }
  }
  if (handledTetrisCalibrate != app.tetrisCalibrateRequest) {
    handledTetrisCalibrate = app.tetrisCalibrateRequest;
    float ax, ay, az;
    if (M5.Imu.isEnabled() && M5.Imu.getAccel(&ax, &ay, &az)) {
      tiltX = -ax;
      lastTiltSampleMs = lastTiltPollMs = nowMs;
      haveTiltSample = true;
      tetrisTilt.calibrate(tiltX);
      preferences.putFloat("tetneutral", tiltX);
      tiltZeroNotice = "Tilt zero saved";
    } else tiltZeroNotice = "Tilt sensor unavailable";
    tiltZeroNoticeMs = nowMs;
  }
  if (handledTetrisLink != app.tetrisLinkRequest) {
    handledTetrisLink = app.tetrisLinkRequest;
    const uint8_t role = app.tetrisLinkRole;
    stopCoop();
    app.tetrisLinkRole = role;
    if (bluetoothPad) bluetoothPad->stop();
    padButtons = previousPadButtons = 0;
    nes.setPrgCache(0);
    pendingControllerNesButtons = 0;
    tetrisRoundPrepared = false;
    const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;
    coopLink.setGameProfile(zero ? flappy::coop::GameProfile::TetrisZero : flappy::coop::GameProfile::Tetris);
    if (zero && role) app.setTetrisZeroPlayers(2);
    if (role == 1) {
      const uint32_t seed = zero ? 0x100U | (uint32_t(app.settings.tetrisZeroGameType) << 4) | app.settings.tetrisZeroLevel :
          (uint32_t(app.settings.tetrisLevel) << 24) | (nonzeroRandom() & 0x00ffffffU);
      coopLink.beginHost(nowMs, nonzeroRandom(), seed ? seed : 0x000200U, zero ? 0 : app.settings.tetrisPal ? 1 : 0);
    } else if (role == 2) coopLink.beginJoin(nowMs);
    if (role) app.closeMenu();
  }
}

void updateTetrisLink(uint32_t nowMs) {
  if (!tetrisLinkActive()) return;
  flappy::coop::TetrisStats local;
  if (zeroLinked()) {
    if (zeroRoundPrepared && !zeroLaunching && flappy::TetrisZero::finished(nes)) zeroFinishedRound = true;
    local.level = zeroOptions.level;
    local.toppedOut = zeroFinishedRound;
  } else {
    local.score = flappy::TetrisGym::score(nes);
    local.lines = flappy::TetrisGym::lines(nes);
    local.level = nes.readRam(0x44);
    local.toppedOut = flappy::TetrisGym::toppedOut(nes);
  }
  coopLink.setLocalTetris(local);
  coopLink.update(nowMs);
  if (zeroLinked() && coopLink.session().connected() && coopLink.session().phase() == flappy::coop::Phase::Lobby && !zeroRoundPrepared) {
    zeroPreparedSeed = coopLink.session().seed();
    zeroRoundPrepared = beginZero(zeroSettings(zeroPreparedSeed));
    if (!zeroRoundPrepared) runtimeNotice = nes.error();
  }
  if (zeroLinked() && zeroReadyWhenPrepared && zeroRoundPrepared && !zeroLaunching) {
    zeroReadyWhenPrepared = false;
    coopLink.requestReady(nowMs);
  }
  flappy::coop::StartInfo start;
  if (coopLink.consumeStart(start)) {
    if (zeroLinked()) {
      if (!zeroRoundPrepared || zeroPreparedSeed != start.seed) {
        zeroPreparedSeed = start.seed;
        zeroRoundPrepared = beginZero(zeroSettings(start.seed));
      }
      stopGameAudio();
      zeroFinishedRound = false;
      zeroSampleFrame = UINT32_MAX;
      zeroSampleSubmitted = false;
      zeroDrop1.reset();
      zeroDrop2.reset();
      tetrisActions[0].reset();
      tetrisActions[1].reset();
      zeroPadArmed = false;
      pendingNesButtons = pendingControllerNesButtons = 0;
      pendingTetrisDrop = pendingControllerTetrisDrop = false;
      tetrisButtons.reset();
      tetrisSoftDrop.reset();
    } else {
      restartTetris(true, &start);
      tetrisRoundPrepared = true;
    }
  }
  if (coopLink.session().phase() == flappy::coop::Phase::Disconnected) {
    tetrisRoundPrepared = false;
    if (zeroLinked()) {
      stopCoop();
      runtimeNotice = "Zero link disconnected";
      app.openMenu(flappy::MenuPage::GameSettings);
    }
  }
}

void showFrame(uint32_t nowMs) {
  static bool previousMenu = false;
  if (previousMenu != app.menuOpen()) invalidateDisplay();
  previousMenu = app.menuOpen();
  if (emulatedGame() && app.menuOpen()) gameMessageShown = 0;
  const bool landscape = app.currentGame == flappy::GameTitle::Nes && !app.menuOpen();
  setNesDisplay(landscape);
  if (tetrisLike() && !app.menuOpen()) {
    if (tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || zeroLaunching || zeroFinishedRound)) {
      static uint32_t lastLobby = UINT32_MAX - 1000;
      if (uint32_t(nowMs - lastLobby) >= 200) {
        lastLobby = nowMs;
        asyncDisplay.drain();
        invalidateDisplay();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(5, 28);
        M5.Display.println(zeroLinked() ? "TETRIS ZERO 2P\n" : "TETRIS SAME-PIECE RACE\n");
        M5.Display.println(coopLink.statusText());
        if (zeroLinked() && zeroLaunching) M5.Display.println("\nPreparing native game");
        else M5.Display.println(zeroFinishedRound ? "\nCenter: rematch\nHold right: menu" : "\nCenter: ready\nHold right: menu");
        if (coopLink.session().localReady()) M5.Display.println("\nYou are ready");
        if (coopLink.session().remoteReady()) M5.Display.println("Peer is ready");
      }
    } else if (tetrisLaunching || zeroLaunching) showGameMessage(1);
    else if (!tetrisNativePlaying()) {
      const bool finished = app.currentGame == flappy::GameTitle::TetrisZero ?
          flappy::TetrisZero::finished(nes) : flappy::TetrisGym::finished(nes);
      showGameMessage(finished ? 3 : 4);
    }
    return;
  }
  if (landscape) {
    if (nativeMario && marioLaunching) showGameMessage(2);
    return;
  }
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
  options.notice = app.menu.page == flappy::MenuPage::TetrisControls &&
      *tiltZeroNotice && uint32_t(nowMs - tiltZeroNoticeMs) < 4000 ? tiltZeroNotice :
      app.menu.page == flappy::MenuPage::Controller && bluetoothPad
      ? bluetoothPad->detailText() : app.menu.page == flappy::MenuPage::TetrisLink ? coopLink.statusText() : runtimeNotice;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  ++appDrawFrames;
  const uint32_t renderStart = micros();
#endif
  renderer->draw(app, options);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  appRenderUs += uint32_t(micros() - renderStart);
#endif
  pushAppCanvas();
}

void stepGame(bool actionEdge, bool menuDown, uint32_t nowMs) {
  const bool menuWasOpen = app.menuOpen();
  const flappy::MenuPage menuBefore = app.menu.page;
  const uint8_t selectionBefore = app.menu.selection;
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
  if (!menuWasOpen && app.menuOpen()) {
    invalidateDisplay();
    pendingNesButtons = pendingControllerNesButtons = 0;
    nesSystemButton.reset(M5.BtnA.isPressed());
    tetrisButtons.reset();
    tetrisSoftDrop.reset();
    pendingTetrisDrop = pendingControllerTetrisDrop = false;
  }
  if (tetrisLinked() && !menuWasOpen && actionEdge) coopLink.requestReady(nowMs);
  if (zeroLinked() && !menuWasOpen && !app.menuOpen() && actionEdge) {
    if (phase == flappy::coop::Phase::Playing && zeroFinishedRound && !zeroLaunching) {
      zeroPreparedSeed = coopLink.session().seed();
      zeroRoundPrepared = beginZero(zeroSettings(zeroPreparedSeed));
      if (!zeroRoundPrepared) runtimeNotice = nes.error();
    }
    if (phase == flappy::coop::Phase::Lobby || zeroFinishedRound) zeroReadyWhenPrepared = true;
  }
  if (liveCoop && app.game.state != flappy::State::Playing) app.game.syncWorldTick(sharedTick);
  playEvents(events);
  if (previousHour != app.settings.clockHour || previousMinute != app.settings.clockMinute) writeClock(nowMs);
  if (!menuWasOpen && app.menuOpen() && coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  if (!tetrisLinkActive() && app.settings.mode != flappy::Mode::Coop && coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  handleCoopRequest(nowMs);
  handleControllerRequest(nowMs);
  handleGameRequest();
  handleTetrisRequests(nowMs);
  menuSound(menuBefore, selectionBefore, actionForApp, nowMs);
  applyOutputSettings();
  if ((events & flappy::GameOver) || app.menuOpen() || app.settingsDirty) saveScores();
  saveSettings();
}

void startupFailure(const char* message) {
  asyncDisplay.drain();
  Serial.begin(115200);
  Serial.printf("STARTUP ERROR: %s\n", message);
  if (M5.Display.width() > 0 && M5.Display.height() > 0) {
    M5.Display.setBrightness(140);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(4, 8);
    M5.Display.println(message);
  }
}

flappy::PowerSource pollPowerSource(uint32_t nowMs, bool force = false) {
  if (!force && powerPolled && uint32_t(nowMs - lastPowerPollMs) < 1000) return powerSource;
  powerPolled = true;
  lastPowerPollMs = nowMs;
  const auto board = M5.getBoard();
  if (board != m5gfx::board_t::board_M5StickC && board != m5gfx::board_t::board_M5StickCPlus)
    return powerSource = flappy::PowerSource::Unknown;
  uint8_t status[2] = {};
  const bool readOk = M5.Power.Axp192.readRegister(0, status, 2);
  return powerSource = flappy::axp192PowerSource(readOk, status[0], status[1]);
}

void clearPendingInput() {
  pendingAction = pendingPadAction = false;
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = 0;
  pendingTetrisDrop = pendingControllerTetrisDrop = false;
  controllerDownTracking = false;
  tetrisButtons.reset();
  tetrisSoftDrop.reset();
  zeroDrop1.reset();
  zeroDrop2.reset();
  nesSystemButton.reset(false);
}

bool sleepOnBattery() {
  if (pollPowerSource(millis(), true) != flappy::PowerSource::Battery) return false;
  const bool resumeController = bluetoothPad && bluetoothPad->canResumePeer();
  const bool linked = coopLink.session().role() != flappy::coop::Role::Off;
  if (linked) {
    stopCoop();
    if (tetrisLike()) app.openTetrisSettings();
    else app.openMenu(flappy::MenuPage::GameSettings);
  }
  if (bluetoothPad) bluetoothPad->stop();
  flappy::radio::EspBackend radioBackend;
  if (radioBackend.hostInitialized() || radioBackend.state() || radioBackend.remainingClients()) {
    inactivity.activity(millis());
    return false;
  }
  controllerConnected = false;
  padButtons = previousPadButtons = 0;
  clearPendingInput();
  saveSettings();
  saveScores();
  stopGameAudio();
  soundOverride = false;
  if (piezoDriver) { piezoOutput.request(0, 0); servicePiezoOutput(); }
  asyncDisplay.drain();
  const bool frontWake = gpio_wakeup_enable(GPIO_NUM_37, GPIO_INTR_LOW_LEVEL) == ESP_OK;
  const bool sideWake = gpio_wakeup_enable(GPIO_NUM_39, GPIO_INTR_LOW_LEVEL) == ESP_OK;
  const bool gpioWake = frontWake && sideWake && esp_sleep_enable_gpio_wakeup() == ESP_OK;
  if (gpioWake) {
    M5.Display.sleep();
    esp_sleep_enable_timer_wakeup(1000000ULL);
    while (pollPowerSource(millis(), true) == flappy::PowerSource::Battery &&
           gpio_get_level(GPIO_NUM_37) && gpio_get_level(GPIO_NUM_39)) {
      if (esp_light_sleep_start() != ESP_OK) break;
      if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) break;
      M5.update();
      if (M5.BtnPWR.wasClicked() || M5.BtnPWR.wasPressed()) break;
    }
    M5.Display.wakeup();
    delay(120);
    M5.Display.setBrightness(app.brightness());
  }
  gpio_wakeup_disable(GPIO_NUM_37);
  gpio_wakeup_disable(GPIO_NUM_39);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  M5.update();
  clearPendingInput();
  inactivity.resumed(millis());
  invalidateDisplay();
  lastClockReadMs = millis() - 1000;
  if (resumeController && bluetoothPad) {
    bluetoothPad->resumePeer(millis());
    app.setControllerStatus(bluetoothPad->statusText());
  }
  nextTick = micros() + TickMicros;
  nextNesTick = micros();
  return true;
}

void setup() {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.begin(115200);
  Serial.printf("BOOT display profile=%u\n", unsigned(M5FLAP_DISPLAY_PROFILE));Serial.flush();
#endif
  prepareStickDisplayPower();
  if (!configureStickDisplay(M5FLAP_DISPLAY_PROFILE)) {
    startupFailure("Display initialization failed");
    return;
  }
  auto cfg = M5.config();
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  cfg.serial_baudrate = 115200;
#else
  cfg.serial_baudrate = 0;
#endif
  cfg.internal_imu = true;
  cfg.internal_rtc = true;
  cfg.internal_mic = false;
  cfg.internal_spk = true;
  cfg.external_display_value = 0;
  cfg.output_power = false;
  M5.begin(cfg);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.printf("BOOT board=%u display=%ux%u heap=%u cpu_mhz=%u boot_flash_hz=%u\n", unsigned(M5.getBoard()), M5.Display.width(), M5.Display.height(), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), ESP.getFlashChipSpeed());Serial.flush();
#endif
  rtc_gpio_pullup_dis(GPIO_NUM_2);
  rtc_gpio_pulldown_en(GPIO_NUM_2);
  initializeSound();
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setSwapBytes(true);
  viewport.configure(M5.Display.width(), M5.Display.height());
  if (viewport.width <= 0 || viewport.height <= 0) {
    startupFailure("LCD not detected. Select a display profile when building.");
    return;
  }
  renderer = new (std::nothrow) flappy::Renderer(canvasStorage);
  if (!renderer) { startupFailure("Display memory unavailable"); return; }
  const bool asyncReady = asyncDisplay.begin();
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.printf("BOOT async_display=%u heap=%u\n", unsigned(asyncReady), ESP.getFreeHeap());
#else
  (void)asyncReady;
#endif
  uint8_t count = renderer->skinCount();
  if (count > MaxSkins) count = MaxSkins;
  for (uint8_t i = 0; i < count; ++i) skinNames[i] = renderer->skinName(i);
  app.setSkinNames(skinNames, count);
  marioCartridge.configure(FlappyRom::data, FlappyRom::length);
  app.setNesAvailable(FlappyRom::length > 0);
  app.setNesTitle(marioCartridge.supported() ? "Mario" : FlappyRom::title);
  app.setTetrisAvailable(flappy::RomFingerprint::tetris(TetrisRom::data, TetrisRom::length, false) &&
      M5.Display.width() >= 131 && M5.Display.height() >= 240);
  app.setTetrisTitle(TetrisRom::title);
  app.setTetrisZeroAvailable(flappy::RomFingerprint::tetris(TetrisZeroRom::data, TetrisZeroRom::length, true) &&
      M5.Display.width() >= 135 && M5.Display.height() >= 240);
  app.setTetrisZeroTitle(TetrisZeroRom::title);
  loadSettings();
  app.restoreBootGame();
  handleGameRequest();
  initializeClock(millis());
  applyOutputSettings();
  showFrame(millis());
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.println("BOOT first frame ready");Serial.flush();
#endif
  nextTick = micros() + TickMicros;
}

#ifdef M5FLAP_STARTUP_DIAGNOSTICS
void serviceDiagnostics() {
  static char command[24];
  static uint8_t length = 0;
  for (uint8_t count = 0; count < 32 && Serial.available(); ++count) {
    const char c = char(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (length < sizeof(command) - 1) command[length++] = c;
      else length = 0;
      continue;
    }
    command[length] = 0;
    if (!strcmp(command, "M5TETRIS")) {
      app.setTetrisPlayers(1);
      app.setCurrentGame(flappy::GameTitle::Tetris);
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5ZERO2")) {
      app.setTetrisPlayers(2);
      app.activateTetris();
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5ZERO")) {
      app.setTetrisPlayers(1);
      app.setCurrentGame(flappy::GameTitle::TetrisZero);
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5FLAPPY")) {
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5MARIO")) {
      app.setCurrentGame(flappy::GameTitle::Nes);
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5DEMO") || !strcmp(command, "M5ORIGINAL")) {
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      app.setMode(!strcmp(command, "M5DEMO") ? flappy::Mode::Demo : flappy::Mode::Original);
      app.closeMenu(); handleGameRequest();
    } else if (!strncmp(command, "M5SKIN ", 7)) {
      unsigned skin = 0;
      if (sscanf(command + 7, "%u", &skin) == 1 && skin < renderer->skinCount()) app.setSkin(uint8_t(skin));
    } else if (!strcmp(command, "M5RUN")) {
      marioBenchmarkFrames = 600;
    } else if (!strcmp(command, "M5TIMING")) {
      timingProbe = true;
      timingIntervals = 0;
      timingStart = timingPrevious = 0;
      timingMin = UINT32_MAX;
      timingMax = 0;
      asyncDisplay.startFrameProbe(500);
    } else if (!strncmp(command, "M5CACHE ", 8)) {
      unsigned mask = 0;
      if (sscanf(command + 8, "%u", &mask) == 1 && (mask == 0 || mask == 2 || mask == 3)) {
        benchmarkPrgCache = uint8_t(mask);
        nes.setPrgCache(benchmarkPrgCache);
      }
    } else if (!strncmp(command, "M5VOLUME ", 9)) {
      unsigned value = 0;
      if (sscanf(command + 9, "%u", &value) == 1 && value < 6) app.setVolume(uint8_t(value));
    } else if (!strncmp(command, "M5PCM ", 6)) {
      unsigned enabled = 0;
      if (sscanf(command + 6, "%u", &enabled) == 1) {
        benchmarkPcm = enabled != 0;
        nes.setPcmEnabled(benchmarkPcm);
      }
    } else if (!strncmp(command, "M5IDLE ", 7)) {
      unsigned enabled = 0;
      if (sscanf(command + 7, "%u", &enabled) == 1 && enabled <= 1) {
        benchmarkIdleJump = uint8_t(enabled);
        nes.setIdleJump(enabled != 0);
      }
    } else if (!strncmp(command, "M5VIDEO ", 8)) {
      unsigned divisor = 0;
      if (sscanf(command + 8, "%u", &divisor) == 1 && divisor <= 4)
        benchmarkVideoDivisor = uint8_t(divisor);
    } else if (!strcmp(command, "M5PAIR")) {
      app.openMenu(flappy::MenuPage::Controller);
      app.step(true, false, millis());
      handleControllerRequest(millis());
    } else if (!strcmp(command, "M5RECONNECT")) {
      if (bluetoothPad) bluetoothPad->resumePeer(millis());
      padButtons = previousPadButtons = 0;
      pendingNesButtons = pendingControllerNesButtons = 0;
    } else if (!strcmp(command, "M5BEEP")) {
      piezo.stop();
      soundOverride = soundReady;
      soundOverrideUntil = millis() + 200;
      soundFrequency = soundReady ? 2500 : 0;
      soundDuty = soundReady ? 511 : 0;
      if (piezoDriver && soundReady) {
        piezoOutput.request(2500, 511);
        servicePiezoOutput();
      }
      else if (soundReady) M5.Speaker.tone(2500, 200, 0, true);
      Serial.printf("BEEP driver=%s ready=%u frequency=%u duration_ms=200\n", piezoDriver ? "GPIO2-LEDC" : "M5Speaker", unsigned(soundReady), unsigned(soundFrequency));
    } else if (!strcmp(command, "M5STATUS")) {
      asyncDisplay.drain();
      Serial.printf("STATUS game=%u menu=%u frames=%lu time=%lu heap=%u gym=%u level=%u score=%lu lines=%u pad=%u\n",
          unsigned(app.currentGame), unsigned(app.menu.page), (unsigned long)nes.frames(),
          (unsigned long)millis(), ESP.getFreeHeap(), unsigned(flappy::TetrisGym::stage(nes)),
          unsigned(flappy::TetrisGym::level(nes)), (unsigned long)flappy::TetrisGym::score(nes),
          unsigned(flappy::TetrisGym::lines(nes)), unsigned(controllerConnected));
      if (nativeMario) {
        const auto player = marioCartridge.sample(nes);
        Serial.printf("MARIO screen=%u mode=%u task=%u y=%d h=%d camera=%d hz=%u divisor=%u launch=%u\n",
            unsigned(marioCartridge.screen(nes)), unsigned(nes.readRam(0x770)), unsigned(nes.readRam(0x772)),
            player.y, player.height, marioViewport.worldTop(), unsigned(nes.frameRate()),
            unsigned(nes.videoDivisor()), unsigned(marioLaunching));
      }
      if (app.currentGame == flappy::GameTitle::TetrisZero)
        Serial.printf("ZERO stage=%u players=%u level1=%u level2=%u active1=%u active2=%u launch=%u linkframe=%lu\n",
            unsigned(flappy::TetrisZero::stage(nes)), unsigned(flappy::TetrisZero::players(nes)),
            unsigned(flappy::TetrisZero::level(nes)), unsigned(flappy::TetrisZero::level(nes, 2)),
            unsigned(flappy::TetrisZero::active(nes)), unsigned(flappy::TetrisZero::active(nes, 2)),
            unsigned(zeroLaunching), (unsigned long)coopLink.zeroFrame());
      Serial.printf("SOUND driver=%s ready=%u volume=%u frequency=%u duty=%u override=%u active=%u\n",
          piezoDriver ? "GPIO2-LEDC" : "M5Speaker", unsigned(soundReady), unsigned(app.settings.volumeIndex),
          unsigned(soundFrequency), unsigned(soundDuty), unsigned(soundOverride), unsigned(piezo.active()));
      Serial.printf("APP ticks=%lu draws=%lu mode=%u skin=%u\n", (unsigned long)appTicks,
          (unsigned long)appDrawFrames, unsigned(app.settings.mode), unsigned(app.settings.skinIndex));
      Serial.printf("SLEEP source=%u timeout_s=%lu idle_ms=%lu index=%u\n", unsigned(powerSource),
          (unsigned long)app.sleepTimeoutSeconds(), (unsigned long)inactivity.idleMs(millis()),
          unsigned(app.settings.sleepIndex));
      Serial.printf("OPT cache=%u pcm=%u render_us=%llu bg_bytes=%u idle=%u\n", unsigned(nes.prgCacheMask()),
          unsigned(nes.pcmEnabled()), (unsigned long long)appRenderUs,
          unsigned(renderer->backgroundCacheBytes()), unsigned(nes.idleJumpEnabled()));
      Serial.printf("PERF nes_us=%llu video_us=%llu source_frames=%lu lcd_frames=%lu lcd_rows=%lu lcd_us=%llu largest_heap=%u\n",
          (unsigned long long)nesWorkUs, (unsigned long long)nesVideoUs,
          (unsigned long)nesRenderedFrames, (unsigned long)lcdFrames, (unsigned long)lcdRows,
          (unsigned long long)lcdUs, ESP.getMaxAllocHeap());
      const auto asyncStats = asyncDisplay.stats();
      Serial.printf("ASYNC strips=%lu bytes=%llu work_us=%llu pending=%u\n",
          (unsigned long)asyncStats.strips, (unsigned long long)asyncStats.bytes,
          (unsigned long long)asyncStats.workUs, asyncStats.pending);
      Serial.printf("TRANSFER submits=%lu waits=%lu wait_us=%llu copy_us=%llu encode_us=%llu max_wait_us=%lu max_copy_us=%lu max_work_us=%lu max_pending=%u frames=%lu frame_us=%llu\n",
          (unsigned long)asyncStats.submits, (unsigned long)asyncStats.waits,
          (unsigned long long)asyncStats.waitUs, (unsigned long long)asyncStats.copyUs,
          (unsigned long long)asyncStats.encodeUs,
          (unsigned long)asyncStats.maxWaitUs, (unsigned long)asyncStats.maxCopyUs,
          (unsigned long)asyncStats.maxWorkUs, asyncStats.maxPending,
          (unsigned long)asyncStats.completedFrames, (unsigned long long)asyncStats.frameIntervalUs);
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
      const auto* profile = nes_test_profile_get();
      Serial.printf("PROFILE frames=%llu frame=%llu cpu=%llu bg=%llu sprite=%llu apu=%llu draw=%llu\n",
          (unsigned long long)profile->frames, (unsigned long long)profile->region_us[0],
          (unsigned long long)profile->region_us[1], (unsigned long long)profile->region_us[2],
          (unsigned long long)profile->region_us[3], (unsigned long long)profile->region_us[4],
          (unsigned long long)profile->region_us[5]);
#endif
      if (bluetoothPad) Serial.printf("CONTROLLER %s: %s seen=%u candidates=%u reports=%lu error=%d buttons=0x%X\n",
          bluetoothPad->statusText(), bluetoothPad->detailText(), unsigned(bluetoothPad->devicesSeen()),
          unsigned(bluetoothPad->candidatesSeen()), (unsigned long)bluetoothPad->reportsReceived(), bluetoothPad->lastError(), unsigned(padButtons));
    }
    length = 0;
  }
}
#endif

void loop() {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  serviceDiagnostics();
#endif
  M5.update();
  if (!renderer) { delay(10); return; }
  if (inactivity.consumeWakeInput(M5.BtnA.isPressed() || M5.BtnB.isPressed() || padButtons, millis())) {
    clearPendingInput();
    nextTick = micros() + TickMicros;
    nextNesTick = micros();
    delay(1);
    return;
  }
  pendingAction |= M5.BtnA.wasPressed();
  const uint32_t nowMs = millis();
  readClock(nowMs);
  updateController(nowMs);
  updateCoop(nowMs);
  updateTetrisLink(nowMs);
  updateTetrisFrontend(nowMs);
  serviceSound(nowMs);
  const bool tetris = tetrisLike();
  const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;
  const bool padConnected = controllerConnected;
  pendingNesButtons |= nesSystemButton.update(M5.BtnA.isPressed(),
      app.currentGame == flappy::GameTitle::Nes && !app.menuOpen(), nowMs);
  const bool lobby = tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || (zeroLinked() && (zeroLaunching || zeroFinishedRound)));
  const auto internal = tetrisButtons.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(),
      M5.BtnPWR.wasClicked(), app.settings.tetrisControls,
      tetris && !app.menuOpen() && !lobby && !tetrisLaunching && !zeroLaunching, nowMs);
  tetrisHeld = internal.held;
  pendingTetrisDrop |= (internal.pressed & flappy::NesRuntime::Down) != 0;
  pendingNesButtons |= internal.pressed;
  if (tetris && app.settings.tetrisTilt && (!padConnected || zero) && M5.Imu.isEnabled()) {
    if (!haveTiltSample || uint32_t(nowMs - lastTiltPollMs) >= 32) {
      lastTiltPollMs = nowMs;
      float ax, ay, az;
      if (M5.Imu.getAccel(&ax, &ay, &az)) {
        tiltX = -ax;
        haveTiltSample = true;
        lastTiltSampleMs = nowMs;
      }
    }
    const bool fresh = haveTiltSample && uint32_t(nowMs - lastTiltSampleMs) < 200;
    const auto tilt = tetrisTilt.update(tiltX, fresh && !app.menuOpen() && !lobby &&
        !tetrisLaunching && !zeroLaunching, app.settings.tetrisControls);
    tetrisHeld |= tilt.held;
    pendingNesButtons |= tilt.pressed;
  }
  const uint32_t now = micros();
  const bool inputActive = M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnPWR.wasClicked() ||
      M5.BtnPWR.wasPressed() || padButtons || pendingAction || pendingPadAction ||
      pendingNesButtons || pendingControllerNesButtons || tetrisHeld;
  if (inactivity.shouldSleep(nowMs, app.sleepTimeoutMs(), pollPowerSource(nowMs), inputActive) && sleepOnBattery()) return;
  if (int32_t(now - nextTick) >= 0) {
    int catchUp = 0;
    do {
      bool action = pendingAction || pendingPadAction;
      if (emulatedGame() && !app.menuOpen() && !lobby &&
          !((tetrisLinked() && flappy::TetrisGym::toppedOut(nes)) || (zeroLinked() && zeroFinishedRound))) {
        action = false;
      }
      const bool physicalMenu = M5.BtnB.isPressed();
      const bool menuDown = physicalMenu;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
      ++appTicks;
#endif
      stepGame(action, menuDown, millis());
      pendingAction = pendingPadAction = false;
      nextTick += TickMicros;
      ++catchUp;
    } while (int32_t(micros() - nextTick) >= 0 && catchUp < 4);
    if (catchUp == 4) nextTick = micros() + TickMicros;
    showFrame(millis());
  }
  if (emulatedGame() && !app.menuOpen() && nes.ready()) {
    const bool frameTetris = app.currentGame == flappy::GameTitle::Tetris;
    const bool frameZero = app.currentGame == flappy::GameTitle::TetrisZero;
    const uint32_t frameNow = micros();
    if (int32_t(frameNow - nextNesTick) >= 0) {
      setNesDisplay(!frameTetris && !frameZero);
      const bool canStep = frameZero && zeroLinked() ? zeroLaunching ||
          (zeroRoundPrepared && !zeroFinishedRound && coopLink.session().phase() == flappy::coop::Phase::Playing) :
          !frameTetris || !tetrisLinked() || coopLink.session().phase() == flappy::coop::Phase::Playing ||
          (tetrisRoundPrepared && tetrisLaunching);
      bool stepped = false;
      if (canStep) {
        uint8_t input1 = 0, input2 = 0;
        bool runFrame = true;
        bool consumedInput = false;
        if (!frameTetris && !frameZero && nativeMario && marioLaunching) {
          const auto launch = marioLauncher.step(marioCartridge.sample(nes).playing);
          input1 = launch.buttons;
          if (launch.complete) {
            marioLaunching = false;
            gameMessageShown = 0;
            invalidateDisplay();
          } else if (launch.failed) {
            marioLaunching = false;
            runtimeNotice = "Mario launch timed out";
            app.openMenu(flappy::MenuPage::GameSettings);
          }
          consumedInput = true;
        } else if (frameZero && zeroLaunching) {
          const auto launch = zeroLauncher.step(nes);
          input1 = launch.player1;
          input2 = launch.player2;
          if (launch.complete) { zeroLaunching = false; runFrame = false; consumedInput = true; }
        } else if (frameZero) {
          uint8_t controller = tetrisControllerButtons(millis());
          uint8_t local = uint8_t(pendingNesButtons | tetrisHeld);
          if (zeroLinked()) {
            const uint32_t frame = coopLink.zeroFrame();
            const uint32_t target = frame + flappy::coop::ZeroInputDelay;
            if (zeroSampleFrame != target) {
              if (!zeroPadArmed && !padButtons && !M5.BtnA.isPressed() && !M5.BtnB.isPressed()) zeroPadArmed = true;
              local |= controller;
              // Linked play uses onboard keys. Select carries a center-tap event.
              if (pendingTetrisDrop || pendingControllerTetrisDrop) local |= flappy::NesRuntime::Select;
              zeroSamplePad = zeroPadArmed ? local : 0;
              zeroSampleFrame = target;
              zeroSampleSubmitted = false;
              consumedInput = true;
            }
            if (!zeroSampleSubmitted) zeroSampleSubmitted = coopLink.submitZeroInput(target, zeroSamplePad);
            uint8_t own = 0, remote = 0;
            runFrame = zeroSampleSubmitted && coopLink.zeroInputs(frame, own, remote);
            const bool host = coopLink.session().role() == flappy::coop::Role::Host;
            input1 = host ? own : remote;
            input2 = host ? remote : own;
            if (runFrame) {
              const bool down1 = zeroDrop1.update((input1 & flappy::NesRuntime::Select) != 0,
                  (input1 & flappy::NesRuntime::Down) != 0, flappy::TetrisZero::pieceY(nes),
                  flappy::TetrisZero::active(nes));
              const bool down2 = zeroDrop2.update((input2 & flappy::NesRuntime::Select) != 0,
                  (input2 & flappy::NesRuntime::Down) != 0, flappy::TetrisZero::pieceY(nes, 2),
                  flappy::TetrisZero::active(nes, 2));
              input1 &= uint8_t(~(flappy::NesRuntime::Select | flappy::NesRuntime::Down));
              input2 &= uint8_t(~(flappy::NesRuntime::Select | flappy::NesRuntime::Down));
              if (down1) input1 = uint8_t((input1 & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
              if (down2) input2 = uint8_t((input2 & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
            }
          } else {
            const bool twoPlayers = zeroOptions.players == 2;
            if (!twoPlayers) local |= controller;
            const bool drop = tetrisSoftDrop.update(pendingTetrisDrop || (!twoPlayers && pendingControllerTetrisDrop),
                (local & flappy::NesRuntime::Down) != 0, flappy::TetrisZero::pieceY(nes),
                flappy::TetrisZero::active(nes));
            local &= uint8_t(~(flappy::NesRuntime::Start | flappy::NesRuntime::Select | flappy::NesRuntime::Down));
            if (drop) local = uint8_t((local & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
            input1 = local;
            if (twoPlayers) {
              const bool controllerDrop = zeroDrop2.update(pendingControllerTetrisDrop,
                  (controller & flappy::NesRuntime::Down) != 0, flappy::TetrisZero::pieceY(nes, 2),
                  flappy::TetrisZero::active(nes, 2));
              controller &= uint8_t(~flappy::NesRuntime::Down);
              if (controllerDrop) controller = uint8_t((controller & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
              input2 = controller;
            }
            consumedInput = true;
          }
        } else {
          input1 = frameTetris && tetrisLaunching ? tetrisLaunchInput() :
              uint8_t((frameTetris ? tetrisControllerButtons(millis()) :
                  nesButtons(padButtons) | pendingControllerNesButtons) | pendingNesButtons | tetrisHeld);
          if (frameTetris && !tetrisLaunching) {
            const bool drop = tetrisSoftDrop.update(pendingTetrisDrop || pendingControllerTetrisDrop,
                (input1 & flappy::NesRuntime::Down) != 0, nes.readRam(0x41),
                flappy::TetrisGym::playing(nes) && nes.readRam(0x48) == 1);
            input1 &= uint8_t(~(flappy::NesRuntime::Start | flappy::NesRuntime::Select | flappy::NesRuntime::Down));
            if (drop) input1 = uint8_t((input1 & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
          }
          consumedInput = true;
        }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
        const uint32_t workStart = micros();
#endif
        if (runFrame) {
          bool hardDropCue = false, swapCue = false;
          if ((frameTetris || frameZero) && !tetrisLaunching && !zeroLaunching) {
            input1 = tetrisActions[0].apply(nes, frameZero, 1, input1);
            hardDropCue = tetrisActions[0].hardDropped();
            swapCue = tetrisActions[0].swapped();
            if (hardDropCue) { tetrisSoftDrop.reset(); zeroDrop1.reset(); }
            if (frameZero && zeroOptions.players == 2) {
              input2 = tetrisActions[1].apply(nes, true, 2, input2);
              hardDropCue |= tetrisActions[1].hardDropped();
              swapCue |= tetrisActions[1].swapped();
              if (tetrisActions[1].hardDropped()) zeroDrop2.reset();
            }
          }
          if (!frameTetris && !frameZero && nativeMario) {
            if (!marioLaunching) input1 = uint8_t(marioController.step(marioCartridge.screen(nes),
                uint8_t(nesButtons(padButtons) | pendingControllerNesButtons)) | pendingNesButtons);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            if (!marioLaunching && marioBenchmarkFrames) {
              const auto screen = marioCartridge.screen(nes);
              const unsigned phase = (600 - marioBenchmarkFrames) % 120;
              input1 = screen == flappy::MarioCartridge::Screen::Title ? flappy::NesRuntime::Start :
                  screen == flappy::MarioCartridge::Screen::Playing ? uint8_t(flappy::NesRuntime::Right |
                  flappy::NesRuntime::B | (phase >= 32 && phase < 52 ? flappy::NesRuntime::A : 0)) : 0;
              --marioBenchmarkFrames;
            }
#endif
            marioViewport.trackPlayer(marioCartridge.sample(nes));
            nes.setVideoRows(16, 31, marioViewport.worldTop(), marioViewport.worldTop() + 118);
          } else if (frameTetris || frameZero) {
            prepareTetrisView(frameZero);
            nes.setVideoRows(frameZero && zeroOptions.players == 2 ? 8 : 24,
                frameZero && zeroOptions.players == 2 ? 223 : 215);
          }
          const uint8_t videoInterval = nativeMario ? 1 : frameZero && zeroOptions.players == 2 ? 3 : 2;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          nes.setVideoDivisor(benchmarkVideoDivisor ? benchmarkVideoDivisor : videoInterval);
#else
          nes.setVideoDivisor(videoInterval);
#endif
          stepped = nes.step(input1, input2);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          if (stepped && timingProbe) {
            const uint32_t stamp = micros();
            if (!timingStart) {
              timingStart = stamp;
              timingSourceStart = nesRenderedFrames;
              const auto displayStats = asyncDisplay.stats();
              timingPresentStart = displayStats.completedFrames;
              timingPresentUs = displayStats.frameIntervalUs;
            }
            else {
              const uint32_t interval = stamp - timingPrevious;
              if (interval < timingMin) timingMin = interval;
              if (interval > timingMax) timingMax = interval;
              if (++timingIntervals == 500) {
                timingProbe = false;
                Serial.printf("TIMING game=%u hz=%u intervals=500 elapsed_us=%lu min_us=%lu max_us=%lu\n",
                    unsigned(app.currentGame), unsigned(nes.frameRate()),
                    (unsigned long)(stamp - timingStart), (unsigned long)timingMin, (unsigned long)timingMax);
                asyncDisplay.drain();
                const auto displayStats = asyncDisplay.stats();
                Serial.printf("PRESENT source_frames=%lu completed_frames=%lu elapsed_us=%llu pending=%u\n",
                    (unsigned long)(nesRenderedFrames - timingSourceStart),
                    (unsigned long)(displayStats.completedFrames - timingPresentStart),
                    (unsigned long long)(displayStats.frameIntervalUs - timingPresentUs), displayStats.pending);
                Serial.printf("DISPLAY_TIMING intervals=%lu elapsed_us=%llu complete=%u\n",
                    (unsigned long)displayStats.probeIntervals,
                    (unsigned long long)displayStats.probeElapsedUs, unsigned(displayStats.probeComplete));
              }
            }
            timingPrevious = stamp;
          }
#endif
          if (stepped) {
            updateNesSound(input1, input2, millis());
            if (hardDropCue) playCue(flappy::PiezoCue::Drop, millis());
            else if (swapCue) playCue(flappy::PiezoCue::Action, millis());
          }
        }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
        nesWorkUs += uint32_t(micros() - workStart);
#endif
        if (consumedInput) {
          pendingNesButtons = pendingControllerNesButtons = 0;
          pendingTetrisDrop = pendingControllerTetrisDrop = false;
        }
      }
      const uint32_t duration = 1000000U / nes.frameRate();
      nextNesTick += duration;
      if (!stepped) nextNesTick = micros() + duration;
      else if (int32_t(micros() - nextNesTick) >= int32_t(duration)) nextNesTick = micros();
    }
  }

  serviceSound(millis());
  static uint32_t lastIdleMs = 0;
  const bool activeNes = emulatedGame() && !app.menuOpen() && nes.ready();
  if (activeNes) {
    int32_t remaining;
    while ((remaining = int32_t(nextNesTick - micros())) > 1250) {
      delay(1);
      lastIdleMs = millis();
    }
    if (remaining > 0) delayMicroseconds(uint32_t(remaining));
    if (uint32_t(millis() - lastIdleMs) < 50) yield();
    else { delay(1); lastIdleMs = millis(); }
  } else { delay(1); lastIdleMs = millis(); }
}
