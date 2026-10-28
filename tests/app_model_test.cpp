#include "../firmware/flappy/app_model.h"
#include "../firmware/flappy/menu_font.h"

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
  void holdMenu() { tick(false, true); tick(false, true, AppModel::MenuHoldMs); tick(false, false); }
};

static void menuEdgesAndPause() {
  Driver d;
  d.tick(true);
  const uint32_t beforeTap = d.app.game.worldTick();
  d.tapMenu();
  assert(!d.app.menuOpen() && d.app.game.worldTick() == beforeTap + 2);
  d.tick(false, true);
  d.tick(false, true, AppModel::MenuHoldMs - 1);
  assert(!d.app.menuOpen());
  const uint32_t frame = d.app.game.stateFrames;
  d.tick(false, true, 1);
  assert(d.app.menu.page == MenuPage::Home && d.app.menu.selection == 0);
  d.tick(false, true, 2000);
  assert(d.app.menu.page == MenuPage::Home && d.app.menu.selection == 0);
  d.tick(false, false);
  assert(d.app.menu.page == MenuPage::Home && d.app.menu.selection == 0);
  const float y = d.app.game.birdY, x = d.app.game.pipes[0].x;
  for (int i = 0; i < 100; ++i) d.tick();
  near(d.app.game.birdY, y);
  near(d.app.game.pipes[0].x, x);
  assert(d.app.game.stateFrames == frame);
  for (unsigned index = 0; index < 3; ++index) d.tapMenu();
  assert(d.app.menu.selection == 3);
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Brightness);
  d.tick(true);
  assert(d.app.brightness() == 65 && d.app.settings.brightnessIndex == 0 && d.app.settingsDirty);
  d.tick(false, true);
  d.tick(false, true, AppModel::MenuHoldMs);
  assert(!d.app.menuOpen() && d.app.game.stateFrames == frame);
  d.tick(false, true, 2000);
  assert(!d.app.menuOpen());
  d.tick(false, false);
  assert(!d.app.menuOpen());
  d.holdMenu();
  assert(d.app.menu.page == MenuPage::Home && d.app.menu.selection == 0);
  d.app.closeMenu();
  d.app.openMenuFromController();
  assert(d.app.menu.page == MenuPage::Home && d.app.menu.selection == 0);
  d.app.openMenuFromController();
  assert(d.app.menu.selection == 1);
  Driver wrapped;
  wrapped.now = UINT32_MAX - 300U;
  wrapped.holdMenu();
  assert(wrapped.app.menuOpen() && wrapped.app.menu.selection == 0);
}

static void everyMenuHasBack() {
  Driver d;
  const char* names[] = {"Original", "Paperwork", "Rubber duck"};
  d.app.setSkinNames(names, 3);
  const MenuPage pages[] = {MenuPage::Home, MenuPage::BootGame, MenuPage::ChangeGame, MenuPage::GameSettings,
    MenuPage::Modes, MenuPage::Brightness, MenuPage::Volume, MenuPage::Skins, MenuPage::Clock,
    MenuPage::Controller, MenuPage::TetrisLevel, MenuPage::TetrisType, MenuPage::TetrisControls,
    MenuPage::TetrisLink, MenuPage::Coop, MenuPage::TetrisZeroPlayers, MenuPage::Sleep, MenuPage::GeometryLevel};
  for (MenuPage page : pages) {
    d.app.openMenu(page);
    const uint8_t count = d.app.menuItemCount();
    assert(count > 1);
    assert(std::string(d.app.menuLabel(count - 1)) == "Back");
    assert(std::string(d.app.menuLabel(count)).empty());
    d.app.menu.selection = count - 1;
    d.tick(true);
    const bool nested = page == MenuPage::Modes || page == MenuPage::Skins ||
      page == MenuPage::Coop || page == MenuPage::TetrisLevel || page == MenuPage::TetrisType ||
      page == MenuPage::TetrisControls || page == MenuPage::TetrisLink || page == MenuPage::TetrisZeroPlayers ||
      page == MenuPage::GeometryLevel;
    assert(page == MenuPage::Home ? !d.app.menuOpen() : d.app.menu.page == (nested ? MenuPage::GameSettings : MenuPage::Home));
  }
  const Mode playableModes[] = {Mode::Original, Mode::Demo, Mode::Arcade, Mode::Big, Mode::Coop};
  for (unsigned i = 0; i < 5; ++i) {
    d.app.openMenu(MenuPage::Modes);
    assert(d.app.menuItemCount() == 6 && playModeChoice(uint8_t(i)) == playableModes[i]);
    assert(std::string(d.app.menuLabel(uint8_t(i))) == modeName(playableModes[i]));
    assert(std::string(d.app.menuLabel(uint8_t(i))) != "Clock");
    d.app.menu.selection = uint8_t(i);
    d.tick(true);
    assert(d.app.settings.mode == playableModes[i] && !d.app.menuOpen());
  }
  d.app.openMenu(MenuPage::Skins);
  d.app.menu.selection = 2;
  assert(std::string(d.app.menuLabel(2)) == "Rubber duck");
  d.tick(true);
  assert(d.app.settings.skinIndex == 2 && !d.app.menuOpen());
  d.app.setSkinCount(1);
  assert(d.app.settings.skinIndex == 0);
  assert(d.app.settings.brightnessIndex == 1);
  const uint8_t levels[] = {65, 140, 255};
  const char* brightnessLabels[] = {"Low", "Medium", "High"};
  d.app.openMenu(MenuPage::Brightness);
  assert(d.app.menuItemCount() == 4);
  for (uint8_t i = 0; i < 3; ++i) {
    d.app.setBrightness(i);
    assert(d.app.brightness() == levels[i]);
    assert(std::string(d.app.menuLabel(i)) == brightnessLabels[i]);
  }
  d.app.setBrightness(UINT8_MAX);
  assert(d.app.settings.brightnessIndex == 2);
}

static void volumeSettingsAndMenuNavigation() {
  Driver d;
  const uint8_t values[] = {0, 51, 102, 153, 204, 255};
  const char* labels[] = {"Off", "20%", "40%", "60%", "80%", "Full"};
  d.app.openMenu(MenuPage::Volume);
  d.app.speakerBatteryCap = true;
  assert(std::string(d.app.menuLabel(4)) == "80% (72% battery)");
  assert(std::string(d.app.menuLabel(5)) == "Full (72% battery)");
  d.app.speakerBatteryCap = false;
  d.app.closeMenu();
  assert(d.app.settings.volumeIndex == 2 && d.app.volume() == 102 && !d.app.settingsDirty);
  d.app.setVolume(2);
  assert(!d.app.settingsDirty);
  d.holdMenu();
  assert(d.app.menuItemCount() == 10);
  const char* homeLabels[] = {"Game at Boot", "Change Game", "Game Settings", "Brightness", "Volume", "Controller", "Sleep", "Set clock", "Game home", "Back"};
  const MenuPage homePages[] = {MenuPage::BootGame, MenuPage::ChangeGame, MenuPage::GameSettings,
    MenuPage::Brightness, MenuPage::Volume, MenuPage::Controller, MenuPage::Sleep, MenuPage::Clock};
  for (uint8_t index = 0; index < 10; ++index) assert(std::string(d.app.menuLabel(index)) == homeLabels[index]);
  assert(std::string(d.app.menuValue(4)) == "40%");
  for (uint8_t index = 0; index < 8; ++index) {
    d.app.openMenu(); d.app.menu.selection = index; d.tick(true);
    assert(d.app.menu.page == homePages[index]);
  }
  d.app.openMenu();
  for (unsigned index = 0; index < 4; ++index) d.tapMenu();
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Volume && d.app.menuItemCount() == 7);
  for (uint8_t index = 0; index < 6; ++index) {
    assert(d.app.menu.selection == index && std::string(d.app.menuLabel(index)) == labels[index]);
    d.app.settingsDirty = false;
    const bool changed = d.app.settings.volumeIndex != index;
    d.tick(true);
    assert(d.app.settings.volumeIndex == index && d.app.volume() == values[index]);
    assert(d.app.settingsDirty == changed);
    for (uint8_t marker = 0; marker < 6; ++marker) assert(std::string(d.app.menuValue(marker)) == (marker == index ? "*" : ""));
    d.app.menu.page = MenuPage::Home;
    assert(std::string(d.app.menuValue(4)) == labels[index]);
    d.app.menu.page = MenuPage::Volume;
    d.app.settingsDirty = false; d.tick(true);
    assert(!d.app.settingsDirty);
    d.tapMenu();
  }
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Home && std::string(d.app.menuValue(4)) == "Full");
  d.app.setVolume(0);
  assert(std::string(d.app.menuValue(4)) == "Off");
  d.app.settingsDirty = false; d.app.setVolume(UINT8_MAX);
  assert(d.app.settings.volumeIndex == 5 && d.app.volume() == 255 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setVolume(UINT8_MAX);
  assert(!d.app.settingsDirty);
  d.app.openMenu(MenuPage::Volume);
  for (int i = 0; i < 7; ++i) d.tapMenu();
  assert(d.app.menu.selection == 0);
  d.holdMenu();
  assert(!d.app.menuOpen());
}

static void sleepSettingsDefaultsChoicesAndNavigation() {
  static_assert(uint8_t(MenuPage::TetrisZeroPlayers) == 16, "Existing menu ordinals remain stable");
  static_assert(uint8_t(MenuPage::Sleep) == 17, "Sleep is appended to the menu enum");
  Driver d;
  const uint32_t seconds[] = {30, 60, 120, 300, 600, 0};
  const char* labels[] = {"30 sec", "1 min", "2 min", "5 min", "10 min", "Off"};
  assert(d.app.settings.sleepIndex == 0 && d.app.sleepTimeoutSeconds() == 30);
  assert(d.app.sleepTimeoutMs() == 30000 && !d.app.settingsDirty);
  d.app.setSleep(0);
  assert(!d.app.settingsDirty);
  d.holdMenu();
  assert(d.app.menuItemCount() == 10 && std::string(d.app.menuLabel(6)) == "Sleep");
  assert(std::string(d.app.menuValue(6)) == "30 sec");
  for (unsigned index = 0; index < 6; ++index) d.tapMenu();
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Sleep && std::string(d.app.menuTitle()) == "Sleep");
  assert(d.app.menuItemCount() == AppModel::SleepChoiceCount + 1);
  const uint32_t worldTick = d.app.game.worldTick();
  const uint32_t rounds = d.app.roundsStarted;
  for (uint8_t index = 0; index < AppModel::SleepChoiceCount; ++index) {
    assert(d.app.menu.selection == index && std::string(d.app.menuLabel(index)) == labels[index]);
    const bool changed = d.app.settings.sleepIndex != index;
    d.app.settingsDirty = false;
    d.tick(true);
    assert(d.app.menu.page == MenuPage::Sleep && d.app.settings.sleepIndex == index);
    assert(d.app.sleepTimeoutSeconds() == seconds[index] && d.app.sleepTimeoutMs() == seconds[index] * 1000);
    assert(d.app.settingsDirty == changed);
    for (uint8_t marker = 0; marker < d.app.menuItemCount(); ++marker)
      assert(std::string(d.app.menuValue(marker)) == (marker == index ? "*" : ""));
    d.app.menu.page = MenuPage::Home;
    assert(std::string(d.app.menuValue(6)) == labels[index]);
    d.app.menu.page = MenuPage::Sleep;
    d.app.settingsDirty = false;
    d.tick(true);
    assert(!d.app.settingsDirty);
    d.tapMenu();
  }
  assert(d.app.menu.selection == 6 && std::string(d.app.menuLabel(6)) == "Back");
  d.app.settingsDirty = false;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::Home && !d.app.settingsDirty);
  assert(d.app.game.worldTick() == worldTick && d.app.roundsStarted == rounds);
  assert(d.app.currentGame == GameTitle::FlappyBird);
  d.app.openMenu(MenuPage::Sleep);
  for (unsigned index = 0; index < d.app.menuItemCount(); ++index) d.tapMenu();
  assert(d.app.menu.selection == 0);
  d.holdMenu();
  assert(!d.app.menuOpen());
  d.app.settingsDirty = false;
  d.app.setSleep(UINT8_MAX);
  assert(d.app.settings.sleepIndex == 0 && d.app.settingsDirty);
  assert(d.app.sleepTimeoutMs() == 30000);
  d.app.settingsDirty = false;
  d.app.setSleep(UINT8_MAX);
  assert(!d.app.settingsDirty);
  d.app.settings.sleepIndex = UINT8_MAX;
  assert(d.app.sleepTimeoutMs() == 30000);
  d.app.openMenu(MenuPage::Home);
  assert(std::string(d.app.menuValue(6)) == "30 sec");
  d.app.openMenu(MenuPage::Sleep);
  assert(std::string(d.app.menuValue(0)) == "*" && std::string(d.app.menuValue(6)).empty());
}

static void launcherBootAndImmediateChanges() {
  Driver d;
  assert(d.app.settings.bootGame == GameTitle::FlappyBird && d.app.currentGame == GameTitle::FlappyBird);
  assert(d.app.gameRequest == 0 && !d.app.nesAvailable() && !d.app.tetrisAvailable() && !d.app.tetrisZeroAvailable() && !d.app.geometryDashAvailable());
  d.app.setBestScore(Mode::Original, 9); d.tick(true);
  for (GameTitle title : {GameTitle::Nes, GameTitle::Tetris, GameTitle::TetrisZero, GameTitle::GeometryDash}) {
    assert(!d.app.setBootGame(title) && !d.app.setCurrentGame(title));
    assert(d.app.gameRequest == 0 && !d.app.settingsDirty);
  }
  for (MenuPage page : {MenuPage::BootGame, MenuPage::ChangeGame}) {
    d.app.openMenu(page);
    assert(d.app.menuItemCount() == VisibleGameCount + 1);
    assert(std::string(d.app.menuLabel(0)) == "Flappy Bird");
    assert(std::string(d.app.menuLabel(1)) == "NES: no ROM");
    assert(std::string(d.app.menuLabel(2)) == "Tetris: no ROM");
    assert(std::string(d.app.menuLabel(3)) == "Geometry Dash: no ROM");
    assert(std::string(d.app.menuLabel(4)) == "Galaga");
    assert(std::string(d.app.menuLabel(5)) == "Arkanoid");
    assert(std::string(d.app.menuLabel(6)) == "Bitty Jet");
    assert(std::string(d.app.menuLabel(7)) == "Back");
    assert(std::string(d.app.menuLabel(8)).empty());
    for (uint8_t index : {uint8_t(1), uint8_t(2), uint8_t(3)}) {
      d.app.menu.selection = index; d.tick(true);
      assert(d.app.menu.page == page && d.app.gameRequest == 0 && !d.app.settingsDirty);
    }
  }
  char title[] = "Super Mario Bros.";
  d.app.setNesTitle(title); title[0] = 'X';
  assert(std::string(d.app.nesTitle()) == "Super Mario Bros.");
  d.app.setNesAvailable(true); d.app.setTetrisAvailable(true); d.app.setTetrisZeroAvailable(true); d.app.setGeometryDashAvailable(true);
  d.app.setTetrisTitle("TetrisGYM");
  assert(std::string(d.app.gameLabel(GameTitle::Tetris)) == "Tetris");
  char zeroTitle[] = "Tetris Zero";
  d.app.setTetrisZeroTitle(zeroTitle); zeroTitle[0] = 'X';
  assert(std::string(d.app.gameLabel(GameTitle::TetrisZero)) == "Tetris");
  for (GameTitle titleChoice : {GameTitle::Nes, GameTitle::Tetris, GameTitle::GeometryDash, GameTitle::Galaga, GameTitle::Arkanoid, GameTitle::JetpackJoyride}) {
    d.app.settingsDirty = false;
    assert(d.app.setBootGame(titleChoice));
    assert(d.app.currentGame == GameTitle::FlappyBird && d.app.settingsDirty);
    d.app.settingsDirty = false; assert(d.app.setBootGame(titleChoice)); assert(!d.app.settingsDirty);
    d.app.restoreBootGame();
    assert(d.app.currentGame == titleChoice && !d.app.settingsDirty);
    const uint32_t pausedFrame = d.app.game.stateFrames;
    const float pausedBird = d.app.game.birdY;
    for (unsigned i = 0; i < 100; ++i) assert(d.tick(true) == NoEvent);
    assert(d.app.game.stateFrames == pausedFrame && !d.app.scoreEligible() && !d.app.autoPlaying());
    near(d.app.game.birdY, pausedBird);
    d.holdMenu(); d.tapMenu(); d.tick(true);
    assert(d.app.menu.page == MenuPage::ChangeGame);
    d.tick(true);
    assert(d.app.currentGame == GameTitle::FlappyBird && !d.app.menuOpen());
    assert(d.app.game.state == State::Ready && d.app.game.bestScore == 9);
    assert(d.app.settings.bootGame == titleChoice);
  }
  assert(!d.app.setCurrentGame(GameTitle(UINT8_MAX)) && !d.app.setBootGame(GameTitle(UINT8_MAX)));
  d.app.settings.bootGame = GameTitle::Tetris;
  d.app.setTetrisAvailable(false); d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.tetrisZeroPlayers == 1);
  d.app.setTetrisAvailable(true); d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::Tetris);
  d.app.setTetrisAvailable(false);
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.bootGame == GameTitle::Tetris);
  d.app.settings.bootGame = GameTitle::TetrisZero;
  d.app.setTetrisPlayers(2);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.bootGame == GameTitle::Tetris);
  assert(d.app.settings.tetrisZeroPlayers == 2);
  d.app.setTetrisZeroAvailable(false);
  assert(d.app.currentGame == GameTitle::FlappyBird && d.app.settings.bootGame == GameTitle::Tetris);
  d.app.setNesTitle(nullptr); d.app.setTetrisTitle(nullptr); d.app.setTetrisZeroTitle(nullptr);
  assert(std::string(d.app.nesTitle()) == "NES" && std::string(d.app.tetrisTitle()) == "Tetris");
  assert(std::string(d.app.tetrisZeroTitle()) == "Tetris Zero");
}

static void geometryDashLauncherSettingsAndAvailability() {
  static_assert(uint8_t(GameTitle::FlappyBird) == 0, "Existing game ordinals remain stable");
  static_assert(uint8_t(GameTitle::Nes) == 1, "Existing game ordinals remain stable");
  static_assert(uint8_t(GameTitle::Tetris) == 2, "Existing game ordinals remain stable");
  static_assert(uint8_t(GameTitle::TetrisZero) == 3, "Existing game ordinals remain stable");
  static_assert(uint8_t(GameTitle::GeometryDash) == 4, "Geometry Dash is appended to the game enum");
  assert(GameTitleCount == 8 && VisibleGameCount == 7);
  const GameTitle visible[] = {GameTitle::FlappyBird, GameTitle::Nes, GameTitle::Tetris, GameTitle::GeometryDash, GameTitle::Galaga, GameTitle::Arkanoid, GameTitle::JetpackJoyride};
  for (uint8_t index = 0; index < VisibleGameCount; ++index) {
    assert(visibleGameTitle(index) == visible[index]);
    assert(visibleGameIndex(visible[index]) == index);
  }
  assert(visibleGameIndex(GameTitle::TetrisZero) == 2);
  assert(visibleGameTitle(VisibleGameCount) == GameTitle(UINT8_MAX));
  assert(visibleGameIndex(GameTitle(UINT8_MAX)) == UINT8_MAX);

  Driver d;
  d.app.settings.bootGame = GameTitle::GeometryDash;
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::FlappyBird && !d.app.settingsDirty);
  d.app.setGeometryDashAvailable(true);
  assert(d.app.geometryDashAvailable() && std::string(d.app.gameLabel(GameTitle::GeometryDash)) == "Geometry Dash");
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::GeometryDash && d.app.gameRequest == 1);
  for (MenuPage page : {MenuPage::BootGame, MenuPage::ChangeGame}) {
    d.app.openMenu(page);
    assert(d.app.menuItemCount() == VisibleGameCount + 1 && std::string(d.app.menuLabel(3)) == "Geometry Dash");
    for (uint8_t index = 0; index < d.app.menuItemCount(); ++index)
      assert(std::string(d.app.menuValue(index)) == (index == 3 ? "*" : ""));
    assert(std::string(d.app.menuLabel(VisibleGameCount)) == "Back");
    d.app.menu.selection = VisibleGameCount; d.tick(true);
    assert(d.app.menu.page == MenuPage::Home && d.app.currentGame == GameTitle::GeometryDash);
  }
  d.app.openMenu(MenuPage::GameSettings);
  assert(d.app.menuItemCount() == 3 && std::string(d.app.menuLabel(0)) == "Level");
  assert(std::string(d.app.menuLabel(1)) == "Restart level" && std::string(d.app.menuLabel(2)) == "Back");
  assert(std::string(d.app.menuValue(0)) == "Stereo Madness");
  d.app.menu.selection = 2; d.tick(true);
  assert(d.app.menu.page == MenuPage::Home && d.app.gameRestartRequest == 0);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 1; d.tick(true);
  assert(!d.app.menuOpen() && d.app.geometryRequest == 1 && d.app.gameRestartRequest == 0);
  assert(d.app.currentGame == GameTitle::GeometryDash);

  d.app.setGeometryDashAvailable(false);
  assert(!d.app.geometryDashAvailable() && d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 2);
  assert(d.app.settings.bootGame == GameTitle::GeometryDash && !d.app.settingsDirty);
  assert(std::string(d.app.gameLabel(GameTitle::GeometryDash)) == "Geometry Dash: no ROM");
  d.app.setGeometryDashAvailable(false);
  assert(d.app.gameRequest == 2);
  for (MenuPage page : {MenuPage::BootGame, MenuPage::ChangeGame}) {
    d.app.openMenu(page); d.app.menu.selection = 3; d.tick(true);
    assert(d.app.menu.page == page && d.app.currentGame == GameTitle::FlappyBird);
    assert(d.app.gameRequest == 2 && !d.app.settingsDirty);
  }
  d.app.setGeometryDashAvailable(true);
  d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = 3; d.tick(true);
  assert(!d.app.menuOpen() && d.app.currentGame == GameTitle::GeometryDash && d.app.gameRequest == 3);
  assert(!d.app.tetrisFrontendActive() && !d.app.scoreEligible() && !d.app.autoPlaying());
  d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = 3; d.tick(true);
  assert(!d.app.menuOpen() && d.app.gameRequest == 3);
  d.app.settings.bootGame = GameTitle::FlappyBird;
  d.app.openMenu(MenuPage::BootGame); d.app.menu.selection = 3; d.tick(true);
  assert(d.app.menu.page == MenuPage::BootGame && d.app.settings.bootGame == GameTitle::GeometryDash && d.app.settingsDirty);
  assert(d.app.currentGame == GameTitle::GeometryDash && d.app.gameRequest == 3);
}

static void geometryLevelSelectionAndRestartRequests() {
  static_assert(uint8_t(MenuPage::GeometryLevel) == 18, "Geometry levels are appended to the menu enum");
  const char* names[] = {
    "Stereo Madness", "Back on Track", "Polargeist", "Dry Out", "Base After Base", "Can't Let Go",
    "Jumper", "Time Machine", "Cycles", "xStep", "Clutterfunk", "Theory of Everything",
    "Electroman Adventures", "Clubstep", "Electrodynamix", "Hexagon Force", "Blast Processing",
    "Theory of Everything 2", "Geometrical Dominator", "Deadlocked", "Fingerdash", "Dash"
  };
  Driver d;
  assert(d.app.settings.geometryLevel == 0 && d.app.geometryRequest == 0);
  d.app.setGeometryDashAvailable(true);
  assert(d.app.setCurrentGame(GameTitle::GeometryDash));
  const uint32_t gameRequest = d.app.gameRequest;
  const uint32_t roundsStarted = d.app.roundsStarted;
  for (uint8_t index = 0; index < GeometryLevelCount; ++index) {
    assert(std::string(geometryLevelName(index)) == names[index]);
    d.app.settingsDirty = false;
    d.app.openMenu(MenuPage::GameSettings);
    assert(d.app.menuItemCount() == 3);
    assert(std::string(d.app.menuLabel(0)) == "Level");
    d.tick(true);
    assert(d.app.menu.page == MenuPage::GeometryLevel);
    assert(d.app.menu.selection == d.app.settings.geometryLevel);
    assert(d.app.menuItemCount() == 23 && std::string(d.app.menuTitle()) == "Level");
    assert(std::string(d.app.menuLabel(22)) == "Back" && std::string(d.app.menuLabel(23)).empty());
    for (uint8_t option = 0; option < GeometryLevelCount; ++option) {
      assert(std::string(d.app.menuLabel(option)) == geometryLevelLabel(option));
      assert(std::string(d.app.menuValue(option)) == (option == d.app.settings.geometryLevel ? "*" : ""));
    }
    d.app.menu.selection = index;
    d.tick(true);
    assert(!d.app.menuOpen() && d.app.settings.geometryLevel == index);
    assert(d.app.geometryRequest == unsigned(index + 1));
    assert(d.app.settingsDirty == (index != 0));
    assert(d.app.currentGame == GameTitle::GeometryDash && d.app.gameRequest == gameRequest);
    assert(d.app.gameRestartRequest == 0 && d.app.roundsStarted == roundsStarted);
    assert(d.app.tetrisRequest == 0 && d.app.tetrisZeroRequest == 0);
  }
  assert(std::string(geometryLevelName(22)).empty() && std::string(geometryLevelName(UINT8_MAX)).empty());
  d.app.settingsDirty = false;
  d.app.openMenu(MenuPage::GeometryLevel);
  assert(d.app.menu.selection == 21);
  d.tapMenu();
  assert(d.app.menu.selection == 22);
  d.tick(true);
  assert(d.app.menu.page == MenuPage::GameSettings && d.app.geometryRequest == 22 && !d.app.settingsDirty);
  d.tick(true);
  assert(d.app.menu.page == MenuPage::GeometryLevel && d.app.menu.selection == 21);
  d.app.menu.selection = 21;
  d.tick(true);
  assert(!d.app.menuOpen() && d.app.geometryRequest == 23 && !d.app.settingsDirty);
  d.app.openMenu(MenuPage::GameSettings);
  d.app.menu.selection = 1;
  d.tick(true);
  assert(!d.app.menuOpen() && d.app.geometryRequest == 24 && d.app.gameRestartRequest == 0);
  assert(d.app.settings.geometryLevel == 21 && !d.app.settingsDirty);
  d.app.setGeometryLevel(21);
  assert(!d.app.settingsDirty && d.app.geometryRequest == 24);
  d.app.setGeometryLevel(UINT8_MAX);
  assert(d.app.settings.geometryLevel == 0 && d.app.settingsDirty && d.app.geometryRequest == 24);
  d.app.settingsDirty = false;
  d.app.setGeometryLevel(22);
  assert(d.app.settings.geometryLevel == 0 && !d.app.settingsDirty);
  d.app.geometryRequest = UINT32_MAX;
  d.app.requestGameRestart();
  assert(d.app.geometryRequest == 0 && d.app.gameRestartRequest == 0);
  d.app.openMenu(MenuPage::GeometryLevel);
  d.app.menu.selection = 22;
  d.tapMenu();
  assert(d.app.menu.selection == 0);
}

static void nativeArcadeLauncherAndBoot() {
  static_assert(uint8_t(GameTitle::Galaga) == 5, "Galaga keeps its persisted game ID");
  static_assert(uint8_t(GameTitle::Arkanoid) == 6, "Arkanoid keeps its persisted game ID");
  static_assert(uint8_t(GameTitle::JetpackJoyride) == 7, "Jetpack Joyride is appended to the game enum");
  assert(GameTitleCount == 8 && VisibleGameCount == 7);
  for (GameTitle title : {GameTitle::FlappyBird, GameTitle::Nes, GameTitle::Tetris, GameTitle::TetrisZero,
                          GameTitle::GeometryDash, GameTitle(UINT8_MAX)}) assert(!nativeArcadeGame(title));
  const GameTitle titles[] = {GameTitle::Galaga, GameTitle::Arkanoid, GameTitle::JetpackJoyride};
  const char* names[] = {"Galaga", "Arkanoid", "Bitty Jet"};
  for (uint8_t game = 0; game < 3; ++game) {
    Driver d;
    const GameTitle title = titles[game];
    const uint8_t index = uint8_t(game + 4);
    assert(nativeArcadeGame(title) && visibleGameTitle(index) == title && visibleGameIndex(title) == index);
    assert(!d.app.nesAvailable() && !d.app.tetrisAvailable() && !d.app.tetrisZeroAvailable() && !d.app.geometryDashAvailable());
    assert(std::string(d.app.gameLabel(title)) == names[game]);
    d.app.openMenu(MenuPage::BootGame);
    assert(d.app.menuItemCount() == VisibleGameCount + 1 && std::string(d.app.menuLabel(index)) == names[game]);
    d.app.menu.selection = index; d.tick(true);
    assert(d.app.settings.bootGame == title && d.app.currentGame == GameTitle::FlappyBird);
    assert(d.app.menu.page == MenuPage::BootGame && d.app.settingsDirty && d.app.gameRequest == 0);
    assert(std::string(d.app.menuValue(index)) == "*");
    d.app.settingsDirty = false; d.tick(true);
    assert(!d.app.settingsDirty && d.app.gameRequest == 0);
    d.app.closeMenu(); d.app.restoreBootGame();
    assert(d.app.currentGame == title && d.app.gameRequest == 1 && !d.app.settingsDirty);
    assert(!d.app.tetrisFrontendActive() && !d.app.scoreEligible() && !d.app.autoPlaying() && !d.app.humanControlling());
    const uint32_t worldTick = d.app.game.worldTick(), rounds = d.app.roundsStarted;
    const float y = d.app.game.birdY;
    for (unsigned frame = 0; frame < 100; ++frame) assert(d.tick(true) == NoEvent);
    assert(d.app.game.worldTick() == worldTick && d.app.roundsStarted == rounds);
    near(d.app.game.birdY, y);
    d.holdMenu();
    assert(d.app.menu.page == MenuPage::Home && d.app.menuItemCount() == 10);
    assert(std::string(d.app.menuValue(0)) == names[game] && std::string(d.app.menuValue(1)) == names[game]);
    d.app.menu.selection = 1; d.tick(true);
    assert(d.app.menu.page == MenuPage::ChangeGame && std::string(d.app.menuValue(index)) == "*");
    d.app.menu.selection = index; d.tick(true);
    assert(!d.app.menuOpen() && d.app.currentGame == title && d.app.gameRequest == 1);
    const uint8_t next = uint8_t((game + 1) % 3);
    d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = uint8_t(4 + next); d.tick(true);
    assert(!d.app.menuOpen() && d.app.currentGame == titles[next] && d.app.gameRequest == 2);
    assert(d.app.settings.bootGame == title && !d.app.settingsDirty);
    d.app.setNesAvailable(false); d.app.setTetrisAvailable(false);
    d.app.setTetrisZeroAvailable(false); d.app.setGeometryDashAvailable(false);
    assert(d.app.currentGame == titles[next] && d.app.gameRequest == 2);
    d.app.restoreBootGame();
    assert(d.app.currentGame == title && d.app.gameRequest == 3);
  }
}

static void nativeArcadeSettingsAndRequests() {
  const char* labels[] = {"Restart game", "Zero tilt", "Sensitivity", "Auto fire", "Difficulty", "Back"};
  const uint8_t choices[] = {2, 0, 1};
  const char* sensitivityValues[] = {"Fast", "Fine", "Normal"};
  const char* difficultyValues[] = {"Hard", "Easy", "Normal"};
  for (GameTitle title : {GameTitle::Galaga, GameTitle::Arkanoid}) {
    Driver d;
    assert(d.app.settings.arcadeSensitivity == 1 && !d.app.settings.arcadeAutoFire && d.app.settings.arcadeDifficulty == 1);
    assert(d.app.arcadeCalibrateRequest == 0);
    assert(d.app.setCurrentGame(title));
    const uint32_t gameRequest = d.app.gameRequest, rounds = d.app.roundsStarted;
    d.app.openMenu(MenuPage::GameSettings);
    assert(d.app.menuItemCount() == 6 && std::string(d.app.menuTitle()) == "Game Settings");
    for (uint8_t index = 0; index < 6; ++index) {
      assert(std::string(d.app.menuLabel(index)) == labels[index]);
      assert(MenuFont::textWidth(d.app.menuLabel(index)) <= 128);
      assert(MenuFont::textWidth(d.app.menuValue(index)) <= 128);
    }
    assert(std::string(d.app.menuLabel(6)).empty());
    assert(std::string(d.app.menuValue(0)).empty() && std::string(d.app.menuValue(5)).empty());
    assert(std::string(d.app.menuValue(1)) == "Hold unit steady");
    assert(std::string(d.app.menuValue(2)) == "Normal" && std::string(d.app.menuValue(3)) == "Off");
    assert(std::string(d.app.menuValue(4)) == "Normal");
    for (uint8_t choice = 0; choice < 3; ++choice) {
      d.app.settingsDirty = false; d.app.menu.selection = 2; d.tick(true);
      assert(d.app.settings.arcadeSensitivity == choices[choice] && d.app.settingsDirty);
      assert(std::string(d.app.menuValue(2)) == sensitivityValues[choice]);
      assert(d.app.menu.page == MenuPage::GameSettings && d.app.menu.selection == 2);
      assert(d.app.settings.arcadeDifficulty == 1 && !d.app.settings.arcadeAutoFire);
    }
    for (uint8_t choice = 0; choice < 3; ++choice) {
      d.app.settingsDirty = false; d.app.menu.selection = 4; d.tick(true);
      assert(d.app.settings.arcadeDifficulty == choices[choice] && d.app.settingsDirty);
      assert(std::string(d.app.menuValue(4)) == difficultyValues[choice]);
      assert(d.app.menu.page == MenuPage::GameSettings && d.app.menu.selection == 4);
      assert(d.app.settings.arcadeSensitivity == 1 && !d.app.settings.arcadeAutoFire);
    }
    for (bool enabled : {true, false}) {
      d.app.settingsDirty = false; d.app.menu.selection = 3; d.tick(true);
      assert(d.app.settings.arcadeAutoFire == enabled && d.app.settingsDirty);
      assert(std::string(d.app.menuValue(3)) == (enabled ? "On" : "Off"));
      assert(d.app.menu.page == MenuPage::GameSettings && d.app.menu.selection == 3);
    }
    d.app.settingsDirty = false; d.app.menu.selection = 1;
    d.tick(true); d.tick(true);
    assert(d.app.arcadeCalibrateRequest == 2 && !d.app.settingsDirty);
    assert(d.app.menu.page == MenuPage::GameSettings && d.app.menu.selection == 1);
    d.app.arcadeCalibrateRequest = UINT32_MAX; d.tick(true);
    assert(d.app.arcadeCalibrateRequest == 0 && !d.app.settingsDirty);
    assert(d.app.gameRestartRequest == 0 && d.app.gameRequest == gameRequest && d.app.roundsStarted == rounds);
    assert(d.app.geometryRequest == 0 && d.app.tetrisRequest == 0 && d.app.tetrisZeroRequest == 0 && d.app.tetrisCalibrateRequest == 0);
    d.app.menu.selection = 0; d.tick(true);
    assert(!d.app.menuOpen() && d.app.gameRestartRequest == 1 && d.app.currentGame == title);
    assert(d.app.gameRequest == gameRequest && d.app.roundsStarted == rounds && !d.app.settingsDirty);
    d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 5; d.tick(true);
    assert(d.app.menu.page == MenuPage::Home && d.app.menuItemCount() == 10 && !d.app.settingsDirty);
    d.app.gameRestartRequest = UINT32_MAX; d.app.requestGameRestart();
    assert(!d.app.menuOpen() && d.app.gameRestartRequest == 0 && d.app.geometryRequest == 0);
  }

  Driver d;
  d.app.setArcadeSensitivity(1); d.app.setArcadeAutoFire(false); d.app.setArcadeDifficulty(1);
  assert(!d.app.settingsDirty);
  d.app.setArcadeSensitivity(0); assert(d.app.settings.arcadeSensitivity == 0 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setArcadeSensitivity(UINT8_MAX);
  assert(d.app.settings.arcadeSensitivity == 1 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setArcadeSensitivity(3); assert(!d.app.settingsDirty);
  d.app.setArcadeDifficulty(0); assert(d.app.settings.arcadeDifficulty == 0 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setArcadeDifficulty(UINT8_MAX);
  assert(d.app.settings.arcadeDifficulty == 1 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setArcadeDifficulty(3); assert(!d.app.settingsDirty);
  d.app.setArcadeAutoFire(true); assert(d.app.settings.arcadeAutoFire && d.app.settingsDirty);
  d.app.settingsDirty = false; d.app.setArcadeAutoFire(true); assert(!d.app.settingsDirty);
}

static void jetpackSettingsAndRestart() {
  Driver d;
  assert(d.app.settings.jetpackDifficulty == 1 && !d.app.settings.jetpackPractice);
  assert(d.app.setCurrentGame(GameTitle::JetpackJoyride));
  const uint32_t request = d.app.gameRequest;
  d.app.openMenu(MenuPage::GameSettings);
  const char* labels[] = {"Restart game", "Difficulty", "Missions", "Power-up upgrades", "Practice", "Back"};
  assert(d.app.menuItemCount() == 6);
  for (uint8_t i = 0; i < 6; ++i) assert(std::string(d.app.menuLabel(i)) == labels[i]);
  d.app.menu.selection = 1; d.tick(true);
  assert(d.app.settings.jetpackDifficulty == 2 && d.app.settingsDirty);
  d.app.menu.selection = 4; d.tick(true);
  assert(d.app.menu.page == MenuPage::JetpackPractice && d.app.menuItemCount() == 5);
  d.tick(true); assert(d.app.settings.jetpackPractice);
  d.tick(true); assert(!d.app.settings.jetpackPractice);
  for (uint8_t ride = 1; ride <= 9; ++ride) {
    d.app.menu.selection = 1; d.tick(true);
    assert(d.app.settings.jetpackRide == ride % 9);
    assert(MenuFont::textWidth(d.app.menuValue(1)) <= 128);
  }
  for (uint8_t gadget = 1; gadget <= 7; ++gadget) {
    d.app.menu.selection = 2; d.tick(true);
    assert(d.app.settings.jetpackGadget == gadget % 7);
  }
  d.app.menu.selection = 3; d.tick(true); assert(d.app.settings.jetpackHeadstart);
  d.tick(true); assert(!d.app.settings.jetpackHeadstart);
  d.app.menu.selection = 4; d.tick(true); assert(d.app.menu.page == MenuPage::GameSettings);
  const uint8_t levels[] = {0,1,4,9};
  d.app.setJetpackProgress(100000, levels);
  d.app.menu.selection = 3; d.tick(true);
  assert(d.app.menu.page == MenuPage::JetpackUpgrades && d.app.menuItemCount() == 6);
  assert(std::string(d.app.menuValue(0)) == "100000");
  assert(std::string(d.app.menuValue(1)) == "Lv1 Buy 45000");
  assert(std::string(d.app.menuValue(2)) == "Lv2 Buy 17500");
  assert(std::string(d.app.menuValue(3)) == "Lv5 Max");
  assert(std::string(d.app.menuValue(4)) == "Requires rank 5");
  d.app.setJetpackRank(5, 0, 5);
  assert(std::string(d.app.menuValue(4)) == "Lv10 Max");
  const uint32_t upgrades = d.app.jetpackUpgradeRequest;
  d.tick(true); assert(d.app.jetpackUpgradeRequest == upgrades);
  for (uint8_t i = 1; i < 5; ++i) {
    d.app.menu.selection = i; d.tick(true);
    assert(d.app.jetpackUpgradeKind == i - 1 && d.app.jetpackUpgradeRequest == upgrades + i);
    assert(MenuFont::textWidth(d.app.menuLabel(i)) <= 128 && MenuFont::textWidth(d.app.menuValue(i)) <= 128);
  }
  d.app.menu.selection = 5; d.tick(true);
  d.app.setJetpackMission(0, "Coins: 150 total", 75, 150, false);
  d.app.setJetpackMission(1, "Fly 1000m/run", 1000, 1000, true);
  d.app.setJetpackRank(2, 1, 3);
  d.app.menu.selection = 2; d.tick(true);
  assert(d.app.menu.page == MenuPage::JetpackMissions && d.app.menuItemCount() == 5);
  assert(std::string(d.app.menuLabel(0)) == "Coins: 150 total" && std::string(d.app.menuValue(0)) == "75/150");
  assert(std::string(d.app.menuValue(1)) == "Complete" && std::string(d.app.menuValue(3)) == "Lv2 1/3 stars");
  d.app.setJetpackRank(UINT16_MAX, UINT16_MAX, UINT16_MAX);
  assert(std::string(d.app.menuValue(3)) == "Lv65535 65535/65535 stars");
  d.app.menu.selection = 4; d.tick(true);
  assert(d.app.menu.page == MenuPage::GameSettings);
  d.app.menu.selection = 0; d.tick(true);
  assert(!d.app.menuOpen() && d.app.gameRestartRequest == 1 && d.app.gameRequest == request);
  d.app.setJetpackRide(8); d.app.setJetpackRide(UINT8_MAX);
  d.app.setJetpackGadget(2); d.app.setJetpackGadget(UINT8_MAX);
  assert(d.app.settings.jetpackRide == 0 && d.app.settings.jetpackGadget == 0);
  d.app.setJetpackDifficulty(UINT8_MAX); assert(d.app.settings.jetpackDifficulty == 1);
}

static void jetpackPracticeCompatibility() {
  for (uint8_t bonus = 4; bonus <= 6; ++bonus) {
    AppModel app;
    app.setJetpackGadget(bonus); app.settingsDirty = false;
    app.setJetpackRide(3);
    assert(app.settings.jetpackRide == 3 && app.settings.jetpackGadget == 0 && !app.settings.jetpackHeadstart && app.settingsDirty);
    app.settingsDirty = false; app.setJetpackRide(3); assert(!app.settingsDirty);
    app.setJetpackGadget(bonus);
    assert(app.settings.jetpackRide == 0 && app.settings.jetpackGadget == bonus && !app.settings.jetpackHeadstart && app.settingsDirty);
    app.settingsDirty = false; app.setJetpackGadget(bonus); assert(!app.settingsDirty);
    app.setJetpackHeadstart(true);
    assert(app.settings.jetpackHeadstart && app.settings.jetpackRide == 0 && app.settings.jetpackGadget == 0 && app.settingsDirty);
    app.settingsDirty = false; app.setJetpackHeadstart(true); assert(!app.settingsDirty);
    app.setJetpackGadget(bonus);
    assert(!app.settings.jetpackHeadstart && app.settings.jetpackGadget == bonus && app.settingsDirty);
  }
  for (uint8_t gadget = 0; gadget <= 3; ++gadget) {
    AppModel app;
    app.setJetpackGadget(gadget); app.setJetpackHeadstart(true); app.settingsDirty = false;
    app.setJetpackRide(4);
    assert(app.settings.jetpackRide == 4 && app.settings.jetpackGadget == gadget && !app.settings.jetpackHeadstart && app.settingsDirty);
    app.settingsDirty = false; app.setJetpackGadget(gadget); assert(!app.settingsDirty);
    app.setJetpackHeadstart(true);
    assert(app.settings.jetpackRide == 0 && app.settings.jetpackGadget == gadget && app.settings.jetpackHeadstart && app.settingsDirty);
    app.settingsDirty = false; app.setJetpackRide(0); app.setJetpackGadget(gadget); assert(!app.settingsDirty);
    app.setJetpackHeadstart(false); assert(app.settingsDirty && app.settings.jetpackGadget == gadget);
    app.settingsDirty = false; app.setJetpackHeadstart(false); assert(!app.settingsDirty);
    AppModel rideFirst; rideFirst.setJetpackRide(5); rideFirst.settingsDirty = false;
    rideFirst.setJetpackGadget(gadget);
    assert(rideFirst.settings.jetpackRide == 5 && rideFirst.settings.jetpackGadget == gadget && rideFirst.settingsDirty == (gadget != 0));
    AppModel headstartFirst; headstartFirst.setJetpackHeadstart(true); headstartFirst.settingsDirty = false;
    headstartFirst.setJetpackGadget(gadget);
    assert(headstartFirst.settings.jetpackHeadstart && headstartFirst.settings.jetpackGadget == gadget && headstartFirst.settingsDirty == (gadget != 0));
  }
  // Re-selecting an unchanged value repairs incompatible restored settings.
  for (uint8_t ride = 1; ride <= 8; ++ride) {
    AppModel app;
    app.settings.jetpackRide = ride; app.settings.jetpackGadget = 6; app.settings.jetpackHeadstart = true;
    app.setJetpackRide(ride);
    assert(app.settingsDirty && app.settings.jetpackRide == ride && !app.settings.jetpackGadget && !app.settings.jetpackHeadstart);
  }
  AppModel restored;
  restored.settings.jetpackRide = 2; restored.settings.jetpackGadget = 4; restored.settings.jetpackHeadstart = true;
  restored.setJetpackGadget(4);
  assert(restored.settingsDirty && !restored.settings.jetpackRide && !restored.settings.jetpackHeadstart && restored.settings.jetpackGadget == 4);
  restored.settingsDirty = false; restored.settings.jetpackRide = 2; restored.settings.jetpackGadget = 6; restored.settings.jetpackHeadstart = true;
  restored.setJetpackHeadstart(true);
  assert(restored.settingsDirty && !restored.settings.jetpackRide && !restored.settings.jetpackGadget && restored.settings.jetpackHeadstart);
}

static void controllerMenuRequestsAndStatus() {
  Driver d;
  assert(d.app.controllerRequest == 0 && d.app.controllerAction == 0);
  d.app.openMenu();
  assert(std::string(d.app.menuLabel(5)) == "Controller" && std::string(d.app.menuLabel(6)) == "Sleep" && std::string(d.app.menuLabel(9)) == "Back");
  d.app.setControllerStatus("Searching");
  assert(std::string(d.app.menuValue(5)) == "Searching");
  d.app.menu.selection = 5; d.tick(true);
  assert(d.app.menu.page == MenuPage::Controller && d.app.menuItemCount() == 4);
  for (uint8_t action = 1; action <= 3; ++action) {
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
      const uint32_t before = d.app.controllerRequest;
      d.app.menu.selection = uint8_t(action - 1); d.tick(true);
      assert(d.app.controllerAction == action && d.app.controllerRequest == before + 1);
      assert(d.app.menu.page == MenuPage::Controller && !d.app.settingsDirty);
      assert(d.app.currentGame == GameTitle::FlappyBird);
    }
  }
  d.app.menu.selection = 0;
  assert(std::string(d.app.menuValue(0)) == "Searching");
  d.app.setControllerStatus(nullptr);
  assert(std::string(d.app.menuValue(0)) == "Off");
  d.app.menu.selection = 3; d.tick(true);
  assert(d.app.menu.page == MenuPage::Home);
}

static void perGameSettingsAndTetrisRequests() {
  Driver d;
  d.app.openMenu(MenuPage::GameSettings);
  assert(d.app.menuItemCount() == 4);
  const MenuPage flappyPages[] = {MenuPage::Modes, MenuPage::Skins, MenuPage::Coop};
  for (uint8_t i = 0; i < 3; ++i) {
    d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = i; d.tick(true);
    assert(d.app.menu.page == flappyPages[i]);
  }
  d.app.setNesAvailable(true); d.app.setCurrentGame(GameTitle::Nes);
  d.app.openMenu(MenuPage::GameSettings);
  assert(d.app.menuItemCount() == 2 && std::string(d.app.menuLabel(0)) == "Restart game");
  d.tick(true);
  assert(d.app.gameRestartRequest == 1 && !d.app.menuOpen() && d.app.currentGame == GameTitle::Nes);
  d.app.setTetrisAvailable(true); d.app.setCurrentGame(GameTitle::Tetris);
  d.app.openMenu(MenuPage::GameSettings);
  assert(d.app.menuItemCount() == 9);
  const char* labels[] = {"Players", "Game type", "Starting level", "Controls", "Speed", "Link", "Board size", "Start / restart", "Back"};
  for (uint8_t i = 0; i < 9; ++i) assert(std::string(d.app.menuLabel(i)) == labels[i]);
  d.app.menu.selection = 2;
  d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisLevel && d.app.menuItemCount() == 31);
  d.app.menu.selection = 29; d.tick(true);
  assert(d.app.settings.tetrisLevel == 29 && d.app.settingsDirty);
  d.app.settingsDirty = false; d.tick(true); assert(!d.app.settingsDirty);
  d.app.setTetrisLevel(255); assert(d.app.settings.tetrisLevel == 255);
  d.app.openMenu(MenuPage::TetrisType); d.app.menu.selection = 1; d.tick(true);
  assert(d.app.settings.tetrisGameType == 6);
  d.app.setTetrisGameType(255); assert(d.app.settings.tetrisGameType == 0);
  d.app.openMenu(MenuPage::TetrisControls);
  assert(d.app.settings.tetrisTilt && d.app.settings.tetrisControls == 0);
  assert(d.app.menuItemCount() == 4);
  assert(std::string(d.app.menuLabel(0)) == "Tilt move");
  assert(std::string(d.app.menuLabel(1)) == "Tilt rotate");
  assert(std::string(d.app.menuLabel(2)) == "Zero tilt");
  assert(std::string(d.app.menuLabel(3)) == "Back");
  assert(std::string(d.app.menuValue(0)) == "*");
  d.app.setTetrisTilt(false);
  d.app.menu.selection = 1; d.tick(true); assert(d.app.settings.tetrisControls == 1);
  assert(d.app.settings.tetrisTilt && std::string(d.app.menuValue(1)) == "*");
  d.app.menu.selection = 2; d.tick(true); d.tick(true); assert(d.app.tetrisCalibrateRequest == 2);
  d.app.setTetrisControls(255); assert(d.app.settings.tetrisControls == 0);
  d.app.menu.selection = 3; d.tick(true); assert(d.app.menu.page == MenuPage::GameSettings);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 4; d.tick(true);
  assert(d.app.settings.tetrisPal);
  d.app.openMenu(MenuPage::TetrisLink);
  const char* linkLabels[] = {"Disconnect", "Host game", "Join game", "Back"};
  for (uint8_t i = 0; i < 4; ++i) assert(std::string(d.app.menuLabel(i)) == linkLabels[i]);
  const Mode before = d.app.settings.mode;
  for (uint8_t role = 0; role < 3; ++role) {
    d.app.menu.selection = role; d.tick(true);
    assert(d.app.tetrisLinkRole == role && d.app.tetrisLinkRequest == uint32_t(role + 1));
    assert(d.app.currentGame == GameTitle::Tetris && d.app.settings.mode == before);
  }
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 6;
  assert(std::string(d.app.menuValue(6)) == "Large 11px");
  d.tick(true); assert(!d.app.settings.tetrisLarge && d.app.settingsDirty && d.app.menuOpen());
  assert(std::string(d.app.menuValue(6)) == "Native 8px");
  d.app.menu.selection = 7; d.tick(true);
  assert(d.app.tetrisRequest == 1 && !d.app.menuOpen() && d.app.currentGame == GameTitle::Tetris);
}

static void perGameSettingsAndZeroRequests() {
  Driver d;
  d.app.setTetrisLevel(18);
  d.app.setTetrisGameType(6);
  d.app.setMode(Mode::Arcade);
  d.app.settings.coopRole = 2;
  d.app.setTetrisZeroAvailable(true);
  d.app.setTetrisPlayers(2);
  assert(d.app.setCurrentGame(GameTitle::TetrisZero));
  assert(d.app.settings.tetrisZeroPlayers == 2);
  d.app.openMenu(MenuPage::GameSettings);
  const char* labels[] = {"Players", "Game type", "Starting level", "Controls", "Link", "Board size", "Start / restart", "Back"};
  assert(d.app.menuItemCount() == 8);
  for (uint8_t i = 0; i < 8; ++i) assert(std::string(d.app.menuLabel(i)) == labels[i]);
  assert(std::string(d.app.menuLabel(8)).empty());
  d.app.menu.selection = 1; d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisType);
  assert(std::string(d.app.menuLabel(0)) == "A-Type");
  d.app.menu.selection = 1; d.tick(true);
  assert(d.app.settings.tetrisZeroGameType == 1 && d.app.settings.tetrisGameType == 6);
  d.app.setTetrisZeroGameType(255); assert(d.app.settings.tetrisZeroGameType == 0);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 2; d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisLevel && d.app.menuItemCount() == 11);
  d.app.menu.selection = 7; d.tick(true);
  assert(d.app.settings.tetrisZeroLevel == 7 && d.app.settings.tetrisLevel == 18);
  assert(std::string(d.app.menuValue(7)) == "*");
  d.app.setTetrisZeroLevel(255); assert(d.app.settings.tetrisZeroLevel == 9);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 0; d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisZeroPlayers && d.app.menuItemCount() == 3);
  assert(std::string(d.app.menuValue(1)) == "*");
  assert(std::string(d.app.menuLabel(0)) == "Solo" && std::string(d.app.menuLabel(1)) == "2 players");
  d.app.menu.selection = 0;
  d.tick(true); assert(d.app.settings.tetrisZeroPlayers == 1);
  d.app.settingsDirty = false; d.tick(true); assert(!d.app.settingsDirty);
  d.app.setTetrisZeroPlayers(255); assert(d.app.settings.tetrisZeroPlayers == 2);
  d.app.setTetrisPlayers(2);
  d.app.menu.selection = 2; d.tick(true); assert(d.app.menu.page == MenuPage::GameSettings);
  d.app.menu.selection = 3; d.tick(true); assert(d.app.menu.page == MenuPage::TetrisControls);
  d.app.menu.selection = 1; d.tick(true); assert(d.app.settings.tetrisControls == 1);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 4; d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisLink && d.app.menuItemCount() == 4);
  assert(d.app.tetrisZeroRequest == 0 && d.app.tetrisRequest == 0);
  const char* linkLabels[] = {"Disconnect", "Host game", "Join game", "Back"};
  for (uint8_t i = 0; i < 4; ++i) assert(std::string(d.app.menuLabel(i)) == linkLabels[i]);
  d.app.settingsDirty = false;
  const uint32_t gameRequest = d.app.gameRequest;
  const char* linkValues[] = {"Off", "Host", "Join"};
  for (uint8_t role = 0; role < 3; ++role) {
    d.app.menu.selection = role; d.tick(true);
    assert(d.app.tetrisLinkRole == role && d.app.tetrisLinkRequest == uint32_t(role + 1));
    assert(d.app.currentGame == GameTitle::TetrisZero && d.app.gameRequest == gameRequest);
    assert(d.app.settings.mode == Mode::Arcade && d.app.settings.coopRole == 2 && !d.app.settingsDirty);
    assert(d.app.settings.tetrisLevel == 18 && d.app.settings.tetrisGameType == 6);
    assert(d.app.settings.tetrisZeroLevel == 9 && d.app.settings.tetrisZeroGameType == 0);
    assert(d.app.settings.tetrisZeroPlayers == 2 && d.app.settings.tetrisControls == 1);
    assert(d.app.tetrisZeroRequest == 0 && d.app.tetrisRequest == 0 && d.app.gameRestartRequest == 0);
    d.app.openMenu(MenuPage::GameSettings);
    assert(std::string(d.app.menuValue(4)) == linkValues[role]);
    d.app.menu.selection = 4; d.tick(true);
  }
  d.app.menu.selection = 3; d.tick(true);
  assert(d.app.menu.page == MenuPage::GameSettings && d.app.tetrisLinkRequest == 3);
  d.app.menu.selection = 7; d.tick(true);
  assert(d.app.menu.page == MenuPage::Home && d.app.tetrisLinkRequest == 3);
  d.app.openMenu(MenuPage::GameSettings); d.app.menu.selection = 5; d.tick(true);
  assert(d.app.settings.tetrisLarge && !d.app.settingsDirty && d.app.menuOpen());
  d.app.menu.selection = 6; d.tick(true);
  assert(d.app.tetrisZeroRequest == 1 && d.app.tetrisRequest == 0 && !d.app.menuOpen());
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.mode == Mode::Arcade);
  assert(d.app.tetrisLinkRole == 2 && d.app.tetrisLinkRequest == 3 && !d.app.settingsDirty);
}

static void unifiedTetrisLauncherAndPlayers() {
  Driver d;
  assert(GameTitleCount == 8 && VisibleGameCount == 7);
  assert(d.app.settings.tetrisPlayers == 1 && d.app.selectedTetrisEngine() == GameTitle::Tetris);
  d.app.openTetrisSettings();
  assert(!d.app.menuOpen() && !d.app.tetrisFrontendActive());
  d.app.setTetrisAvailable(true); d.app.setTetrisZeroAvailable(true);
  d.app.setTetrisLevel(24); d.app.setTetrisGameType(6);
  d.app.setTetrisZeroLevel(7); d.app.setTetrisZeroGameType(1);
  d.app.settingsDirty = false;
  d.app.setTetrisPlayers(2);
  assert(d.app.settingsDirty && d.app.currentGame == GameTitle::FlappyBird && d.app.gameRequest == 0);
  assert(d.app.selectedTetrisEngine() == GameTitle::TetrisZero);
  assert(d.app.setBootGame(GameTitle::Tetris) && d.app.currentGame == GameTitle::FlappyBird);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.tetrisZeroPlayers == 2);
  assert(d.app.settings.bootGame == GameTitle::Tetris && d.app.tetrisFrontendActive());
  d.app.openMenu(MenuPage::BootGame);
  assert(d.app.menuItemCount() == VisibleGameCount + 1 && std::string(d.app.menuLabel(2)) == "Tetris");
  assert(std::string(d.app.menuLabel(3)) == "Geometry Dash: no ROM" && std::string(d.app.menuLabel(VisibleGameCount)) == "Back");
  assert(std::string(d.app.menuValue(2)) == "*" && std::string(d.app.menuValue(3)).empty());
  d.app.openMenu(MenuPage::ChangeGame);
  assert(std::string(d.app.menuValue(2)) == "*");
  d.app.menu.selection = 2; d.tick(true);
  assert(d.app.currentGame == GameTitle::TetrisZero && !d.app.menuOpen());
  const uint32_t before = d.app.gameRequest;
  d.app.openTetrisSettings();
  assert(d.app.menu.page == MenuPage::GameSettings && std::string(d.app.menuValue(0)) == "2 players");
  d.tick(true);
  assert(d.app.menu.page == MenuPage::TetrisZeroPlayers);
  d.app.menu.selection = 0; d.tick(true);
  assert(d.app.currentGame == GameTitle::Tetris && d.app.settings.tetrisPlayers == 1);
  assert(d.app.gameRequest == before + 1 && d.app.menu.page == MenuPage::TetrisZeroPlayers);
  assert(d.app.settings.tetrisLevel == 24 && d.app.settings.tetrisGameType == 6);
  assert(d.app.settings.tetrisZeroLevel == 7 && d.app.settings.tetrisZeroGameType == 1);
  d.app.openTetrisSettings();
  assert(d.app.menuItemCount() == 9 && std::string(d.app.menuValue(0)) == "Solo");
  assert(std::string(d.app.menuValue(1)) == "B-Type" && std::string(d.app.menuValue(2)) == "24");
  d.app.openMenu(MenuPage::TetrisLevel); assert(d.app.menuItemCount() == 31);
  d.app.openMenu(MenuPage::TetrisZeroPlayers); d.app.menu.selection = 1; d.tick(true);
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.gameRequest == before + 2);
  d.app.openTetrisSettings();
  assert(d.app.menuItemCount() == 8 && std::string(d.app.menuValue(2)) == "7");
  assert(std::string(d.app.menuLabel(4)) == "Link");
  d.app.openMenu(MenuPage::TetrisLevel); assert(d.app.menuItemCount() == 11);
  d.app.openMenu(MenuPage::TetrisZeroPlayers); d.app.menu.selection = 1;
  d.app.settingsDirty = false; d.tick(true);
  assert(!d.app.settingsDirty && d.app.gameRequest == before + 2);
  assert(d.app.tetrisRequest == 0 && d.app.tetrisZeroRequest == 0 && d.app.gameRestartRequest == 0);
  d.app.settings.bootGame = GameTitle::TetrisZero;
  d.app.setTetrisPlayers(1); d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::Tetris && d.app.settings.bootGame == GameTitle::Tetris);
  d.app.settings.bootGame = GameTitle::TetrisZero;
  d.app.setTetrisPlayers(2); d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.bootGame == GameTitle::Tetris);
}

static void unifiedTetrisMissingImagesAndSoloFallback() {
  Driver d;
  d.app.setTetrisAvailable(true);
  assert(d.app.activateTetris() && d.app.currentGame == GameTitle::Tetris);
  d.app.openMenu(MenuPage::TetrisZeroPlayers); d.app.menu.selection = 1;
  assert(std::string(d.app.menuLabel(1)) == "2 players: no ROM");
  const uint32_t before = d.app.gameRequest;
  d.app.settingsDirty = false; d.tick(true);
  assert(d.app.currentGame == GameTitle::Tetris && d.app.gameRequest == before && !d.app.settingsDirty);
  d.app.setTetrisPlayers(2);
  assert(d.app.selectedTetrisEngine() == GameTitle::Tetris);
  assert(d.app.activateTetris() && d.app.settings.tetrisPlayers == 1);
  d.app.setTetrisZeroAvailable(true); d.app.setTetrisAvailable(false);
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.tetrisZeroPlayers == 1);
  const uint32_t fallbackRequest = d.app.gameRequest;
  d.app.openMenu(MenuPage::TetrisZeroPlayers); d.app.menu.selection = 1; d.tick(true);
  assert(d.app.currentGame == GameTitle::TetrisZero && d.app.settings.tetrisZeroPlayers == 2);
  assert(d.app.gameRequest == fallbackRequest + 1 && d.app.settings.tetrisPlayers == 2);
  d.app.menu.selection = 0; d.tick(true);
  assert(d.app.settings.tetrisPlayers == 1 && d.app.settings.tetrisZeroPlayers == 1);
  assert(d.app.gameRequest == fallbackRequest + 2);
  d.app.setTetrisPlayers(UINT8_MAX);
  assert(d.app.settings.tetrisPlayers == 1);
  assert(d.app.setBootGame(GameTitle::TetrisZero) && d.app.settings.bootGame == GameTitle::Tetris);
  d.app.setTetrisZeroAvailable(false);
  assert(d.app.currentGame == GameTitle::FlappyBird && !d.app.activateTetris());
  assert(std::string(d.app.gameLabel(GameTitle::TetrisZero)) == "Tetris: no ROM");
  assert(!d.app.setBootGame(GameTitle::Tetris));
  d.app.restoreBootGame(); assert(d.app.currentGame == GameTitle::FlappyBird);
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

static void cartridgeCatalogSelectionAndBoot() {
  Driver d;
  char title[] = "First cartridge";
  d.app.addCartridgeChoice(8, title); title[0] = 'X';
  d.app.addCartridgeChoice(27, "Second cartridge");
  d.app.setNesAvailable(true);
  assert(d.app.gameChoiceCount() == VisibleGameCount + 1);
  assert(std::string(d.app.gameChoiceLabel(1)) == "First cartridge");
  assert(d.app.gameChoiceCartridge(1) == 8);
  assert(d.app.gameChoiceCartridge(VisibleGameCount) == 27);
  assert(!d.app.gameChoiceAvailable(d.app.gameChoiceCount()));
  d.app.openMenu(MenuPage::ChangeGame);
  d.app.menu.selection = VisibleGameCount;
  d.tick(true);
  assert(!d.app.menuOpen() && d.app.currentGame == GameTitle::Nes && d.app.currentCartridge == 27);
  const uint32_t request = d.app.gameRequest;
  d.app.openMenu(MenuPage::ChangeGame);
  d.app.menu.selection = 1;
  d.tick(true);
  assert(!d.app.menuOpen() && d.app.currentCartridge == 8 && d.app.gameRequest > request);
  d.app.openMenu(MenuPage::BootGame);
  d.app.menu.selection = VisibleGameCount;
  d.tick(true);
  assert(d.app.settings.bootGame == GameTitle::Nes && d.app.settings.bootCartridge == 27);
  assert(d.app.currentCartridge == 8);
  assert(std::string(d.app.menuValue(VisibleGameCount)) == "*");
  d.app.clearCartridgeChoices();
  d.app.setNesAvailable(false);
  assert(d.app.currentGame == GameTitle::FlappyBird && !d.app.chooseCartridge(27));
  assert(d.app.gameChoiceCount() == VisibleGameCount);
  d.app.restoreBootGame();
  assert(d.app.currentGame == GameTitle::FlappyBird);
  for (uint8_t id = 1; id < 20; ++id) d.app.addCartridgeChoice(id, "Stored cartridge");
  assert(d.app.gameChoiceCount() == VisibleGameCount + 11);
  assert(d.app.gameChoiceCartridge(d.app.gameChoiceCount() - 1) == 12);
}

static void sharedClockAndHomeRequests() {
  Driver d;
  d.app.setNesAvailable(true);
  d.app.setTetrisAvailable(true);
  d.app.setTetrisZeroAvailable(true);
  d.app.setGeometryDashAvailable(true);
  d.app.setClock(13, 42, d.now);
  assert(d.app.homeRequest == 0);
  for (uint8_t index = 0; index < VisibleGameCount; ++index) {
    const GameTitle title = visibleGameTitle(index);
    assert(d.app.setCurrentGame(title));
    d.app.openMenu();
    assert(d.app.menuItemCount() == 10);
    assert(std::string(d.app.menuLabel(7)) == "Set clock");
    assert(std::string(d.app.menuValue(7)) == "13:42");
    assert(std::string(d.app.menuLabel(8)) == "Game home");
    assert(std::string(d.app.menuValue(8)) == d.app.gameLabel(title));
    d.app.menu.selection = 7; d.tick(true);
    assert(d.app.menu.page == MenuPage::Clock && std::string(d.app.menuTitle()) == "Set clock");
    assert(std::string(d.app.menuValue(0)) == "13" && std::string(d.app.menuValue(1)) == "42");
    d.app.menu.selection = 3; d.tick(true);
    assert(d.app.menu.page == MenuPage::Home && d.app.currentGame == title);

    const uint32_t home = d.app.homeRequest, game = d.app.gameRequest, restart = d.app.gameRestartRequest;
    d.app.settingsDirty = false;
    d.app.menu.selection = 8; d.tick(true);
    assert(!d.app.menuOpen() && d.app.homeRequest == home + 1);
    assert(d.app.currentGame == title && d.app.gameRequest == game && d.app.gameRestartRequest == restart);
    assert(!d.app.settingsDirty);

    // Selecting the current game still requests its clock home.
    d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = index; d.tick(true);
    assert(!d.app.menuOpen() && d.app.currentGame == title && d.app.homeRequest == home + 2);
    assert(d.app.gameRequest == game);
    const uint8_t next = uint8_t((index + 1u) % VisibleGameCount);
    d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = next; d.tick(true);
    assert(!d.app.menuOpen() && d.app.currentGame == visibleGameTitle(next) && d.app.homeRequest == home + 3);
    assert(d.app.gameRequest == game + 1);
    d.app.openMenu(); d.app.menu.selection = 9; d.tick(true);
    assert(!d.app.menuOpen() && d.app.homeRequest == home + 3);
  }
  d.app.setNesAvailable(false);
  const uint32_t home = d.app.homeRequest;
  d.app.openMenu(MenuPage::ChangeGame); d.app.menu.selection = 1; d.tick(true);
  assert(d.app.menu.page == MenuPage::ChangeGame && d.app.homeRequest == home);

  d.app.setNesAvailable(true); d.app.setNesTitle("Mario");
  assert(d.app.setCurrentGame(GameTitle::Nes));
  d.app.openMenu(MenuPage::GameSettings);
  assert(d.app.menuItemCount() == 2 && std::string(d.app.menuLabel(0)) == "Restart game");
  const uint32_t restart = d.app.gameRestartRequest;
  d.tick(true);
  assert(!d.app.menuOpen() && d.app.gameRestartRequest == restart + 1 && d.app.homeRequest == home);

  d.app.homeRequest = UINT32_MAX;
  d.app.openMenu(); d.app.menu.selection = 8; d.tick(true);
  assert(!d.app.menuOpen() && d.app.homeRequest == 0);
}

int main() {
  sharedClockAndHomeRequests();
  cartridgeCatalogSelectionAndBoot();
  menuEdgesAndPause();
  everyMenuHasBack();
  volumeSettingsAndMenuNavigation();
  sleepSettingsDefaultsChoicesAndNavigation();
  launcherBootAndImmediateChanges();
  geometryDashLauncherSettingsAndAvailability();
  geometryLevelSelectionAndRestartRequests();
  nativeArcadeLauncherAndBoot();
  nativeArcadeSettingsAndRequests();
  jetpackSettingsAndRestart();
  jetpackPracticeCompatibility();
  controllerMenuRequestsAndStatus();
  perGameSettingsAndTetrisRequests();
  perGameSettingsAndZeroRequests();
  unifiedTetrisLauncherAndPlayers();
  unifiedTetrisMissingImagesAndSoloFallback();
  clockAndCoopMenus();
  scoresStayWithTheirMode();
  takeoverAndResume();
  demosRepeatAndVary();
  bigCollisionAndPlayability();
  arcadeProgression();
  sharedWorldMap();
  std::cout << "App, menu, automatic play, clock, arcade, big mode and shared map checks passed.\n";
}
