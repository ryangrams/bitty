#pragma once

#include <stdint.h>
#include <stdio.h>
#include "game_core.h"

namespace flappy {

enum class Mode : uint8_t { Original, Clock, Demo, Arcade, Big, Coop, Count };
enum class MenuPage : uint8_t { Closed, Home, Modes, Brightness, Skins, Clock, Coop };

struct Settings {
  Mode mode = Mode::Original;
  uint8_t brightnessIndex = 3;
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
  bool settingsDirty = false;
  bool highScoresDirty = false;
  uint32_t coopRequest = 0;
  uint32_t roundsStarted = 0;

  explicit AppModel(uint32_t seed = 0x8f6e37a1U) : game(seed), randomState_(seed ? seed : 1) {
    for (unsigned i = 0; i < unsigned(Mode::Count); ++i) bestScores_[i] = 0;
    beginRound(seed);
  }

  bool menuOpen() const { return menu.page != MenuPage::Closed; }
  bool scoreEligible() const { return settings.mode != Mode::Clock && settings.mode != Mode::Demo && settings.mode != Mode::Coop; }
  bool humanControlling() const { return !automatedMode() || humanOverride_; }
  bool autoPlaying() const { return automatedMode() && !humanOverride_; }
  uint8_t demoSkill() const { return demoSkill_; }

  uint8_t brightness() const {
    constexpr uint8_t levels[] = {4, 20, 65, 140, 255};
    return levels[settings.brightnessIndex < 5 ? settings.brightnessIndex : 3];
  }

  void setBrightness(uint8_t index) {
    if (index >= 5) index = 4;
    if (settings.brightnessIndex != index) settingsDirty = true;
    settings.brightnessIndex = index;
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

  void closeMenu() {
    menu.page = MenuPage::Closed;
    menu.selection = 0;
  }

  const char* menuTitle() const {
    switch (menu.page) {
      case MenuPage::Modes: return "Mode";
      case MenuPage::Brightness: return "Brightness";
      case MenuPage::Skins: return "Skins";
      case MenuPage::Clock: return "Set clock";
      case MenuPage::Coop: return "Co-op link";
      default: return "Menu";
    }
  }

  uint8_t menuItemCount() const {
    switch (menu.page) {
      case MenuPage::Home: return 6;
      case MenuPage::Modes: return uint8_t(Mode::Count) + 1;
      case MenuPage::Brightness: return 6;
      case MenuPage::Skins: return uint8_t(skinCount_ + 1);
      case MenuPage::Clock: return 4;
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
        constexpr const char* labels[] = {"Mode", "Brightness", "Skins", "Set clock", "Co-op link"};
        return labels[index];
      }
      case MenuPage::Modes: return modeName(Mode(index));
      case MenuPage::Brightness: {
        constexpr const char* labels[] = {"1: Night", "2: Dim", "3: Medium", "4: Bright", "5: Full"};
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
        if (index == 0) return modeName(settings.mode);
        if (index == 1) {
          snprintf(valueBuffer_, sizeof(valueBuffer_), "%u/5", unsigned(settings.brightnessIndex + 1));
          return valueBuffer_;
        }
        if (index == 2) return skinNames_ ? skinNames_[settings.skinIndex] : "";
        if (index == 4) return settings.coopRole == 1 ? "Host" : settings.coopRole == 2 ? "Join" : "Off";
        break;
      case MenuPage::Modes: return index == uint8_t(settings.mode) ? "*" : "";
      case MenuPage::Brightness: return index == settings.brightnessIndex ? "*" : "";
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
    }
    if (menuDown && !holdHandled_ && uint32_t(nowMs - menuPressMs_) >= MenuHoldMs) {
      closeMenu();
      holdHandled_ = true;
      menuHandled = true;
    }
    if (!menuDown && menuWasDown_) {
      if (!holdHandled_) {
        if (menuOpen()) menu.selection = uint8_t((menu.selection + 1) % menuItemCount());
        else openMenu();
      }
      menuHandled = true;
    }
    menuWasDown_ = menuDown;
    if (menuOpen()) {
      if (actionEdge && !menuHandled) selectMenuItem(nowMs);
      return NoEvent;
    }
    if (wasOpen || menuHandled) return NoEvent;

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
  uint32_t randomState_;
  uint32_t menuPressMs_ = 0;
  uint32_t clockBaseMs_ = 0;
  uint32_t lastNowMs_ = 0;
  bool menuWasDown_ = false;
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
      else openMenu();
      return;
    }
    switch (menu.page) {
      case MenuPage::Home: {
        constexpr MenuPage destinations[] = {MenuPage::Modes, MenuPage::Brightness, MenuPage::Skins,
                                             MenuPage::Clock, MenuPage::Coop};
        openMenu(destinations[menu.selection]);
        break;
      }
      case MenuPage::Modes:
        setMode(Mode(menu.selection));
        closeMenu();
        break;
      case MenuPage::Brightness: setBrightness(menu.selection); break;
      case MenuPage::Skins: setSkin(menu.selection); break;
      case MenuPage::Clock:
        if (menu.selection == 0) setClock(uint8_t(clockHour(nowMs) + 1), clockMinute(nowMs), nowMs);
        else if (menu.selection == 1) setClock(clockHour(nowMs), uint8_t(clockMinute(nowMs) + 1), nowMs);
        else { settings.clock24Hour = !settings.clock24Hour; settingsDirty = true; }
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
