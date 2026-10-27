#include "../firmware/flappy/app_model.h"

#include <assert.h>
#include <cmath>
#include <iostream>
#include <set>

using namespace flappy;

static void near(float actual, float expected) { assert(std::fabs(actual - expected) < 0.001f); }

struct Driver {
  AppModel app;
  uint32_t now = 0;
  explicit Driver(uint32_t seed = 9) : app(seed) {}
  uint8_t tick(bool action = false, bool menu = false, uint32_t elapsed = 17) {
    now += elapsed;
    return app.step(action, menu, now);
  }
  void tapMenu() { tick(false, true); tick(false, false); }
};

static void menuEdgesAndPause() {
  Driver d;
  assert(!d.app.menuOpen());
  d.tick(true);
  assert(d.app.game.state == State::Playing);
  d.tapMenu();
  assert(d.app.menu.page == MenuPage::Home);
  assert(d.app.menu.selection == 0);
  const float y = d.app.game.birdY;
  const float x = d.app.game.pipes[0].x;
  const uint32_t frame = d.app.game.stateFrames;
  for (int i = 0; i < 100; ++i) d.tick();
  near(d.app.game.birdY, y);
  near(d.app.game.pipes[0].x, x);
  assert(d.app.game.stateFrames == frame);
  d.tapMenu();
  d.tapMenu();
  d.tapMenu();
  assert(d.app.menu.selection == 3);
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Brightness);
  assert(d.app.menu.selection == 0);
  d.tick(true);
  assert(d.app.brightness() == 4);
  assert(d.app.settings.brightnessIndex == 0);
  assert(d.app.settingsDirty);
  d.tick(false, true);
  d.tick(false, true, AppModel::MenuHoldMs);
  assert(!d.app.menuOpen());
  assert(d.app.game.stateFrames == frame);
  d.tick(false, false);
  assert(!d.app.menuOpen());
  assert(d.app.game.stateFrames == frame);
  d.tick();
  assert(d.app.game.stateFrames == frame + 1);
  d.tick(false, true);
  d.tick(false, true, AppModel::MenuHoldMs);
  d.tick(false, false);
  assert(!d.app.menuOpen());
}

static void everyMenuHasBack() {
  Driver d;
  const char* names[] = {"Original", "Paperwork", "Rubber duck"};
  d.app.setSkinNames(names, 3);
  const MenuPage pages[] = {MenuPage::Home, MenuPage::BootGame, MenuPage::ChangeGame,
                            MenuPage::Modes, MenuPage::Brightness, MenuPage::Volume,
                            MenuPage::Skins, MenuPage::Clock, MenuPage::Controller, MenuPage::Coop};
  for (MenuPage page : pages) {
    d.app.openMenu(page);
    const uint8_t count = d.app.menuItemCount();
    assert(count > 1);
    assert(std::string(d.app.menuLabel(count - 1)) == "Back");
    assert(std::string(d.app.menuLabel(count)).empty());
    d.app.menu.selection = count - 1;
    d.tick(true);
    assert(page == MenuPage::Home ? !d.app.menuOpen() : d.app.menu.page == MenuPage::Home);
  }
  for (unsigned i = 0; i < unsigned(Mode::Count); ++i) {
    d.app.openMenu(MenuPage::Modes);
    d.app.menu.selection = uint8_t(i);
    d.tick(true);
    assert(d.app.settings.mode == Mode(i));
    assert(!d.app.menuOpen());
  }
  d.app.openMenu(MenuPage::Skins);
  d.app.menu.selection = 2;
  d.tick(true);
  assert(d.app.settings.skinIndex == 2);
  assert(std::string(d.app.menuLabel(2)) == "Rubber duck");
  d.app.setSkinCount(1);
  assert(d.app.settings.skinIndex == 0);
  for (uint8_t i = 0; i < 5; ++i) {
    d.app.setBrightness(i);
    assert(d.app.brightness() > 0);
    if (i) { const uint8_t b = d.app.brightness(); d.app.setBrightness(i - 1); assert(b > d.app.brightness()); }
  }
  d.app.setBrightness(255);
  assert(d.app.settings.brightnessIndex == 4);
}

static void volumeSettingsAndMenuNavigation() {
  Driver d;
  const uint8_t values[] = {0, 51, 102, 153, 204, 255};
  const char* labels[] = {"Off", "20%", "40%", "60%", "80%", "Full"};
  assert(d.app.settings.volumeIndex == 2);
  assert(d.app.volume() == 102);
  assert(!d.app.settingsDirty);
  d.app.setVolume(2);
  assert(!d.app.settingsDirty);
  d.tapMenu();
  assert(d.app.menuItemCount() == 10);
  const char* homeLabels[] = {"Game at Boot", "Change Game", "Mode", "Brightness", "Volume",
                            "Skins", "Set clock", "Co-op link", "Controller", "Back"};
  const MenuPage homePages[] = {MenuPage::BootGame, MenuPage::ChangeGame, MenuPage::Modes,
                               MenuPage::Brightness, MenuPage::Volume, MenuPage::Skins,
                               MenuPage::Clock, MenuPage::Coop, MenuPage::Controller};
  for (uint8_t index = 0; index < 10; ++index) {
    assert(std::string(d.app.menuLabel(index)) == homeLabels[index]);
  }
  assert(std::string(d.app.menuValue(4)) == "40%");
  for (uint8_t index = 0; index < 9; ++index) {
    d.app.openMenu();
    d.app.menu.selection = index;
    d.tick(true);
    assert(d.app.menu.page == homePages[index]);
  }
  d.app.openMenu();
  for (unsigned index = 0; index < 4; ++index) d.tapMenu();
  assert(d.app.menu.selection == 4);
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Volume);
  assert(std::string(d.app.menuTitle()) == "Volume");
  assert(d.app.menuItemCount() == 7);
  for (uint8_t index = 0; index < 6; ++index) {
    assert(d.app.menu.selection == index);
    assert(std::string(d.app.menuLabel(index)) == labels[index]);
    d.app.settingsDirty = false;
    const bool changed = d.app.settings.volumeIndex != index;
    d.tick(true);
    assert(d.app.settings.volumeIndex == index);
    assert(d.app.volume() == values[index]);
    assert(d.app.settingsDirty == changed);
    for (uint8_t marker = 0; marker < 6; ++marker) {
      assert(std::string(d.app.menuValue(marker)) == (marker == index ? "*" : ""));
    }
    d.app.menu.page = MenuPage::Home;
    assert(std::string(d.app.menuValue(4)) == labels[index]);
    d.app.menu.page = MenuPage::Volume;
    d.app.settingsDirty = false;
    d.tick(true);
    assert(!d.app.settingsDirty);
    d.tapMenu();
  }
  assert(d.app.menu.selection == 6);
  assert(std::string(d.app.menuLabel(6)) == "Back");
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Home);
  assert(std::string(d.app.menuValue(4)) == "Full");
  d.app.setVolume(0);
  assert(std::string(d.app.menuValue(4)) == "Off");
  d.app.settingsDirty = false;
  d.app.setVolume(UINT8_MAX);
  assert(d.app.settings.volumeIndex == 5 && d.app.volume() == 255);
  assert(d.app.settingsDirty);
  d.app.settingsDirty = false;
  d.app.setVolume(UINT8_MAX);
  assert(!d.app.settingsDirty);
  d.app.openMenu(MenuPage::Volume);
  for (int index = 0; index < 7; ++index) d.tapMenu();
  assert(d.app.menu.selection == 0);
  d.tick(false, true);
  d.tick(false, true, AppModel::MenuHoldMs);
  assert(!d.app.menuOpen());
  d.tick(false, false);
  assert(!d.app.menuOpen());
}

static void launcherBootAndImmediateChanges() {
  Driver d;
  assert(d.app.settings.bootGame == GameTitle::FlappyBird);
  assert(d.app.currentGame == GameTitle::FlappyBird);
  assert(d.app.gameRequest == 0 && !d.app.nesAvailable());
  assert(std::string(d.app.nesTitle()) == "NES");
  assert(std::string(d.app.gameLabel(GameTitle::FlappyBird)) == "Flappy Bird");
  assert(std::string(d.app.gameLabel(GameTitle::Nes)) == "NES: no ROM");
  d.app.setBestScore(Mode::Original, 9);
  d.tick(true);
  assert(d.app.game.state == State::Playing);
  assert(!d.app.setBootGame(GameTitle::Nes));
  assert(!d.app.setCurrentGame(GameTitle::Nes));
  assert(d.app.settings.bootGame == GameTitle::FlappyBird && !d.app.settingsDirty);
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 0);
  for (MenuPage page : {MenuPage::BootGame, MenuPage::ChangeGame}) {
    d.app.openMenu(page);
    assert(d.app.menuItemCount() == 3);
    assert(std::string(d.app.menuTitle()) == (page == MenuPage::BootGame ? "Game at Boot" : "Change Game"));
    assert(std::string(d.app.menuLabel(0)) == "Flappy Bird");
    assert(std::string(d.app.menuLabel(1)) == "NES: no ROM");
    assert(std::string(d.app.menuLabel(2)) == "Back");
    assert(std::string(d.app.menuValue(0)) == "*");
    d.app.menu.selection = 1;
    d.tick(true);
    assert(d.app.menu.page == page && !d.app.settingsDirty);
    assert(d.app.gameRequest == 0 && d.app.currentGame == GameTitle::FlappyBird);
  }
  char title[] = "Super Mario Bros.";
  d.app.setNesTitle(title);
  title[0] = 'X';
  assert(std::string(d.app.nesTitle()) == "Super Mario Bros.");
  d.app.setNesAvailable(true);
  d.app.openMenu(MenuPage::BootGame);
  assert(std::string(d.app.menuLabel(1)) == "Super Mario Bros.");
  d.app.menu.selection = 1;
  const uint32_t beforeBootRound = d.app.roundsStarted;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::BootGame);
  assert(d.app.settings.bootGame == GameTitle::Nes && d.app.settingsDirty);
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 0);
  assert(d.app.roundsStarted == beforeBootRound && d.app.game.state == State::Playing);
  assert(std::string(d.app.menuValue(1)) == "*");
  d.app.settingsDirty = false;
  d.tick(true);
  assert(!d.app.settingsDirty);
  d.app.openMenu();
  assert(std::string(d.app.menuValue(0)) == "Super Mario Bros.");
  assert(std::string(d.app.menuValue(1)) == "Flappy Bird");
  d.app.menu.selection = 1;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::ChangeGame);
  d.tapMenu();
  d.tick(true);
  assert(d.app.currentGame == GameTitle::Nes && d.app.gameRequest == 1);
  assert(!d.app.menuOpen() && !d.app.settingsDirty);
  assert(!d.app.scoreEligible() && !d.app.humanControlling() && !d.app.autoPlaying());
  const uint32_t pausedFrame = d.app.game.stateFrames;
  const uint32_t pausedWorld = d.app.game.worldTick();
  const float pausedBird = d.app.game.birdY, pausedPipe = d.app.game.pipes[0].x;
  for (unsigned i = 0; i < 100; ++i) assert(d.tick(true) == NoEvent);
  assert(d.app.game.stateFrames == pausedFrame && d.app.game.worldTick() == pausedWorld);
  near(d.app.game.birdY, pausedBird);
  near(d.app.game.pipes[0].x, pausedPipe);
  d.tapMenu();
  d.tapMenu();
  d.tick(true);
  assert(d.app.menu.page == MenuPage::ChangeGame);
  assert(std::string(d.app.menuValue(1)) == "*");
  d.tick(true);
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 2);
  assert(d.app.game.state == State::Ready && d.app.game.bestScore == 9);
  assert(!d.app.menuOpen() && !d.app.settingsDirty);
  assert(d.app.settings.bootGame == GameTitle::Nes);
  assert(d.app.setCurrentGame(GameTitle::FlappyBird));
  assert(d.app.gameRequest == 2);
  assert(!d.app.setCurrentGame(GameTitle(UINT8_MAX)));
  assert(!d.app.setBootGame(GameTitle(UINT8_MAX)));
  assert(!d.app.settingsDirty && d.app.gameRequest == 2);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::Nes && d.app.gameRequest == 3);
  assert(!d.app.settingsDirty);
  d.app.setNesAvailable(false);
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 4);
  assert(d.app.settings.bootGame == GameTitle::Nes && !d.app.settingsDirty);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 4);
  d.app.setNesAvailable(true);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::Nes && d.app.gameRequest == 5);
  assert(d.app.setBootGame(GameTitle::FlappyBird));
  assert(d.app.currentGame == GameTitle::Nes && d.app.settingsDirty);
  d.app.settingsDirty = false;
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 6);
  assert(!d.app.settingsDirty);
  d.app.settings.bootGame = GameTitle(UINT8_MAX);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 6);
  d.app.setNesTitle(nullptr);
  assert(std::string(d.app.nesTitle()) == "NES");
  const std::string longTitle(200, 'A');
  d.app.setNesTitle(longTitle.c_str());
  assert(std::string(d.app.nesTitle()).size() == 63);
}

static void controllerMenuRequestsAndStatus() {
  Driver d;
  assert(d.app.controllerRequest == 0 && d.app.controllerAction == 0);
  assert(std::string(d.app.controllerStatus) == "Off");
  d.app.openMenu();
  assert(std::string(d.app.menuLabel(8)) == "Controller");
  assert(std::string(d.app.menuLabel(9)) == "Back");
  assert(std::string(d.app.menuValue(8)) == "Off");
  d.app.setControllerStatus("Searching");
  assert(std::string(d.app.menuValue(8)) == "Searching");
  d.app.setControllerStatus(nullptr);
  assert(std::string(d.app.menuValue(8)) == "Off");
  d.app.setControllerStatus("");
  assert(std::string(d.app.menuValue(8)) == "Off");
  d.app.menu.selection = 8;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Controller);
  assert(std::string(d.app.menuTitle()) == "Controller");
  assert(d.app.menuItemCount() == 4);
  const char* labels[] = {"Pair controller", "Disconnect", "Forget pairing", "Back"};
  for (uint8_t index = 0; index < 4; ++index) assert(std::string(d.app.menuLabel(index)) == labels[index]);
  for (uint8_t action = 1; action <= 3; ++action) {
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
      const uint32_t before = d.app.controllerRequest;
      d.app.openMenu(MenuPage::Controller);
      d.app.menu.selection = uint8_t(action - 1);
      d.tick(true);
      assert(d.app.controllerAction == action && d.app.controllerRequest == before + 1);
      assert(!d.app.menuOpen() && !d.app.settingsDirty);
      assert(d.app.currentGame == GameTitle::FlappyBird);
    }
  }
  d.app.openMenu(MenuPage::Controller);
  d.app.menu.selection = 3;
  const uint32_t before = d.app.controllerRequest;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Home && d.app.controllerRequest == before);
  d.app.setNesAvailable(true);
  assert(d.app.setCurrentGame(GameTitle::Nes));
  d.app.openMenu(MenuPage::Controller);
  d.tick(true);
  assert(d.app.currentGame == GameTitle::Nes && d.app.controllerAction == 1);
  assert(d.app.controllerRequest == before + 1);
}

static void clockAndCoopMenus() {
  Driver d;
  d.app.setClock(23, 59, 0);
  assert(d.app.clockHour(59999) == 23 && d.app.clockMinute(59999) == 59);
  assert(d.app.clockHour(60000) == 0 && d.app.clockMinute(60000) == 0);
  const uint32_t anchor = UINT32_MAX - 20000U;
  d.app.setClock(12, 12, anchor);
  assert(d.app.clockHour(anchor + 60000U) == 12 && d.app.clockMinute(anchor + 60000U) == 13);
  d.app.setClock(23, 59, d.now);
  d.app.openMenu(MenuPage::Clock);
  d.tick(true);
  assert(d.app.settings.clockHour == 0);
  d.app.menu.selection = 1;
  d.tick(true);
  assert(d.app.settings.clockMinute == 0);
  d.app.menu.selection = 2;
  d.tick(true);
  assert(!d.app.settings.clock24Hour);
  d.app.openMenu(MenuPage::Coop);
  d.app.menu.selection = 1;
  d.tick(true);
  assert(d.app.settings.coopRole == 1);
  assert(d.app.settings.mode == Mode::Coop);
  assert(d.app.coopRequest == 1);
  d.app.openMenu(MenuPage::Coop);
  d.app.menu.selection = 2;
  d.tick(true);
  assert(d.app.settings.coopRole == 2 && d.app.coopRequest == 2);
  d.app.openMenu(MenuPage::Coop);
  d.tick(true);
  assert(d.app.settings.coopRole == 0 && d.app.settings.mode == Mode::Original);
}

static void forcePoint(Driver& d) {
  d.app.game.state = State::Playing;
  d.app.game.birdY = 190;
  d.app.game.velocityY = 0;
  d.app.game.pipes[0] = {49, 160, false};
  assert(d.tick() & Point);
}

static void scoresStayWithTheirMode() {
  Driver d;
  d.app.setBestScore(Mode::Original, 9);
  d.app.game.score = 9;
  forcePoint(d);
  assert(d.app.bestScore(Mode::Original) == 10);
  assert(d.app.highScoresDirty);
  d.app.setMode(Mode::Big);
  assert(d.app.game.bestScore == 0);
  d.app.setMode(Mode::Original);
  assert(d.app.game.bestScore == 10);
  const Mode excluded[] = {Mode::Clock, Mode::Demo, Mode::Coop};
  for (Mode mode : excluded) {
    d.app.highScoresDirty = false;
    d.app.setMode(mode);
    if (mode != Mode::Coop) {
      assert(!d.app.humanControlling());
      d.tick(true);
      assert(d.app.humanControlling());
    }
    assert(!d.app.scoreEligible());
    forcePoint(d);
    assert(d.app.game.score == 1);
    assert(d.app.game.bestScore == 0 && !d.app.game.newRecord);
    assert(d.app.bestScore(mode) == 0 && !d.app.highScoresDirty);
  }
}

static void takeoverAndResume() {
  Driver d;
  for (Mode mode : {Mode::Clock, Mode::Demo}) {
    d.app.setMode(mode);
    d.tick();
    assert(d.app.autoPlaying());
    assert(d.app.game.state == State::Playing);
    d.tick(true);
    assert(d.app.humanControlling());
    assert(d.app.game.state == State::Playing && d.app.game.score == 0);
    d.app.game.birdY = GroundY - BirdHeight;
    d.app.game.velocityY = 5;
    d.tick();
    d.tick();
    assert(d.app.game.state == State::GameOver);
    for (unsigned i = 0; i < AppModel::AutoResumeFrames + 2; ++i) d.tick();
    assert(d.app.autoPlaying());
    assert(d.app.game.state == State::Playing);
    assert(!d.app.scoreEligible());
  }
}

static void demosRepeatAndVary() {
  Driver first(763), same(763);
  first.app.setMode(Mode::Demo);
  same.app.setMode(Mode::Demo);
  std::set<unsigned> skills;
  unsigned rounds = 0, minimum = UINT16_MAX, maximum = 0;
  for (unsigned i = 0; i < 200000; ++i) {
    const uint8_t events = first.tick();
    assert(events == same.tick());
    assert(first.app.game.state == same.app.game.state);
    near(first.app.game.birdY, same.app.game.birdY);
    near(first.app.game.pipes[0].x, same.app.game.pipes[0].x);
    assert(first.app.game.score == same.app.game.score);
    skills.insert(first.app.demoSkill());
    if (events & GameOver) {
      ++rounds;
      const unsigned score = first.app.game.score;
      if (score < minimum) minimum = score;
      if (score > maximum) maximum = score;
    }
  }
  assert(rounds > 10 && skills.size() == 4);
  assert(minimum <= 2 && maximum >= 10);
  assert(first.app.game.bestScore == 0 && !first.app.highScoresDirty);
  Driver clock;
  clock.app.setMode(Mode::Clock);
  for (int i = 0; i < 20000; ++i) clock.tick();
  assert(clock.app.game.state == State::Playing);
  assert(clock.app.game.score >= 100);
  for (unsigned seed = 1; seed <= 32; ++seed) {
    Driver varied(seed);
    varied.app.setMode(Mode::Clock);
    for (unsigned frame = 0; frame < 10000; ++frame) assert(!(varied.tick() & GameOver));
    assert(varied.app.game.score >= 100);
  }
}

static void bigCollisionAndPlayability() {
  Game game(2);
  game.setRules(rulesForMode(Mode::Big));
  game.reset(2);
  assert(game.birdWidth() == BirdWidth * 2 && game.birdHeight() == BirdHeight * 2);
  assert(game.pipeWidth() == PipeWidth * 2 && game.pipeHeight() == PipeHeight * 2);
  assert(game.rules().groundY > GroundY);
  uint64_t masks[3][BirdHeight] = {};
  masks[1][8] = uint64_t(1) << 10;
  game.setBirdMasks(masks);
  game.birdFrame = 1;
  game.birdY = 100;
  Pipe pipe = {float(game.birdX()), 118, false};
  assert(game.collidesWithPipe(pipe));
  pipe.gapY = 116;
  assert(!game.collidesWithPipe(pipe));
  pipe.gapY = 118;
  pipe.x = float(game.birdX() + 21);
  assert(game.collidesWithPipe(pipe));
  pipe.x = float(game.birdX() + 22);
  assert(!game.collidesWithPipe(pipe));
  game.setBirdMasks(nullptr);
  game.reset(2);
  game.step(true);
  unsigned ticks = 0;
  while (game.state == State::Playing && game.score < 20 && ticks++ < 6000) {
    const Pipe* nearest = nullptr;
    for (int i = 0; i < PipeCount; ++i) if (game.pipes[i].x + game.pipeWidth() > game.birdX() &&
        (!nearest || game.pipes[i].x < nearest->x)) nearest = &game.pipes[i];
    assert(nearest);
    const float target = nearest->gapY + 124.0f;
    game.step(game.velocityY >= -0.5f && game.birdY + game.velocityY * 4 > target);
  }
  assert(game.state == State::Playing && game.score == 20);
}

static void arcadeProgression() {
  Driver d;
  d.app.setMode(Mode::Arcade);
  assert(d.app.game.rules().pipeGap == 128);
  assert(d.app.arcadeTickets() == 0 && d.app.arcadePointsRemaining() == 10);
  const uint16_t gates[] = {10, 20, 35, 50, 60};
  const uint16_t tickets[] = {20, 50, 115, 630, 1640};
  for (unsigned i = 0; i < 5; ++i) {
    d.app.game.score = gates[i];
    assert(d.app.arcadeTickets() == tickets[i]);
    assert(d.app.arcadePointsRemaining() == (i < 4 ? gates[i + 1] - gates[i] : 0));
  }
  d.app.game.score = 1;
  assert(d.app.arcadeTickets() == 1);
  d.app.game.score = 9;
  assert(d.app.arcadeTickets() == 9);
  forcePoint(d);
  assert(d.app.arcadeTickets() == 20);
  assert(d.app.game.rules().pipeGap < 128);
  d.app.game.score = 65000;
  forcePoint(d);
  assert(d.app.game.rules().pipeGap == 88);
  assert(d.app.arcadeTickets() == UINT16_MAX);
  d.app.game.score = UINT16_MAX;
  assert(d.app.arcadeTickets() == UINT16_MAX);
}

static void sharedWorldMap() {
  Game first(12), second(23), other(12);
  first.enableSharedMap(190);
  second.enableSharedMap(190);
  other.enableSharedMap(191);
  first.step(true);
  second.step(true);
  assert(first.worldTick() == 1 && second.worldTick() == 1);
  bool different = false;
  for (uint32_t tick : {0U, 1U, 270U, 271U, 342U, 343U, 1000U, 100000U, 10000000U}) {
    first.syncWorldTick(tick);
    second.syncWorldTick(tick);
    other.syncWorldTick(tick);
    for (int i = 0; i < PipeCount; ++i) {
      near(first.pipes[i].x, second.pipes[i].x);
      assert(first.pipes[i].gapY == second.pipes[i].gapY);
      assert(first.pipes[i].gapY >= 80 && first.pipes[i].gapY < 222);
      assert(first.pipes[i].x >= -PipeWidth);
      if (i) near(first.pipes[i].x - first.pipes[i - 1].x, float(PipeSpacing));
      different |= first.pipes[i].gapY != other.pipes[i].gapY;
    }
  }
  assert(different);
  first.syncWorldTick(240);
  first.pipes[0].scored = true;
  first.syncWorldTick(241);
  assert(first.pipes[0].scored);
  first.pipes[1].scored = true;
  first.syncWorldTick(271);
  assert(first.pipes[0].scored);
  first.state = State::GameOver;
  const float old = first.pipes[0].x;
  first.syncWorldTick(272);
  near(first.pipes[0].x, old - PipeSpeed);
  first.disableSharedMap();
  first.reset(2);
  assert(first.worldTick() == 0);
}

int main() {
  menuEdgesAndPause();
  everyMenuHasBack();
  volumeSettingsAndMenuNavigation();
  launcherBootAndImmediateChanges();
  controllerMenuRequestsAndStatus();
  clockAndCoopMenus();
  scoresStayWithTheirMode();
  takeoverAndResume();
  demosRepeatAndVary();
  bigCollisionAndPlayability();
  arcadeProgression();
  sharedWorldMap();
  std::cout << "App, menu, automatic play, clock, arcade, big mode and shared map checks passed.\n";
}
