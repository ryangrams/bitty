#pragma once
#ifdef M5FLAP_USB_LIBRARY
#include <stdlib.h>
#include <stdarg.h>
#include "library_protocol.h"

flappy::LibraryFlash libraryFlash;
flappy::UsbCartridges* cartridgeStorage = nullptr;
#define cartridges (*cartridgeStorage)
bool libraryActive = false;
uint32_t libraryLastInput = 0;
uint32_t libraryRequestId = 0;
uint16_t libraryQuarantine = 0;

void libraryReply(const char* text) { Serial.printf("TP1 %lu\t%s\n", static_cast<unsigned long>(libraryRequestId), text); }
void libraryPrintf(const char* format, ...) {
  char buffer[160]; va_list args; va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
  const size_t length = strlen(buffer);
  if (length && buffer[length - 1] == '\n') buffer[length - 1] = 0;
  libraryReply(buffer);
}

void refreshCartridgeViews() {
  if (!cartridgeStorage) return;
  FlappyRom::data = TetrisRom::data = TetrisZeroRom::data = GeometryRom::data = nullptr;
  FlappyRom::length = TetrisRom::length = TetrisZeroRom::length = GeometryRom::length = 0;
  app.clearCartridgeChoices();
  const flappy::UsbCartridges::Record* selected = nullptr;
  for (unsigned i = 0; i < flappy::UsbCartridges::MaxRecords; ++i) {
    const auto& record = cartridges.at(i);
    if (!record.id || (libraryQuarantine & (1u << i))) continue;
    const uint8_t* data = libraryFlash.view(record.offset, record.length);
    if (!data) continue;
    if (record.kind == 1) {
      app.addCartridgeChoice(record.id, record.title);
      if (!selected || record.id == app.currentCartridge) selected = &record;
    } else if (record.kind == 2 && !TetrisRom::data) {
      TetrisRom::data = data; TetrisRom::length = record.length; TetrisRom::title = record.title;
    } else if (record.kind == 3 && !TetrisZeroRom::data) {
      TetrisZeroRom::data = data; TetrisZeroRom::length = record.length; TetrisZeroRom::title = record.title;
    } else if (record.kind == 4 && !GeometryRom::data) {
      GeometryRom::data = data; GeometryRom::length = record.length; GeometryRom::title = record.title;
    }
  }
  if (selected) {
    app.currentCartridge = selected->id;
    FlappyRom::data = libraryFlash.view(selected->offset, selected->length);
    FlappyRom::length = selected->length; FlappyRom::title = selected->title;
  }
  marioCartridge.configure(FlappyRom::data, FlappyRom::length);
  app.setNesAvailable(FlappyRom::length > 0);
  app.setNesTitle(marioCartridge.supported() ? "Mario" : FlappyRom::title);
  app.setGeometryDashAvailable(GeometryRom::length > 0);
  app.setTetrisAvailable(TetrisRom::length > 0);
  app.setTetrisTitle(TetrisRom::title);
  app.setTetrisZeroAvailable(TetrisZeroRom::length > 0);
  app.setTetrisZeroTitle(TetrisZeroRom::title);
}

void initializeLibrary() {
  cartridgeStorage = new (std::nothrow) flappy::UsbCartridges(libraryFlash);
  if (!cartridgeStorage) return;
  const bool ready = libraryFlash.begin() && cartridges.mount();
  if (ready) {
    // Verify payloads once at boot before exposing mapped data to an emulator.
    for (unsigned i = 0; i < flappy::UsbCartridges::MaxRecords; ++i) {
      const auto& record = cartridges.at(i);
      if (record.id && !cartridges.verify(record)) {
        Serial.printf("TP1 WARN\tCRC\t%u\n", record.id);
        libraryQuarantine |= uint16_t(1u << i);
      }
    }
    app.currentCartridge = app.settings.bootCartridge;
    refreshCartridgeViews();
  }
}

void libraryText(const char* text, int y) {
  for (int i = 0; text[i]; ++i) {
    const uint8_t* rows = flappy::MenuFont::glyph(text[i]);
    for (int row = 0; row < 12; ++row) for (int x = 0; x < 6; ++x)
      if (rows[row] & (1u << (5 - x))) M5.Display.drawPixel(6 + i * 6 + x, y + row, TFT_WHITE);
  }
}

void enterLibrary() {
  if (libraryActive) return;
  asyncDisplay.drain(); stopGameAudio(); nes.unload(); releaseNativeArcade();
  if (coopLink.session().role() != flappy::coop::Role::Off) stopCoop();
  gameHome.enter(); homeClocks.release(); clearPendingInput();
  delete geometryPresentation; geometryPresentation = nullptr;
  saveSettings();
  if (bluetoothPad) bluetoothPad->stop();
  controllerConnected = false; padButtons = previousPadButtons = 0;
  renderer->setBackgroundCacheEnabled(false);
  setNesDisplay(false);
  M5.Display.fillScreen(TFT_BLACK);
  libraryText("BITTY USB LIBRARY", 18);
  libraryText("Manage in your", 52); libraryText("browser.", 66);
  libraryText("Files stay local.", 102);
  libraryText("Hold right to exit", 150);
  libraryText("or disconnect.", 164);
  libraryActive = true;
}

void leaveLibrary() {
  if (cartridgeStorage) {
    cartridges.abort();
    if (!cartridges.ready()) cartridges.mount();
  }
  refreshCartridgeViews();
  libraryActive = false;
  handledGameRequest = UINT32_MAX;
  handleGameRequest();
  invalidateDisplay(); showFrame(millis());
  if (bluetoothPad) bluetoothPad->resumePeer(millis());
  nextTick = micros() + TickMicros; nextNesTick = micros();
}

bool libraryNumber(const char* text, uint32_t& value, int base = 10) {
  return flappy::libraryUnsigned(text, value, base);
}

bool validateLibraryRom(const flappy::UsbCartridges::Record& record) {
  const auto* data = libraryFlash.view(record.offset, record.length);
  if (record.kind == 2 && !flappy::RomFingerprint::tetris(data, record.length, false)) return false;
  if (record.kind == 3 && !flappy::RomFingerprint::tetris(data, record.length, true)) return false;
  if (record.kind == 4 && !flappy::RomFingerprint::famidash(data, record.length)) return false;
  const bool valid = nes.load(data, record.length) && nes.frameRate() == 60;
  nes.unload();
  return valid;
}

void libraryCommand(char* line) {
  char* fields[9] = {}; unsigned count = 0;
  char* context = nullptr;
  for (char* part = strtok_r(line, "\t", &context); part; part = strtok_r(nullptr, "\t", &context)) {
    if (count == 9) { libraryReply("ERROR\tFIELDS"); return; }
    fields[count++] = part;
  }
  if (count < 3 || strcmp(fields[0], "TP1") || !libraryNumber(fields[1], libraryRequestId) || !libraryRequestId) return;
  for (unsigned i = 1; i + 1 < count; ++i) fields[i] = fields[i + 1];
  --count;
  if (!strcmp(fields[1], "HELLO") && count == 2) {
    enterLibrary();
    libraryPrintf("HELLO\t1\tM5StickCPlus\t%lu\t%u\n", static_cast<unsigned long>(libraryFlash.size()), unsigned(cartridgeStorage && cartridges.ready()));
    return;
  }
  if (!libraryActive) { libraryReply("ERROR\tHELLO_REQUIRED"); return; }
  if (!strcmp(fields[1], "EXIT") && count == 2) { libraryReply("OK\tEXIT"); Serial.flush(); leaveLibrary(); return; }
  if (!strcmp(fields[1], "PING") && count == 2) { libraryReply("OK\tPING"); return; }
  if (!cartridgeStorage || !cartridges.ready()) { libraryReply("ERROR\tSTORE_UNAVAILABLE"); return; }
  if (!strcmp(fields[1], "LIST") && count == 2) {
    for (unsigned i = 0; i < flappy::UsbCartridges::MaxRecords; ++i) {
      const auto& r = cartridges.at(i);
      if (r.id) libraryPrintf("ITEM\t%u\t%u\t%lu\t%08lx\t%s\t%u\n", r.id, r.kind,
          static_cast<unsigned long>(r.length), static_cast<unsigned long>(r.crc), r.title, (libraryQuarantine & (1u << i)) ? 1 : 0);
    }
    libraryPrintf("OK\t%lu\t%lu\t%lu\n", static_cast<unsigned long>(cartridges.capacity()),
        static_cast<unsigned long>(cartridges.used()), static_cast<unsigned long>(cartridges.largestFree())); return;
  }
  if (!strcmp(fields[1], "ABORT") && count == 2) { cartridges.abort(); libraryReply("OK\tABORT"); return; }
  uint32_t id = 0, kind = 0, length = 0, crc = 0;
  if (!strcmp(fields[1], "BEGIN") && count == 7 && libraryNumber(fields[2], id) && id <= 255 &&
      libraryNumber(fields[3], kind) && kind <= 4 && libraryNumber(fields[4], length) &&
      libraryNumber(fields[5], crc, 16)) {
    // Specialized frontends each use one payload. Generic NES slots are plural.
    bool duplicate = false;
    for (unsigned i = 0; i < flappy::UsbCartridges::MaxRecords; ++i)
      if (kind != 1 && cartridges.at(i).id && cartridges.at(i).id != id && cartridges.at(i).kind == kind) duplicate = true;
    const bool ok = !duplicate && cartridges.begin(uint8_t(id), uint8_t(kind), fields[6], length, crc);
    libraryReply(ok ? "OK\tBEGIN" : "ERROR\tNO_SPACE_OR_BAD_PACKAGE"); return;
  }
  if (!strcmp(fields[1], "CHUNK") && count == 5 && libraryNumber(fields[2], length) && libraryNumber(fields[3], crc, 16)) {
    const size_t hexLength = strlen(fields[4]);
    if (!hexLength || hexLength % 2 || hexLength > flappy::UsbCartridges::Chunk * 2u) { libraryReply("ERROR\tCHUNK"); return; }
    uint8_t bytes[flappy::UsbCartridges::Chunk];
    for (unsigned i = 0; i < hexLength / 2; ++i) {
      char pair[3] = {fields[4][i * 2], fields[4][i * 2 + 1], 0}; uint32_t value = 0;
      if (!libraryNumber(pair, value, 16)) { libraryReply("ERROR\tHEX"); return; }
      bytes[i] = uint8_t(value);
    }
    if (cartridges.chunk(length, bytes, hexLength / 2, crc)) libraryPrintf("OK\t%lu\n", static_cast<unsigned long>(cartridges.received()));
    else libraryReply("ERROR\tCHUNK");
    return;
  }
  if (!strcmp(fields[1], "COMMIT") && count == 2) {
    const auto* incoming = cartridges.incoming();
    if (!incoming || cartridges.received() != incoming->length || !cartridges.verify(*incoming) || !validateLibraryRom(*incoming)) {
      libraryReply("ERROR\tROM_OR_CRC"); return;
    }
    const bool ok = cartridges.commit();
    if (ok) {
      for (unsigned i = 0; i < flappy::UsbCartridges::MaxRecords; ++i)
        if (cartridges.at(i).id == incoming->id) libraryQuarantine &= uint16_t(~(1u << i));
      refreshCartridgeViews();
    }
    libraryReply(ok ? "OK\tCOMMIT" : "ERROR\tCOMMIT"); return;
  }
  if (count == 3 && libraryNumber(fields[2], id) && id <= 255) {
    const auto* record = cartridges.find(uint8_t(id));
    if (!strcmp(fields[1], "REMOVE")) {
      const bool ok = cartridges.remove(uint8_t(id)); if (ok) refreshCartridgeViews();
      libraryReply(ok ? "OK\tREMOVE" : "ERROR\tREMOVE"); return;
    }
    if ((!strcmp(fields[1], "LAUNCH") || !strcmp(fields[1], "BOOT")) && (record || id == 0)) {
      for (unsigned i = 0; record && i < flappy::UsbCartridges::MaxRecords; ++i)
        if (cartridges.at(i).id == id && (libraryQuarantine & (1u << i))) {
          libraryReply("ERROR\tPAYLOAD_CORRUPT"); return;
        }
      const auto title = !record ? flappy::GameTitle::FlappyBird : record->kind == 2 ? flappy::GameTitle::Tetris :
          record->kind == 3 ? flappy::GameTitle::TetrisZero : record->kind == 4 ? flappy::GameTitle::GeometryDash : flappy::GameTitle::Nes;
      if (title == flappy::GameTitle::TetrisZero) { app.setTetrisPlayers(2); app.setTetrisZeroPlayers(2); }
      else if (title == flappy::GameTitle::Tetris) app.setTetrisPlayers(1);
      if (!strcmp(fields[1], "BOOT")) {
        app.setBootGame(title); app.settings.bootCartridge = record && record->kind == 1 ? uint8_t(id) : 0;
        app.settingsDirty = true; saveSettings(); libraryReply("OK\tBOOT");
      } else {
        if (record && record->kind == 1) app.chooseCartridge(uint8_t(id));
        app.setCurrentGame(title); app.closeMenu();
        libraryReply("OK\tLAUNCH"); Serial.flush(); leaveLibrary();
      }
      return;
    }
  }
  libraryReply("ERROR\tCOMMAND");
}

bool serviceLibrary() {
  static char* line = nullptr; static unsigned length = 0;
  static bool overflow = false; static uint32_t heldSince = 0;
  if (!line && Serial.available()) line = new (std::nothrow) char[768];
  if (!line) return false;
  while (Serial.available()) {
    if (!length && !overflow && !libraryActive && Serial.peek() != 'T') {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
      break;
#else
      Serial.read(); continue;
#endif
    }
    const char c = char(Serial.read()); libraryLastInput = millis();
    if (c == '\n') {
      line[length] = 0;
      if (overflow) Serial.println("TP1 ERROR\tLINE_TOO_LONG");
      else libraryCommand(line);
      length = 0; overflow = false;
    } else if (c != '\r') {
      if (uint8_t(c) < 32 && c != '\t') overflow = true;
      else if (length + 1 < 768 && !overflow) line[length++] = c;
      else overflow = true;
    }
  }
  if ((length || overflow) && uint32_t(millis() - libraryLastInput) > 2000) { length = 0; overflow = false; }
  if (libraryActive) {
    M5.update();
    if (M5.BtnB.isPressed()) { if (!heldSince) heldSince = millis(); }
    else heldSince = 0;
    if (uint32_t(millis() - libraryLastInput) > 30000 || (heldSince && uint32_t(millis() - heldSince) > 800)) {
      Serial.println("TP1 BYE"); leaveLibrary(); heldSince = 0;
    }
    delay(1);
  }
  return libraryActive || length || overflow;
}
#endif
