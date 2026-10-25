// ---------------------------------------------------------------------------
//  GameBox :: game_rng.cpp — «RNG: крутки» (в духе Sol's RNG)
//
//  Вкладки:
//    0 КРУТКА     — ручная крутка (ВЫБОР) и анимация выпавшей ауры
//    1 КОЛЛЕКЦИЯ  — все открытые ауры (вглубь — страницы списка)
//    2 СТАТИСТИКА — круток, пыль, рекорд
//    3 УЛУЧШЕНИЯ  — автокрутка, удача, скорость (вглубь — по пунктам)
// ---------------------------------------------------------------------------
#include "../core/game.h"
#include "../core/util.h"
#include "../core/ui.h"
#include "../core/storage.h"
#include "../gfx/gfx.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

struct AuraDef {
  const char* name;
  uint32_t oneIn;     // «1 из N»
  uint32_t dust;      // пыль за находку
  uint8_t tier;       // 0..5 — насколько мощная анимация
};

const uint8_t kAuras = 26;
const AuraDef kAura[kAuras] = {
  {"Обычность",        1,        1,        0},
  {"Пылинка",          2,        2,        0},
  {"Ветерок",          5,        5,        0},
  {"Искра",            10,       10,       1},
  {"Туман",            25,       22,       1},
  {"Капля",            50,       40,       1},
  {"Полуночь",         100,      80,       2},
  {"Снегопад",         250,      200,      2},
  {"Гроза",            500,      400,      2},
  {"Метеор",           1000,     800,      3},
  {"Северное сияние",  2500,     2000,     3},
  {"Сингулярность",    5000,     4000,     3},
  {"Кварк",            10000,    8000,     3},
  {"Галактика",        25000,    20000,    4},
  {"Чёрная дыра",      50000,    40000,    4},
  {"Космическая пыль", 100000,   80000,    4},
  {"Хронос",           250000,   200000,   4},
  {"Абсолют",          500000,   400000,   4},
  {"Пустота",          1000000,  800000,   5},
  {"Изначальный свет", 2500000,  2000000,  5},
  {"Кровь звёзд",      5000000,  4000000,  5},
  {"Эхо творения",     10000000, 8000000,  5},
  {"Тишина",           25000000, 20000000, 5},
  {"Грань",            50000000, 40000000, 5},
  {"Ноль",             100000000,80000000, 5},
  {"Запредельность",   1000000000,800000000,5},
};

struct RngState {
  uint32_t rolls = 0;
  double dust = 0;
  uint8_t best = 0;                 // лучшая аура (индекс)
  uint8_t found[kAuras];
  uint8_t autoRoll = 0;
  uint8_t luckLvl = 0;              // 0..10
  uint8_t speedLvl = 0;             // 0..10
  uint16_t luckPot = 0;             // временные усиления (секунды)
  uint16_t hastePot = 0;
  // анимация
  uint8_t animPhase = 0;
  uint32_t animStart = 0;
  uint8_t lastAura = 0;
  uint32_t lastRoll = 0;
  uint32_t animSeed = 0;
};

Store rngStore("gb_rng");

class GameRng : public Game {
 public:
  const char* title() const override { return "RNG: крутки"; }
  const char* shortName() const override { return "RNG"; }
  const Bmp& banner() const override { return bmpBannerRng; }
  uint8_t tabs() const override { return 4; }
  const char* tabName(uint8_t t) const override {
    static const char* n[4] = {"КРУТКА", "КОЛЛЕКЦИЯ", "СТАТИСТИКА", "УЛУЧШЕНИЯ"};
    return n[t < 4 ? t : 0];
  }
  uint8_t vpages(uint8_t t) const override {
    if (t == 1) return (uint8_t)((kAuras + 2) / 3);   // по 3 ауры на страницу
    if (t == 3) return 3;
    return 1;
  }

  void enter() override {
    rngStore.begin();
    load();
    s.lastRoll = gbMillis();
  }
  void leave() override { save(); }

  void onEvent(Ev e, uint32_t now) override {
    if (s.animPhase && (e == EV_SELECT || e == EV_STICK)) {
      if (now - s.animStart > 700) s.animPhase = 0;   // пропустить анимацию
      return;
    }
    switch (tab) {
      case 0:
        if (e == EV_SELECT) doRoll(now);
        break;
      case 3:
        if (e == EV_SELECT) upgrade((uint8_t)vpage, now);
        break;
      default: break;
    }
  }

  void update(uint32_t dt, uint32_t now) override {
    if (s.luckPot && (now % 1000 == 0)) { /* счётчик по времени ниже */ }
    // отсчёт временных усилений
    static uint32_t lastSec = 0;
    if (now - lastSec >= 1000) {
      lastSec = now;
      if (s.luckPot) s.luckPot--;
      if (s.hastePot) s.hastePot--;
    }
    if (s.animPhase && now - s.animStart > animDuration(s.lastAura)) s.animPhase = 0;
    // автокрутка
    if (s.autoRoll && tab == 0 && !s.animPhase) {
      uint32_t interval = autoInterval();
      if (now - s.lastRoll >= interval) doRoll(now);
    }
    (void)dt;
  }

  void draw(Canvas& c, uint32_t now, int8_t t, int8_t vp) override {
    c.clear();
    switch (t) {
      case 0: drawRoll(c, now); break;
      case 1: drawCollection(c, vp); break;
      case 2: drawStats(c); break;
      default: drawUpgrades(c, vp); break;
    }
    ui::noticeDraw(c);
  }

  const char* help() const override {
    return
      "ИДЕЯ\n"
      "Крутите и собирайте ауры.\n"
      "Чем реже аура, тем больше\n"
      "пыли она даёт и тем\n"
      "зрелищнее анимация.\n\n"
      "КРУТКА\n"
      "ВЫБОР — крутить.\n"
      "СТИК — автокрутка вкл/выкл\n"
      "(только на этой вкладке).\n\n"
      "КОЛЛЕКЦИЯ\n"
      "Вверх/вниз — страницы.\n"
      "Найдена ли аура — видно по\n"
      "галочке.\n\n"
      "УЛУЧШЕНИЯ\n"
      "Вверх/вниз — пункт:\n"
      " 1) автокрутка,\n"
      " 2) удача (шанс редких),\n"
      " 3) скорость (интервал).\n"
      "Покупается за пыль.\n\n"
      "СБРОС — обнулить статистику\n"
      "круток (коллекция остаётся).";
  }

 private:
  RngState s;

  float luck() const { return 1.0f + s.luckLvl * 0.35f + (s.luckPot ? 1.5f : 0.0f); }

  uint32_t autoInterval() const {
    uint32_t base = 900;
    uint32_t red = s.speedLvl * 70;
    if (red > 700) red = 700;
    uint32_t v = base - red;
    if (s.hastePot) v = v / 2;
    return v < 120 ? 120 : v;
  }

  uint32_t animDuration(uint8_t aura) const {
    return 900 + kAura[aura].tier * 420;
  }

  void load() {
    s.rolls = rngStore.getU32("rolls", 0);
    s.dust = rngStore.getF("dust", 0);
    s.best = rngStore.getU8("best", 0);
    s.autoRoll = rngStore.getU8("auto", 0);
    s.luckLvl = rngStore.getU8("luck", 0);
    s.speedLvl = rngStore.getU8("speed", 0);
    rngStore.getBytes("found", s.found, sizeof(s.found));
  }

  void save() {
    rngStore.setU32("rolls", s.rolls);
    rngStore.setF("dust", (float)s.dust);
    rngStore.setU8("best", s.best);
    rngStore.setU8("auto", s.autoRoll);
    rngStore.setU8("luck", s.luckLvl);
    rngStore.setU8("speed", s.speedLvl);
    rngStore.setBytes("found", s.found, sizeof(s.found));
  }

  // -------------------------------------------------------------- крутка
  void doRoll(uint32_t now) {
    s.rolls++;
    s.lastRoll = now;
    // идём от самых редких
    uint8_t got = 0;
    for (int i = kAuras - 1; i >= 0; i--) {
      float p = luck() / (float)kAura[i].oneIn;
      if (p >= 1.0f) p = 1.0f;
      if (gbChance(p)) {
        got = (uint8_t)i;
        break;
      }
    }
    s.lastAura = got;
    s.found[got] = 1;
    s.dust += kAura[got].dust;
    if (got > s.best) s.best = got;
    s.animPhase = 1;
    s.animStart = now;
    s.animSeed = gbRandomU32();
    if (kAura[got].tier >= 3) ui::beep(2600 + kAura[got].tier * 200, 60);
    else ui::beep(1900, 15);
    if (got >= 12) save();                 // редкие сохраняем сразу
  }

  void upgrade(uint8_t idx, uint32_t now) {
    double cost;
    if (idx == 0) {
      s.autoRoll = !s.autoRoll;
      char b[40];
      snprintf(b, sizeof(b), "Автокрутка: %s", s.autoRoll ? "вкл" : "выкл");
      ui::notice(b, 1000);
      save();
      return;
    }
    if (idx == 1) {
      if (s.luckLvl >= 10) { ui::notice("Удача максимальна", 1000); return; }
      cost = 250.0 * pow(2.2, s.luckLvl);
      if (s.dust < cost) { notEnough(cost); return; }
      s.dust -= cost;
      s.luckLvl++;
      ui::notice("Удача повышена", 1000);
      save();
      return;
    }
    if (s.speedLvl >= 10) { ui::notice("Скорость максимальна", 1000); return; }
    cost = 400.0 * pow(2.0, s.speedLvl);
    if (s.dust < cost) { notEnough(cost); return; }
    s.dust -= cost;
    s.speedLvl++;
    ui::notice("Скорость повышена", 1000);
    save();
    (void)now;
  }

  void notEnough(double cost) {
    char m[20], b[48];
    fmtNum(m, sizeof(m), cost);
    snprintf(b, sizeof(b), "Нужно пыли: %s", m);
    ui::notice(b, 1300);
  }

  // ----------------------------------------------------------- отрисовка
  void topBar(Canvas& c, const char* label, uint8_t t) {
    c.text(2, 1, label, font_small, 1);
    char m[20], line[32];
    fmtNum(m, sizeof(m), s.dust);
    snprintf(line, sizeof(line), "%s", m);
    c.textR(103, 1, line, font_small, 1);
    ui::tabsDots(c, t, tabs(), 126, 3);
    ui::separator(c, 12);
  }

  void drawAuraFx(Canvas& c, uint8_t aura, uint32_t now) {
    // Анимация: чем выше tier — тем больше слоёв
    uint8_t tier = kAura[aura].tier;
    uint32_t t = now - s.animStart;
    uint32_t dur = animDuration(aura);
    if (t > dur) t = dur;
    float p = (float)t / (float)dur;

    // вспышка на весь экран
    if (tier >= 4 && t < 140) c.clear(1);

    // лучи
    if (tier >= 2) {
      int n = 6 + tier * 4;
      int r0 = 8 + (int)(p * 30);
      for (int i = 0; i < n; i++) {
        float a = (2 * 3.14159f / n) * i + p * 3.0f + (s.animSeed % 7) * 0.1f;
        int x1 = 64 + (int)(r0 * cosf(a));
        int y1 = 32 + (int)(r0 * sinf(a));
        int x2 = 64 + (int)((r0 + 8 + tier * 3) * cosf(a));
        int y2 = 32 + (int)((r0 + 8 + tier * 3) * sinf(a));
        c.line(x1, y1, x2, y2, tier >= 4 && t < 220 ? 0 : 1);
      }
    }
    // расходящиеся кольца
    if (tier >= 3) {
      int rr = (int)(p * 60);
      if (rr > 4) c.circle(64, 32, rr, tier >= 4 ? 0 : 1);
      if (rr > 12) c.circle(64, 32, rr - 8, tier >= 4 ? 0 : 1);
    }
    // звезда
    if (tier >= 1) {
      int r1 = 8 + (int)(10 * sinf(p * 3.14f));
      int r2 = r1 / 2;
      int cx = 64, cy = 32;
      for (int i = 0; i < 10; i++) {
        float a1 = 3.14159f * i / 5 - 1.57f;
        float a2 = a1 + 0.314f;
        int r = (i % 2) ? r2 : r1;
        int x1 = cx + (int)(r * cosf(a1));
        int y1 = cy + (int)(r * sinf(a1));
        int x2 = cx + (int)(r * cosf(a2));
        int y2 = cy + (int)(r * sinf(a2));
        c.line(x1, y1, x2, y2, 1);
      }
    }
    // искры для tier 5
    if (tier >= 5) {
      for (int i = 0; i < 12; i++) {
        uint32_t h = s.animSeed ^ (i * 2654435761u);
        int x = (int)(h % 128);
        int y = (int)((h >> 8) % 64);
        c.px(x, y, (t / 90 + i) % 2);
      }
    }
  }

  void drawRoll(Canvas& c, uint32_t now) {
    topBar(c, "КРУТКА", 0);
    if (s.animPhase) {
      drawAuraFx(c, s.lastAura, now);
      const AuraDef& a = kAura[s.lastAura];
      if (now - s.animStart > 300) {
        c.textC(24, a.name, font_big, 1);
        char b[48];
        snprintf(b, sizeof(b), "1 из %lu · +%lu пыли", (unsigned long)a.oneIn, (unsigned long)a.dust);
        c.textC(44, b, font_small, 1);
      }
      return;
    }
    // звезда + кубик как символ крутки
    for (int i = 0; i < 10; i++) {
      float a1 = 3.14159f * i / 5 - 1.57f + (now / 400.0f);
      float a2 = a1 + 0.314f;
      int r1 = (i % 2) ? 9 : 18, r2 = (i % 2) ? 18 : 9;
      int cx = 30, cy = 28;
      c.line(cx + (int)(r1 * cosf(a1)), cy + (int)(r1 * sinf(a1)),
             cx + (int)(r2 * cosf(a2)), cy + (int)(r2 * sinf(a2)), 1);
    }
    c.roundRect(58, 16, 22, 22, 3, 1);
    static const uint8_t pips[3] = {1, 4, 6};
    (void)pips;
    for (int i = 0; i < 5; i++) c.px(64 + (i % 3) * 5, 21 + (i / 3) * 5, 1);

    char buf[64], nm[40];
    c.copyFit(nm, sizeof(nm), kAura[s.best].name, font_small, 62);
    snprintf(buf, sizeof(buf), "лучшая: %s", nm);
    c.text(2, 40, buf, font_small, 1);

    if (s.autoRoll) {
      c.text(2, 52, "АВТО вкл", font_small, 1);
      if (ui::blink(500)) ui::triangle(c, 120, 47, 1, 3, true);
    } else if (ui::blink()) {
      c.text(2, 52, "ВЫБОР — крутить", font_small, 1);
    }
  }

  void drawCollection(Canvas& c, int8_t vp) {
    uint8_t page = (uint8_t)vp;
    topBar(c, "КОЛЛЕКЦИЯ", 1);
    uint8_t first = page * 3;
    char pg[16];
    snprintf(pg, sizeof(pg), "%u/%u ↑↓", page + 1, vpages(1));
    c.textR(126, 13, pg, font_small, 1);

    for (uint8_t k = 0; k < 3; k++) {
      uint8_t i = (uint8_t)(first + k);
      if (i >= kAuras) break;
      int y = 25 + k * 12;
      bool found = s.found[i] != 0;
      if (!found) {
        c.text(4, y, "???", font_small, 1);
      } else if (i >= 12) {                       // редкие ауры — инверсия
        c.fillRoundRect(1, y - 1, 124, 12, 3, 1);
        c.text(4, y, kAura[i].name, font_small, 0);
      } else {
        c.text(4, y, kAura[i].name, font_small, 1);
      }
      char r[20];
      snprintf(r, sizeof(r), "1/%lu", (unsigned long)kAura[i].oneIn);
      uint8_t fg = (found && i >= 12) ? 0 : 1;
      c.textR(118, y, r, font_small, fg);
      if (found) { c.line(121, y + 4, 123, y + 7, fg); c.line(123, y + 7, 126, y + 1, fg); }
    }
  }

  void drawStats(Canvas& c) {
    topBar(c, "СТАТИСТИКА", 2);
    char buf[80], m[20], nm2[40];
    snprintf(buf, sizeof(buf), "Круток: %lu", (unsigned long)s.rolls);
    c.text(2, 13, buf, font_small, 1);
    fmtNum(m, sizeof(m), s.dust);
    snprintf(buf, sizeof(buf), "Пыли: %s", m);
    c.text(2, 25, buf, font_small, 1);
    c.copyFit(nm2, sizeof(nm2), kAura[s.best].name, font_small, 78);
    snprintf(buf, sizeof(buf), "Лучшая: %s", nm2);
    c.text(2, 37, buf, font_small, 1);
    uint8_t count = 0;
    for (uint8_t i = 0; i < kAuras; i++) if (s.found[i]) count++;
    snprintf(buf, sizeof(buf), "Открыто: %u из %u", count, kAuras);
    c.text(2, 49, buf, font_small, 1);
  }

  void drawUpgrades(Canvas& c, int8_t vp) {
    uint8_t idx = (uint8_t)vp;
    topBar(c, "УЛУЧШЕНИЯ", 3);
    // список трёх пунктов, подсвечен выбранный
    const char* names[3] = {"Автокрутка", "Удача", "Скорость"};
    for (int i = 0; i < 3; i++) {
      int y = 16 + i * 12;
      bool sel = (i == idx);
      if (sel) { c.fillRoundRect(2, y - 1, 124, 12, 3, 1); c.textOutlined(6, y + 1, names[i], font_small, 0); }
      else c.text(6, y + 1, names[i], font_small, 1);
      char v[32];
      if (i == 0) snprintf(v, sizeof(v), "%s", s.autoRoll ? "вкл" : "выкл");
      else if (i == 1) snprintf(v, sizeof(v), "ур.%u  x%.2f", s.luckLvl, luck());
      else snprintf(v, sizeof(v), "ур.%u", s.speedLvl);
      if (sel) c.textR(122, y + 1, v, font_small, 0);
      else c.textR(122, y + 1, v, font_small, 1);
    }
    char buf[64], m[20];
    if (idx == 1) {
      fmtNum(m, sizeof(m), 250.0 * pow(2.2, s.luckLvl));
      snprintf(buf, sizeof(buf), "Цена: %s пыли", m);
    } else if (idx == 2) {
      fmtNum(m, sizeof(m), 400.0 * pow(2.0, s.speedLvl));
      snprintf(buf, sizeof(buf), "Цена: %s пыли", m);
    } else {
      snprintf(buf, sizeof(buf), "ВЫБОР — переключить");
    }
    c.text(2, 52, buf, font_small, 1);
    if (idx != 0) c.textR(126, 52, "ВЫБОР — купить", font_small, ui::blink(900));
  }
};

}  // namespace

Game* makeGameRng() {
  static GameRng g;
  return &g;
}
