#include "build_display.h"
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
#include "speaker_audio.h"
#include "nes_viewport.h"
#include "mario_viewport.h"
#include "clock_runtime.h"
#include "game_home.h"
#include "rom_fingerprint.h"
#include "runtime_cartridges.h"
#include "usb_library.h"
#include "geometry_input.h"
#include "geometry_scene.h"
#include "geometry_level.h"
#include "geometry_renderer.h"
#include "geometry_launcher.h"
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
#include "arcade_support.h"
#include "galaga_game.h"
#include "arkanoid_game.h"
#include "jetpack_game.h"

flappy::AppModel app;
#ifdef M5FLAP_USB_LIBRARY
void initializeLibrary();
void refreshCartridgeViews();
bool serviceLibrary();
#endif
flappy::GalagaGame* galagaGame = nullptr;
flappy::ArkanoidGame* arkanoidGame = nullptr;
flappy::JetpackGame* jetpackGame = nullptr;
flappy::JetpackProfile jetpackProfile;
uint32_t handledJetpackUpgrade = 0;
flappy::ArcadeTilt arcadeTilt;
flappy::ArcadeControls arcadeControls;
bool arcadePaused = false, arcadePauseRequested = false, jetpackCashRequested = false;
uint32_t handledArcadeCalibrate = 0, arcadeBestGalaga = 0, arcadeBestArkanoid = 0;
uint32_t arcadeBestJetpack = 0;
float arcadeActivityTilt = 0;
uint32_t arcadeFrames = 0;
int16_t arcadeMove = 0;
bool arcadeScoresDirty = false;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
int16_t diagnosticArcadeMove = 0;
bool diagnosticArcadeFire = false;
bool diagnosticArcadeTesting = false;
uint8_t diagnosticJetpackRide = 255, diagnosticJetpackGadget = 0;
bool diagnosticJetpackHeadstart = false;
#endif
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
flappy::GeometryButtons geometryButtons;
uint8_t geometryHeld = 0;
flappy::GeometryRenderer geometryRenderer;
struct GeometryPresentation {
  flappy::GeometryWorld world;
  flappy::GeometryLevelReader level;
};
GeometryPresentation* geometryPresentation = nullptr;
flappy::GeometryController geometryController;
flappy::GeometryLauncher geometryLauncher;
uint32_t handledGeometryRequest = 0;
flappy::NesViewport nesViewport;
flappy::MarioViewport marioViewport;
flappy::MarioCartridge marioCartridge;
flappy::MarioLauncher marioLauncher;
flappy::MarioController marioController;
flappy::GameHome gameHome;
flappy::ClockRuntime homeClocks;
uint32_t handledHomeGeneration = UINT32_MAX, homeFrames = 0;
bool pendingHomeNext = false;
flappy::TetrisViewport tetrisViewport;
flappy::TetrisZeroViewport zeroViewport;
flappy::TetrisZero::Options zeroOptions;
flappy::TetrisZero::Launcher zeroLauncher;
flappy::DamageRows displayDamage;
flappy::AsyncDisplay asyncDisplay;
flappy::InactivityPolicy inactivity;
flappy::PowerSource powerSource = flappy::PowerSource::Unknown;
flappy::PowerSource pollPowerSource(uint32_t nowMs, bool force = false);
flappy::BatteryStatus menuBattery;
flappy::BatteryStatus pollMenuBattery(uint32_t nowMs, bool force = false);
uint32_t lastBatteryPollMs = 0;
bool batteryPolled = false;
void servicePiezoOutput();
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
bool geometryScanOrder = false;
flappy::SpeakerPcmQueue* nesAudioQueue = nullptr;
uint8_t appliedSpeakerVolume = UINT8_MAX;
const char* runtimeNotice = "";
bool pendingAction = false;
bool pendingTetrisDrop = false;
bool pendingControllerTetrisDrop = false;
bool controllerDownTracking = false;
uint32_t controllerDownSince = 0;
bool nativeMario = false, marioLaunching = false;
bool nativeGeometry = false;
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
uint64_t geometryStageUs[6] = {};
uint32_t geometryProfileFrames = 0;
void geometryTimeStage(unsigned stage, uint32_t& started) {
  const uint32_t now = micros();
  geometryStageUs[stage] += uint32_t(now - started);
  started = now;
}
uint16_t marioBenchmarkFrames = 0, diagnosticPadFrames = 0;
uint8_t diagnosticPad = 0;
uint8_t benchmarkPrgCache = 0, benchmarkVideoDivisor = 0;
uint8_t benchmarkIdleJump = 2;
bool benchmarkPcm = false, benchmarkPcmOverride = false;
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

bool nativeArcade() { return flappy::nativeArcadeGame(app.currentGame); }
bool jetpackSelected() { return app.currentGame == flappy::GameTitle::JetpackJoyride; }
bool emulatedGame() { return app.currentGame != flappy::GameTitle::FlappyBird && !nativeArcade(); }

#ifdef M5FLAP_STARTUP_DIAGNOSTICS
void recordFrameTiming(uint32_t sourceFrames, uint8_t hz) {
  if (!timingProbe) return;

  const uint32_t stamp = micros();
  if (!timingStart) {
    timingStart = stamp;
    timingSourceStart = sourceFrames;
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
          unsigned(app.currentGame), unsigned(hz),
          (unsigned long)(stamp - timingStart), (unsigned long)timingMin, (unsigned long)timingMax);
      asyncDisplay.drain();
      const auto displayStats = asyncDisplay.stats();
      Serial.printf("PRESENT source_frames=%lu completed_frames=%lu elapsed_us=%llu pending=%u\n",
          (unsigned long)(sourceFrames - timingSourceStart),
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

void configureNesPerformance() {
  nes.setPcmEnabled(!piezoDriver && soundReady && nesAudioQueue);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  if (benchmarkPcmOverride) nes.setPcmEnabled(benchmarkPcm);
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
  else if (soundReady) { piezoOutput.request(0, 0); servicePiezoOutput(); }
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
  } else {
    // M5Unified owns ES8311 clocking, codec power and the M5PM1 amplifier gate.
    // isEnabled() only checks the pin configuration; begin() checks I2S startup.
    M5.Speaker.setVolume(0);
    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
      auto config = M5.Speaker.config();
      config.sample_rate = 24000;
      M5.Speaker.config(config);
    }
    soundReady = M5.Speaker.isEnabled() && M5.Speaker.begin();
    if (soundReady && M5.getBoard() == m5::board_t::board_M5StickS3) {
      uint8_t codec = 0, amplifier = 0;
      soundReady = M5.In_I2C.readRegister(0x18, 0x00, &codec, 1, 100000) &&
          M5.In_I2C.readRegister(0x6e, 0x11, &amplifier, 1, 100000) && (amplifier & 8u);
      if (!soundReady) M5.Speaker.end();
    }
  }
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
  // Speaker devices play the cartridge's mixed APU output, including music.
  // Input-based cue approximations are reserved for the passive buzzer.
  if (!piezoDriver) return;
  if (!nes.ready() || app.menuOpen() || tetrisLaunching || zeroLaunching || gameHome.active() ||
      (nativeGeometry && geometryLauncher.active()) ||
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
  if (app.settings.mode == flappy::Mode::Clock) app.settings.mode = flappy::Mode::Original;
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
  app.setGeometryLevel(preferences.getUChar("geolevel", 0));
  app.setArcadeSensitivity(preferences.getUChar("arcsens", 1));
  app.setArcadeAutoFire(preferences.getBool("arcauto", false));
  app.setArcadeDifficulty(preferences.getUChar("arcdiff", 1));
  uint8_t jetpackSaved[flappy::JetpackProfile::StorageSize];
  if (preferences.getBytesLength("jetprofile") == sizeof(jetpackSaved) &&
      preferences.getBytes("jetprofile", jetpackSaved, sizeof(jetpackSaved)) == sizeof(jetpackSaved))
    jetpackProfile.decode(jetpackSaved, sizeof(jetpackSaved));
  app.setJetpackPractice(preferences.getBool("jetprac", false));
  app.setJetpackDifficulty(preferences.getUChar("jetdiff", 1));
  app.setJetpackRide(preferences.getUChar("jetride", 0));
  app.setJetpackGadget(preferences.getUChar("jetgadget", 0));
  app.setJetpackHeadstart(preferences.getBool("jethead", false));
  arcadeTilt.calibrate(preferences.getFloat("arcneutral", 0));
  arcadeBestGalaga = preferences.getUInt("galagabest", 0);
  arcadeBestArkanoid = preferences.getUInt("arkanbest", 0);
  arcadeBestJetpack = preferences.getUInt("jetbest", 0);
  app.settings.clock24Hour = preferences.getBool("clock24", true);
  const uint8_t boot = preferences.getUChar("bootgame", 0);
  app.settings.bootGame = boot < flappy::GameTitleCount ? flappy::GameTitle(boot) : flappy::GameTitle::FlappyBird;
  app.settings.bootCartridge = preferences.getUChar("bootcart", 0);
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
  preferences.putUChar("bootcart", app.settings.bootCartridge);
  preferences.putUChar("mode", uint8_t(app.settings.mode));
  preferences.putUChar("brightness", app.settings.brightnessIndex);
  preferences.putUChar("volume", app.settings.volumeIndex);
  preferences.putUChar("sleepidx", app.settings.sleepIndex);
  preferences.putUChar("skin", app.settings.skinIndex);
  preferences.putUChar("geolevel", app.settings.geometryLevel);
  preferences.putUChar("arcsens", app.settings.arcadeSensitivity);
  preferences.putBool("arcauto", app.settings.arcadeAutoFire);
  preferences.putUChar("arcdiff", app.settings.arcadeDifficulty);
  preferences.putBool("jetprac", app.settings.jetpackPractice);
  preferences.putUChar("jetdiff", app.settings.jetpackDifficulty);
  preferences.putUChar("jetride", app.settings.jetpackRide);
  preferences.putUChar("jetgadget", app.settings.jetpackGadget);
  preferences.putBool("jethead", app.settings.jetpackHeadstart);
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

bool playSound(const FlappySounds::Sound& sound) {
  if (piezoDriver || !soundReady || !app.volume() || !sound.length || !sound.samples) return false;
  if (!M5.Speaker.playRaw(sound.samples, sound.length, sound.sampleRate, false, 1, 2, true)) return false;
  piezo.stop();
  return true;
}

void applyOutputSettings() {
  if (appliedVolume != app.settings.volumeIndex) {
    const bool preview = appliedVolume != UINT8_MAX && app.menu.page == flappy::MenuPage::Volume;
    piezo.setVolumeIndex(app.settings.volumeIndex);
    appliedVolume = app.settings.volumeIndex;
    if (!app.volume()) stopGameAudio();
    else if (preview) playCue(flappy::PiezoCue::Preview, millis());
  }
  if (!piezoDriver && soundReady) {
    const bool stickS3 = M5.getBoard() == m5::board_t::board_M5StickS3;
    const bool external = pollPowerSource(millis()) == flappy::PowerSource::External;
    app.speakerBatteryCap = stickS3 && !external;
    const uint8_t level = flappy::speakerVolume(app.volume(), stickS3, external);
    if (appliedSpeakerVolume != level) {
      M5.Speaker.setVolume(level);
      appliedSpeakerVolume = level;
    }
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
  // Authored tones also cover builds whose local sample imports are empty.
  if (flags & flappy::Hit) {
    if (!playSound(FlappySounds::hit)) playCue(flappy::PiezoCue::Crash, millis());
  } else if (flags & flappy::Die) {
    if (!playSound(FlappySounds::die)) playCue(flappy::PiezoCue::Crash, millis());
  } else if (flags & flappy::Point) {
    if (!playSound(FlappySounds::point)) playCue(flappy::PiezoCue::Point, millis());
  } else if (flags & flappy::Flap) {
    if (!playSound(FlappySounds::wing)) playCue(flappy::PiezoCue::Flap, millis());
  }
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
  const bool scanOrder = landscape && nativeGeometry;
  if (nesLandscape == landscape && geometryScanOrder == scanOrder) return;
  asyncDisplay.drain();
  asyncDisplay.configureScanSync(false);
  nesLandscape = landscape;
  geometryScanOrder = scanOrder;
  invalidateDisplay();
  M5.Display.setRotation(landscape && !scanOrder ? 1 : 0);
  M5.Display.fillScreen(TFT_BLACK);
  if (scanOrder) asyncDisplay.configureScanSync(true);
  if (landscape && !scanOrder) {
    nesViewport.configure(M5.Display.width(), M5.Display.height());
    marioViewport.configure(M5.Display.width(), M5.Display.height());
  }
  else if (!landscape) viewport.configure(M5.Display.width(), M5.Display.height());
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
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = geometryHeld = 0;
  geometryButtons.reset();
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
  if (nativeGeometry) return;
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
  if (y2 == 239) {
    displayDamage.endFrame();
    if (asyncDisplay.enabled()) asyncDisplay.submitEndFrame();
  }
}

void nesAudio(void*, const int16_t* samples, size_t count, uint32_t rate) {
  if (piezoDriver || !soundReady) return;
  if (tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || zeroLaunching)) return;
  if (!nesAudioQueue || !app.volume() || app.menuOpen() || tetrisLaunching || zeroLaunching ||
      (nativeGeometry && geometryLauncher.active())) return;
  nesAudioQueue->submit(M5.Speaker, samples, count, rate);
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
  if (justConnected && nativeArcade()) arcadeControls.blockAction();
  if (justConnected && app.menu.page == flappy::MenuPage::Controller) {
    app.closeMenu();
    pendingNesButtons = pendingControllerNesButtons = 0;
    pendingPadAction = pendingAction = false;
    menuSound(menuBefore, selectionBefore, false, nowMs);
    return;
  }
  if (gameHome.active() && !app.menuOpen()) {
    if (pressed & (Menu | B)) app.openMenuFromController();
    else if (pressed & Left) pendingHomeNext = true;
    else pendingPadAction |= (pressed & (A | Start)) != 0;
    pendingNesButtons = pendingControllerNesButtons = 0;
    return;
  }
  if (tetrisLike()) {
    if (!app.menuOpen() && (pressed & (Start | Select))) openTetrisFrontend(false);
    pressed &= uint16_t(~(Start | Select));
    padButtons &= uint16_t(~(Start | Select));
    if (!app.menuOpen() && (pressed & Down)) pendingControllerTetrisDrop = true;
  }
  if ((pressed & Menu) || ((pressed & B) && (app.menuOpen() || (!emulatedGame() && !nativeArcade())))) {
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
  } else if (nativeArcade()) {
    if (jetpackGame && jetpackGame->phase() == flappy::JetpackGame::Phase::Spin && (pressed & Down)) jetpackCashRequested = true;
    if (pressed & Start) arcadePauseRequested = true;
    pendingPadAction |= (pressed & (A | B)) != 0;
  } else if (app.currentGame == flappy::GameTitle::FlappyBird) {
    pendingPadAction |= (pressed & (A | Start)) != 0;
  } else {
    if (tetrisLinkActive() && (coopLink.session().phase() == flappy::coop::Phase::Lobby ||
        (zeroLinked() ? zeroFinishedRound : flappy::TetrisGym::toppedOut(nes))))
      pendingPadAction |= (pressed & (A | Start)) != 0;
    pendingControllerNesButtons |= nesButtons(padButtons | pressed) & uint8_t(~nesButtons(padButtons & ~pressed));
  }
  menuSound(menuBefore, selectionBefore, false, nowMs);
  if (nativeArcade() && menuBefore != app.menu.page) arcadeControls.blockAction();
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
  if (!piezoDriver && soundReady && !nesAudioQueue) nesAudioQueue = new (std::nothrow) flappy::SpeakerPcmQueue;
  nes.setCallbacks(nesVideo, piezoDriver || !soundReady ? nullptr : nesAudio, nullptr);
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
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = geometryHeld = 0;
  geometryButtons.reset();
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

void saveArcadeScores() {
  if (!arcadeScoresDirty) return;
  preferences.putUInt("galagabest", arcadeBestGalaga);
  preferences.putUInt("arkanbest", arcadeBestArkanoid);
  preferences.putUInt("jetbest", arcadeBestJetpack);
  arcadeScoresDirty = false;
}

void syncJetpackMenus() {
  app.setJetpackProgress(jetpackProfile.coins(), jetpackProfile.state().levels);
  const auto& missions = jetpackProfile.missions();
  for (uint8_t i = 0; i < 3; ++i)
    app.setJetpackMission(i, missions.label(i), missions.progress(i), missions.target(i), missions.complete(i));
  app.setJetpackRank(missions.rank(), missions.stars(), missions.starTarget());
}

void saveJetpackProfile() {
  if (jetpackGame && jetpackProfile.active()) jetpackProfile.checkpointCoins(jetpackGame->coins());
  if (jetpackProfile.dirty()) {
    uint8_t saved[flappy::JetpackProfile::StorageSize];
    jetpackProfile.encode(saved);
    if (preferences.putBytes("jetprofile", saved, sizeof(saved)) == sizeof(saved)) jetpackProfile.clearDirty();
  }
  syncJetpackMenus();
}

void handleJetpackUpgrade() {
  if (handledJetpackUpgrade == app.jetpackUpgradeRequest) return;
  handledJetpackUpgrade = app.jetpackUpgradeRequest;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  if (diagnosticArcadeTesting) return;
#endif
  if (jetpackGame && jetpackProfile.active()) jetpackProfile.checkpointCoins(jetpackGame->coins());
  jetpackProfile.buyUpgrade(app.jetpackUpgradeKind);
  saveJetpackProfile();
}

void releaseNativeArcade() {
  if (jetpackGame) jetpackGame->settleRun();
  saveJetpackProfile();
  saveArcadeScores();
  delete galagaGame;
  delete arkanoidGame;
  delete jetpackGame;
  galagaGame = nullptr;
  arkanoidGame = nullptr;
  jetpackGame = nullptr;
  arcadePaused = arcadePauseRequested = jetpackCashRequested = false;
  arcadeControls.reset();
  arcadeTilt.reset();
  arcadeMove = 0;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  diagnosticArcadeFire = false;
  diagnosticArcadeMove = 0;
#endif
}

void configureJetpackLoadout() {
  if (!jetpackGame) return;
  bool eligible = !app.settings.jetpackPractice;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  eligible = eligible && !diagnosticArcadeTesting && diagnosticJetpackRide == 255;
  if (diagnosticJetpackRide != 255) {
    jetpackGame->setProfileEnabled(false);
    jetpackGame->setNextLoadout(diagnosticJetpackRide, diagnosticJetpackGadget, diagnosticJetpackHeadstart);
    return;
  }
#endif
  jetpackGame->setProfileEnabled(eligible);
  jetpackGame->setNextLoadout(app.settings.jetpackPractice && app.settings.jetpackRide ? app.settings.jetpackRide + 1 : 0,
      app.settings.jetpackPractice ? app.settings.jetpackGadget : 0,
      app.settings.jetpackPractice && app.settings.jetpackHeadstart);
}

void resetNativeArcade() {
  saveArcadeScores();
  if (galagaGame) galagaGame->reset(nonzeroRandom(), app.settings.arcadeDifficulty);
  if (arkanoidGame) arkanoidGame->reset(nonzeroRandom(), app.settings.arcadeDifficulty);
  if (jetpackGame) {
    jetpackGame->settleRun();
    saveJetpackProfile();
    jetpackGame->reset(nonzeroRandom(), app.settings.jetpackDifficulty);
    configureJetpackLoadout();
  }
  arcadeFrames = 0;
  arcadePaused = arcadePauseRequested = jetpackCashRequested = false;
  arcadeControls.blockAction();
  arcadeTilt.reset();
}

bool beginNativeArcade() {
  const int width = jetpackSelected() ? 240 : 135;
  const int height = jetpackSelected() ? 135 : 240;
  if (M5.Display.width() != width || M5.Display.height() != height) return false;
  if (app.currentGame == flappy::GameTitle::Galaga) galagaGame = new (std::nothrow) flappy::GalagaGame;
  else if (jetpackSelected()) jetpackGame = new (std::nothrow) flappy::JetpackGame;
  else arkanoidGame = new (std::nothrow) flappy::ArkanoidGame;
  if (!galagaGame && !arkanoidGame && !jetpackGame) return false;
  if (jetpackGame) jetpackGame->attachProfile(&jetpackProfile);
  resetNativeArcade();
  return true;
}

void calibrateArcadeTilt(uint32_t nowMs) {
  float ax, ay, az;
  if (M5.Imu.isEnabled() && M5.Imu.getAccel(&ax, &ay, &az) && arcadeTilt.calibrate(-ax)) {
    tiltX = -ax;
    haveTiltSample = true;
    lastTiltSampleMs = nowMs;
    arcadeActivityTilt = tiltX;
    preferences.putFloat("arcneutral", arcadeTilt.neutral());
    tiltZeroNotice = "Tilt zero saved";
    playCue(flappy::PiezoCue::MenuSelect, nowMs);
  } else tiltZeroNotice = "Tilt sensor unavailable";
  tiltZeroNoticeMs = nowMs;
  arcadeControls.blockAction();
}

void tickNativeArcade(bool actionEdge, uint32_t nowMs) {
  if (!galagaGame && !arkanoidGame && !jetpackGame) return;
  if (jetpackCashRequested) {
    jetpackCashRequested = false;
    if (jetpackGame && jetpackGame->cashInSpinTokens()) {
      saveJetpackProfile(); arcadePaused = arcadePauseRequested = false;
      arcadeControls.blockAction(); return;
    }
  }
  if (arcadePauseRequested || (arcadePaused && actionEdge)) {
    arcadePaused = !arcadePaused;
    arcadePauseRequested = false;
    arcadeControls.blockAction();
    arcadeTilt.reset();
    return;
  }
  if (arcadePaused) return;
  using namespace flappy::controller;
  const bool held = M5.BtnA.isPressed() || (controllerConnected && (padButtons & (A | B)))
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
      || diagnosticArcadeFire
#endif
      ;
  int move = jetpackGame ? 0 : controllerConnected ? ((padButtons & Right) ? 1024 : 0) - ((padButtons & Left) ? 1024 : 0) :
      arcadeTilt.update(tiltX, haveTiltSample && uint32_t(nowMs - lastTiltSampleMs) < 200, app.settings.arcadeSensitivity);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  if (diagnosticArcadeMove) move = diagnosticArcadeMove;
#endif
  flappy::ArcadeInput input = arcadeControls.input(move, held, actionEdge);
  input.action = input.action || input.pressed;
  arcadeMove = input.move;
  if (!jetpackGame && app.settings.arcadeAutoFire && arcadeControls.armed()) input.action = true;
  if (galagaGame) galagaGame->tick(input);
  else if (arkanoidGame) arkanoidGame->tick(input);
  else {
    jetpackGame->setNextDifficulty(app.settings.jetpackDifficulty);
    configureJetpackLoadout();
    jetpackGame->tick(input);
  }
  ++arcadeFrames;
  const uint32_t score = galagaGame ? galagaGame->score() : arkanoidGame ? arkanoidGame->score() : jetpackGame->score();
  uint32_t& best = galagaGame ? arcadeBestGalaga : arkanoidGame ? arcadeBestArkanoid : arcadeBestJetpack;
  bool recordScore = true;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  recordScore = !diagnosticArcadeTesting;
#endif
  if (jetpackGame && !jetpackGame->rankedRun()) recordScore = false;
  if (jetpackGame && jetpackGame->gameOver() && jetpackProfile.dirty()) saveJetpackProfile();
  if (recordScore && score > best) { best = score; arcadeScoresDirty = true; }
  if (recordScore && (galagaGame ? galagaGame->gameOver() : arkanoidGame ? arkanoidGame->gameOver() : jetpackGame->gameOver())) saveArcadeScores();
  const flappy::ArcadeCue cue = galagaGame ? galagaGame->takeCue() : arkanoidGame ? arkanoidGame->takeCue() : jetpackGame->takeCue();
  switch (cue) {
    case flappy::ArcadeCue::Fire: playCue(flappy::PiezoCue::Move, nowMs); break;
    case flappy::ArcadeCue::Hit: playCue(flappy::PiezoCue::Lock, nowMs); break;
    case flappy::ArcadeCue::Death: playCue(flappy::PiezoCue::Crash, nowMs); break;
    case flappy::ArcadeCue::Power: playCue(flappy::PiezoCue::Coin, nowMs); break;
    case flappy::ArcadeCue::Win: playCue(flappy::PiezoCue::LevelUp, nowMs); break;
    default: break;
  }
}

void drawNativeArcade() {
  if (!galagaGame && !arkanoidGame && !jetpackGame) return;
  asyncDisplay.drain();
  const uint32_t drawnAt = micros();
  if (galagaGame) galagaGame->draw(canvasStorage, arcadeBestGalaga);
  else if (arkanoidGame) arkanoidGame->draw(canvasStorage, arcadeBestArkanoid);
  else jetpackGame->draw(canvasStorage, arcadeBestJetpack);
  if (arcadePaused) {
    if (jetpackGame) {
      for (int y = 49; y < 86; ++y)
        for (int x = 60; x < 180; ++x) canvasStorage[y * 240 + x] = 0;
      flappy::MenuFont::draw(canvasStorage, 240, 135, "PAUSED", 102, 53, 0xffff);
      flappy::MenuFont::draw(canvasStorage, 240, 135, "M5 TO RESUME", 84, 68, 0xffff);
    } else {
      flappy::ArcadeCanvas canvas(canvasStorage);
      canvas.rect(9, 103, 117, 37, 0);
      canvas.text("PAUSED", 48, 110, 0xffff);
      canvas.text("M5 TO RESUME", 24, 125, 0xffff);
    }
  }
  const int width = jetpackGame ? 240 : 135, height = jetpackGame ? 135 : 240;
  if (asyncDisplay.submitBorrowedFrame(canvasStorage, width, height)) asyncDisplay.submitEndFrame();
  else {
    submitDisplay(0, 0, width, height, canvasStorage);
    if (asyncDisplay.enabled()) asyncDisplay.submitEndFrame();
  }
  recordLcdFrame(height, drawnAt);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  recordFrameTiming(arcadeFrames, 60);
#endif
}

void handlePlayGameRequest() {
  if (handledGameRequest == app.gameRequest) return;
  handledGameRequest = app.gameRequest;
  asyncDisplay.drain();
  if (app.currentGame == flappy::GameTitle::FlappyBird && app.settings.mode == flappy::Mode::Coop &&
      coopLink.session().role() == flappy::coop::Role::Off) app.setMode(flappy::Mode::Original);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  marioBenchmarkFrames = diagnosticPadFrames = 0;
  timingProbe = false;
#endif
  stopGameAudio();
  nes.unload();
#ifdef M5FLAP_USB_LIBRARY
  refreshCartridgeViews();
#endif
  releaseNativeArcade();
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
  nativeGeometry = false;
  if (app.currentGame != flappy::GameTitle::GeometryDash) {
    delete geometryPresentation;
    geometryPresentation = nullptr;
  }
  gameMessageShown = 0;
  tetrisFrontendAt = 0;
  runtimeNotice = "";
  if (nativeArcade()) {
    nes.unload();
    renderer->setBackgroundCacheEnabled(false);
    if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
    setNesDisplay(jetpackSelected());
    if (!beginNativeArcade()) {
      runtimeNotice = "Arcade memory/display unavailable";
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      handledGameRequest = app.gameRequest;
      app.openMenu(flappy::MenuPage::ChangeGame);
      releaseNativeArcade();
      renderer->setBackgroundCacheEnabled(true);
    }
  } else if (emulatedGame()) {
    renderer->setBackgroundCacheEnabled(false);
    if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
    if (!piezoDriver && soundReady && !nesAudioQueue) nesAudioQueue = new (std::nothrow) flappy::SpeakerPcmQueue;
    nes.setCallbacks(nesVideo, piezoDriver || !soundReady ? nullptr : nesAudio, nullptr);
    const bool tetris = app.currentGame == flappy::GameTitle::Tetris;
    const bool zero = app.currentGame == flappy::GameTitle::TetrisZero;
    const bool geometry = app.currentGame == flappy::GameTitle::GeometryDash;
    geometryHeld = 0;
    geometryButtons.reset();
    setNesDisplay(!tetris && !zero);
    const bool displayReady = geometry ? M5.Display.width() == 240 && M5.Display.height() == 135 :
        zero ? zeroViewport.configure(M5.Display.width(), M5.Display.height()) :
        !tetris || tetrisViewport.configure(M5.Display.width(), M5.Display.height());
    const bool validCartridge = geometry ? flappy::RomFingerprint::famidash(GeometryRom::data, GeometryRom::length) :
        tetris ? flappy::RomFingerprint::tetris(TetrisRom::data, TetrisRom::length, false) :
        !zero || flappy::RomFingerprint::tetris(TetrisZeroRom::data, TetrisZeroRom::length, true);
    if (geometry && displayReady && validCartridge && !geometryPresentation)
      geometryPresentation = new (std::nothrow) GeometryPresentation;
    const bool presentationReady = !geometry || geometryPresentation;
    const bool loaded = displayReady && validCartridge && presentationReady && (zero ? beginZero(zeroSettings()) :
        nes.load(tetris ? TetrisRom::data : geometry ? GeometryRom::data : FlappyRom::data,
                 tetris ? TetrisRom::length : geometry ? GeometryRom::length : FlappyRom::length));
    const bool ntscCartridge = tetris || zero || !loaded || nes.frameRate() == 60;
    if (!loaded || !ntscCartridge) {
      runtimeNotice = !ntscCartridge ? "Import an NTSC ROM" : !validCartridge ?
          (geometry ? "Geometry Dash ROM mismatch" : "Unsupported Tetris cartridge") :
          !displayReady ? geometry ? "Geometry needs 240x135" : "Tetris needs a 135x240 display" :
          !presentationReady ? "Geometry memory unavailable" : nes.error();
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      handledGameRequest = app.gameRequest;
      app.openMenu(flappy::MenuPage::ChangeGame);
      nes.unload();
      delete geometryPresentation;
      geometryPresentation = nullptr;
      renderer->setBackgroundCacheEnabled(true);
      return;
    }
    nes.setPal(false);
    configureNesPerformance();
    if (geometry) {
      nativeGeometry = true;
      geometryRenderer.reset();
      geometryPresentation->world.reset();
      geometryPresentation->level.reset();
      geometryController.reset();
      geometryLauncher.request(app.settings.geometryLevel);
      runtimeNotice = "";
      handledGeometryRequest = app.geometryRequest;
      const uint8_t noRgb[30] = {};
      nes.setVideoRowMask(noRgb, sizeof(noRgb));
    }
    if (tetris) {
      tetrisLaunching = true;
      tetrisLaunchStage = tetrisStableFrames = 0;
      tetrisOptions.level = app.settings.tetrisLevel;
      tetrisOptions.trainer = app.settings.tetrisGameType;
      tetrisOptions.pal = app.settings.tetrisPal;
    } else if (!zero && !geometry) {
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

flappy::ClockTime homeTime(uint32_t nowMs) {
  flappy::ClockTime time;
  time.hour = app.clockHour(nowMs); time.minute = app.clockMinute(nowMs);
  time.second = app.clockSecond(nowMs); time.ms = nowMs;
  time.twentyFourHour = app.settings.clock24Hour;
  return time;
}

void handleGameRequest() {
  gameHome.observe(app);
  if (handledHomeGeneration == gameHome.generation() && handledGameRequest == app.gameRequest) return;
  handledHomeGeneration = gameHome.generation();
  asyncDisplay.drain();
  if (!gameHome.active()) {
    homeClocks.release();
    handledGameRequest = ~app.gameRequest;
    handlePlayGameRequest();
    return;
  }
  stopGameAudio();
  nes.unload();
  releaseNativeArcade();
  delete geometryPresentation; geometryPresentation = nullptr;
#ifdef M5FLAP_USB_LIBRARY
  refreshCartridgeViews();
#endif
  if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  tetrisLaunching = zeroLaunching = marioLaunching = false;
  nativeMario = nativeGeometry = false;
  renderer->setBackgroundCacheEnabled(false);
  clearPendingInput();
  nesSystemButton.reset(M5.BtnA.isPressed());
  setNesDisplay(false);
  homeClocks.reset(app.currentGame,nonzeroRandom(),FlappyRom::data,FlappyRom::length);
  runtimeNotice = homeClocks.ready() ? "" : "Clock memory unavailable";
  homeFrames = 0;
  handledGameRequest = app.gameRequest;
  invalidateDisplay();
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  marioBenchmarkFrames = diagnosticPadFrames = 0;
  timingProbe = false;
#endif
  nextNesTick = micros();
}

void drawGameHome(uint32_t nowMs) {
  asyncDisplay.drain();
  const uint32_t drawnAt = micros();
  const bool compact = viewport.width < 135 || viewport.height < 240;
  const int width = compact ? viewport.width : 135, height = compact ? viewport.height : 240;
  if (compact) {
    const auto time = homeTime(nowMs);
    memset(canvasStorage,0,size_t(width)*height*sizeof(uint16_t));
    char label[6]; snprintf(label,sizeof(label),"%02u:%02u",unsigned(time.displayHour()),unsigned(time.minute));
    flappy::MenuFont::draw(canvasStorage,width,height,label,(width-60)/2,height/2-12,flappy::BittyUi::Text,2);
    flappy::MenuFont::draw(canvasStorage,width,height,app.gameLabel(app.currentGame),4,12,flappy::BittyUi::Muted,1,width/6-1);
    flappy::MenuFont::draw(canvasStorage,width,height,"M5 PLAY",4,height-30,flappy::BittyUi::Muted);
    flappy::MenuFont::draw(canvasStorage,width,height,"LEFT NEXT",4,height-16,flappy::BittyUi::Muted);
  } else {
    homeClocks.draw(canvasStorage,homeTime(nowMs),renderer->skin());
    if (!homeClocks.ready()) {
      flappy::MenuFont::draw(canvasStorage,135,240,"CLOCK MEMORY",25,98,flappy::BittyUi::Text);
      flappy::MenuFont::draw(canvasStorage,135,240,"PRESS M5 TO PLAY",19,116,flappy::BittyUi::Text);
    }
    flappy::ClockScene::rect(canvasStorage,0,225,135,15,flappy::BittyUi::Black);
    flappy::MenuFont::draw(canvasStorage,135,240,"M5 PLAY  LEFT NEXT",16,227,flappy::BittyUi::Muted);
  }
  if (asyncDisplay.submitBorrowedFrame(canvasStorage,width,height)) asyncDisplay.submitEndFrame();
  else {
    submitDisplay(0,0,width,height,canvasStorage);
    if (asyncDisplay.enabled()) asyncDisplay.submitEndFrame();
  }
  recordLcdFrame(height,drawnAt);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  recordFrameTiming(homeFrames,60);
#endif
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
  if (handledGeometryRequest != app.geometryRequest) {
    handledGeometryRequest = app.geometryRequest;
    if (nativeGeometry) {
      asyncDisplay.drain();
      stopGameAudio();
      nes.softReset();
      geometryRenderer.reset();
      geometryPresentation->world.reset();
      geometryPresentation->level.reset();
      geometryController.reset();
      geometryLauncher.request(app.settings.geometryLevel);
      runtimeNotice = "";
      pendingNesButtons = pendingControllerNesButtons = geometryHeld = 0;
      geometryButtons.reset();
      nextNesTick = micros();
    }
  }
  if (handledRestart != app.gameRestartRequest) {
    handledRestart = app.gameRestartRequest;
    if (nativeArcade()) resetNativeArcade();
    if (app.currentGame == flappy::GameTitle::Nes || app.currentGame == flappy::GameTitle::GeometryDash) {
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
  if (previousMenu != app.menuOpen()) { asyncDisplay.drain(); invalidateDisplay(); }
  previousMenu = app.menuOpen();
  if (emulatedGame() && app.menuOpen()) gameMessageShown = 0;
  const bool landscape = (app.currentGame == flappy::GameTitle::Nes ||
      app.currentGame == flappy::GameTitle::GeometryDash || jetpackSelected()) && !app.menuOpen() && !gameHome.active();
  setNesDisplay(landscape);
  if (gameHome.active() && !app.menuOpen()) { drawGameHome(nowMs); return; }
  if (nativeArcade() && !app.menuOpen()) { drawNativeArcade(); return; }
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
  if (app.menuOpen()) options.battery = pollMenuBattery(nowMs);
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
  options.notice = (app.menu.page == flappy::MenuPage::TetrisControls ||
      (nativeArcade() && app.menu.page == flappy::MenuPage::GameSettings)) &&
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

void handleHomePlayRequests(bool coopStart) {
  gameHome.observe(app);
  const bool tetrisLinkStart = gameHome.active() && app.tetrisLinkRole && app.tetrisLinkRequest != handledTetrisLink;
  if (tetrisLinkStart) app.closeMenu();
  if (gameHome.active() && !app.menuOpen() && (coopStart || tetrisLinkStart || app.gameRestartRequest != handledRestart ||
      app.geometryRequest != handledGeometryRequest || app.tetrisRequest != handledTetrisRequest ||
      app.tetrisZeroRequest != handledZeroRequest)) gameHome.play();
}

void prepareNativeHomePlay() {
  arcadeControls.input(0,false,false);
  if (jetpackGame) jetpackGame->tick(flappy::ArcadeInput());
}

void stepGame(bool actionEdge, bool menuDown, uint32_t nowMs) {
  gameHome.observe(app);
  if (pendingHomeNext && gameHome.active() && !app.menuOpen()) {
    gameHome.cycle(app);
    handleGameRequest();
    actionEdge = false;
  }
  pendingHomeNext = false;
  if (gameHome.active() && !app.menuOpen() && actionEdge) {
    gameHome.play();
    if (app.currentGame == flappy::GameTitle::FlappyBird) app.restart();
    handleGameRequest();
    if (nativeArcade()) prepareNativeHomePlay();
    nesSystemButton.reset(M5.BtnA.isPressed());
  }
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
  const uint8_t events = app.step(actionForApp, menuDown, nowMs, !gameHome.active());
  if (nativeArcade() && menuWasOpen != app.menuOpen()) {
    arcadeControls.blockAction();
    arcadeTilt.reset();
  }
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
  const bool coopStart = app.coopRequest != handledCoopRequest && app.settings.coopRole;
  handleCoopRequest(nowMs);
  handleControllerRequest(nowMs);
  handleHomePlayRequests(coopStart);
  handleGameRequest();
  handleTetrisRequests(nowMs);
  handleJetpackUpgrade();
  if (handledArcadeCalibrate != app.arcadeCalibrateRequest) {
    handledArcadeCalibrate = app.arcadeCalibrateRequest;
    calibrateArcadeTilt(nowMs);
  }
  if (gameHome.active() && !app.menuOpen()) { homeClocks.step(homeTime(nowMs)); ++homeFrames; }
  if (!gameHome.active() && nativeArcade() && !menuWasOpen && !app.menuOpen()) tickNativeArcade(actionEdge, nowMs);
  menuSound(menuBefore, selectionBefore, actionForApp, nowMs);
  applyOutputSettings();
  if ((events & flappy::GameOver) || app.menuOpen() || app.settingsDirty) saveScores();
  if (app.menuOpen()) { saveArcadeScores(); saveJetpackProfile(); }
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

flappy::PowerSource pollPowerSource(uint32_t nowMs, bool force) {
  if (!force && powerPolled && uint32_t(nowMs - lastPowerPollMs) < 1000) return powerSource;
  powerPolled = true;
  lastPowerPollMs = nowMs;
  const auto board = M5.getBoard();
  if (board == m5gfx::board_t::board_M5StickS3) {
    uint8_t source = 0;
    const bool readOk = M5.In_I2C.readRegister(0x6e, 0x04, &source, 1, 100000);
    return powerSource = flappy::pm1PowerSource(readOk, source);
  }
  if (board != m5gfx::board_t::board_M5StickC && board != m5gfx::board_t::board_M5StickCPlus)
    return powerSource = flappy::PowerSource::Unknown;
#if defined(CONFIG_IDF_TARGET_ESP32)
  uint8_t status[2] = {};
  const bool readOk = M5.Power.Axp192.readRegister(0, status, 2);
  return powerSource = flappy::axp192PowerSource(readOk, status[0], status[1]);
#else
  return powerSource = flappy::PowerSource::Unknown;
#endif
}

flappy::BatteryStatus pollMenuBattery(uint32_t nowMs, bool force) {
  if (!force && batteryPolled && uint32_t(nowMs - lastBatteryPollMs) < 1000) return menuBattery;
  batteryPolled = true;
  lastBatteryPollMs = nowMs;
  const auto source = pollPowerSource(nowMs);
  const auto board = M5.getBoard();
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  if (board == m5gfx::board_t::board_M5StickS3) {
    uint16_t mv = 0;
    const bool valid = M5.Power.M5pm1.getBatteryVoltage(&mv);
    // The pinned M5PM1 driver does not report charge activity. Show USB power only.
    return menuBattery = flappy::batteryStatus(valid,mv,source);
  }
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
  if (board == m5gfx::board_t::board_M5StickC || board == m5gfx::board_t::board_M5StickCPlus) {
    uint8_t status[2] = {}, voltage[2] = {};
    const bool statusOk = M5.Power.Axp192.readRegister(0,status,2);
    const bool voltageOk = M5.Power.Axp192.readRegister(0x78,voltage,2);
    const unsigned raw = (unsigned(voltage[0]) << 4) | (voltage[1] & 15u);
    return menuBattery = flappy::batteryStatus(statusOk && voltageOk && (status[1] & 0x20u),
        int(raw * 11 / 10),flappy::axp192PowerSource(statusOk,status[0],status[1]),status[0] & 4u);
  }
#endif
  return menuBattery = flappy::batteryStatus(false,0,source);
}

void showBootSplash() {
  const uint32_t started = millis();
  uint32_t next = micros();
  unsigned frames = 0;
  do {
    const uint32_t elapsed = uint32_t(millis() - started);
    flappy::BittyUi::splash(canvasStorage,flappy::PixelWidth,flappy::PixelHeight,elapsed);
    pushAppCanvas();
    asyncDisplay.drain();
    ++frames;
    M5.update();
    if (elapsed >= 160 && (M5.BtnA.isPressed() || M5.BtnB.isPressed())) break;
    next += TickMicros;
    while (int32_t(micros() - next) < 0) delay(1);
  } while (uint32_t(millis() - started) < flappy::BittyUi::SplashMs);
  asyncDisplay.drain();
  invalidateDisplay();
  clearPendingInput();
  inactivity.resumed(millis());
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.printf("BOOT splash_ms=%lu splash_frames=%u\n",(unsigned long)(millis() - started),frames);
#endif
}

void clearPendingInput() {
  pendingHomeNext = false;
  arcadeControls.blockAction();
  arcadeTilt.reset();
  arcadePauseRequested = false;
  pendingAction = pendingPadAction = false;
  pendingNesButtons = pendingControllerNesButtons = tetrisHeld = geometryHeld = 0;
  geometryButtons.reset();
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
  saveArcadeScores();
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
  saveArcadeScores();
  saveJetpackProfile();
  stopGameAudio();
  soundOverride = false;
  if (piezoDriver) { piezoOutput.request(0, 0); servicePiezoOutput(); }
  else if (soundReady) M5.Speaker.end();
  asyncDisplay.drain();
  const bool stickS3 = M5.getBoard() == m5gfx::board_t::board_M5StickS3;
  const gpio_num_t frontPin = stickS3 ? GPIO_NUM_11 : GPIO_NUM_37;
  const gpio_num_t sidePin = stickS3 ? GPIO_NUM_12 : GPIO_NUM_39;
  const bool frontWake = gpio_wakeup_enable(frontPin, GPIO_INTR_LOW_LEVEL) == ESP_OK;
  const bool sideWake = gpio_wakeup_enable(sidePin, GPIO_INTR_LOW_LEVEL) == ESP_OK;
  const bool gpioWake = frontWake && sideWake && esp_sleep_enable_gpio_wakeup() == ESP_OK;
  if (gpioWake) {
    M5.Display.sleep();
    esp_sleep_enable_timer_wakeup(1000000ULL);
    while (pollPowerSource(millis(), true) == flappy::PowerSource::Battery &&
           gpio_get_level(frontPin) && gpio_get_level(sidePin)) {
      if (esp_light_sleep_start() != ESP_OK) break;
      if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) break;
      M5.update();
      if (M5.BtnPWR.wasClicked() || M5.BtnPWR.wasPressed()) break;
    }
    M5.Display.wakeup();
    delay(120);
    M5.Display.setBrightness(app.brightness());
  }
  gpio_wakeup_disable(frontPin);
  gpio_wakeup_disable(sidePin);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  M5.update();
  clearPendingInput();
  inactivity.resumed(millis());
  invalidateDisplay();
  lastClockReadMs = millis() - 1000;
  if (!piezoDriver && soundReady) {
    M5.Speaker.setVolume(0);
    soundReady = M5.Speaker.begin();
    appliedSpeakerVolume = UINT8_MAX;
    applyOutputSettings();
  }
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
#if defined(M5FLAP_STARTUP_DIAGNOSTICS) || defined(M5FLAP_USB_LIBRARY)
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
#ifdef M5FLAP_BOARD_STICKS3
  if (M5.getBoard() != m5::board_t::board_M5StickS3) {
    startupFailure("StickS3 hardware not detected");
    return;
  }
#endif
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  Serial.printf("BOOT board=%u display=%ux%u heap=%u cpu_mhz=%u boot_flash_hz=%u\n", unsigned(M5.getBoard()), M5.Display.width(), M5.Display.height(), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), ESP.getFlashChipSpeed());Serial.flush();
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
  rtc_gpio_pullup_dis(GPIO_NUM_2);
  rtc_gpio_pulldown_en(GPIO_NUM_2);
#endif
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
  app.setGeometryDashAvailable(GeometryRom::length > 0);
  app.setNesTitle(marioCartridge.supported() ? "Mario" : FlappyRom::title);
  app.setTetrisAvailable(flappy::RomFingerprint::tetris(TetrisRom::data, TetrisRom::length, false) &&
      M5.Display.width() >= 131 && M5.Display.height() >= 240);
  app.setTetrisTitle(TetrisRom::title);
  app.setTetrisZeroAvailable(flappy::RomFingerprint::tetris(TetrisZeroRom::data, TetrisZeroRom::length, true) &&
      M5.Display.width() >= 135 && M5.Display.height() >= 240);
  app.setTetrisZeroTitle(TetrisZeroRom::title);
  loadSettings();
  applyOutputSettings();
  showBootSplash();
#ifdef M5FLAP_USB_LIBRARY
  initializeLibrary();
#endif
  syncJetpackMenus();
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
    } else if (!strcmp(command, "M5GALAGA") || !strcmp(command, "M5ARKANOID") || !strcmp(command, "M5JETPACK")) {
      app.setCurrentGame(!strcmp(command, "M5JETPACK") ? flappy::GameTitle::JetpackJoyride :
          !strcmp(command, "M5GALAGA") ? flappy::GameTitle::Galaga : flappy::GameTitle::Arkanoid);
      app.closeMenu(); handleGameRequest();
    } else if (!strncmp(command, "M5ARCADE ", 9)) {
      int move = 0; unsigned fire = 0;
      if (sscanf(command + 9, "%d %u", &move, &fire) == 2 && move >= -1024 && move <= 1024 && fire <= 1) {
        diagnosticArcadeMove = int16_t(move);
        pendingPadAction |= fire && !diagnosticArcadeFire;
        diagnosticArcadeFire = fire;
      }
    } else if (!strncmp(command, "M5JETLOAD ", 10)) {
      unsigned ride = 0, gadget = 0, headstart = 0;
      if (!strcmp(command + 10, "CLEAR")) {
        diagnosticJetpackRide = 255;
        if (jetpackGame) resetNativeArcade();
      } else if (sscanf(command + 10, "%u %u %u", &ride, &gadget, &headstart) == 3 &&
                 ride <= 9 && gadget <= 6 && headstart <= 1) {
        diagnosticJetpackRide = uint8_t(ride);
        diagnosticJetpackGadget = uint8_t(gadget);
        diagnosticJetpackHeadstart = headstart != 0;
        if (jetpackGame) resetNativeArcade();
      }
    } else if (!strncmp(command, "M5JETSAFE ", 10)) {
      unsigned value = 0;
      if (jetpackGame && sscanf(command + 10, "%u", &value) == 1 && value <= 1)
        jetpackGame->setDiagnosticProtection(value != 0);
    } else if (!strncmp(command, "M5JETPOWER ", 11)) {
      unsigned power = 0, level = 0;
      if (diagnosticArcadeTesting && jetpackGame && sscanf(command + 11, "%u %u", &power, &level) >= 1 && power < 7 && level <= 9)
        jetpackGame->diagnosticPower(uint8_t(power), uint8_t(level));
    } else if (!strcmp(command, "M5JETBURST")) {
      if (diagnosticArcadeTesting && jetpackGame) jetpackGame->diagnosticCoinBurst();
    } else if (!strncmp(command, "M5JETSPIN ", 10)) {
      unsigned tokens = 0;
      if (diagnosticArcadeTesting && jetpackGame && sscanf(command + 10, "%u", &tokens) == 1 && tokens <= 65535)
        jetpackGame->diagnosticSpin(uint16_t(tokens));
    } else if (!strcmp(command, "M5JETCASH")) {
      jetpackCashRequested = true;
    } else if (!strcmp(command, "M5JETTRACE")) {
      if (jetpackGame) {
        const uint32_t last = jetpackGame->eventSerial();
        for (uint32_t i = last > 15 ? last - 15 : 1; i && i <= last; ++i) {
          flappy::JetpackProgression::Event event;
          if (jetpackGame->event(i, event))
            Serial.printf("JETPACK_EVENT serial=%lu kind=%u detail=%u native=%lu encounter=%lu streak=%u\n",
                (unsigned long)i, unsigned(event.kind), unsigned(event.detail), (unsigned long)event.nativeDistance,
                (unsigned long)event.encounterDistance, unsigned(event.streak));
        }
        const auto& progression = jetpackGame->progression();
        Serial.printf("JETPACK_DUE content=%lu power_m=%lu vehicle_m=%lu blocked_m=%lu recovery=%lu\n",
            (unsigned long)progression.nextContent(), (unsigned long)progression.powerDue(),
            (unsigned long)progression.vehicleDue(), (unsigned long)progression.vehicleBlockedUntil(),
            (unsigned long)progression.recoveryUntil());
      }
    } else if (!strcmp(command, "M5PAUSE")) {
      if (nativeArcade()) arcadePauseRequested = true;
    } else if (!strcmp(command, "M5MENU")) {
      app.openMenu(flappy::MenuPage::Home);
      clearPendingInput();
    } else if (!strncmp(command,"M5UI ",5)) {
      unsigned page = 0, selection = 0;
      if (sscanf(command + 5,"%u %u",&page,&selection) == 2 && page >= 1 &&
          page <= unsigned(flappy::MenuPage::JetpackMissions)) {
        app.openMenu(flappy::MenuPage(page));
        if (selection < app.menuItemCount()) app.menu.selection = uint8_t(selection);
        clearPendingInput();
      }
    } else if (!strcmp(command,"M5GAMEFRAME") && !app.menuOpen() && (gameHome.active() || (nativeMario && !marioLaunching))) {
      asyncDisplay.drain();
      const int width = gameHome.active() ? (viewport.width < 135 ? viewport.width : 135) : 240;
      const int height = gameHome.active() ? (viewport.height < 240 ? viewport.height : 240) : 135;
      Serial.printf("GAMEFRAME %d %d\n",width,height);
      Serial.write(reinterpret_cast<const uint8_t*>(canvasStorage),size_t(width)*height*sizeof(uint16_t));
      Serial.println("\nGAMEFRAME_END");
      nextNesTick = micros() + 1000000u / (gameHome.active() ? 60u : nes.frameRate());
      nextTick = micros() + TickMicros;
    } else if (!strcmp(command,"M5UIFRAME") && app.menuOpen()) {
      showFrame(millis());
      asyncDisplay.drain();
      Serial.printf("UIFRAME %d %d\n",viewport.width,viewport.height);
      for (int y = 0; y < viewport.height; y += StripRows) {
        const int rows = viewport.height - y < StripRows ? viewport.height - y : StripRows;
        viewport.copyRows(canvasStorage,stripStorage,y,rows);
        Serial.write(reinterpret_cast<const uint8_t*>(stripStorage),size_t(viewport.width) * rows * 2);
      }
      Serial.println("\nUIFRAME_END");
    } else if (!strncmp(command, "M5TESTMODE ", 11)) {
      unsigned value = 0;
      if (sscanf(command + 11, "%u", &value) == 1 && value <= 1 && diagnosticArcadeTesting != (value != 0)) {
        diagnosticArcadeTesting = value != 0;
        if (jetpackGame) resetNativeArcade();
      }
    } else if (!strcmp(command, "M5GEOMETRY")) {
      app.setCurrentGame(flappy::GameTitle::GeometryDash);
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5MARIOCLOCK")) {
      app.setCurrentGame(flappy::GameTitle::Nes);
      gameHome.observe(app); gameHome.enter();
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5MARIO")) {
      app.setCurrentGame(flappy::GameTitle::Nes);
      gameHome.observe(app); gameHome.play();
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5PLAY")) {
      gameHome.observe(app); gameHome.play();
      app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5HOME")) {
      gameHome.enter(); app.closeMenu(); handleGameRequest();
    } else if (!strcmp(command, "M5NEXT")) {
      gameHome.cycle(app); handleGameRequest();
    } else if (!strcmp(command, "M5DEMO") || !strcmp(command, "M5ORIGINAL")) {
      app.setCurrentGame(flappy::GameTitle::FlappyBird);
      app.setMode(!strcmp(command, "M5DEMO") ? flappy::Mode::Demo : flappy::Mode::Original);
      app.closeMenu(); handleGameRequest();
    } else if (!strncmp(command, "M5SKIN ", 7)) {
      unsigned skin = 0;
      if (sscanf(command + 7, "%u", &skin) == 1 && skin < renderer->skinCount()) app.setSkin(uint8_t(skin));
    } else if (!strncmp(command, "M5PAD ", 6)) {
      unsigned mask = 0, frames = 0;
      if (sscanf(command + 6, "%x %u", &mask, &frames) == 2 && mask <= 255 && frames <= 600) {
        diagnosticPad = uint8_t(mask);
        diagnosticPadFrames = uint16_t(frames);
      }
    } else if (!strncmp(command, "M5LEVEL ", 8)) {
      unsigned level = 0;
      if (sscanf(command + 8, "%u", &level) == 1 && level < flappy::GeometryLevelCount) {
        app.setGeometryLevel(uint8_t(level));
        app.requestGameRestart();
      }
    } else if (!strcmp(command, "M5RUN")) {
      marioBenchmarkFrames = 600;
    } else if (!strcmp(command, "M5SCAN")) {
      asyncDisplay.drain();
      uint32_t stamps[96];
      uint16_t lines[96];
      for (unsigned i = 0; i < 96; ++i) {
        stamps[i] = micros();
        lines[i] = M5.Display.getScanLine();
        delayMicroseconds(500);
      }
      for (unsigned i = 0; i < 96; ++i)
        Serial.printf("SCAN sample=%u t=%lu line=%u\n", i, (unsigned long)stamps[i], unsigned(lines[i]));
    } else if (!strcmp(command, "M5PANEL")) {
      asyncDisplay.drain();
      auto* bus = M5.Display.panel()->getBus();
      const uint32_t priorClock = bus->getReadClock();
      const auto burst = [bus](uint8_t reg, uint8_t dummy, uint8_t bits) {
        M5.Display.startWrite();
        bus->writeCommand(reg, 8);
        bus->wait();
        bus->beginRead(dummy);
        const uint32_t result = bus->readData(bits);
        bus->endRead();
        M5.Display.endWrite();
        return result;
      };
      const uint8_t dummyBits[] = {0, 1, 8, 9};
      const uint32_t clocks[] = {1000000, 5000000, 15000000};
      for (uint32_t clock : clocks) {
        bus->setReadClock(clock);
        for (uint8_t dummy : dummyBits) {
          uint32_t stamps[96], raw[96];
          for (unsigned i = 0; i < 96; ++i) {
            stamps[i] = micros();
            raw[i] = burst(0x45, dummy, 16);
            delayMicroseconds(500);
          }
          Serial.printf("PANEL clock=%lu dummy=%u madctl=%08lx colmod=%08lx\n",
              (unsigned long)clock, unsigned(dummy),
              (unsigned long)burst(0x0b, dummy, 16), (unsigned long)burst(0x0c, dummy, 16));
          for (unsigned i = 0; i < 96; ++i)
            Serial.printf("RAWSCAN sample=%u t=%lu raw=%08lx\n", i,
                (unsigned long)stamps[i], (unsigned long)raw[i]);
        }
      }
      bus->setReadClock(priorClock);
    } else if (!strcmp(command, "M5TIMING")) {
      for (auto& value : geometryStageUs) value = 0;
      geometryProfileFrames = 0;
      timingProbe = true;
      timingIntervals = 0;
      timingStart = timingPrevious = 0;
      timingMin = UINT32_MAX;
      timingMax = 0;
      asyncDisplay.startFrameProbe(500);
    } else if (!strncmp(command, "M5CACHE ", 8)) {
      unsigned mask = 0;
      if (!gameHome.active() && sscanf(command + 8, "%u", &mask) == 1 && (mask == 0 || mask == 2 || mask == 3)) {
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
        benchmarkPcmOverride = true;
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
      Serial.printf("HOME active=%u frames=%lu state_bytes=%u fallback=%u generation=%lu hour=%u minute=%u\n",
          unsigned(gameHome.active()), (unsigned long)homeFrames, unsigned(homeClocks.stateBytes()),
          unsigned(homeClocks.fallback()), (unsigned long)gameHome.generation(),
          unsigned(app.clockHour(millis())), unsigned(app.clockMinute(millis())));
      if (nativeMario) {
        const auto player = marioCartridge.sample(nes);
        Serial.printf("MARIO screen=%u mode=%u task=%u y=%d h=%d camera=%d hz=%u divisor=%u launch=%u x=%u engine=%u\n",
            unsigned(marioCartridge.screen(nes)), unsigned(nes.readRam(0x770)), unsigned(nes.readRam(0x772)),
            player.y, player.height, marioViewport.worldTop(), unsigned(nes.frameRate()),
            unsigned(nes.videoDivisor()), unsigned(marioLaunching),
            unsigned(nes.readRam(0x6d)) * 256 + unsigned(nes.readRam(0x86)), unsigned(nes.readRam(0x0e)));
      }
      if (nativeGeometry) {
        flappy::GeometryScene scene;
        scene.sample([](uint16_t address) { return nes.readRam(address); });
        Serial.printf("GEOMETRY state=%u level=%u mode=%u x=%u y=%u scroll_x=%lu scroll_y=%u hz=%u divisor=%u\n",
            unsigned(scene.state), unsigned(scene.level), unsigned(scene.mode),
            unsigned(scene.players[0].pixelX()), unsigned(scene.players[0].pixelY()),
            (unsigned long)scene.scrollX, unsigned(scene.scrollY), unsigned(nes.frameRate()),
            unsigned(nes.videoDivisor()));
      }
      if (app.currentGame == flappy::GameTitle::TetrisZero)
        Serial.printf("ZERO stage=%u players=%u level1=%u level2=%u active1=%u active2=%u launch=%u linkframe=%lu\n",
            unsigned(flappy::TetrisZero::stage(nes)), unsigned(flappy::TetrisZero::players(nes)),
            unsigned(flappy::TetrisZero::level(nes)), unsigned(flappy::TetrisZero::level(nes, 2)),
            unsigned(flappy::TetrisZero::active(nes)), unsigned(flappy::TetrisZero::active(nes, 2)),
            unsigned(zeroLaunching), (unsigned long)coopLink.zeroFrame());
      if (nativeArcade()) Serial.printf("ARCADE frames=%lu paused=%u stage=%u score=%lu over=%u move=%d armed=%u\n",
          (unsigned long)arcadeFrames, unsigned(arcadePaused),
          unsigned(galagaGame ? galagaGame->stage() : arkanoidGame ? arkanoidGame->stage() : 0),
          (unsigned long)(galagaGame ? galagaGame->score() : arkanoidGame ? arkanoidGame->score() : jetpackGame ? jetpackGame->score() : 0),
          unsigned(galagaGame ? galagaGame->gameOver() : arkanoidGame ? arkanoidGame->gameOver() : jetpackGame ? jetpackGame->gameOver() : false),
          int(arcadeMove), unsigned(arcadeControls.armed()));
      if (jetpackGame) Serial.printf("JETPACK phase=%u meters=%lu coins=%lu y=%d speed_q8=%ld vehicle=%u\n",
          unsigned(jetpackGame->phase()), (unsigned long)jetpackGame->distance(),
          (unsigned long)jetpackGame->coins(), jetpackGame->playerY(),
          (long)jetpackGame->speed(), unsigned(jetpackGame->vehicle()));
      if (jetpackGame) Serial.printf("JETPACK_EXTRA tokens=%u letters=%u interactions=%u bounces=%u state_bytes=%u ride_override=%u\n",
          unsigned(jetpackGame->spinTokens()), unsigned(jetpackGame->samLetters()),
          unsigned(jetpackGame->scientistInteractions()), unsigned(jetpackGame->crashBounces()),
          unsigned(sizeof(flappy::JetpackGame)), unsigned(diagnosticJetpackRide));
      if (jetpackGame) Serial.printf("JETPACK_BONUS scale_q16=%lu waves=%u shots=%u drops=%u choices=%u attracted=%u shower_pending=%u reels=%u reel_age=%u\n",
          (unsigned long)jetpackGame->movementScale(), unsigned(jetpackGame->rocketWaveCount()),
          unsigned(jetpackGame->rocketShotCount()), unsigned(jetpackGame->rocketDroppedShots()),
          unsigned(jetpackGame->rocketChoiceCount()), unsigned(jetpackGame->attractedCoinCount()),
          unsigned(jetpackGame->cashShowerPending()), unsigned(jetpackGame->spinPresentation().running()),
          unsigned(jetpackGame->spinPresentation().age()));
#ifdef M5FLAP_JETPACK_IMPORTED
      Serial.printf("JETPACK_PACK patterns=%u points=%u original_runner=%u\n",
          unsigned(flappy::JetpackImported::PatternCount), unsigned(flappy::JetpackImported::CoinPointCount),
          unsigned(flappy::JetpackImported::HasRunner));
#endif
      uint8_t profileBytes[flappy::JetpackProfile::StorageSize];
      jetpackProfile.encode(profileBytes);
      const auto& profile = jetpackProfile.state();
      Serial.printf("JETPACK_PROFILE coins=%lu runs=%lu levels=%u,%u,%u,%u sam=%u compensation=%lu headstarts=%u doubles=%u crc=%02x%02x%02x%02x ranked=%u\n",
          (unsigned long)profile.coins, (unsigned long)profile.runs,
          unsigned(profile.levels[0]), unsigned(profile.levels[1]), unsigned(profile.levels[2]), unsigned(profile.levels[3]),
          unsigned(profile.sam), (unsigned long)profile.samDistance, unsigned(profile.headstarts), unsigned(profile.doubleRuns),
          unsigned(profileBytes[127]), unsigned(profileBytes[126]), unsigned(profileBytes[125]), unsigned(profileBytes[124]),
          unsigned(jetpackGame && jetpackGame->rankedRun()));
      Serial.printf("ARCADE_RECORDS galaga=%lu arkanoid=%lu jetpack=%lu testing=%u\n",
          (unsigned long)arcadeBestGalaga, (unsigned long)arcadeBestArkanoid,
          (unsigned long)arcadeBestJetpack, unsigned(diagnosticArcadeTesting));
      Serial.printf("SOUND driver=%s ready=%u volume=%u frequency=%u duty=%u override=%u active=%u speaker_volume=%u pcm_blocks=%lu pcm_dropped=%lu\n",
          piezoDriver ? "GPIO2-LEDC" : "M5Speaker", unsigned(soundReady), unsigned(app.settings.volumeIndex),
          unsigned(soundFrequency), unsigned(soundDuty), unsigned(soundOverride), unsigned(piezo.active()),
          unsigned(appliedSpeakerVolume), (unsigned long)(nesAudioQueue ? nesAudioQueue->submitted() : 0),
          (unsigned long)(nesAudioQueue ? nesAudioQueue->dropped() : 0));
      Serial.printf("APP ticks=%lu draws=%lu mode=%u skin=%u\n", (unsigned long)appTicks,
          (unsigned long)appDrawFrames, unsigned(app.settings.mode), unsigned(app.settings.skinIndex));
      Serial.printf("SLEEP source=%u timeout_s=%lu idle_ms=%lu index=%u\n", unsigned(powerSource),
          (unsigned long)app.sleepTimeoutSeconds(), (unsigned long)inactivity.idleMs(millis()),
          unsigned(app.settings.sleepIndex));
      const auto battery = pollMenuBattery(millis(),true);
      Serial.printf("BATTERY percent=%d mv=%u source=%u charging=%u estimated=1\n",
          int(battery.percent),unsigned(battery.millivolts),unsigned(battery.source),unsigned(battery.charging));
      Serial.printf("OPT cache=%u pcm=%u render_us=%llu bg_bytes=%u idle=%u\n", unsigned(nes.prgCacheMask()),
          unsigned(nes.pcmEnabled()), (unsigned long long)appRenderUs,
          unsigned(renderer->backgroundCacheBytes()), unsigned(nes.idleJumpEnabled()));
      Serial.printf("PERF nes_us=%llu video_us=%llu source_frames=%lu lcd_frames=%lu lcd_rows=%lu lcd_us=%llu largest_heap=%u\n",
          (unsigned long long)nesWorkUs, (unsigned long long)nesVideoUs,
          (unsigned long)nesRenderedFrames, (unsigned long)lcdFrames, (unsigned long)lcdRows,
          (unsigned long long)lcdUs, ESP.getMaxAllocHeap());
      const auto asyncStats = asyncDisplay.stats();
      Serial.printf("GEOPERF frames=%lu step_us=%llu drain_us=%llu sample_us=%llu decode_us=%llu draw_us=%llu submit_us=%llu\n",
          (unsigned long)geometryProfileFrames, (unsigned long long)geometryStageUs[0],
          (unsigned long long)geometryStageUs[1], (unsigned long long)geometryStageUs[2],
          (unsigned long long)geometryStageUs[3], (unsigned long long)geometryStageUs[4],
          (unsigned long long)geometryStageUs[5]);
      Serial.printf("DISPLAY rotation=%u width=%u height=%u logical_landscape=%u scan_order=%u\n",
          unsigned(M5.Display.getRotation()), unsigned(M5.Display.width()), unsigned(M5.Display.height()),
          unsigned(nesLandscape), unsigned(geometryScanOrder));
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
      Serial.printf("SCANSYNC enabled=%u probes=%lu passed=%lu reads=%lu probe_us=%llu period_us=%lu low_us=%lu high_us=%lu frames=%lu waits=%lu wait_us=%llu max_wait_us=%lu timeouts=%lu invalid=%lu\n",
          unsigned(asyncStats.scanSyncEnabled), (unsigned long)asyncStats.scanProbeAttempts,
          (unsigned long)asyncStats.scanProbePasses, (unsigned long)asyncStats.scanProbeReads,
          (unsigned long long)asyncStats.scanProbeUs, (unsigned long)asyncStats.scanPeriodUs,
          (unsigned long)asyncStats.scanLowUs, (unsigned long)asyncStats.scanHighUs,
          (unsigned long)asyncStats.scanFrames, (unsigned long)asyncStats.scanWaits,
          (unsigned long long)asyncStats.scanWaitUs, (unsigned long)asyncStats.maxScanWaitUs,
          (unsigned long)asyncStats.scanTimeouts, (unsigned long)asyncStats.scanInvalidReads);
      Serial.printf("SCANDETAIL row_min_ns=%lu row_max_ns=%lu max_launch_us=%lu stale_reads=%lu\n",
          (unsigned long)asyncStats.scanRowMinNs, (unsigned long)asyncStats.scanRowMaxNs,
          (unsigned long)asyncStats.maxScanLaunchUs, (unsigned long)asyncStats.scanStaleReads);
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

#include "library_service.h"

void loop() {
#ifdef M5FLAP_USB_LIBRARY
  if (serviceLibrary()) return;
#endif
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
  if (gameHome.active() && !app.menuOpen()) pendingHomeNext |= M5.BtnPWR.wasClicked();
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
  const bool geometry = app.currentGame == flappy::GameTitle::GeometryDash;
  const auto geometryInput = geometryButtons.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(),
      M5.BtnPWR.wasClicked(), geometry && !padConnected && !app.menuOpen() && !gameHome.active(), nowMs);
  geometryHeld = geometryInput.held;
  pendingNesButtons |= geometryInput.pressed;
  pendingNesButtons |= nesSystemButton.update(M5.BtnA.isPressed(),
      (app.currentGame == flappy::GameTitle::Nes || (geometry && padConnected)) && !app.menuOpen() && !gameHome.active(), nowMs);
  const bool lobby = tetrisLinkActive() && (coopLink.session().phase() != flappy::coop::Phase::Playing || (zeroLinked() && (zeroLaunching || zeroFinishedRound)));
  const auto internal = tetrisButtons.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(),
      M5.BtnPWR.wasClicked(), app.settings.tetrisControls,
      tetris && !app.menuOpen() && !gameHome.active() && !lobby && !tetrisLaunching && !zeroLaunching, nowMs);
  tetrisHeld = internal.held;
  pendingTetrisDrop |= (internal.pressed & flappy::NesRuntime::Down) != 0;
  pendingNesButtons |= internal.pressed;
  if (tetris && !gameHome.active() && app.settings.tetrisTilt && (!padConnected || zero) && M5.Imu.isEnabled()) {
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
  if (nativeArcade() && !jetpackSelected() && !controllerConnected && M5.Imu.isEnabled() &&
      (!haveTiltSample || uint32_t(nowMs - lastTiltPollMs) >= 16)) {
    lastTiltPollMs = nowMs;
    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      tiltX = -ax;
      haveTiltSample = true;
      lastTiltSampleMs = nowMs;
      if (fabsf(tiltX - arcadeActivityTilt) > .025f) {
        arcadeActivityTilt = tiltX;
        inactivity.activity(nowMs);
      }
    }
  }
  const auto arcadeSide = arcadeControls.update(M5.BtnB.isPressed(), M5.BtnPWR.wasClicked(),
      nativeArcade() && !app.menuOpen() && !gameHome.active(), nowMs);
  if (arcadeSide.pause) arcadePauseRequested = true;
  if (arcadeSide.zero) {
    if (jetpackSelected()) {
      if (jetpackGame && jetpackGame->phase() == flappy::JetpackGame::Phase::Spin) jetpackCashRequested = true;
      else arcadePauseRequested = true;
    } else {
      calibrateArcadeTilt(nowMs);
      arcadePaused = true;
    }
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
      if (emulatedGame() && !gameHome.active() && !app.menuOpen() && !lobby &&
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
  if (emulatedGame() && !gameHome.active() && !app.menuOpen() && nes.ready()) {
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
                  nesButtons(padButtons) | pendingControllerNesButtons) | pendingNesButtons | tetrisHeld | geometryHeld);
          if (frameTetris && !tetrisLaunching) {
            const bool drop = tetrisSoftDrop.update(pendingTetrisDrop || pendingControllerTetrisDrop,
                (input1 & flappy::NesRuntime::Down) != 0, nes.readRam(0x41),
                flappy::TetrisGym::playing(nes) && nes.readRam(0x48) == 1);
            input1 &= uint8_t(~(flappy::NesRuntime::Start | flappy::NesRuntime::Select | flappy::NesRuntime::Down));
            if (drop) input1 = uint8_t((input1 & ~(flappy::NesRuntime::Left | flappy::NesRuntime::Right)) | flappy::NesRuntime::Down);
          }
          consumedInput = true;
        }
        if (nativeGeometry) {
          const auto controls = geometryController.update(input1);
          if (!geometryLauncher.active() && (controls.openSettings || nes.readRam(0x0498) == 1 || nes.readRam(0x0498) == 6)) {
            app.openMenu(flappy::MenuPage::GameSettings);
            runFrame = false;
          } else if (geometryLauncher.active()) {
            const auto launch = geometryLauncher.update(nes.readRam(0x0498), nes.readRam(0x047b));
            if (launch.writeLevel) nes.writeRam(0x047b, launch.level);
            input1 = launch.buttons;
          } else input1 = controls.buttons;
          if (geometryLauncher.failed()) {
            runtimeNotice = "Restart level to retry";
            app.openMenu(flappy::MenuPage::GameSettings);
            runFrame = false;
          }
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
          const uint8_t videoInterval = nativeMario || nativeGeometry ? 1 : frameZero && zeroOptions.players == 2 ? 3 : 2;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          nes.setVideoDivisor(benchmarkVideoDivisor ? benchmarkVideoDivisor : videoInterval);
#else
          nes.setVideoDivisor(videoInterval);
#endif
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          if (diagnosticPadFrames) { input1 |= diagnosticPad; --diagnosticPadFrames; }
#endif
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          uint32_t geometryStageStarted = micros();
#endif
          stepped = nes.step(input1, input2);
          if (stepped && nativeGeometry) {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(0, geometryStageStarted);
#endif
            asyncDisplay.drain();
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(1, geometryStageStarted);
#endif
            const uint32_t drawnAt = micros();
            const auto readRam = [](uint16_t address) { return nes.readRam(address); };
            const auto readRom = [](uint32_t offset) {
              return offset < GeometryRom::length ? GeometryRom::data[offset] : uint8_t(0);
            };
            flappy::GeometryScene scene;
            scene.sample(readRam);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(2, geometryStageStarted);
#endif
            geometryPresentation->level.prepare(scene, readRom, geometryPresentation->world);
            scene.bindWorld(&geometryPresentation->world);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(3, geometryStageStarted);
#endif
            flappy::GeometryRenderOptions options;
            options.attempt = scene.attempt;
            options.progress = scene.percentage(readRom);
            options.useSourceColor = true;
            options.backgroundColor = nes.core()->nes_ppu.palette[0];
            options.wireOrder = asyncDisplay.enabled();
            options.physicalScanOrder = geometryScanOrder;
            if (geometryLauncher.active()) {
              scene.state = 1;
              options.status = "PREPARING LEVEL";
              options.levelTitle = flappy::geometryLevelName(app.settings.geometryLevel);
              options.footer = "Hold right: menu";
            }
            geometryRenderer.draw(scene, readRam, readRom, canvasStorage, options);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(4, geometryStageStarted);
#endif
            const int transferWidth = geometryScanOrder ? 135 : 240;
            const int transferHeight = geometryScanOrder ? 240 : 135;
            if (asyncDisplay.submitBorrowedFrame(canvasStorage, transferWidth, transferHeight,
                options.wireOrder, geometryScanOrder)) {
              asyncDisplay.submitEndFrame();
            } else {
              if (options.wireOrder) for (unsigned i = 0; i < 240u * 135u; ++i) {
                const uint16_t color = canvasStorage[i];
                canvasStorage[i] = uint16_t((color << 8) | (color >> 8));
              }
              submitDisplay(0, 0, transferWidth, transferHeight, canvasStorage);
              if (asyncDisplay.enabled()) asyncDisplay.submitEndFrame();
            }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
            geometryTimeStage(5, geometryStageStarted);
            ++geometryProfileFrames;
#endif
            recordLcdFrame(135, drawnAt);
          }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
          if (stepped) recordFrameTiming(nesRenderedFrames, nes.frameRate());
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
  const bool activeArcade = nativeArcade() && !app.menuOpen();
  if (activeNes || activeArcade) {
    const uint32_t deadline = activeNes ? nextNesTick : nextTick;
    int32_t remaining;
    while ((remaining = int32_t(deadline - micros())) > 1250) {
      delay(1);
      lastIdleMs = millis();
    }
    if (remaining > 0) delayMicroseconds(uint32_t(remaining));
    if (uint32_t(millis() - lastIdleMs) < 50) yield();
    else { delay(1); lastIdleMs = millis(); }
  } else { delay(1); lastIdleMs = millis(); }
}
