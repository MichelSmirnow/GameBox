// ---------------------------------------------------------------------------
//  GameBox :: app.h — приложение: загрузка, меню, селектор игр, настройки
// ---------------------------------------------------------------------------
#pragma once
#include "canvas.h"
#include "input.h"
#include "host.h"
#include "storage.h"

class Game;

enum Screen : uint8_t {
  SCR_BOOT = 0,
  SCR_MENU,
  SCR_GAMES,      // карусель игр
  SCR_SETTINGS,
  SCR_GAME,
  SCR_CONFIRM
};

class App {
 public:
  void begin();
  void tick();
  void inject(Ev e) { in.inject(e); }
  void bootPreview(Canvas& c, uint32_t phase);
  void openGameDebug(uint8_t idx) { openGame(idx); }   // используется симулятором
  const Canvas& frame() const { return cv; }
  uint8_t debugPhase() const { return host.game() ? host.game()->debugPhase() : 0; }
  Screen screen() const { return scr; }

 private:
  Canvas cv;
  Input in;
  Host host;
  Screen scr = SCR_BOOT;

  uint32_t lastTick = 0;
  uint32_t t0 = 0;

  // ---- меню ----
  uint8_t menuIdx = 0;
  int8_t menuSlide = 0;        // анимация выбора

  // ---- карусель игр ----
  uint8_t gameIdx = 0;
  int8_t selFrom = 0, selDir = 0;
  uint32_t selStart = 0;

  // ---- настройки ----
  uint8_t setIdx = 0;
  uint8_t setFirst = 0;        // первая видимая строка
  uint8_t setPage = 0;         // 0 = список, 1 = текст, 2 = список игр
  uint8_t setGame = 0;
  int16_t textScroll = 0;
  const char* textTitle = nullptr;
  const char* textBody = nullptr;

  // ---- подтверждение ----
  Screen confirmBack = SCR_MENU;
  const char* confirmText = nullptr;
  uint8_t confirmAction = 0;

  // ---------------- отрисовка ----------------
  void drawBoot(uint32_t phase);
  void drawMenu();
  void drawGames();
  void drawSettings();
  void drawGame();
  void drawConfirm();

  // ---------------- логика ----------------
  void handleEvent(Ev e);
  void handleMenu(Ev e);
  void handleGames(Ev e);
  void handleSettings(Ev e);
  void handleConfirm(Ev e);

  void openGame(uint8_t idx);
  void closeGame();
  void startReboot();
  void eraseAll();

  uint32_t now() const;
  void pushFrame();
};
