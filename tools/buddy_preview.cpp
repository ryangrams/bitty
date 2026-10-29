#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "../firmware/flappy/buddy_renderer.h"
#include "../firmware/flappy/buddy_clock.h"

using flappy::BuddyGame;
using flappy::BuddyRenderer;

static bool ppm(const char* path, const uint16_t* pixels) {
  FILE* output = path ? fopen(path,"wb") : stdout;
  if (!output) { perror(path); return false; }
  fprintf(output,"P6\n135 240\n255\n");
  for (unsigned i = 0; i < 135 * 240; ++i) {
    const uint16_t color = pixels[i];
    const uint8_t rgb[] = {uint8_t(((color >> 11) & 31) * 255 / 31),
      uint8_t(((color >> 5) & 63) * 255 / 63),uint8_t((color & 31) * 255 / 31)};
    if (fwrite(rgb,1,3,output) != 3) { if (path) fclose(output); return false; }
  }
  return path ? fclose(output) == 0 : fflush(output) == 0;
}

static void preview(uint16_t* pixels,unsigned theme,uint32_t frame,const char* kind) {
  if (!strcmp(kind,"clock")) {
    flappy::BuddyClock clock; clock.reset(theme * 7);
    flappy::ClockTime time; time.hour = 10; time.minute = 28; time.second = uint8_t(frame / 60 % 60);
    for (uint32_t i = 0; i < frame; ++i) clock.step(time);
    clock.draw(pixels,time); return;
  }
  BuddyRenderer::Scene scene;
  BuddyRenderer::Object objects[20];
  scene.theme = BuddyGame::Theme(theme % 5); scene.frame = frame;
  scene.phaseTicks = frame;
  scene.score = 1280 + frame * 2; scene.coins = 42; scene.levelTicks = frame * 8 + 8300;
  scene.mask = 1; scene.level = theme;
  const unsigned cycle = frame % 480;
  scene.lane = cycle < 120 ? 1 : cycle < 160 ? 1 + (cycle - 120) / 40.f :
    cycle < 240 ? 2 : cycle < 320 ? 2 - (cycle - 240) / 40.f : cycle < 400 ? 0 : (cycle - 400) / 80.f;
  const unsigned jump = frame % 200;
  scene.jump = jump >= 40 && jump <= 100 ? (jump - 40) * (100 - jump) / 900.f : 0;
  scene.rolling = jump >= 135 && jump < 177;
  scene.board = frame % 900 > 480; scene.snake = frame % 900 > 700 ? 10 : 0;
  if (!strcmp(kind,"crash")) scene.phase = BuddyGame::Phase::Crashed;
  if (!strcmp(kind,"celebrate")) scene.phase = BuddyGame::Phase::Celebrate;
  if (!strcmp(kind,"roll")) scene.rolling = true;
  if (!strcmp(kind,"jump")) { scene.rolling = false; scene.jump = .85f; }
  if (!strcmp(kind,"flight")) { scene.lift = 100; scene.magnet = true; scene.section = BuddyGame::Section::CoinRush; }
  if (!strcmp(kind,"board")) { scene.board = true; scene.snake = 10; }
  auto add = [&](BuddyGame::ObjectKind type,float lane,float depth,unsigned variant) {
    const float shift = float(frame % 480) / 480;
    float moving = depth + shift;
    while (moving > 1.12f) moving -= 1.05f;
    objects[scene.count++] = {type,lane,moving,uint16_t(variant)};
  };
  add(BuddyGame::ObjectKind::Person,0,.81f,1);
  add(BuddyGame::ObjectKind::Straddle,2,.95f,2);
  add(BuddyGame::ObjectKind::LowObstacle,1,.55f,1);
  add(BuddyGame::ObjectKind::Person,2,.32f,0);
  add(BuddyGame::ObjectKind::LowObstacle,0,.44f,0);
  add(BuddyGame::ObjectKind::Hoverboard,0,.26f,0);
  add(BuddyGame::ObjectKind::Coin,1,.91f,0);
  add(BuddyGame::ObjectKind::Coin,1,.82f,7);
  add(BuddyGame::ObjectKind::Coin,1,.72f,14);
  add(BuddyGame::ObjectKind::Coin,0,.65f,21);
  add(BuddyGame::ObjectKind::Coin,0,.58f,28);
  add(BuddyGame::ObjectKind::Coin,2,.51f,35);
  add(BuddyGame::ObjectKind::Electronics,2,.68f,0);
  scene.objects = objects;
  BuddyRenderer::drawScene(pixels,scene);
}

static void poses(uint16_t* pixels,int individual = -1) {
  const uint16_t bg = BuddyRenderer::rgb(247,225,192), ink = BuddyRenderer::rgb(56,37,58);
  for (unsigned i = 0; i < 135 * 240; ++i) pixels[i] = bg;
  static const char* names[] = {"TINY FEET","HAND PUSH","AIR BUDDY","ROLL OUT","BUDDY BOOGIE","OOPS"};
  BuddyRenderer::drawText(pixels,"LITTLE BUDDY",20,9,ink,2);
  if (individual >= 0) {
    BuddyRenderer::drawBuddy(pixels,67,166,BuddyRenderer::Pose(individual),14,0,1,3);
    BuddyRenderer::drawText(pixels,names[individual],67 - int(strlen(names[individual])) * 4,191,ink,2);
    BuddyRenderer::drawText(pixels,"EXACT GAME PIXELS X3",30,217,ink);
  } else {
    for (int i = 0; i < 6; ++i) {
      const int x = i % 2 ? 101 : 34, y = 77 + i / 2 * 68;
      BuddyRenderer::drawBuddy(pixels,x,y,BuddyRenderer::Pose(i),14,0,1,1);
      BuddyRenderer::drawText(pixels,names[i],x - int(strlen(names[i])) * 2,y + 9,ink);
    }
    BuddyRenderer::drawText(pixels,"NO LEGS. LOTS OF NERVE.",24,230,ink);
  }
}

int main(int argc,char** argv) {
  const char* out = nullptr; const char* scene = "game";
  unsigned theme = 0, frames = 0; uint32_t frame = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i],"--out") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i],"--scene") && i + 1 < argc) scene = argv[++i];
    else if (!strcmp(argv[i],"--theme") && i + 1 < argc) theme = unsigned(strtoul(argv[++i],nullptr,10)) % 5;
    else if (!strcmp(argv[i],"--frame") && i + 1 < argc) frame = uint32_t(strtoul(argv[++i],nullptr,10));
    else if (!strcmp(argv[i],"--frames") && i + 1 < argc) frames = unsigned(strtoul(argv[++i],nullptr,10));
    else { fprintf(stderr,"Usage: buddy_preview [--out DIR] [--scene game|clock|poses|roll|jump|board|crash|celebrate] [--theme 0..4] [--frame N] [--frames N]\n"); return 2; }
  }
  if (frames > 3600 || (frames && !out)) { fprintf(stderr,"Sequence output needs --out and accepts up to 3600 frames.\n"); return 2; }
  if (out && mkdir(out,0755) != 0 && errno != EEXIST) { perror(out); return 1; }
  std::vector<uint16_t> pixels(135 * 240);
  auto save = [&](const std::string& name) { return ppm(out ? (std::string(out) + "/" + name + ".ppm").c_str() : nullptr,pixels.data()); };
  if (frames) {
    for (unsigned i = 0; i < frames; ++i) {
      preview(pixels.data(),theme,frame + i * 2,scene);
      char name[32]; snprintf(name,sizeof(name),"frame-%04u",i);
      if (!save(name)) return 1;
    }
  } else if (out && !strcmp(scene,"game")) {
    static const char* names[] = {"mall","school","park","volcano","noodles"};
    for (unsigned i = 0; i < 5; ++i) { preview(pixels.data(),i,frame,scene); if (!save(names[i])) return 1; }
    preview(pixels.data(),theme,frame,"clock"); if (!save("clock")) return 1;
    poses(pixels.data()); if (!save("poses")) return 1;
    static const char* poseNames[] = {"run","push","jump","roll","dance","crash"};
    for (int i = 0; i < 6; ++i) { poses(pixels.data(),i); if (!save(std::string("pose-") + poseNames[i])) return 1; }
  } else {
    if (!strcmp(scene,"poses")) poses(pixels.data()); else preview(pixels.data(),theme,frame,scene);
    if (!save(scene)) return 1;
  }
  return 0;
}
