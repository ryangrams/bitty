#pragma once
#include "app_model.h"

namespace flappy {

// One home policy covers every game. Physical input is sampled by the caller.
class GameHome {
 public:
  bool active() const { return active_; }
  uint32_t generation() const { return generation_; }

  void observe(const AppModel& app) {
    if (!observed_ || gameRequest_ != app.gameRequest || homeRequest_ != app.homeRequest) {
      gameRequest_ = app.gameRequest;
      homeRequest_ = app.homeRequest;
      observed_ = true;
      enter();
    }
  }
  void enter() { active_ = true; ++generation_; }
  void play() { if (active_) { active_ = false; ++generation_; } }

  bool cycle(AppModel& app) {
    if (!active_ || app.menuOpen()) return false;
    const uint8_t count = app.gameChoiceCount();
    unsigned current = 0;
    for (unsigned i = 0; i < count; ++i) {
      const auto title = app.gameChoiceTitle(uint8_t(i));
      if (visibleGameIndex(title) == visibleGameIndex(app.currentGame) &&
          (title != GameTitle::Nes || app.gameChoiceCartridge(uint8_t(i)) == app.currentCartridge)) {
        current = i; break;
      }
    }
    for (unsigned offset = 1; offset <= count; ++offset) {
      const uint8_t index = uint8_t((current + offset) % count);
      if (!app.gameChoiceAvailable(index)) continue;
      const auto title = app.gameChoiceTitle(index);
      if (title == GameTitle::Nes && !app.chooseCartridge(app.gameChoiceCartridge(index))) continue;
      if (title == GameTitle::Tetris ? !app.activateTetris() : !app.setCurrentGame(title)) continue;
      observe(app);
      return true;
    }
    return false;
  }

 private:
  uint32_t gameRequest_ = 0, homeRequest_ = 0, generation_ = 0;
  bool observed_ = false, active_ = true;
};
}  // namespace flappy
