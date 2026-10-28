#include "../firmware/flappy/app_model.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <new>
#include <string>

flappy::AppModel app;
static uint32_t now = 1000, writes = 0;
uint32_t millis() { return now; }
void writeClock(uint32_t value) { assert(value == now); ++writes; }
struct SerialPort {
  std::string input, output;
  int available() const { return int(input.size()); }
  int peek() const { return input.empty() ? -1 : input[0]; }
  int read() { if (input.empty()) return -1; const char c = input[0]; input.erase(0,1); return c; }
  void println(const char* text) { output += text; output += '\n'; }
  void printf(const char* format, ...) {
    char text[1024]; va_list args; va_start(args,format);
    vsnprintf(text,sizeof(text),format,args); va_end(args); output += text;
  }
} Serial;
#include "../firmware/flappy/library_service.h"

static void send(const char* command) { Serial.input += command; assert(!serviceLibrary()); }
int main() {
  send("TP1\t1\tTIME\t2028\t2\t29\t23\t59\t58\n");
  assert(writes == 1 && app.clockHour(now) == 23 && app.clockMinute(now) == 59 && app.clockSecond(now) == 58);
  assert(app.clockDate(now).year == 2028 && app.clockDate(now).day == 29);
  assert(Serial.output.find("TP1 1\tOK\tTIME\t2028\t2\t29\t23\t59\t58\n") != std::string::npos);
  const char* bad[] = {
    "TP1\t2\tTIME\t2027\t2\t29\t12\t0\t0\n",
    "TP1\t3\tTIME\t2028\t2\t29\t24\t0\t0\n",
    "TP1\t4\tTIME\t67564\t2\t29\t12\t0\t0\n",
    "TP1\t5\tTIME\t2028\t2\t29\t12\t60\t0\n",
    "TP1\t6\tTIME\t2028\t2\t29\t12\t0\t60\n",
    "TP1\t7\tTIME\t2028\t0\t0\t12\t0\t0\n",
    "TP1\t8\tTIME\t2028\t2\t29\t12\t0\n"
  };
  for (auto line : bad) { send(line); assert(writes == 1); assert(app.clockHour(now) == 23); }
  // A partial message stays bounded and cannot merge with a later request.
  Serial.input = "TP1\t9\tTI"; assert(serviceLibrary()); now += 2001; assert(!serviceLibrary());
  send("TP1\t10\tTIME\t2026\t10\t28\t8\t4\t3\r\n");
  assert(writes == 2 && app.clockHour(now) == 8 && app.clockDate(now).year == 2026);
  assert(!libraryActive && !app.menuOpen());
  app.powerOffAvailable = true; app.openMenu(flappy::MenuPage::Sleep);
  assert(app.menuItemCount() == flappy::AppModel::SleepChoiceCount + 2);
  assert(!strcmp(app.menuLabel(flappy::AppModel::SleepChoiceCount),"Power off"));
  app.menu.selection = flappy::AppModel::SleepChoiceCount;
  app.step(true,false,now,false); assert(app.powerOffRequest == 1);
  puts("USB clock: production handler, local date, invalid fields, leap days and partial-line timeout passed");
}
