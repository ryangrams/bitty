#pragma once

#include <stdint.h>
#include <stdio.h>
#include "game_core.h"

namespace flappy {

enum class GameTitle : uint8_t { FlappyBird, Nes, Tetris, TetrisZero };
constexpr uint8_t GameTitleCount = 4;
constexpr uint8_t VisibleGameCount = 3;
enum class Mode : uint8_t { Original, Clock, Demo, Arcade, Big, Coop, Count };
enum class MenuPage : uint8_t { Closed, Home, BootGame, ChangeGame, GameSettings, Modes, Brightness, Volume, Skins, Clock, Controller, TetrisLevel, TetrisType, TetrisControls, TetrisLink, Coop, TetrisZeroPlayers };

struct Settings {
  GameTitle bootGame = GameTitle::FlappyBird;
  Mode mode = Mode::Original;
  uint8_t brightnessIndex = 1;
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
  Game game;
  Settings settings;
  MenuState menu;
  GameTitle currentGame = GameTitle::FlappyBird;
  uint32_t gameRequest = 0;
  uint32_t gameRestartRequest = 0;
  uint32_t tetrisRequest = 0;
  uint32_t tetrisZeroRequest = 0;
  uint32_t tetrisCalibrateRequest = 0;
  uint32_t tetrisLinkRequest = 0;
  uint8_t tetrisLinkRole = 0;
  uint32_t controllerRequest = 0;
  uint8_t controllerAction = 0;
  const char* controllerStatus = "Off";
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

  const char* gameLabel(GameTitle title) const {
    if (title == GameTitle::Nes) return nesAvailable_ ? nesTitle_ : "NES: no ROM";
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
    ++gameRestartRequest;
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
    menu.selection = 0;
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
      case MenuPage::TetrisType: return "Game type";
      case MenuPage::TetrisControls: return "Controls";
      case MenuPage::TetrisLink: return "Tetris link";
      case MenuPage::TetrisZeroPlayers: return "Players";
      case MenuPage::Modes: return "Mode";
      case MenuPage::Brightness: return "Brightness";
      case MenuPage::Volume: return "Volume";
      case MenuPage::Skins: return "Skins";
      case MenuPage::Clock: return "Set clock";
      case MenuPage::Controller: return "Controller";
      case MenuPage::Coop: return "Co-op link";
      default: return "Menu";
    }
  }

  uint8_t menuItemCount() const {
    switch (menu.page) {
      case MenuPage::Home: return 7;
      case MenuPage::BootGame:
      case MenuPage::ChangeGame: return VisibleGameCount + 1;
      case MenuPage::GameSettings:
        return currentGame == GameTitle::FlappyBird ? 5 : currentGame == GameTitle::Tetris ? 9 : currentGame == GameTitle::TetrisZero ? 8 : 2;
      case MenuPage::Modes: return uint8_t(Mode::Count) + 1;
      case MenuPage::Brightness: return 4;
      case MenuPage::TetrisLevel: return currentGame == GameTitle::TetrisZero ? 11 : 31;
      case MenuPage::TetrisType: return 3;
      case MenuPage::TetrisControls: return 4;
      case MenuPage::TetrisLink: return 4;
      case MenuPage::TetrisZeroPlayers: return 3;
      case MenuPage::Volume: return 7;
      case MenuPage::Skins: return uint8_t(skinCount_ + 1);
      case MenuPage::Clock: return 4;
      case MenuPage::Controller: return 4;
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
        constexpr const char* labels[] = {"Game at Boot", "Change Game", "Game Settings", "Brightness", "Volume", "Controller"};
        return labels[index];
      }
      case MenuPage::BootGame:
      case MenuPage::ChangeGame: return gameLabel(GameTitle(index));
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
        return "Restart game";
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
        constexpr const char* labels[] = {"Off", "20%", "40%", "60%", "80%", "Full"};
        return labels[index];
      }
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
        }
        break;
      case MenuPage::TetrisLevel: return index == (currentGame == GameTitle::TetrisZero ? settings.tetrisZeroLevel : settings.tetrisLevel) ? "*" : "";
      case MenuPage::TetrisType:
        return index == (currentGame == GameTitle::TetrisZero ? settings.tetrisZeroGameType : settings.tetrisGameType == 6 ? 1 : 0) ? "*" : "";
      case MenuPage::TetrisZeroPlayers: return index + 1 == settings.tetrisPlayers ? "*" : "";
      case MenuPage::TetrisControls:
        if (index < 2) return index == settings.tetrisControls ? "*" : "";
        if (index == 2) return "Hold unit steady";
        break;
      case MenuPage::Controller: return index == 0 ? controllerStatus : "";
      case MenuPage::BootGame: return index == uint8_t(settings.bootGame == GameTitle::TetrisZero ? GameTitle::Tetris : settings.bootGame) ? "*" : "";
      case MenuPage::ChangeGame: return index == uint8_t(currentGame == GameTitle::TetrisZero ? GameTitle::Tetris : currentGame) ? "*" : "";
      case MenuPage::Modes: return index == uint8_t(settings.mode) ? "*" : "";
      case MenuPage::Brightness: return index == settings.brightnessIndex ? "*" : "";
      case MenuPage::Volume: return index == settings.volumeIndex ? "*" : "";
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
  uint16_t bestScores_[unsigned(Mode::Count)];
  const char* const* skinNames_ = nullptr;
  uint8_t skinCount_ = 1;
  bool nesAvailable_ = false;
  bool tetrisAvailable_ = false;
  bool tetrisZeroAvailable_ = false;
  char tetrisZeroTitle_[64] = "Tetris Zero";
  char tetrisTitle_[64] = "Tetris";
  char nesTitle_[64] = "NES";
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
  mutable char labelBuffer_[20] = {};
  mutable char valueBuffer_[12] = {};

  bool gameAvailable(GameTitle title) const {
    return title == GameTitle::FlappyBird || (title == GameTitle::Nes && nesAvailable_) ||
           (title == GameTitle::Tetris && tetrisAvailable_) ||
           (title == GameTitle::TetrisZero && tetrisZeroAvailable_);
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
               menu.page == MenuPage::TetrisLink || menu.page == MenuPage::TetrisZeroPlayers) openMenu(MenuPage::GameSettings);
      else openMenu();
      return;
    }
    switch (menu.page) {
      case MenuPage::Home: {
        constexpr MenuPage destinations[] = {MenuPage::BootGame, MenuPage::ChangeGame, MenuPage::GameSettings,
                                             MenuPage::Brightness, MenuPage::Volume, MenuPage::Controller};
        openMenu(destinations[menu.selection]);
        break;
      }
      case MenuPage::BootGame: setBootGame(GameTitle(menu.selection)); break;
      case MenuPage::ChangeGame:
        if (menu.selection == uint8_t(GameTitle::Tetris) ? activateTetris() : setCurrentGame(GameTitle(menu.selection))) closeMenu();
        break;
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
        } else requestGameRestart();
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
