#pragma once

#include <stdint.h>
#include <stdio.h>
#include "game_core.h"
#include "jetpack_profile.h"

namespace flappy {

enum class GameTitle : uint8_t {
  FlappyBird = 0, Nes = 1, Tetris = 2, TetrisZero = 3, GeometryDash = 4, Galaga = 5, Arkanoid = 6,
  JetpackJoyride = 7
};
constexpr uint8_t GameTitleCount = 8;
constexpr uint8_t VisibleGameCount = 7;

inline bool nativeArcadeGame(GameTitle title) {
  return title == GameTitle::Galaga || title == GameTitle::Arkanoid || title == GameTitle::JetpackJoyride;
}

inline GameTitle visibleGameTitle(uint8_t index) {
  constexpr GameTitle titles[] = {GameTitle::FlappyBird, GameTitle::Nes, GameTitle::Tetris, GameTitle::GeometryDash, GameTitle::Galaga, GameTitle::Arkanoid, GameTitle::JetpackJoyride};
  return index < VisibleGameCount ? titles[index] : GameTitle(UINT8_MAX);
}

inline uint8_t visibleGameIndex(GameTitle title) {
  switch (title) {
    case GameTitle::FlappyBird: return 0;
    case GameTitle::Nes: return 1;
    case GameTitle::Tetris:
    case GameTitle::TetrisZero: return 2;
    case GameTitle::GeometryDash: return 3;
    case GameTitle::Galaga: return 4;
    case GameTitle::Arkanoid: return 5;
    case GameTitle::JetpackJoyride: return 6;
    default: return UINT8_MAX;
  }
}
enum class Mode : uint8_t { Original, Clock, Demo, Arcade, Big, Coop, Count };
enum class MenuPage : uint8_t { Closed, Home, BootGame, ChangeGame, GameSettings, Modes, Brightness, Volume, Skins, Clock, Controller, TetrisLevel, TetrisType, TetrisControls, TetrisLink, Coop, TetrisZeroPlayers, Sleep, GeometryLevel, JetpackPractice, JetpackUpgrades, JetpackMissions };

constexpr uint8_t GeometryLevelCount = 22;

inline const char* geometryLevelName(uint8_t index) {
  constexpr const char* names[] = {
    "Stereo Madness", "Back on Track", "Polargeist", "Dry Out", "Base After Base", "Can't Let Go",
    "Jumper", "Time Machine", "Cycles", "xStep", "Clutterfunk", "Theory of Everything",
    "Electroman Adventures", "Clubstep", "Electrodynamix", "Hexagon Force", "Blast Processing",
    "Theory of Everything 2", "Geometrical Dominator", "Deadlocked", "Fingerdash", "Dash"
  };
  static_assert(sizeof(names) / sizeof(names[0]) == GeometryLevelCount, "Every original level has a name");
  return index < GeometryLevelCount ? names[index] : "";
}

inline const char* geometryLevelLabel(uint8_t index) {
  return index == 17 ? "Theory of Everyth. 2" : geometryLevelName(index);
}

struct Settings {
  GameTitle bootGame = GameTitle::FlappyBird;
  uint8_t bootCartridge = 0;
  Mode mode = Mode::Original;
  uint8_t brightnessIndex = 1;
  uint8_t sleepIndex = 0;
  uint8_t tetrisLevel = 0;
  uint8_t tetrisGameType = 0;
  uint8_t tetrisControls = 0;
  uint8_t tetrisPlayers = 1;
  bool tetrisTilt = true;
  bool tetrisPal = false;
  bool tetrisLarge = true;
  uint8_t tetrisZeroLevel = 0;
  uint8_t tetrisZeroGameType = 0;
  uint8_t tetrisZeroPlayers = 2;
  uint8_t volumeIndex = 2;
  uint8_t skinIndex = 0;
  uint8_t clockHour = 12;
  uint8_t clockMinute = 0;
  bool clock24Hour = true;
  uint8_t coopRole = 0;
  uint8_t geometryLevel = 0;
  uint8_t arcadeSensitivity = 1;
  bool arcadeAutoFire = false;
  uint8_t arcadeDifficulty = 1;
  uint8_t jetpackDifficulty = 1;
  uint8_t jetpackRide = 0;
  uint8_t jetpackGadget = 0;
  bool jetpackHeadstart = false;
  bool jetpackPractice = false;
};

struct MenuState {
  MenuPage page = MenuPage::Closed;
  uint8_t selection = 0;
};

inline const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::Original: return "Original";
    case Mode::Clock: return "Clock";
    case Mode::Demo: return "Demo";
    case Mode::Arcade: return "Arcade";
    case Mode::Big: return "Big 2x";
    case Mode::Coop: return "Co-op";
    default: return "Original";
  }
}

inline Rules rulesForMode(Mode mode) {
  Rules rules;
  if (mode == Mode::Big) {
    rules.spriteScale = 2;
    rules.groundY = 480;
    rules.birdX = 40;
    rules.pipeGap = 190;
    rules.pipeSpacing = 256;
    rules.gapMin = 66;
    rules.gapMaxExclusive = 240;
    rules.pipeSpeed = 2.0f;
  } else if (mode == Mode::Clock) {
    rules.pipeGap = 148;
    rules.pipeSpacing = 190;
    rules.gapMin = 64;
    rules.gapMaxExclusive = 208;
  } else if (mode == Mode::Arcade) {
    rules.pipeGap = 128;
    rules.pipeSpacing = 166;
    rules.pipeSpeed = 2.4f;
    rules.gapMin = 64;
    rules.gapMaxExclusive = 222;
  }
  return rules;
}

// Hardware buttons and clocks stay outside this state machine.
class AppModel {
 public:
  static constexpr uint32_t MenuHoldMs = 700;
  static constexpr uint32_t AutoResumeFrames = 120;
  static constexpr uint8_t SleepChoiceCount = 6;
  Game game;
  Settings settings;
  MenuState menu;
  GameTitle currentGame = GameTitle::FlappyBird;
  uint32_t gameRequest = 0;
  uint32_t gameRestartRequest = 0;
  uint32_t geometryRequest = 0;
  uint32_t arcadeCalibrateRequest = 0;
  uint32_t jetpackUpgradeRequest = 0;
  uint8_t jetpackUpgradeKind = 0;
  uint32_t tetrisRequest = 0;
  uint32_t tetrisZeroRequest = 0;
  uint32_t tetrisCalibrateRequest = 0;
  uint32_t tetrisLinkRequest = 0;
  uint8_t tetrisLinkRole = 0;
  uint32_t controllerRequest = 0;
  uint8_t controllerAction = 0;
  const char* controllerStatus = "Off";
  bool speakerBatteryCap = false;
  bool settingsDirty = false;
  bool highScoresDirty = false;
  uint32_t coopRequest = 0;
  uint32_t roundsStarted = 0;

  explicit AppModel(uint32_t seed = 0x8f6e37a1U) : game(seed), randomState_(seed ? seed : 1) {
    for (unsigned i = 0; i < unsigned(Mode::Count); ++i) bestScores_[i] = 0;
    beginRound(seed);
  }

  bool menuOpen() const { return menu.page != MenuPage::Closed; }
  bool scoreEligible() const { return currentGame == GameTitle::FlappyBird && settings.mode != Mode::Clock && settings.mode != Mode::Demo && settings.mode != Mode::Coop; }
  bool humanControlling() const { return currentGame == GameTitle::FlappyBird && (!automatedMode() || humanOverride_); }
  bool autoPlaying() const { return currentGame == GameTitle::FlappyBird && automatedMode() && !humanOverride_; }
  uint8_t demoSkill() const { return demoSkill_; }

  void setControllerStatus(const char* status) {
    controllerStatus = status && *status ? status : "Off";
  }

  bool nesAvailable() const { return nesAvailable_; }

  void setNesAvailable(bool available) {
    nesAvailable_ = available;
    if (!available && currentGame == GameTitle::Nes) setCurrentGame(GameTitle::FlappyBird);
  }

  void setNesTitle(const char* title) {
    snprintf(nesTitle_, sizeof(nesTitle_), "%s", title && *title ? title : "NES");
  }

  const char* nesTitle() const { return nesTitle_; }

  struct CartridgeChoice { uint8_t id = 0; char title[33] = {}; };
  uint8_t currentCartridge = 0;
  void clearCartridgeChoices() { cartridgeCount_ = 0; }
  void addCartridgeChoice(uint8_t id, const char* title) {
    if (!id || cartridgeCount_ >= 12) return;
    auto& choice = cartridgeChoices_[cartridgeCount_++];
    choice.id = id;
    snprintf(choice.title, sizeof(choice.title), "%s", title && *title ? title : "NES");
  }
  uint8_t gameChoiceCount() const { return VisibleGameCount + (cartridgeCount_ ? cartridgeCount_ - 1 : 0); }
  GameTitle gameChoiceTitle(uint8_t index) const { return index < VisibleGameCount ? visibleGameTitle(index) : GameTitle::Nes; }
  uint8_t gameChoiceCartridge(uint8_t index) const {
    const unsigned slot = index == 1 ? 0 : index >= VisibleGameCount ? index - VisibleGameCount + 1 : 12;
    return slot < cartridgeCount_ ? cartridgeChoices_[slot].id : 0;
  }
  const char* gameChoiceLabel(uint8_t index) const {
    const unsigned slot = index == 1 ? 0 : index >= VisibleGameCount ? index - VisibleGameCount + 1 : 12;
    return slot < cartridgeCount_ ? cartridgeChoices_[slot].title : gameLabel(gameChoiceTitle(index));
  }
  bool gameChoiceAvailable(uint8_t index) const {
    if (index >= gameChoiceCount()) return false;
    const auto title = gameChoiceTitle(index);
    return title == GameTitle::Tetris ? tetrisAvailable_ || tetrisZeroAvailable_ : gameAvailable(title);
  }
  bool chooseCartridge(uint8_t id) {
    for (unsigned i = 0; i < cartridgeCount_; ++i) if (cartridgeChoices_[i].id == id) {
      if (currentCartridge != id) { currentCartridge = id; ++gameRequest; }
      setNesTitle(cartridgeChoices_[i].title);
      return true;
    }
    return id == 0;
  }

  bool geometryDashAvailable() const { return geometryDashAvailable_; }

  void setGeometryDashAvailable(bool available) {
    geometryDashAvailable_ = available;
    if (!available && currentGame == GameTitle::GeometryDash) setCurrentGame(GameTitle::FlappyBird);
  }

  void setGeometryLevel(uint8_t level) {
    if (level >= GeometryLevelCount) level = 0;
    if (settings.geometryLevel != level) settingsDirty = true;
    settings.geometryLevel = level;
  }

  void setArcadeSensitivity(uint8_t sensitivity) {
    if (sensitivity > 2) sensitivity = 1;
    if (settings.arcadeSensitivity != sensitivity) settingsDirty = true;
    settings.arcadeSensitivity = sensitivity;
  }

  void setArcadeAutoFire(bool enabled) {
    if (settings.arcadeAutoFire != enabled) settingsDirty = true;
    settings.arcadeAutoFire = enabled;
  }

  void setArcadeDifficulty(uint8_t difficulty) {
    if (difficulty > 2) difficulty = 1;
    if (settings.arcadeDifficulty != difficulty) settingsDirty = true;
    settings.arcadeDifficulty = difficulty;
  }

  void setJetpackDifficulty(uint8_t difficulty) {
    if (difficulty > 2) difficulty = 1;
    if (settings.jetpackDifficulty != difficulty) settingsDirty = true;
    settings.jetpackDifficulty = difficulty;
  }

  void setJetpackRide(uint8_t ride) {
    if (ride > 8) ride = 0;
    if (settings.jetpackRide != ride) settingsDirty = true;
    settings.jetpackRide = ride;
    if (ride) {
      if (settings.jetpackHeadstart || settings.jetpackGadget >= 4) settingsDirty = true;
      settings.jetpackHeadstart = false;
      if (settings.jetpackGadget >= 4) settings.jetpackGadget = 0;
    }
  }

  void setJetpackGadget(uint8_t gadget) {
    if (gadget > 6) gadget = 0;
    if (settings.jetpackGadget != gadget) settingsDirty = true;
    settings.jetpackGadget = gadget;
    if (gadget >= 4) {
      if (settings.jetpackRide || settings.jetpackHeadstart) settingsDirty = true;
      settings.jetpackRide = 0;
      settings.jetpackHeadstart = false;
    }
  }

  void setJetpackHeadstart(bool enabled) {
    if (settings.jetpackHeadstart != enabled) settingsDirty = true;
    settings.jetpackHeadstart = enabled;
    if (enabled) {
      if (settings.jetpackRide || settings.jetpackGadget >= 4) settingsDirty = true;
      settings.jetpackRide = 0;
      if (settings.jetpackGadget >= 4) settings.jetpackGadget = 0;
    }
  }

  void setJetpackPractice(bool enabled) {
    if (settings.jetpackPractice != enabled) settingsDirty = true;
    settings.jetpackPractice = enabled;
  }

  void setJetpackProgress(uint32_t coins, const uint8_t* levels) {
    jetpackCoins_ = coins;
    for (uint8_t i = 0; i < 4; ++i) jetpackLevels_[i] = levels ? levels[i] : 0;
  }

  void setJetpackMission(uint8_t index, const char* label, uint32_t progress, uint32_t target, bool done) {
    if (index >= 3) return;
    jetpackMissionLabels_[index] = label;
    jetpackMissionProgress_[index] = progress;
    jetpackMissionTargets_[index] = target;
    jetpackMissionDone_[index] = done;
  }

  void setJetpackRank(uint16_t rank, uint16_t stars, uint16_t target) {
    jetpackRank_ = rank; jetpackStars_ = stars; jetpackStarTarget_ = target;
  }

  const char* gameLabel(GameTitle title) const {
    if (title == GameTitle::Galaga) return "Galaga";
    if (title == GameTitle::Arkanoid) return "Arkanoid";
    if (title == GameTitle::JetpackJoyride) return "Jetpack Joyride";
    if (title == GameTitle::Nes) return nesAvailable_ ? nesTitle_ : "NES: no ROM";
    if (title == GameTitle::GeometryDash) return geometryDashAvailable_ ? "Geometry Dash" : "Geometry Dash: no ROM";
    if (title == GameTitle::Tetris || title == GameTitle::TetrisZero)
      return tetrisAvailable_ || tetrisZeroAvailable_ ? "Tetris" : "Tetris: no ROM";
    return "Flappy Bird";
  }

  bool setBootGame(GameTitle title) {
    if (title == GameTitle::TetrisZero) title = GameTitle::Tetris;
    if (title == GameTitle::Tetris ? !tetrisAvailable_ && !tetrisZeroAvailable_ : !gameAvailable(title)) return false;
    if (settings.bootGame != title) settingsDirty = true;
    settings.bootGame = title;
    return true;
  }

  bool setCurrentGame(GameTitle title) {
    if (!gameAvailable(title)) return false;
    if (currentGame == title) return true;
    currentGame = title;
    ++gameRequest;
    if (title == GameTitle::FlappyBird) restart();
    return true;
  }

  void restoreBootGame() {
    if (settings.bootGame == GameTitle::TetrisZero) {
      settings.bootGame = GameTitle::Tetris;
      settingsDirty = true;
    }
    if (settings.bootGame == GameTitle::Tetris) {
      if (!activateTetris()) setCurrentGame(GameTitle::FlappyBird);
    } else setCurrentGame(gameAvailable(settings.bootGame) ? settings.bootGame : GameTitle::FlappyBird);
  }

  bool tetrisFrontendActive() const {
    return currentGame == GameTitle::Tetris || currentGame == GameTitle::TetrisZero;
  }

  void openTetrisSettings() {
    if (tetrisFrontendActive()) openMenu(MenuPage::GameSettings);
  }

  void setTetrisPlayers(uint8_t players) {
    if (players != 2) players = 1;
    if (settings.tetrisPlayers != players) settingsDirty = true;
    settings.tetrisPlayers = players;
  }

  GameTitle selectedTetrisEngine() const {
    if (settings.tetrisPlayers == 2)
      return tetrisZeroAvailable_ || !tetrisAvailable_ ? GameTitle::TetrisZero : GameTitle::Tetris;
    return tetrisAvailable_ || !tetrisZeroAvailable_ ? GameTitle::Tetris : GameTitle::TetrisZero;
  }

  bool activateTetris() {
    if (!tetrisAvailable_ && !tetrisZeroAvailable_) return false;
    const GameTitle engine = selectedTetrisEngine();
    if (engine == GameTitle::Tetris) setTetrisPlayers(1);
    const bool playerChange = engine == GameTitle::TetrisZero && settings.tetrisZeroPlayers != settings.tetrisPlayers;
    if (engine == GameTitle::TetrisZero) setTetrisZeroPlayers(settings.tetrisPlayers);
    const bool sameEngine = currentGame == engine;
    if (!setCurrentGame(engine)) return false;
    if (sameEngine && playerChange) ++gameRequest;
    return true;
  }

  bool tetrisAvailable() const { return tetrisAvailable_; }

  void setTetrisAvailable(bool available) {
    tetrisAvailable_ = available;
    if (!available && currentGame == GameTitle::Tetris && !activateTetris()) setCurrentGame(GameTitle::FlappyBird);
  }

  void setTetrisTitle(const char* title) {
    snprintf(tetrisTitle_, sizeof(tetrisTitle_), "%s", title && *title ? title : "Tetris");
  }

  const char* tetrisTitle() const { return tetrisTitle_; }

  bool tetrisZeroAvailable() const { return tetrisZeroAvailable_; }

  void setTetrisZeroAvailable(bool available) {
    tetrisZeroAvailable_ = available;
    if (!available && currentGame == GameTitle::TetrisZero && !activateTetris()) setCurrentGame(GameTitle::FlappyBird);
  }

  void setTetrisZeroTitle(const char* title) {
    snprintf(tetrisZeroTitle_, sizeof(tetrisZeroTitle_), "%s", title && *title ? title : "Tetris Zero");
  }

  const char* tetrisZeroTitle() const { return tetrisZeroTitle_; }

  void setTetrisZeroLevel(uint8_t level) {
    if (level > 9) level = 9;
    if (settings.tetrisZeroLevel != level) settingsDirty = true;
    settings.tetrisZeroLevel = level;
  }

  void setTetrisZeroGameType(uint8_t type) {
    if (type > 1) type = 0;
    if (settings.tetrisZeroGameType != type) settingsDirty = true;
    settings.tetrisZeroGameType = type;
  }

  void setTetrisZeroPlayers(uint8_t players) {
    if (players != 1 && players != 2) players = 2;
    if (settings.tetrisZeroPlayers != players) settingsDirty = true;
    settings.tetrisZeroPlayers = players;
  }

  void requestGameRestart() {
    if (currentGame == GameTitle::GeometryDash) ++geometryRequest;
    else ++gameRestartRequest;
    if (currentGame == GameTitle::FlappyBird) restart();
    closeMenu();
  }

  void setTetrisLevel(uint8_t level) {
    if (settings.tetrisLevel != level) settingsDirty = true;
    settings.tetrisLevel = level;
  }

  void setTetrisGameType(uint8_t type) {
    if (type != 0 && type != 6) type = 0;
    if (settings.tetrisGameType != type) settingsDirty = true;
    settings.tetrisGameType = type;
  }

  void setTetrisControls(uint8_t preset) {
    if (preset > 1) preset = 0;
    if (settings.tetrisControls != preset) settingsDirty = true;
    settings.tetrisControls = preset;
    setTetrisTilt(true);
  }

  void setTetrisTilt(bool enabled) {
    if (settings.tetrisTilt != enabled) settingsDirty = true;
    settings.tetrisTilt = enabled;
  }

  void setTetrisLarge(bool enabled) {
    if (settings.tetrisLarge != enabled) settingsDirty = true;
    settings.tetrisLarge = enabled;
  }

  void setTetrisPal(bool enabled) {
    if (settings.tetrisPal != enabled) settingsDirty = true;
    settings.tetrisPal = enabled;
  }

  uint8_t brightness() const {
    constexpr uint8_t levels[] = {65, 140, 255};
    return levels[settings.brightnessIndex < 3 ? settings.brightnessIndex : 1];
  }

  void setBrightness(uint8_t index) {
    if (index >= 3) index = 2;
    if (settings.brightnessIndex != index) settingsDirty = true;
    settings.brightnessIndex = index;
  }

  uint8_t volume() const {
    constexpr uint8_t levels[] = {0, 51, 102, 153, 204, 255};
    return levels[settings.volumeIndex < 6 ? settings.volumeIndex : 2];
  }

  void setVolume(uint8_t index) {
    if (index >= 6) index = 5;
    if (settings.volumeIndex != index) settingsDirty = true;
    settings.volumeIndex = index;
  }

  uint32_t sleepTimeoutSeconds() const {
    constexpr uint16_t seconds[] = {30, 60, 120, 300, 600, 0};
    return seconds[settings.sleepIndex < SleepChoiceCount ? settings.sleepIndex : 0];
  }

  uint32_t sleepTimeoutMs() const { return sleepTimeoutSeconds() * 1000u; }

  void setSleep(uint8_t index) {
    if (index >= SleepChoiceCount) index = 0;
    if (settings.sleepIndex != index) settingsDirty = true;
    settings.sleepIndex = index;
  }

  void setSkinNames(const char* const* names, uint8_t count) {
    skinNames_ = names;
    setSkinCount(count);
  }

  void setSkinCount(uint8_t count) {
    skinCount_ = count ? (count > 254 ? 254 : count) : 1;
    if (settings.skinIndex >= skinCount_) settings.skinIndex = 0;
  }

  uint8_t skinCount() const { return skinCount_; }

  void setSkin(uint8_t index) {
    if (index >= skinCount_) return;
    if (settings.skinIndex != index) settingsDirty = true;
    settings.skinIndex = index;
  }

  void setMode(Mode mode) {
    if (unsigned(mode) >= unsigned(Mode::Count)) mode = Mode::Original;
    if (settings.mode != mode) settingsDirty = true;
    settings.mode = mode;
    humanOverride_ = false;
    beginRound(nextRandom());
  }

  void setBestScore(Mode mode, uint16_t best) {
    if (unsigned(mode) >= unsigned(Mode::Count)) return;
    bestScores_[unsigned(mode)] = best;
    if (mode == settings.mode) game.setBestScore(scoreEligible() ? best : 0);
  }

  uint16_t bestScore(Mode mode) const {
    return unsigned(mode) < unsigned(Mode::Count) ? bestScores_[unsigned(mode)] : 0;
  }

  // Restore after loading settings or receiving a shared co-op round seed.
  void restart(uint32_t seed = 0) {
    humanOverride_ = false;
    beginRound(seed ? seed : nextRandom());
  }

  void startCoopRound(uint32_t seed) {
    if (settings.mode != Mode::Coop) settingsDirty = true;
    settings.mode = Mode::Coop;
    humanOverride_ = false;
    closeMenu();
    beginRound(seed ? seed : 1);
  }

  void setClock(uint8_t hour, uint8_t minute, uint32_t nowMs) {
    settings.clockHour = uint8_t(hour % 24);
    settings.clockMinute = uint8_t(minute % 60);
    clockBaseMs_ = nowMs;
    settingsDirty = true;
  }

  uint8_t clockHour(uint32_t nowMs) const { return uint8_t(clockMinutes(nowMs) / 60); }
  uint8_t clockMinute(uint32_t nowMs) const { return uint8_t(clockMinutes(nowMs) % 60); }

  void openMenu(MenuPage page = MenuPage::Home) {
    menu.page = page;
    menu.selection = page == MenuPage::GeometryLevel && settings.geometryLevel < GeometryLevelCount ? settings.geometryLevel : 0;
  }

  void openMenuFromController() {
    if (menuOpen()) {
      const uint8_t count = menuItemCount();
      if (count) menu.selection = uint8_t((menu.selection + 1) % count);
    } else openMenu();
  }

  void closeMenu() {
    menu.page = MenuPage::Closed;
    menu.selection = 0;
  }

  const char* menuTitle() const {
    switch (menu.page) {
      case MenuPage::BootGame: return "Game at Boot";
      case MenuPage::ChangeGame: return "Change Game";
      case MenuPage::GameSettings: return "Game Settings";
      case MenuPage::TetrisLevel: return "Starting level";
      case MenuPage::GeometryLevel: return "Level";
      case MenuPage::TetrisType: return "Game type";
      case MenuPage::TetrisControls: return "Controls";
      case MenuPage::TetrisLink: return "Tetris link";
      case MenuPage::TetrisZeroPlayers: return "Players";
      case MenuPage::Modes: return "Mode";
      case MenuPage::Brightness: return "Brightness";
      case MenuPage::Volume: return "Volume";
      case MenuPage::Sleep: return "Sleep";
      case MenuPage::Skins: return "Skins";
      case MenuPage::Clock: return "Set clock";
      case MenuPage::Controller: return "Controller";
      case MenuPage::JetpackPractice: return "Practice (no coins)";
      case MenuPage::JetpackUpgrades: return "Power-up upgrades";
      case MenuPage::JetpackMissions: return "Mission goals";
      case MenuPage::Coop: return "Co-op link";
      default: return "Menu";
    }
  }

  uint8_t menuItemCount() const {
    switch (menu.page) {
      case MenuPage::Home: return 8;
      case MenuPage::BootGame:
      case MenuPage::ChangeGame: return gameChoiceCount() + 1;
      case MenuPage::GameSettings:
        if (currentGame == GameTitle::FlappyBird) return 5;
        if (currentGame == GameTitle::Tetris) return 9;
        if (currentGame == GameTitle::TetrisZero) return 8;
        if (currentGame == GameTitle::GeometryDash) return 3;
        if (currentGame == GameTitle::JetpackJoyride) return 6;
        return nativeArcadeGame(currentGame) ? 6 : 2;
      case MenuPage::GeometryLevel: return GeometryLevelCount + 1;
      case MenuPage::Modes: return uint8_t(Mode::Count) + 1;
      case MenuPage::Brightness: return 4;
      case MenuPage::TetrisLevel: return currentGame == GameTitle::TetrisZero ? 11 : 31;
      case MenuPage::TetrisType: return 3;
      case MenuPage::TetrisControls: return 4;
      case MenuPage::TetrisLink: return 4;
      case MenuPage::TetrisZeroPlayers: return 3;
      case MenuPage::Volume: return 7;
      case MenuPage::Sleep: return SleepChoiceCount + 1;
      case MenuPage::Skins: return uint8_t(skinCount_ + 1);
      case MenuPage::Clock: return 4;
      case MenuPage::Controller: return 4;
      case MenuPage::JetpackPractice: return 5;
      case MenuPage::JetpackUpgrades: return 6;
      case MenuPage::JetpackMissions: return 5;
      case MenuPage::Coop: return 4;
      default: return 0;
    }
  }

  const char* menuLabel(uint8_t index) const {
    const uint8_t count = menuItemCount();
    if (!count || index >= count) return "";
    if (index == count - 1) return "Back";
    switch (menu.page) {
      case MenuPage::Home: {
        constexpr const char* labels[] = {"Game at Boot", "Change Game", "Game Settings", "Brightness", "Volume", "Controller", "Sleep"};
        return labels[index];
      }
      case MenuPage::BootGame:
      case MenuPage::ChangeGame: return gameChoiceLabel(index);
      case MenuPage::GameSettings:
        if (currentGame == GameTitle::FlappyBird) {
          constexpr const char* labels[] = {"Mode", "Skins", "Set clock", "Co-op link"};
          return labels[index];
        }
        if (currentGame == GameTitle::Tetris) {
          constexpr const char* labels[] = {"Players", "Game type", "Starting level", "Controls", "Speed", "Link", "Board size", "Start / restart"};
          return labels[index];
        }
        if (currentGame == GameTitle::TetrisZero) {
          constexpr const char* labels[] = {"Players", "Game type", "Starting level", "Controls", "Link", "Board size", "Start / restart"};
          return labels[index];
        }
        if (currentGame == GameTitle::GeometryDash) return index == 0 ? "Level" : "Restart level";
        if (currentGame == GameTitle::JetpackJoyride) {
          constexpr const char* labels[] = {"Restart game", "Difficulty", "Missions", "Power-up upgrades", "Practice"};
          return labels[index];
        }
        if (nativeArcadeGame(currentGame)) {
          constexpr const char* labels[] = {"Restart game", "Zero tilt", "Sensitivity", "Auto fire", "Difficulty"};
          return labels[index];
        }
        return "Restart game";
      case MenuPage::GeometryLevel: return geometryLevelLabel(index);
      case MenuPage::JetpackPractice: {
        constexpr const char* labels[] = {"Practice mode", "Starting ride", "Starting bonus", "Headstart"};
        return labels[index];
      }
      case MenuPage::JetpackUpgrades: {
        constexpr const char* labels[] = {"Coins", "Shield", "Boost", "Cash Dash", "Rocket Time"};
        return labels[index];
      }
      case MenuPage::JetpackMissions:
        return index < 3 ? (jetpackMissionLabels_[index] ? jetpackMissionLabels_[index] : "Mission") : "Rank";
      case MenuPage::TetrisLevel:
        snprintf(labelBuffer_, sizeof(labelBuffer_), "Level %u", unsigned(index));
        return labelBuffer_;
      case MenuPage::TetrisType: return index == 0 ? "A-Type" : "B-Type";
      case MenuPage::TetrisZeroPlayers:
        if (index == 0) return tetrisAvailable_ || tetrisZeroAvailable_ ? "Solo" : "Solo: no ROM";
        return tetrisZeroAvailable_ ? "2 players" : "2 players: no ROM";
      case MenuPage::TetrisControls: {
        constexpr const char* labels[] = {"Tilt move", "Tilt rotate", "Zero tilt"};
        return labels[index];
      }
      case MenuPage::TetrisLink: {
        constexpr const char* labels[] = {"Disconnect", "Host game", "Join game"};
        return labels[index];
      }
      case MenuPage::Modes: return modeName(Mode(index));
      case MenuPage::Brightness: {
        constexpr const char* labels[] = {"Low", "Medium", "High"};
        return labels[index];
      }
      case MenuPage::Volume: {
        if (speakerBatteryCap && index == 4) return "80% (72% battery)";
        if (speakerBatteryCap && index == 5) return "Full (72% battery)";
        constexpr const char* labels[] = {"Off", "20%", "40%", "60%", "80%", "Full"};
        return labels[index];
      }
      case MenuPage::Sleep: return sleepLabel(index);
      case MenuPage::Skins:
        if (skinNames_) return skinNames_[index];
        if (!index) return "Original";
        snprintf(labelBuffer_, sizeof(labelBuffer_), "Skin %u", unsigned(index + 1));
        return labelBuffer_;
      case MenuPage::Clock: {
        constexpr const char* labels[] = {"Hour", "Minute", "24-hour"};
        return labels[index];
      }
      case MenuPage::Controller: {
        constexpr const char* labels[] = {"Pair controller", "Disconnect", "Forget pairing"};
        return labels[index];
      }
      case MenuPage::Coop: {
        constexpr const char* labels[] = {"Disconnect", "Host game", "Join game"};
        return labels[index];
      }
      default: return "";
    }
  }

  const char* menuValue(uint8_t index) const {
    switch (menu.page) {
      case MenuPage::Home:
        if (index == 0) return gameLabel(settings.bootGame);
        if (index == 1) return gameLabel(currentGame);
        if (index == 2) return gameLabel(currentGame);
        if (index == 3) {
          snprintf(valueBuffer_, sizeof(valueBuffer_), "%u/3", unsigned(settings.brightnessIndex + 1));
          return valueBuffer_;
        }
        if (index == 4) {
          if (!settings.volumeIndex) return "Off";
          if (settings.volumeIndex == 5) return "Full";
          snprintf(valueBuffer_, sizeof(valueBuffer_), "%u%%", unsigned(settings.volumeIndex * 20));
          return valueBuffer_;
        }
        if (index == 5) return controllerStatus;
        if (index == 6) return sleepLabel(settings.sleepIndex);
        break;
      case MenuPage::GameSettings:
        if (currentGame == GameTitle::FlappyBird) {
          if (index == 0) return modeName(settings.mode);
          if (index == 1) return skinNames_ ? skinNames_[settings.skinIndex] : "";
          if (index == 3) return settings.coopRole == 1 ? "Host" : settings.coopRole == 2 ? "Join" : "Off";
        } else if (currentGame == GameTitle::Tetris) {
          if (index == 0) return "Solo";
          if (index == 2) {
            snprintf(valueBuffer_, sizeof(valueBuffer_), "%u", unsigned(settings.tetrisLevel));
            return valueBuffer_;
          }
          if (index == 1) return settings.tetrisGameType == 6 ? "B-Type" : "A-Type";
          if (index == 3) return settings.tetrisControls == 1 ? "Tilt rotate" : "Tilt move";
          if (index == 4) return settings.tetrisPal ? "PAL 50 Hz" : "NTSC 60 Hz";
          if (index == 5) return tetrisLinkRole == 1 ? "Host" : tetrisLinkRole == 2 ? "Join" : "Off";
          if (index == 6) return settings.tetrisLarge ? "Large 11px" : "Native 8px";
        } else if (currentGame == GameTitle::TetrisZero) {
          if (index == 0) return settings.tetrisPlayers == 2 ? "2 players" : "Solo";
          if (index == 1) return settings.tetrisZeroGameType ? "B-Type" : "A-Type";
          if (index == 2) {
            snprintf(valueBuffer_, sizeof(valueBuffer_), "%u", unsigned(settings.tetrisZeroLevel));
            return valueBuffer_;
          }
          if (index == 3) return settings.tetrisControls == 1 ? "Tilt rotate" : "Tilt move";
          if (index == 4) return tetrisLinkRole == 1 ? "Host" : tetrisLinkRole == 2 ? "Join" : "Off";
          if (index == 5) return settings.tetrisPlayers == 2 ? "6px (2P)" : settings.tetrisLarge ? "Large 11px" : "Native 8px";
        } else if (currentGame == GameTitle::GeometryDash) {
          if (index == 0) return geometryLevelLabel(settings.geometryLevel);
        } else if (currentGame == GameTitle::JetpackJoyride) {
          if (index == 1) return settings.jetpackDifficulty == 0 ? "Easy" : settings.jetpackDifficulty == 2 ? "Hard" : "Normal";
          if (index == 3) {
            snprintf(valueBuffer_, sizeof(valueBuffer_), "%lu coins", static_cast<unsigned long>(jetpackCoins_));
            return valueBuffer_;
          }
          if (index == 4) return settings.jetpackPractice ? "On: no records" : "Off";
        } else if (nativeArcadeGame(currentGame)) {
          if (index == 1) return "Hold unit steady";
          if (index == 2) return settings.arcadeSensitivity == 0 ? "Fine" : settings.arcadeSensitivity == 2 ? "Fast" : "Normal";
          if (index == 3) return settings.arcadeAutoFire ? "On" : "Off";
          if (index == 4) return settings.arcadeDifficulty == 0 ? "Easy" : settings.arcadeDifficulty == 2 ? "Hard" : "Normal";
        }
        break;
      case MenuPage::GeometryLevel: return index == settings.geometryLevel ? "*" : "";
      case MenuPage::JetpackPractice:
        if (index == 0) return settings.jetpackPractice ? "On: no coins" : "Off";
        if (index == 1) {
          constexpr const char* rides[] = {"On foot", "Gravity suit", "Hog", "Stomper", "Bird", "Teleporter", "Dragon", "S.A.M.", "Random"};
          return rides[settings.jetpackRide <= 8 ? settings.jetpackRide : 0];
        }
        if (index == 2) {
          constexpr const char* bonuses[] = {"Off", "Coin magnet", "Double coins", "Shield", "Boost", "Cash Dash", "Rocket Time"};
          return bonuses[settings.jetpackGadget <= 6 ? settings.jetpackGadget : 0];
        }
        if (index == 3) return settings.jetpackHeadstart ? "On" : "Off";
        break;
      case MenuPage::JetpackUpgrades:
        if (index == 0) snprintf(valueBuffer_, sizeof(valueBuffer_), "%lu", static_cast<unsigned long>(jetpackCoins_));
        else if (index < 5) {
          const uint8_t kind = uint8_t(index - 1), level = jetpackLevels_[kind];
          if (kind == JetpackProfile::RocketTime && jetpackRank_ < 5) return "Requires rank 5";
          if (level >= JetpackProfile::maxLevel(kind)) snprintf(valueBuffer_, sizeof(valueBuffer_), "Lv%u Max", unsigned(level + 1));
          else {
            const uint32_t cost = JetpackProfile::cost(kind, uint8_t(level + 1));
            snprintf(valueBuffer_, sizeof(valueBuffer_), "Lv%u %s %lu", unsigned(level + 1), jetpackCoins_ >= cost ? "Buy" : "Need", static_cast<unsigned long>(cost));
          }
        } else return "";
        return valueBuffer_;
      case MenuPage::JetpackMissions:
        if (index < 3) {
          if (jetpackMissionDone_[index]) return "Complete";
          snprintf(valueBuffer_, sizeof(valueBuffer_), "%lu/%lu", static_cast<unsigned long>(jetpackMissionProgress_[index]), static_cast<unsigned long>(jetpackMissionTargets_[index]));
        } else if (index == 3) {
          if (!jetpackStarTarget_) snprintf(valueBuffer_, sizeof(valueBuffer_), "Lv%u complete", unsigned(jetpackRank_));
          else snprintf(valueBuffer_, sizeof(valueBuffer_), "Lv%u %u/%u stars", unsigned(jetpackRank_), unsigned(jetpackStars_), unsigned(jetpackStarTarget_));
        }
        else return "";
        return valueBuffer_;
      case MenuPage::TetrisLevel: return index == (currentGame == GameTitle::TetrisZero ? settings.tetrisZeroLevel : settings.tetrisLevel) ? "*" : "";
      case MenuPage::TetrisType:
        return index == (currentGame == GameTitle::TetrisZero ? settings.tetrisZeroGameType : settings.tetrisGameType == 6 ? 1 : 0) ? "*" : "";
      case MenuPage::TetrisZeroPlayers: return index + 1 == settings.tetrisPlayers ? "*" : "";
      case MenuPage::TetrisControls:
        if (index < 2) return index == settings.tetrisControls ? "*" : "";
        if (index == 2) return "Hold unit steady";
        break;
      case MenuPage::Controller: return index == 0 ? controllerStatus : "";
      case MenuPage::BootGame: return (gameChoiceTitle(index) == settings.bootGame ||
          (gameChoiceTitle(index) == GameTitle::Tetris && settings.bootGame == GameTitle::TetrisZero)) &&
          (settings.bootGame != GameTitle::Nes || gameChoiceCartridge(index) == settings.bootCartridge) ? "*" : "";
      case MenuPage::ChangeGame: return (gameChoiceTitle(index) == currentGame ||
          (gameChoiceTitle(index) == GameTitle::Tetris && currentGame == GameTitle::TetrisZero)) &&
          (currentGame != GameTitle::Nes || gameChoiceCartridge(index) == currentCartridge) ? "*" : "";
      case MenuPage::Modes: return index == uint8_t(settings.mode) ? "*" : "";
      case MenuPage::Brightness: return index == settings.brightnessIndex ? "*" : "";
      case MenuPage::Volume: return index == settings.volumeIndex ? "*" : "";
      case MenuPage::Sleep: return index == (settings.sleepIndex < SleepChoiceCount ? settings.sleepIndex : 0) ? "*" : "";
      case MenuPage::Skins: return index == settings.skinIndex ? "*" : "";
      case MenuPage::Clock:
        if (index < 2) {
          snprintf(valueBuffer_, sizeof(valueBuffer_), "%02u", unsigned(index ? clockMinute(lastNowMs_) : clockHour(lastNowMs_)));
          return valueBuffer_;
        }
        if (index == 2) return settings.clock24Hour ? "On" : "Off";
        break;
      default: break;
    }
    return "";
  }

  uint16_t arcadeTickets() const {
    constexpr uint16_t gates[] = {10, 20, 35, 50, 60};
    constexpr uint16_t bonuses[] = {10, 20, 50, 500, 1000};
    uint32_t tickets = game.score;
    for (unsigned i = 0; i < 5; ++i) if (game.score >= gates[i]) tickets += bonuses[i];
    return uint16_t(tickets > UINT16_MAX ? UINT16_MAX : tickets);
  }

  uint16_t arcadePointsRemaining() const {
    constexpr uint16_t gates[] = {10, 20, 35, 50, 60};
    for (unsigned i = 0; i < 5; ++i) if (game.score < gates[i]) return uint16_t(gates[i] - game.score);
    return 0;
  }

  // Call at 60 Hz. The menu consumes input and freezes the game simulation.
  uint8_t step(bool actionEdge, bool menuDown, uint32_t nowMs) {
    lastNowMs_ = nowMs;
    const bool wasOpen = menuOpen();
    bool menuHandled = false;
    if (menuDown && !menuWasDown_) {
      menuPressMs_ = nowMs;
      holdHandled_ = false;
      menuPressBeganOpen_ = menuOpen();
    }
    if (menuDown && !holdHandled_ && uint32_t(nowMs - menuPressMs_) >= MenuHoldMs) {
      if (menuPressBeganOpen_) closeMenu();
      else openMenu();
      holdHandled_ = true;
      menuHandled = true;
    }
    if (!menuDown && menuWasDown_) {
      if (!holdHandled_) {
        if (menuPressBeganOpen_ && menuOpen()) {
          menu.selection = uint8_t((menu.selection + 1) % menuItemCount());
          menuHandled = true;
        }
      }
    }
    menuWasDown_ = menuDown;
    if (menuOpen()) {
      if (actionEdge && !menuHandled) selectMenuItem(nowMs);
      return NoEvent;
    }
    if (wasOpen || menuHandled || currentGame != GameTitle::FlappyBird) return NoEvent;

    if (automatedMode() && actionEdge && !humanOverride_) {
      humanOverride_ = true;
      beginRound(nextRandom());
    }
    if (automatedMode() && game.state == State::GameOver && game.stateFrames >= AutoResumeFrames) {
      humanOverride_ = false;
      beginRound(nextRandom());
    }
    bool flap = actionEdge;
    if (autoPlaying()) flap = autopilotFlap();
    const uint8_t events = game.step(flap);
    if (scoreEligible() && game.bestScore > bestScores_[unsigned(settings.mode)]) {
      bestScores_[unsigned(settings.mode)] = game.bestScore;
      highScoresDirty = true;
    }
    if (settings.mode == Mode::Arcade && (events & Point)) {
      Rules rules = game.rules();
      const int gap = 128 - int(game.score / 5) * 2;
      rules.pipeGap = int16_t(gap > 88 ? gap : 88);
      game.setRules(rules);
    }
    return events;
  }

 private:
  static const char* sleepLabel(uint8_t index) {
    constexpr const char* labels[] = {"30 sec", "1 min", "2 min", "5 min", "10 min", "Off"};
    return labels[index < SleepChoiceCount ? index : 0];
  }

  uint16_t bestScores_[unsigned(Mode::Count)];
  const char* const* skinNames_ = nullptr;
  uint8_t skinCount_ = 1;
  bool nesAvailable_ = false;
  bool geometryDashAvailable_ = false;
  bool tetrisAvailable_ = false;
  bool tetrisZeroAvailable_ = false;
  char tetrisZeroTitle_[64] = "Tetris Zero";
  char tetrisTitle_[64] = "Tetris";
  char nesTitle_[64] = "NES";
  CartridgeChoice cartridgeChoices_[12];
  uint8_t cartridgeCount_ = 0;
  uint32_t randomState_;
  uint32_t menuPressMs_ = 0;
  uint32_t clockBaseMs_ = 0;
  uint32_t lastNowMs_ = 0;
  bool menuWasDown_ = false;
  bool menuPressBeganOpen_ = false;
  bool holdHandled_ = false;
  bool humanOverride_ = false;
  uint8_t demoSkill_ = 3;
  uint8_t reactionCountdown_ = 0;
  uint16_t lapseCountdown_ = 0;
  uint16_t nextLapseFrames_ = 0;
  float targetError_ = 0;
  bool previousAutoFlap_ = false;
  uint32_t jetpackCoins_ = 0;
  uint8_t jetpackLevels_[4] = {};
  const char* jetpackMissionLabels_[3] = {};
  uint32_t jetpackMissionProgress_[3] = {}, jetpackMissionTargets_[3] = {};
  bool jetpackMissionDone_[3] = {};
  uint16_t jetpackRank_ = 1, jetpackStars_ = 0, jetpackStarTarget_ = 3;
  mutable char labelBuffer_[24] = {};
  mutable char valueBuffer_[32] = {};

  bool gameAvailable(GameTitle title) const {
    return title == GameTitle::FlappyBird || nativeArcadeGame(title) || (title == GameTitle::Nes && nesAvailable_) ||
           (title == GameTitle::Tetris && tetrisAvailable_) ||
           (title == GameTitle::TetrisZero && tetrisZeroAvailable_) ||
           (title == GameTitle::GeometryDash && geometryDashAvailable_);
  }

  bool automatedMode() const { return settings.mode == Mode::Clock || settings.mode == Mode::Demo; }

  uint32_t nextRandom() {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    return randomState_;
  }

  uint16_t clockMinutes(uint32_t nowMs) const {
    const uint32_t elapsedMinutes = uint32_t(nowMs - clockBaseMs_) / 60000U;
    return uint16_t((settings.clockHour * 60U + settings.clockMinute + elapsedMinutes) % 1440U);
  }

  void beginRound(uint32_t seed) {
    game.disableSharedMap();
    game.setRules(rulesForMode(settings.mode));
    game.setHighScoreEligible(scoreEligible());
    game.setBestScore(scoreEligible() ? bestScores_[unsigned(settings.mode)] : 0);
    game.reset(seed);
    ++roundsStarted;
    demoSkill_ = settings.mode == Mode::Clock ? 3 : uint8_t(nextRandom() % 4);
    targetError_ = demoSkill_ == 3 ? 0.0f : float(int(nextRandom() % 61) - 30) * float(3 - demoSkill_) * 0.4f;
    reactionCountdown_ = 0;
    lapseCountdown_ = 0;
    nextLapseFrames_ = nextLapseDelay();
    previousAutoFlap_ = false;
  }

  uint16_t nextLapseDelay() {
    const uint16_t base = demoSkill_ == 3 ? 1200 : demoSkill_ == 2 ? 600 : demoSkill_ == 1 ? 300 : 180;
    const uint16_t range = demoSkill_ == 3 ? 3600 : demoSkill_ == 2 ? 1800 : demoSkill_ == 1 ? 600 : 240;
    return uint16_t(base + nextRandom() % range);
  }

  bool autopilotFlap() {
    if (game.state == State::Ready) return true;
    if (game.state != State::Playing) return false;
    const Pipe* nearest = nullptr;
    for (int i = 0; i < PipeCount; ++i) {
      const Pipe& pipe = game.pipes[i];
      if (pipe.x + game.pipeWidth() > game.birdX() && (!nearest || pipe.x < nearest->x)) nearest = &pipe;
    }
    if (!nearest) return false;
    if (settings.mode == Mode::Demo) {
      if (lapseCountdown_) {
        --lapseCountdown_;
        previousAutoFlap_ = false;
        return false;
      }
      if (nextLapseFrames_) --nextLapseFrames_;
      else {
        const uint16_t maximum = demoSkill_ == 3 ? 32 : demoSkill_ == 2 ? 34 : demoSkill_ == 1 ? 42 : 52;
        lapseCountdown_ = uint16_t(1 + nextRandom() % maximum);
        nextLapseFrames_ = nextLapseDelay();
      }
      if (reactionCountdown_) {
        --reactionCountdown_;
        return previousAutoFlap_ = false;
      }
      reactionCountdown_ = demoSkill_ < 2 ? uint8_t(nextRandom() % (5 - demoSkill_)) : 0;
    }
    const float target = nearest->gapY + game.rules().pipeGap * 0.64f + targetError_;
    const bool flap = game.velocityY >= -0.5f && game.birdY + game.velocityY * 4.0f > target;
    const bool edge = flap && !previousAutoFlap_;
    previousAutoFlap_ = flap;
    return edge;
  }

  void selectMenuItem(uint32_t nowMs) {
    if (menu.selection >= menuItemCount() - 1) {
      if (menu.page == MenuPage::Home) closeMenu();
      else if (menu.page == MenuPage::Modes || menu.page == MenuPage::Skins || menu.page == MenuPage::Clock ||
               menu.page == MenuPage::Coop || menu.page == MenuPage::TetrisLevel ||
               menu.page == MenuPage::TetrisType || menu.page == MenuPage::TetrisControls ||
               menu.page == MenuPage::TetrisLink || menu.page == MenuPage::TetrisZeroPlayers ||
               menu.page == MenuPage::GeometryLevel || menu.page == MenuPage::JetpackPractice ||
               menu.page == MenuPage::JetpackUpgrades || menu.page == MenuPage::JetpackMissions) openMenu(MenuPage::GameSettings);
      else openMenu();
      return;
    }
    switch (menu.page) {
      case MenuPage::Home: {
        constexpr MenuPage destinations[] = {MenuPage::BootGame, MenuPage::ChangeGame, MenuPage::GameSettings,
                                             MenuPage::Brightness, MenuPage::Volume, MenuPage::Controller, MenuPage::Sleep};
        openMenu(destinations[menu.selection]);
        break;
      }
      case MenuPage::BootGame:
        if (gameChoiceAvailable(menu.selection) && setBootGame(gameChoiceTitle(menu.selection))) {
          const uint8_t cartridge = gameChoiceCartridge(menu.selection);
          if (settings.bootCartridge != cartridge) settingsDirty = true;
          settings.bootCartridge = cartridge;
        }
        break;
      case MenuPage::ChangeGame: {
        const GameTitle title = gameChoiceTitle(menu.selection);
        if (title == GameTitle::Nes && !chooseCartridge(gameChoiceCartridge(menu.selection))) break;
        if (title == GameTitle::Tetris ? activateTetris() : setCurrentGame(title)) closeMenu();
        break;
      }
      case MenuPage::GameSettings:
        if (currentGame == GameTitle::FlappyBird) {
          constexpr MenuPage destinations[] = {MenuPage::Modes, MenuPage::Skins, MenuPage::Clock, MenuPage::Coop};
          openMenu(destinations[menu.selection]);
        } else if (currentGame == GameTitle::Tetris) {
          if (menu.selection == 0) openMenu(MenuPage::TetrisZeroPlayers);
          else if (menu.selection == 1) openMenu(MenuPage::TetrisType);
          else if (menu.selection == 2) openMenu(MenuPage::TetrisLevel);
          else if (menu.selection == 3) openMenu(MenuPage::TetrisControls);
          else if (menu.selection == 4) setTetrisPal(!settings.tetrisPal);
          else if (menu.selection == 5) openMenu(MenuPage::TetrisLink);
          else if (menu.selection == 6) setTetrisLarge(!settings.tetrisLarge);
          else { ++tetrisRequest; closeMenu(); }
        } else if (currentGame == GameTitle::TetrisZero) {
          if (menu.selection == 0) openMenu(MenuPage::TetrisZeroPlayers);
          else if (menu.selection == 1) openMenu(MenuPage::TetrisType);
          else if (menu.selection == 2) openMenu(MenuPage::TetrisLevel);
          else if (menu.selection == 3) openMenu(MenuPage::TetrisControls);
          else if (menu.selection == 4) openMenu(MenuPage::TetrisLink);
          else if (menu.selection == 5) { if (settings.tetrisPlayers == 1) setTetrisLarge(!settings.tetrisLarge); }
          else { ++tetrisZeroRequest; closeMenu(); }
        } else if (currentGame == GameTitle::GeometryDash) {
          if (menu.selection == 0) openMenu(MenuPage::GeometryLevel);
          else requestGameRestart();
        } else if (currentGame == GameTitle::JetpackJoyride) {
          if (menu.selection == 0) requestGameRestart();
          else if (menu.selection == 1) setJetpackDifficulty(uint8_t((settings.jetpackDifficulty + 1) % 3));
          else if (menu.selection == 2) openMenu(MenuPage::JetpackMissions);
          else if (menu.selection == 3) openMenu(MenuPage::JetpackUpgrades);
          else if (menu.selection == 4) openMenu(MenuPage::JetpackPractice);
        } else if (nativeArcadeGame(currentGame)) {
          if (menu.selection == 0) requestGameRestart();
          else if (menu.selection == 1) ++arcadeCalibrateRequest;
          else if (menu.selection == 2) setArcadeSensitivity(uint8_t((settings.arcadeSensitivity + 1) % 3));
          else if (menu.selection == 3) setArcadeAutoFire(!settings.arcadeAutoFire);
          else if (menu.selection == 4) setArcadeDifficulty(uint8_t((settings.arcadeDifficulty + 1) % 3));
        } else requestGameRestart();
        break;
      case MenuPage::JetpackPractice:
        if (menu.selection == 0) setJetpackPractice(!settings.jetpackPractice);
        else if (menu.selection == 1) setJetpackRide(uint8_t((settings.jetpackRide + 1) % 9));
        else if (menu.selection == 2) setJetpackGadget(uint8_t((settings.jetpackGadget + 1) % 7));
        else if (menu.selection == 3) setJetpackHeadstart(!settings.jetpackHeadstart);
        break;
      case MenuPage::JetpackUpgrades:
        if (menu.selection > 0 && menu.selection < 5) {
          jetpackUpgradeKind = uint8_t(menu.selection - 1); ++jetpackUpgradeRequest;
        }
        break;
      case MenuPage::JetpackMissions: break;
      case MenuPage::GeometryLevel:
        if (currentGame == GameTitle::GeometryDash && geometryDashAvailable_) {
          setGeometryLevel(menu.selection);
          ++geometryRequest;
          closeMenu();
        }
        break;
      case MenuPage::TetrisLevel:
        if (currentGame == GameTitle::TetrisZero) setTetrisZeroLevel(menu.selection);
        else setTetrisLevel(menu.selection);
        break;
      case MenuPage::TetrisType:
        if (currentGame == GameTitle::TetrisZero) setTetrisZeroGameType(menu.selection);
        else setTetrisGameType(menu.selection == 1 ? 6 : 0);
        break;
      case MenuPage::TetrisZeroPlayers:
        if ((menu.selection == 0 && (tetrisAvailable_ || tetrisZeroAvailable_)) ||
            (menu.selection == 1 && tetrisZeroAvailable_)) {
          setTetrisPlayers(menu.selection + 1);
          activateTetris();
        }
        break;
      case MenuPage::TetrisControls:
        if (menu.selection < 2) setTetrisControls(menu.selection);
        else ++tetrisCalibrateRequest;
        break;
      case MenuPage::TetrisLink:
        tetrisLinkRole = menu.selection;
        ++tetrisLinkRequest;
        break;
      case MenuPage::Modes:
        setMode(Mode(menu.selection));
        closeMenu();
        break;
      case MenuPage::Brightness: setBrightness(menu.selection); break;
      case MenuPage::Volume: setVolume(menu.selection); break;
      case MenuPage::Sleep: setSleep(menu.selection); break;
      case MenuPage::Skins: setSkin(menu.selection); closeMenu(); break;
      case MenuPage::Clock:
        if (menu.selection == 0) setClock(uint8_t(clockHour(nowMs) + 1), clockMinute(nowMs), nowMs);
        else if (menu.selection == 1) setClock(clockHour(nowMs), uint8_t(clockMinute(nowMs) + 1), nowMs);
        else { settings.clock24Hour = !settings.clock24Hour; settingsDirty = true; }
        break;
      case MenuPage::Controller:
        controllerAction = uint8_t(menu.selection + 1);
        ++controllerRequest;
        break;
      case MenuPage::Coop:
        settings.coopRole = menu.selection;
        settingsDirty = true;
        ++coopRequest;
        if (settings.coopRole) setMode(Mode::Coop);
        else if (settings.mode == Mode::Coop) setMode(Mode::Original);
        closeMenu();
        break;
      default: break;
    }
  }
};

}  // namespace flappy
