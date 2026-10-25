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
#include "../gfx/gfx_their.h"
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

// Названия и шансы — с вики Sol's RNG (sol-rng.fandom.com/wiki/Auras).
// Имена оставлены как в игре-первоисточнике, остальной текст — по-русски.
const uint8_t kAuras = 48;
const AuraDef kAura[kAuras] = {
  // Basic
  {"Common",            2,          1,         0},
  {"Uncommon",          4,          2,         0},
  {"Good",              5,          3,         0},
  {"Natural",           8,          4,         0},
  {"Rare",              16,         8,         0},
  {"Divinus",           32,         16,        0},
  {"Crystallized",      64,         30,        1},
  {"Dizzy",             123,        55,        1},
  {"Rage",              128,        60,        1},
  {"Topaz",             150,        70,        1},
  {"Ruby",              350,        160,       1},
  {"Forbidden",         403,        180,       1},
  {"Emerald",           500,        220,       1},
  {"Gilded",            512,        240,       1},
  {"Ink",               700,        320,       1},
  {"Jackpot",           777,        350,       1},
  {"Sapphire",          800,        380,       1},
  {"Aquamarine",        900,        420,       1},
  {"Wind",              900,        420,       1},
  // Epic
  {"Diaboli",           1004,       480,       2},
  {"Penumbra",          1010,       480,       2},
  {"Umbra",             1010,       480,       2},
  {"Precious",          1024,       500,       2},
  {"Hydrogen",          1111,       520,       2},
  {"Atomic",            1180,       560,       2},
  {"Glock",             1700,       800,       2},
  {"Magnetic",          2048,       980,       2},
  {"Glacier",           2304,       1100,      2},
  {"Flora",             3700,       1800,      2},
  {"Sidereum",          4096,       2000,      2},
  {"Bleeding",          4444,       2100,      2},
  {"HellFire",          4776,       2300,      2},
  // Unique
  {"Undead",            12000,      6000,      3},
  {"Corrosive",         12000,      6000,      3},
  {"Kawaii",            12300,      6200,      3},
  {"Powered",           16384,      8000,      3},
  {"Marsh",             25000,      12000,     3},
  {"Watt",              32768,      16000,     3},
  {"Aquatic",           40000,      20000,     3},
  {"Starlight",         50000,      25000,     3},
  // Legendary
  {"Exotic",            99999,      50000,     4},
  {"Comet",             120000,     60000,     4},
  {"Celestial",         350000,     180000,    4},
  {"Vortex",            399999,     200000,    4},
  {"Raven",             500000,     250000,    4},
  // Mythic
  {"Arcane",            1000000,    500000,    5},
  {"Cosmos",            1520000,    760000,    5},
  {"Virtual",           2500000,    1250000,   5},
};

// Зелья Sol's RNG (sol-rng.fandom.com/wiki/Items): имена и эффекты с вики
struct PotDef {
  const char* name;
  float luck;        // множитель удачи
  float speed;       // множитель скорости крутки
  uint16_t dur;      // секунды
  uint32_t price;    // пыль
};
const uint8_t kPots = 8;
const PotDef kPot[kPots] = {
  {"Lucky Potion",        1.5f,  1.0f,  120,  400},
  {"Speed Potion",        1.0f,  1.6f,  120,  500},
  {"Mixed Potion",        1.25f, 1.1f,  180,  900},
  {"Gladiator Potion",    2.0f,  1.0f,  120,  2500},
  {"Forbidden Potion I",  1.7f,  1.1f,  240,  6000},
  {"Forbidden Potion II", 3.25f, 1.25f, 300,  20000},
  {"Strange Potion I",    1.0f,  1.0f,  300,  3500},   // эффект случайный
  {"Heavenly Potion I",   6.0f,  1.0f,   60,  150000},
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
  float potLuck = 1.0f, potSpeed = 1.0f;   // активное зелье
  uint8_t potId = 0;
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
  const Bmp* cover() const override { return &bmpTheirMenu2; }
  const Bmp& banner() const override { return bmpBannerRng; }
  uint8_t tabs() const override { return 5; }
  const char* tabName(uint8_t t) const override {
    static const char* n[5] = {"КРУТКА", "КОЛЛЕКЦИЯ", "ЗЕЛЬЯ", "УЛУЧШЕНИЯ", "ИТОГИ"};
    return n[t < 5 ? t : 0];
  }
  uint8_t vpages(uint8_t t) const override {
    if (t == 1) return (uint8_t)((kAuras + 2) / 3);   // по 3 ауры на страницу
    if (t == 2) return kPots;                         // по зелью на страницу
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
        else if (e == EV_STICK) { s.autoRoll = !s.autoRoll; save(); }
        break;
      case 2:
        if (e == EV_SELECT) brew((uint8_t)vpage);
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
      if (s.luckPot) {
        s.luckPot--;
        if (!s.luckPot) { s.potLuck = 1.0f; s.potSpeed = 1.0f; }
      }
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
      case 2: drawPotions(c, vp); break;
      case 3: drawUpgrades(c, vp); break;
      default: drawStats(c); break;
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
      "ЗЕЛЬЯ\n"
      "Зелья Sol's RNG: Lucky,\n"
      "Speed, Mixed, Gladiator,\n"
      "Forbidden I и II, Strange I\n"
      "(случайный эффект из The\n"
      "List I) и Heavenly I.\n"
      "Вверх/вниз — зелье,\n"
      "ВЫБОР — купить и выпить,\n"
      "плата — пыль.\n\n"
      "УЛУЧШЕНИЯ\n"
      "Вверх/вниз — пункт:\n"
      " 1) автокрутка,\n"
      " 2) удача (шанс редких),\n"
      " 3) скорость (интервал).\n"
      "Покупается за пыль.\n\n"
      "Названия аур и зелий —\n"
      "строго с вики Sol's RNG.\n\n"
      "СБРОС — обнулить статистику\n"
      "круток (коллекция остаётся).";
  }

 private:
  RngState s;

  float luck() const { return (1.0f + s.luckLvl * 0.35f) * s.potLuck; }

  uint32_t autoInterval() const {
    uint32_t base = 900;
    uint32_t red = s.speedLvl * 70;
    if (red > 700) red = 700;
    uint32_t v = base - red;
    v = (uint32_t)((float)v / s.potSpeed);
    return v < 90 ? 90 : v;
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

  // Зелья Sol's RNG: покупка за пыль и мгновенное применение
  void brew(uint8_t idx) {
    if (idx >= kPots) return;
    const PotDef& p = kPot[idx];
    if (s.dust < p.price) { notEnough(p.price); return; }
    s.dust -= p.price;
    s.potLuck = p.luck;
    s.potSpeed = p.speed;
    s.potId = idx;
    s.luckPot = p.dur;
    char b[56];
    if (idx == 6) {                       // Strange Potion I — эффект случаен
      uint8_t r = (uint8_t)gbRandomRange(0, 4);
      switch (r) {
        case 0: s.potLuck = 2.5f; s.potSpeed = 1.3f; snprintf(b, sizeof(b), "The Chosen!"); break;
        case 1: s.potLuck = 2.5f; snprintf(b, sizeof(b), "The Power I"); break;
        case 2: s.potSpeed = 1.3f; s.potLuck = 1.0f; snprintf(b, sizeof(b), "The Knowledge I"); break;
        case 3: s.potLuck = 0.5f; snprintf(b, sizeof(b), "The Wrath I"); break;
        default: s.potSpeed = 0.85f; s.potLuck = 1.0f; snprintf(b, sizeof(b), "The Sloth I"); break;
      }
      ui::notice(b, 1600);
    } else {
      snprintf(b, sizeof(b), "%s применено", p.name);
      ui::notice(b, 1300);
    }
    ui::beep(2600, 30);
    save();
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

    if (s.luckPot) {
      char pb[40];
      c.copyFit(pb, sizeof(pb), kPot[s.potId].name, font_small, 52);
      snprintf(buf, sizeof(buf), "%s %us", pb, (unsigned)s.luckPot);
      c.textR(126, 52, buf, font_small, 1);
    }
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
    topBar(c, "ИТОГИ", 4);
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

  void drawPotions(Canvas& c, int8_t vp) {
    uint8_t idx = (uint8_t)vp;
    if (idx >= kPots) idx = 0;
    const PotDef& p = kPot[idx];
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "ЗЕЛЬЯ %u/%u", idx + 1, kPots);
    topBar(c, hdr, 2);
    char buf[64], m[20];
    c.bitmap(2, 14, bmpTheirPotion, BMP_COPY);
    nameFitSmall(c, 44, 14, p.name, 80);

    snprintf(buf, sizeof(buf), "удача x%.2f", p.luck);
    c.text(44, 27, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "скорость x%.2f", p.speed);
    c.text(44, 37, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "на %u с", (unsigned)p.dur);
    c.text(44, 47, buf, font_small, 1);

    if (s.luckPot) {
      snprintf(buf, sizeof(buf), "%s %us", kPot[s.potId].name, (unsigned)s.luckPot);
      c.text(2, 52, buf, font_small, 1);
    } else if (s.dust < p.price) {
      fmtNum(m, sizeof(m), p.price);
      snprintf(buf, sizeof(buf), "нужно %s пыли", m);
      c.text(2, 52, buf, font_small, 1);
    } else if (ui::blink(900)) {
      c.text(2, 52, "ВЫБОР — выпить", font_small, 1);
    }
  }

  static void nameFitSmall(Canvas& c, int x, int y, const char* str, int maxW) {
    char b[40];
    c.copyFit(b, sizeof(b), str, font_small, maxW);
    c.text(x, y, b, font_small, 1);
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
