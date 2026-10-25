// ---------------------------------------------------------------------------
//  GameBox :: app.cpp
// ---------------------------------------------------------------------------
#include "app.h"
#include "config.h"
#include "oled.h"
#include "util.h"
#include "ui.h"
#include "../games/games.h"
#include "../gfx/gfx.h"
#include <string.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
#define GB_MILLIS() millis()
#else
#define GB_MILLIS() gbMillis()
#endif

static Store appStore(NVS_NAMESPACE);

// --------------------------- тексты справки --------------------------------
static const char* kHelpButtons =
    "ВЫБОР — подтвердить\nдействие, забросить,\nвыстрелить, крутить.\n\n"
    "НАЗАД — отмена, шаг\nназад, выход из игры\nв селектор игр.\n\n"
    "СБРОС — сброс сцены или\nпрогресса (игра сама\nспросит подтверждение\nтам, где это важно).\n\n"
    "МЕНЮ — пауза, настройки\nтекущей игры, справка\nпо управлению.\n\n"
    "СТИК (кнопка на\nджойстике) — быстрый\nдоступ: табло в рыбалке,\nпредмет в Buckshot,\nавтобой в SPTS.";

static const char* kHelpStick =
    "ДЖОЙСТИК:\n"
    "влево/вправо — основные\nстраницы (табы);\n"
    "вверх/вниз — вглубь\nстраницы.\n\n"
    "Одно отклонение — одно\nпереключение. Чтобы\nперелистнуть ещё раз,\nверните стик в центр\nили отведите в другую\nсторону.\n\n"
    "Слабое отклонение\n(меньше порога ~1100 мВ)\nне считается нажатием,\nпоэтому случайные\nкасания страницы\nне листают.";

static const char* kHelpAbout =
    "GameBox — портативная\nприставка на ESP32-C3\nSuper Mini.\n\n"
    "Экран: OLED 128x64,\nI2C, адрес 0x3C.\n"
    "SDA — пин 8, SCL — пин 9.\n\n"
    "Джойстик: X — пин 0,\nY — пин 1, кнопка — 3.\n"
    "Кнопки: выбор — 4,\nназад — 5, сброс — 6,\nменю — 7.\n\n"
    "Сохранения хранятся во\nвстроенной flash (NVS),\nвеб-подключения нет.\n\n"
    "Прошивка: версия 1.0\nГрафика: собственный\n1-битный рендерер + UTF-8\nшрифты с кириллицей.";

static const char* kHelpSettings[] = {
    "Кнопки", "Джойстик", "Управление в играх",
    "Калибровка", "О приставке", "Сброс данных"};
static const char* kMenuItems[] = {"ИГРЫ", "НАСТРОЙКИ", "ПЕРЕЗАГРУЗКА"};

// ------------------------------- иконки меню --------------------------------
static void iconGamepad(Canvas& c, int x, int y) {
  c.roundRect(x, y + 2, 14, 9, 3, 1);
  c.px(x + 4, y + 6, 1); c.px(x + 3, y + 5, 1); c.px(x + 3, y + 7, 1); c.px(x + 2, y + 6, 1);
  c.px(x + 10, y + 5, 1); c.px(x + 11, y + 6, 1);
}
static void iconGear(Canvas& c, int x, int y) {
  c.circle(x + 7, y + 7, 5, 1);
  c.circle(x + 7, y + 7, 2, 1);
  c.px(x + 7, y, 1); c.px(x + 7, y + 13, 1); c.px(x, y + 7, 1); c.px(x + 13, y + 7, 1);
}
static void iconPower(Canvas& c, int x, int y) {
  c.circle(x + 7, y + 8, 5, 1);
  c.vline(x + 7, y, 6, 0);
  for (int i = 0; i < 5; i++) { c.px(x + 7, y + i, 1); c.px(x + 6, y + i, 0); }
}

// =============================== ЗАПУСК =====================================
void App::bootPreview(Canvas& c, uint32_t phase) { drawBoot(phase); blit(c, cv, 0, 0); }

void App::begin() {
  // 1) дисплей — первым делом, чтобы сразу показать логотип
  oled.begin();
  cv.clear();
  drawBoot(0);
  oled.flush(cv, true);

  gbRandomSeed(GB_MILLIS() ^ 0x9E3779B9u);

  // 2) анимированная загрузка: логотип + вращающийся полукруг
  uint32_t phase = 0;
  for (int step = 0; step < 26; step++) {
    if (step == 6) in.begin();                 // калибровка джойстика
    if (step == 12) appStore.begin();          // открываем NVS
    if (step == 18) {                          // загрузка параметров
      registerGames();
    }
    drawBoot(phase);
    oled.flush(cv, false);
    phase++;
#ifdef ARDUINO
    delay(24);
#endif
  }

  // 3) восстанавливаем последнюю вкладку меню
  uint8_t last = appStore.getU8("menu", 0);
  menuIdx = last < 3 ? last : 0;

  scr = SCR_MENU;
  lastTick = GB_MILLIS();
  t0 = lastTick;
}

uint32_t App::now() const { return GB_MILLIS(); }

void App::pushFrame() {
  ui::noticeDraw(cv);
  oled.flush(cv, false);
}

// =============================== ЭКРАН ЗАГРУЗКИ =============================
void App::drawBoot(uint32_t phase) {
  cv.clear();
  // логотип по центру сверху
  cv.bitmap((128 - bmpLogo.w) / 2, 6, bmpLogo, BMP_MASK);
  cv.textC(48, "GameBox", font_big, 1);
  // вращающийся полукруг
  int a = (int)(phase * 26) % 360;
  int cx = 64, cy = 40;
  (void)cx; (void)cy;
  cv.arc(24, 40, 6, a, a + 150, 1);
  cv.arc(104, 40, 6, a + 180, a + 330, 1);
}

// =============================== МЕНЮ =======================================
static void menuRow(Canvas& c, int y, const char* label, int icon, bool sel) {
  if (sel) {
    c.fillRoundRect(6, y, 116, 15, 4, 1);
    c.text(30, y + 3, label, font_small, 0);
    // иконка инверсией
    Canvas tmp;
    tmp.clear();
    if (icon == 0) iconGamepad(tmp, 0, 0);
    else if (icon == 1) iconGear(tmp, 0, 0);
    else iconPower(tmp, 0, 0);
    for (int j = 0; j < 14; j++)
      for (int i = 0; i < 14; i++)
        if (tmp.get(i, j)) c.px(12 + i, y + 2 + j, 0);
  } else {
    c.text(30, y + 3, label, font_small, 1);
    Canvas tmp;
    tmp.clear();
    if (icon == 0) iconGamepad(tmp, 0, 0);
    else if (icon == 1) iconGear(tmp, 0, 0);
    else iconPower(tmp, 0, 0);
    for (int j = 0; j < 14; j++)
      for (int i = 0; i < 14; i++)
        if (tmp.get(i, j)) c.px(12 + i, y + 2 + j, 1);
  }
}

void App::drawMenu() {
  cv.clear();
  cv.text(2, 1, "GameBox", font_small, 1);
  char buf[16];
  snprintf(buf, sizeof(buf), "v1.0");
  cv.textR(126, 1, buf, font_small, 1);
  ui::separator(cv, 12);
  for (int i = 0; i < 3; i++) menuRow(cv, 16 + i * 16, kMenuItems[i], i, menuIdx == i);
  ui::triangle(cv, 120, 58, 1, 3, ui::blink());
}

void App::handleMenu(Ev e) {
  switch (e) {
    case EV_UP:
      if (menuIdx > 0) { menuIdx--; ui::beep(1800, 8); }
      break;
    case EV_DOWN:
      if (menuIdx < 2) { menuIdx++; ui::beep(1800, 8); }
      break;
    case EV_SELECT:
      appStore.setU8("menu", menuIdx);
      if (menuIdx == 0) { scr = SCR_GAMES; selDir = 0; }
      else if (menuIdx == 1) { scr = SCR_SETTINGS; setPage = 0; setIdx = 0; setFirst = 0; }
      else startReboot();
      break;
    default: break;
  }
}

// =============================== СЕЛЕКТОР ИГР ===============================
void App::drawGames() {
  cv.clear();
  const uint8_t n = gameCount();
  if (n == 0) { cv.textC(28, "Нет игр", font_big, 1); return; }

  // анимация слайда между баннерами
  if (selDir != 0) {
    float t = (float)(now() - selStart) / (float)UI_ANIM_MS;
    if (t >= 1.0f) { selDir = 0; }
  }

  auto drawOne = [&](Canvas& c, uint8_t idx, int dx) {
    Game* g = gameAt(idx);
    const Bmp* cov = g->cover();
    if (cov) {
      int cy = (46 - cov->h) / 2;
      c.bitmap(dx, cy < 0 ? 0 : cy, *cov, BMP_COPY);
      c.hline(dx, 46, 128, 1);
      c.textC(48, g->title(), font_small, 1);
      char b[16];
      snprintf(b, sizeof(b), "%u/%u", (unsigned)(idx + 1), (unsigned)n);
      c.textOutlined(dx + 3, 2, b, font_small, 1);
    } else {
      c.bitmap(dx, 0, g->banner(), BMP_COPY);
      c.hline(dx, 32, 128, 1);
      c.textC(36, g->title(), font_small, 1);
      char b[16];
      snprintf(b, sizeof(b), "%u/%u", (unsigned)(idx + 1), (unsigned)n);
      c.textOutlined(dx + 3, 52, b, font_small, 1);
      c.textOutlined(dx + 22, 52, "← → листать · ВЫБОР", font_small, 1);
    }
  };

  if (selDir == 0) {
    drawOne(cv, gameIdx, 0);
  } else {
    float t = (float)(now() - selStart) / (float)UI_ANIM_MS;
    if (t > 1) t = 1;
    float p = easeOutCubic(t);
    int off = (int)((1.0f - p) * 128 + 0.5f);
    int8_t from = selFrom;
    Canvas a, b;
    a.clear(); drawOne(a, (uint8_t)from, 0);
    b.clear(); drawOne(b, gameIdx, 0);
    int bShift = (selDir > 0) ? off : -off;
    int aShift = (selDir > 0) ? off - 128 : 128 - off;
    blit(cv, a, aShift, 0, false);
    blit(cv, b, bShift, 0, true);
  }
  // мигающие треугольники «листать» и «выбрать»
  if (ui::blink()) {
    if (gameCount() > 1) {
      ui::triangle(cv, 4, 58, 3, 4, true);
      ui::triangle(cv, 119, 58, 1, 4, true);
    }
  }
}

void App::handleGames(Ev e) {
  const uint8_t n = gameCount();
  switch (e) {
    case EV_LEFT:
      if (n > 1 && gameIdx > 0) {
        selFrom = gameIdx; gameIdx--; selDir = -1; selStart = now(); appStore.setU8("game", gameIdx);
      } else if (n > 1) {   // карусель по кругу
        selFrom = gameIdx; gameIdx = n - 1; selDir = -1; selStart = now(); appStore.setU8("game", gameIdx);
      }
      break;
    case EV_RIGHT:
      if (n > 1) {
        selFrom = gameIdx; gameIdx = (uint8_t)((gameIdx + 1) % n); selDir = 1; selStart = now();
        appStore.setU8("game", gameIdx);
      }
      break;
    case EV_SELECT: openGame(gameIdx); break;
    case EV_BACK: scr = SCR_MENU; break;
    case EV_MENU:
      // быстрая справка по управлению игрой
      textTitle = gameAt(gameIdx)->helpTitle();
      textBody = gameAt(gameIdx)->help();
      confirmBack = SCR_GAMES;
      scr = SCR_SETTINGS;
      setPage = 1;
      textFirst = 0;
      break;
    default: break;
  }
}

void App::openGame(uint8_t idx) {
  Game* g = gameAt(idx);
  if (!g) return;
  host.open(g, now());
  scr = SCR_GAME;
  ui::beep(2200, 10);
}

void App::closeGame() {
  host.close();
  scr = SCR_GAMES;
}

void App::drawGame() {
  cv.clear();
  host.draw(cv, now());
  ui::noticeDraw(cv);
}

// =============================== НАСТРОЙКИ ==================================
void App::drawSettings() {
  cv.clear();
  const uint8_t items = 6;
  if (setPage == 0) {
    ui::header(cv, "НАСТРОЙКИ");
    const int rowH = 12;
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t idx = (uint8_t)(setFirst + i);
      if (idx >= items) break;
      int y = 14 + i * rowH;
      bool sel = idx == setIdx;
      if (sel) {
        cv.fillRoundRect(2, y, 118, rowH, 3, 1);
        cv.text(6, y + 1, kHelpSettings[idx], font_small, 0);
      } else {
        cv.text(6, y + 1, kHelpSettings[idx], font_small, 1);
      }
    }
    ui::scrollbar(cv, 123, 14, 47, items, 4, setFirst);
  } else if (setPage == 1) {
    ui::header(cv, textTitle ? textTitle : "");
    if (textBody) {
      // Текст разбиваем на строки и рисуем построчно со смещением:
      // раньше он верстался в буфер 128x64 и физически обрывался после 5 строк.
      static char lines[240][80];
      const int lineH = font_small.lineH;
      const int areaY = 13, areaH = 51, visible = areaH / lineH;
      int total = cv.wrapSplit(textBody, font_small, 106, &lines[0][0], 80, 240);
      int maxFirst = total > visible ? total - visible : 0;
      if (textFirst > maxFirst) textFirst = maxFirst;
      if (textFirst < 0) textFirst = 0;

      // плавная прокрутка: цель — textFirst строк
      float target = (float)textFirst;
      float elapsed = (float)(now() - textScrollStart);
      float t = (UI_ANIM_MS > 0) ? elapsed / (float)UI_ANIM_MS : 1.0f;
      if (t > 1.0f) t = 1.0f;
      float p = 1.0f - (1.0f - t) * (1.0f - t);          // easeOutQuad
      float cur = textScrollFrom + (target - textScrollFrom) * p;
      if (t >= 1.0f) { cur = target; textScrollFrom = target; textScrollY = target; }
      textScrollY = cur;

      // рисуем в отдельный буфер и переносим под шапку — так строки
      // обрезаются ровно по границе области, а не посреди глифов
      static Canvas tmpCanvas;
      tmpCanvas.clear();
      int off = (int)(cur * lineH + 0.5f);
      for (int i = 0; i < total; i++) {
        int y = i * lineH - off;
        // рисуем только строки, которые целиком попадают в область:
        // так ни один глиф не обрезается посередине
        if (y + font_small.ascent > areaH) break;
        if (y < 0) continue;
        char clipped[80];
        cv.copyFit(clipped, sizeof(clipped), lines[i], font_small, 106);
        tmpCanvas.text(2, y, clipped, font_small, 1);
      }
      blit(cv, tmpCanvas, 0, areaY, true);

      ui::scrollbar(cv, 126, areaY, areaH, total, visible, (int)(cur + 0.5f));
      if (total > visible && ui::blink(700)) {
        if (textFirst > 0) ui::triangle(cv, 120, 17, 0, 3, true);
        if (textFirst < maxFirst) ui::triangle(cv, 120, 57, 2, 3, true);
      }
    }
  } else if (setPage == 3) {
    ui::header(cv, "СПРАВКА ИГР");
    const uint8_t n = gameCount();
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t idx = (uint8_t)(setFirst + i);
      if (idx >= n) break;
      int y = 14 + i * 13 - 1;
      bool sel = idx == setGame;
      if (sel) { cv.fillRoundRect(2, y, 118, 13, 3, 1); cv.text(6, y + 2, gameAt(idx)->shortName(), font_small, 0); }
      else cv.text(6, y + 2, gameAt(idx)->shortName(), font_small, 1);
    }
    ui::scrollbar(cv, 123, 14, 48, n, 4, setFirst);
  }
  ui::noticeDraw(cv);
}

void App::handleSettings(Ev e) {
  if (setPage == 1) {                     // текстовая страница
    static char tmpLines[220][78];
    int total = textBody ? cv.wrapSplit(textBody, font_small, 108, &tmpLines[0][0], 78, 220) : 0;
    int maxFirst = total > 4 ? total - 4 : 0;
    switch (e) {
      case EV_DOWN: if (textFirst < maxFirst) { textScrollFrom = textScrollY; textScrollStart = now(); textFirst++; } break;
      case EV_UP:   if (textFirst > 0) { textScrollFrom = textScrollY; textScrollStart = now(); textFirst--; } break;
      case EV_BACK: setPage = confirmBack == SCR_GAMES ? 0 : 0; textFirst = 0;
                    if (confirmBack == SCR_GAMES) { scr = SCR_GAMES; }
                    break;
      case EV_SELECT: setPage = 0; textFirst = 0; if (confirmBack == SCR_GAMES) scr = SCR_GAMES; break;
      default: break;
    }
    return;
  }
  if (setPage == 3) {                     // список игр -> справка
    const uint8_t n = gameCount();
    switch (e) {
      case EV_UP: if (setGame > 0) setGame--; break;
      case EV_DOWN: if (setGame + 1 < n) setGame++; break;
      case EV_SELECT:
        textTitle = gameAt(setGame)->helpTitle();
        textBody = gameAt(setGame)->help();
        setPage = 1;
        textFirst = 0;
        break;
      case EV_BACK: setPage = 0; break;
      default: break;
    }
    setFirst = (setGame > 3) ? (uint8_t)(setGame - 3) : 0;
    return;
  }

  // список настроек
  switch (e) {
    case EV_UP: if (setIdx > 0) setIdx--; break;
    case EV_DOWN: if (setIdx < 5) setIdx++; break;
    case EV_BACK: scr = SCR_MENU; break;
    case EV_SELECT:
      if (setIdx == 0) { textTitle = "КНОПКИ"; textBody = kHelpButtons; setPage = 1; textFirst = 0; confirmBack = SCR_SETTINGS; }
      else if (setIdx == 1) { textTitle = "ДЖОЙСТИК"; textBody = kHelpStick; setPage = 1; textFirst = 0; confirmBack = SCR_SETTINGS; }
      else if (setIdx == 2) { setPage = 3; setGame = 0; setFirst = 0; }
      else if (setIdx == 3) { in.recalibrate(); ui::notice("Джойстик откалиброван", 1200); }
      else if (setIdx == 4) { textTitle = "О ПРИСТАВКЕ"; textBody = kHelpAbout; setPage = 1; textFirst = 0; confirmBack = SCR_SETTINGS; }
      else if (setIdx == 5) {
        confirmText = "Удалить все сохранения\nигр и настроек?";
        confirmAction = 1;
        confirmBack = SCR_SETTINGS;
        scr = SCR_CONFIRM;
      }
      break;
    default: break;
  }
  setFirst = (setIdx > 3) ? (uint8_t)(setIdx - 3) : 0;
}

// =============================== ПОДТВЕРЖДЕНИЕ ==============================
void App::drawConfirm() {
  if (confirmBack == SCR_SETTINGS) drawSettings();
  else drawMenu();
  // затемнение фона редкой штриховкой, чтобы диалог читался
  cv.fillRect(0, 0, 128, 64, 0);     // диалог рисуем на чистом фоне
  cv.roundRect(2, 1, 124, 62, 5, 1);
  cv.roundRect(4, 3, 120, 58, 4, 1);
  cv.textWrap(9, 7, 110, confirmText, font_small, 1, 3);
  ui::separator(cv, 33);
  cv.textC(37, "да — ВЫБОР", font_small, 1);
  cv.textC(48, "нет — НАЗАД", font_small, 1);
}

void App::handleConfirm(Ev e) {
  if (e == EV_SELECT) {
    if (confirmAction == 1) eraseAll();
    else if (confirmAction == 2) {
#ifdef ARDUINO
      ESP.restart();
#endif
    }
    scr = confirmBack;
  } else if (e == EV_BACK || e == EV_MENU) {
    scr = confirmBack;
  }
}

void App::startReboot() {
  confirmText = "Перезагрузить\nприставку?";
  confirmAction = 2;
  confirmBack = SCR_MENU;
  scr = SCR_CONFIRM;
}

void App::eraseAll() {
#ifdef ARDUINO
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.clear();
  p.end();
#endif
  ui::notice("Сохранения удалены", 1500);
}

// =============================== ЦИКЛ ======================================
void App::handleEvent(Ev e) {
  if (e == EV_NONE) return;
  // Кнопка "меню" открывает подтверждение/паузу на любом экране
  switch (scr) {
    case SCR_MENU: handleMenu(e); break;
    case SCR_GAMES: handleGames(e); break;
    case SCR_SETTINGS: handleSettings(e); break;
    case SCR_CONFIRM: handleConfirm(e); break;
    case SCR_GAME: {
      if (e == EV_BACK && host.game() && host.game()->modal()) { host.event(e, now()); break; }
      if (e == EV_BACK) {
        if (host.atRoot()) closeGame();
        else host.back(now());
        break;
      }
      // показывается баннер-уведомление: закрываем его первым нажатием
      if (ui::noticeActive()) { ui::noticeDismiss(); break; }
      host.event(e, now());
      break;
    }
    default: break;
  }
}

void App::tick() {
  uint32_t t = now();
  uint32_t dt = t - lastTick;
  if (dt > 100) dt = 100;
  lastTick = t;

  in.update(t);
  if (scr == SCR_GAME) host.update(dt, t);

  Ev e;
  while ((e = in.pop()) != EV_NONE) handleEvent(e);

  switch (scr) {
    case SCR_BOOT: drawBoot(t / 40); break;
    case SCR_MENU: drawMenu(); break;
    case SCR_GAMES: drawGames(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_GAME: drawGame(); break;
    case SCR_CONFIRM: drawConfirm(); break;
  }
  ui::noticeDraw(cv);
  oled.flush(cv, false);
}
