#include "../firmware/flappy/game_home.h"
#include <assert.h>
#include "../firmware/flappy/stick_input.h"
#include "../firmware/flappy/menu_font.h"
#include <string.h>
#include <iostream>
#include <set>

using namespace flappy;

static void lifecycle() {
  AppModel app(19);
  GameHome home;
  assert(home.active());
  home.observe(app);
  const uint32_t initial = home.generation();
  home.observe(app); assert(home.generation() == initial);
  home.play(); assert(!home.active() && home.generation() == initial+1);
  home.play(); assert(home.generation() == initial+1);
  assert(!home.cycle(app));
  app.openMenu(); app.menu.selection = 8;
  app.step(true,false,17,false);
  assert(!app.menuOpen() && app.homeRequest == 1);
  home.observe(app); assert(home.active());
  home.play();
  app.openMenu(MenuPage::ChangeGame); app.menu.selection = 0;
  app.step(true,false,34,false);
  home.observe(app);
  assert(home.active() && app.currentGame == GameTitle::FlappyBird);
  app.openMenu(); assert(!home.cycle(app)); app.closeMenu();
  home.play();
  assert(app.setCurrentGame(GameTitle::Galaga)); home.observe(app);
  assert(home.active());
}

static void cycleAvailabilityAndCartridges() {
  AppModel app(31); GameHome home; home.observe(app);
  const GameTitle publicOrder[] = {GameTitle::Galaga,GameTitle::Arkanoid,GameTitle::JetpackJoyride,GameTitle::HockeyQueen,GameTitle::FlappyBird};
  for (auto title : publicOrder) { assert(home.cycle(app)); assert(app.currentGame == title && home.active()); }
  app.setNesAvailable(true); app.setTetrisAvailable(true); app.setTetrisZeroAvailable(true);
  app.setGeometryDashAvailable(true);
  app.addCartridgeChoice(11,"Mario"); app.addCartridgeChoice(22,"Another NES");
  app.addCartridgeChoice(33,"Third NES");
  const GameTitle fullOrder[] = {GameTitle::Nes,GameTitle::Tetris,GameTitle::GeometryDash,GameTitle::Galaga,
    GameTitle::Arkanoid,GameTitle::JetpackJoyride,GameTitle::HockeyQueen,GameTitle::Nes,GameTitle::Nes,GameTitle::FlappyBird};
  unsigned nesIndex = 0;
  for (auto title : fullOrder) {
    assert(home.cycle(app)); assert(app.currentGame == title);
    if (title == GameTitle::Nes) { const uint8_t ids[] = {11,22,33}; assert(app.currentCartridge == ids[nesIndex++]); }
  }
  app.setTetrisPlayers(2);
  assert(home.cycle(app) && app.currentGame == GameTitle::Nes);
  assert(home.cycle(app) && app.currentGame == GameTitle::TetrisZero);
  assert(home.cycle(app) && app.currentGame == GameTitle::GeometryDash);
  app.setTetrisAvailable(false); app.setGeometryDashAvailable(false); app.setNesAvailable(false);
  app.setCurrentGame(GameTitle::FlappyBird); home.observe(app);
  assert(home.cycle(app) && app.currentGame == GameTitle::TetrisZero);
  app.setTetrisZeroAvailable(false); home.observe(app);
  assert(app.currentGame == GameTitle::FlappyBird);
  assert(home.cycle(app) && app.currentGame == GameTitle::Galaga);
}

static void frozenGameplayAndBoot() {
  for (unsigned title = 0; title < GameTitleCount; ++title) {
    AppModel app(17); app.setNesAvailable(true); app.setTetrisAvailable(true);
    app.setTetrisZeroAvailable(true); app.setGeometryDashAvailable(true);
    assert(app.setBootGame(GameTitle(title))); app.restoreBootGame();
    GameHome home; home.observe(app); assert(home.active());
    const uint32_t tick = app.game.worldTick(), rounds = app.roundsStarted;
    const float y = app.game.birdY;
    for (unsigned f = 0; f < 20000; ++f) {
      app.step(false,false,f*17,false); home.observe(app);
      if (f%500 == 0) { app.openMenu(MenuPage::Clock); app.step(true,false,f*17,false); app.closeMenu(); }
    }
    assert(app.game.worldTick() == tick && app.game.birdY == y);
    assert(app.roundsStarted == rounds && !app.highScoresDirty);
    for (unsigned m = 0; m < unsigned(Mode::Count); ++m) assert(app.bestScore(Mode(m)) == 0);
  }
  AppModel missing; missing.settings.bootGame = GameTitle::Nes; missing.restoreBootGame();
  GameHome fallback; fallback.observe(missing);
  assert(fallback.active() && missing.currentGame == GameTitle::FlappyBird);
}

int main() {
  lifecycle(); cycleAvailabilityAndCartridges(); frozenGameplayAndBoot();
  for (bool s3 : {false,true}) for (bool home : {false,true}) for (bool menu : {false,true}) {
    assert(StickInput::nextGame(s3,home,menu,true,false) == (home && !menu && !s3));
    assert(StickInput::nextGame(s3,home,menu,false,true) == (home && !menu && s3));
    assert(!StickInput::nextGame(s3,home,menu,false,false));
    assert(MenuFont::textWidth(StickInput::homeLabel(s3)) <= 135 - 26);
  }
  assert(!strcmp(StickInput::nextLabel(true),"RIGHT NEXT"));
  assert(StickInput::horizontalTilt(false,.5f,-.25f) == -.5f);
  assert(StickInput::horizontalTilt(true,.5f,-.25f) == .25f);
  assert(StickInput::horizontalTilt(true,.9f,0) == 0);
  assert(StickInput::horizontalTilt(true,0,-.6f) > 0); // Right edge lowered.
  assert(StickInput::horizontalTilt(true,0,.6f) < 0);
  assert(strcmp(StickInput::neutralKey(true,true),StickInput::neutralKey(false,true)));
  assert(!strcmp(StickInput::neutralKey(false,true),"tetneutral"));
  assert(!strcmp(StickInput::neutralKey(false,false),"arcneutral"));
  assert(StickInput::horizontalTilt(false,0,.9f) == 0);
  std::cout << "Game homes: boot, menu return, same-game selection, cartridge cycling, missing games and frozen scores passed\n";
}
