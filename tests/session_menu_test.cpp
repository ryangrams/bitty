#include "../firmware/flappy/app_model.h"
#include <assert.h>
#include <string>
#include <iostream>
using namespace flappy;
int main() {
  AppModel app;
  app.setNesAvailable(true);app.setTetrisAvailable(true);app.setTetrisZeroAvailable(true);app.setGeometryDashAvailable(true);
  app.addCartridgeChoice(4,"First");app.addCartridgeChoice(7,"Second");
  for(unsigned i=0;i<VisibleGameCount;++i) {
    assert(app.setCurrentGame(visibleGameTitle(uint8_t(i))));
    app.rememberCurrentGame();app.settingsDirty=false;
    const auto stored=app.settings;
    AppModel reboot;reboot.setNesAvailable(true);reboot.setTetrisAvailable(true);reboot.setGeometryDashAvailable(true);
    reboot.settings=stored;reboot.restoreBootGame();
    assert(reboot.currentGame==app.currentGame);
    app.rememberCurrentGame();assert(!app.settingsDirty);
  }
  app.setCurrentGame(GameTitle::Nes);assert(app.chooseCartridge(7));app.rememberCurrentGame();
  assert(app.settings.bootGame==GameTitle::Nes && app.settings.bootCartridge==7);
  app.openMenu();assert(app.menuItemCount()==9 && std::string(app.menuLabel(0))=="Change Game");
  for(unsigned i=0;i<app.menuItemCount();++i)assert(std::string(app.menuLabel(uint8_t(i)))!="Game at Boot");
  app.setCurrentGame(GameTitle::HockeyQueen);app.openMenu(MenuPage::GameSettings);
  assert(app.settings.hockeySolo && app.settings.hockeyPlayer==0 && app.settings.hockeyCpu==1);
  assert(app.menuItemCount()==10 && std::string(app.menuValue(2))=="Medium");
  for(unsigned i=0;i<4;++i) {app.menu.selection=2;app.step(true,false,100+i*20,false);assert(app.settings.hockeyCpu==(i+2)%4);}
  app.menu.selection=1;app.step(true,false,200,false);assert(app.settings.hockeyPlayer==1);
  app.menu.selection=0;app.step(true,false,230,false);assert(!app.settings.hockeySolo);
  app.menu.selection=3;app.step(true,false,250,false);assert(app.menu.page==MenuPage::HockeyLink);
  std::cout<<"Session menu: last game and cartridge, stable saves, compact home and solo hockey choices passed\n";
}
