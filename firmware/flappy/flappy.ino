#include <M5Unified.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include "game_core.h"
#include "render.h"
#include "generated_sounds.h"
#include "generated_collision.h"
#include "stick_power.h"

flappy::Game game;
flappy::Viewport viewport;
Preferences scores;
constexpr int StripRows = 16;
alignas(4) uint16_t canvasStorage[flappy::PixelWidth * flappy::PixelHeight];
alignas(4) uint16_t stripStorage[flappy::PixelWidth * StripRows];
uint16_t* canvas = canvasStorage;
uint16_t* screen = stripStorage;
flappy::Renderer* renderer = nullptr;
uint32_t nextTick = 0;
uint32_t lastReport = 0;
uint32_t frames = 0;
uint32_t lastFrameCount = 0;
float measuredFps = 0;
bool pendingFlap = false;
uint16_t savedBest = 0;

void showFrame() {
  renderer->draw(game);
  M5.Display.startWrite();
  for (int y = 0; y < viewport.height; y += StripRows) {
    const int rows = viewport.height - y < StripRows ? viewport.height - y : StripRows;
    viewport.copyRows(canvas, screen, y, rows);
    M5.Display.pushImage(viewport.outputX, viewport.outputY + y, viewport.width, rows, screen);
  }
  M5.Display.endWrite();
  ++frames;
}

void status() {
  Serial.printf("STATUS {\"board\":%u,\"width\":%d,\"height\":%d,\"native\":%s,\"cropX\":%d,\"cropY\":%d,\"state\":%u,\"y\":%.2f,\"vy\":%.2f,\"score\":%u,\"best\":%u,\"fps\":%.1f,\"heap\":%u,\"readable\":%s,\"speaker\":%s,\"pipes\":[",
    (unsigned)M5.getBoard(), M5.Display.width(), M5.Display.height(),
    viewport.native ? "true" : "false", viewport.sourceX, viewport.sourceY,
    (unsigned)game.state, game.birdY, game.velocityY, game.score, game.bestScore,
    measuredFps, ESP.getFreeHeap(), M5.Display.isReadable() ? "true" : "false",
    M5.Speaker.isEnabled() ? "true" : "false");
  for (int i = 0; i < flappy::PipeCount; ++i) {
    Serial.printf("%s[%.0f,%d]", i ? "," : "", game.pipes[i].x, game.pipes[i].gapY);
  }
  Serial.println("]}");
}

void playSound(const FlappySounds::Sound& sound) {
  if (sound.length && sound.samples && M5.Speaker.isEnabled()) {
    M5.Speaker.playRaw(sound.samples, sound.length, sound.sampleRate, false, 1, 0, true);
  }
}

void events(uint8_t flags) {
  if (flags & flappy::Hit) playSound(FlappySounds::hit);
  else if (flags & flappy::Die) playSound(FlappySounds::die);
  else if (flags & flappy::Point) playSound(FlappySounds::point);
  else if (flags & flappy::Flap) playSound(FlappySounds::wing);
  if (flags & flappy::GameOver) {
    if (game.bestScore > savedBest) {
      savedBest = game.bestScore;
      scores.putUShort("best", savedBest);
    }
  }
}

void dumpFrame(bool panel) {
  if (panel && !M5.Display.isReadable()) {
    Serial.println("ERROR panel readback unavailable");
    return;
  }
  const int width = panel ? M5.Display.width() : viewport.width;
  const int height = panel ? M5.Display.height() : viewport.height;
  Serial.printf("FRAME %d %d %s\n", width, height, panel ? "panel" : "buffer");
  if (panel) {
    lgfx::rgb565_t row[320];
    for (int y = 0; y < height; ++y) {
      M5.Display.readRect(0, y, width, 1, row);
      Serial.write(reinterpret_cast<const uint8_t*>(row), width * sizeof(uint16_t));
    }
  } else {
    for (int y = 0; y < height; y += StripRows) {
      const int rows = height - y < StripRows ? height - y : StripRows;
      viewport.copyRows(canvas, screen, y, rows);
      Serial.write(reinterpret_cast<const uint8_t*>(screen), width * rows * sizeof(uint16_t));
    }
  }
  Serial.flush();
  Serial.println("\nEND_FRAME");
  nextTick = micros() + 16667;
  lastReport = millis();
  lastFrameCount = frames;
}

void setup() {
  Serial.setTxBufferSize(2048);
  prepareStickDisplayPower();
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = true;
  cfg.external_display_value = 0;
  cfg.output_power = false;
  M5.begin(cfg);
  rtc_gpio_pullup_dis(GPIO_NUM_2);
  rtc_gpio_pulldown_en(GPIO_NUM_2);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(180);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setSwapBytes(true);
  M5.Speaker.setVolume(80);
  viewport.configure(M5.Display.width(), M5.Display.height());
  status();
  if (viewport.width <= 0 || viewport.height <= 0) {
    Serial.println("ERROR display detection failed");
    return;
  }
  renderer = new flappy::Renderer(canvas);
  scores.begin("flappy", false);
  savedBest = scores.getUShort("best", 0);
  game.reset(esp_random());
  game.setBirdMasks(FlappyCollision::birdMasks);
  game.setBestScore(savedBest);
  showFrame();
  nextTick = micros() + 16667;
  lastReport = millis();
  status();
}

void loop() {
  M5.update();
  if (!renderer) {
    while (Serial.available()) if (Serial.read() == 's') status();
    delay(10);
    return;
  }
  pendingFlap |= M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed();
  if (M5.Touch.isEnabled()) pendingFlap |= M5.Touch.getDetail().wasPressed();
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'f') pendingFlap = true;
    else if (command == 's') status();
    else if (command == 'r') {
      game.reset(2);
      pendingFlap = false;
      showFrame();
    } else if (command == 'd' || command == 'c') dumpFrame(command == 'c');
  }
  const uint32_t now = micros();
  if (int32_t(now - nextTick) >= 0) {
    int catchUp = 0;
    do {
      events(game.step(pendingFlap));
      pendingFlap = false;
      nextTick += 16667;
      ++catchUp;
    } while (int32_t(micros() - nextTick) >= 0 && catchUp < 4);
    if (catchUp == 4) nextTick = micros() + 16667;
    showFrame();
  } else {
    delay(1);
  }
  const uint32_t elapsed = millis() - lastReport;
  if (elapsed >= 3000) {
    measuredFps = (frames - lastFrameCount) * 1000.0f / elapsed;
    lastFrameCount = frames;
    lastReport = millis();
    status();
  }
}
