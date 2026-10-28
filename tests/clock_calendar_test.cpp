#include "../firmware/flappy/app_model.h"
#include "../firmware/flappy/clock_overlay.h"
#include <algorithm>
#include <array>
#include <assert.h>
#include <iostream>
#include <string>
#include <vector>
#include <time.h>

using namespace flappy;

static ClockDate date(unsigned year, unsigned month, unsigned day) {
  ClockDate result; result.year = uint16_t(year); result.month = uint8_t(month); result.day = uint8_t(day);
  return result;
}

static void calendarBoundaries() {
  assert(ClockCalendar::valid(date(2024,2,29)));
  assert(!ClockCalendar::valid(date(2025,2,29)));
  assert(!ClockCalendar::valid(date(2100,2,29)));
  assert(ClockCalendar::valid(date(2400,2,29)));
  assert(!ClockCalendar::valid(date(2026,4,31)));
  assert(!ClockCalendar::valid(date(2026,0,1)));
  assert(!ClockCalendar::valid(date(2026,13,1)));
  assert(!ClockCalendar::valid(date(2026,1,0)));
  assert(ClockCalendar::addDays(date(2024,2,28),1) == date(2024,2,29));
  assert(ClockCalendar::addDays(date(2024,2,28),2) == date(2024,3,1));
  assert(ClockCalendar::addDays(date(2025,2,28),1) == date(2025,3,1));
  assert(ClockCalendar::addDays(date(2026,12,31),1) == date(2027,1,1));
  ClockDate current = date(2000,1,1);
  for (unsigned day = 0; day < 36525; ++day) {
    assert(ClockCalendar::valid(current));
    uint32_t encoded = 0;
    assert(ClockCalendar::clockSeconds(current,23,59,58,encoded));
    const time_t value = encoded;
    struct tm civil = {}; assert(gmtime_r(&value,&civil));
    assert(civil.tm_year + 1900 == current.year && civil.tm_mon + 1 == current.month && civil.tm_mday == current.day);
    assert(civil.tm_hour == 23 && civil.tm_min == 59 && civil.tm_sec == 58);
    assert(ClockCalendar::weekDay(current) == (day + 6) % 7);
    current = ClockCalendar::addDays(current,1);
  }
  assert(current == date(2100,1,1));
  uint32_t seconds = 0;
  assert(ClockCalendar::clockSeconds(date(2000,1,1),0,0,0,seconds) && seconds == 946684800u);
  assert(ClockCalendar::clockSeconds(date(2099,12,31),23,59,59,seconds) && seconds == 4102444799u);
  assert(!ClockCalendar::clockSeconds(date(2100,1,1),0,0,0,seconds));
  assert(!ClockCalendar::clockSeconds(date(2026,2,29),0,0,0,seconds));
  assert(!ClockCalendar::clockSeconds(date(2026,1,1),24,0,0,seconds));
  char label[20];
  ClockCalendar::format(label,sizeof(label),date(2026,10,28));
  assert(std::string(label) == "WED OCT 28 2026");
  ClockCalendar::format(label,sizeof(label),date(2026,10,28),true);
  assert(std::string(label) == "WED 10/28");
}

static void midnightAndUptimeWrap() {
  AppModel app;
  app.settingsDirty = false;
  const uint32_t anchor = UINT32_MAX - 1500u;
  assert(app.synchronizeClock(date(2024,2,28),23,59,59,anchor));
  assert(app.clockDate(anchor + 999u) == date(2024,2,28));
  assert(app.clockDate(anchor + 1000u) == date(2024,2,29));
  assert(app.clockHour(anchor + 1000u) == 0 && app.clockMinute(anchor + 1000u) == 0);
  assert(app.clockSecond(anchor + 1000u) == 0);
  app.advanceClock(anchor + 1250u);
  app.advanceClock(anchor + 1750u);
  assert(app.clockSecond(anchor + 1999u) == 0);
  assert(app.clockSecond(anchor + 2000u) == 1);
  assert(!app.settingsDirty && app.clockWriteRequest == 0);
  assert(!app.synchronizeClock(date(2026,2,30),1,0,0,0));
  assert(!app.synchronizeClock(date(2026,2,28),24,0,0,0));
  assert(!app.synchronizeClock(date(2026,2,28),23,60,0,0));
  assert(!app.synchronizeClock(date(2026,2,28),23,59,60,0));
  assert(app.synchronizeClock(date(2026,12,31),23,59,59,1000));
  app.setClock(4,12,2000);
  assert(app.clockDate(2000) == date(2027,1,1));
  assert(app.setClockDate(date(2024,2,29),3250));
  assert(app.clockDate(3250) == date(2024,2,29) && app.clockSecond(4000) == 2);
  assert(!app.setClockDate(date(2025,2,29),4000));

  uint32_t now = 47;
  assert(app.synchronizeClock(date(2024,1,1),0,0,0,now));
  for (unsigned day = 1; day <= 100; ++day) {
    now += 86400000u;
    app.advanceClock(now);
    assert(app.clockDate(now) == ClockCalendar::addDays(date(2024,1,1),day));
    assert(app.clockHour(now) == 0 && app.clockMinute(now) == 0);
  }
}

static void select(AppModel& app, MenuPage page, uint8_t index, uint32_t now = 1000) {
  app.openMenu(page); app.menu.selection = index; app.step(true,false,now,false);
}

static void menuAndFormat() {
  AppModel app;
  assert(app.settings.clockDatePosition == ClockDatePosition::None);
  app.synchronizeClock(date(2024,2,29),23,59,12,1000);
  app.openMenu(MenuPage::Clock);
  assert(app.menuItemCount() == 6);
  assert(std::string(app.menuLabel(2)) == "Time format");
  assert(std::string(app.menuValue(2)) == "24-hour");
  select(app,MenuPage::Clock,2);
  assert(std::string(app.menuValue(2)) == "12-hour" && app.clockWriteRequest == 0);
  for (const auto position : {ClockDatePosition::Top,ClockDatePosition::Bottom,ClockDatePosition::None}) {
    select(app,MenuPage::Clock,3);
    assert(app.settings.clockDatePosition == position && app.settingsDirty);
  }
  app.setClockDatePosition(255);
  assert(app.settings.clockDatePosition == ClockDatePosition::None);
  select(app,MenuPage::Clock,4);
  assert(app.menu.page == MenuPage::ClockDate && app.menuItemCount() == 4);
  select(app,MenuPage::ClockDate,0);
  assert(app.clockDate(1000) == date(2025,2,28));
  select(app,MenuPage::ClockDate,2);
  assert(app.clockDate(1000) == date(2025,2,1));
  select(app,MenuPage::ClockDate,1);
  assert(app.clockDate(1000) == date(2025,3,1));
  select(app,MenuPage::ClockDate,3);
  assert(app.menu.page == MenuPage::Clock);
  select(app,MenuPage::Clock,5);
  assert(app.menu.page == MenuPage::Home && app.clockWriteRequest == 3);
  ClockTime time;
  for (unsigned h = 0; h < 24; ++h) {
    time.hour = uint8_t(h); time.twentyFourHour = true;
    assert(time.displayHour() == h);
    time.twentyFourHour = false;
    assert(time.displayHour() == (h % 12 ? h % 12 : 12));
    assert(time.digit(0) * 10u + time.digit(1) == time.displayHour());
  }
}

static void overlayBounds() {
  ClockTime time; time.date = date(2026,10,28);
  for (const auto size : {std::array<int,2>{{135,240}}, std::array<int,2>{{80,160}}}) {
    const int width = size[0], height = size[1];
    const uint16_t empty = 0x1234;
    for (const auto position : {ClockDatePosition::None,ClockDatePosition::Top,ClockDatePosition::Bottom}) {
      std::vector<uint16_t> pixels(size_t(width * height) + 2,empty);
      ClockOverlay::draw(pixels.data()+1,width,height,time,position);
      assert(pixels.front() == empty && pixels.back() == empty);
      const int top = position == ClockDatePosition::Top ? 0 : height - 15;
      unsigned changed = 0, text = 0;
      for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto pixel = pixels[size_t(y * width + x) + 1];
        const bool band = position != ClockDatePosition::None && y >= top && y < top + 15;
        assert(band || pixel == empty);
        changed += pixel != empty; text += pixel == 0xce59;
      }
      assert(position == ClockDatePosition::None ? changed == 0 : changed == unsigned(width * 15) && text > 100);
    }
  }
  uint16_t guard = 0x9876;
  ClockOverlay::draw(&guard,1,1,time,ClockDatePosition::Bottom);
  ClockOverlay::draw(&guard,1,1,time,ClockDatePosition(255));
  assert(guard == 0x9876);
}

int main() {
  calendarBoundaries(); midnightAndUptimeWrap(); menuAndFormat(); overlayBounds();
  std::cout << "Clock calendar: 100-year weekdays, leap dates, midnight, uptime wrap, 12/24-hour menu and native date overlays passed\n";
}
