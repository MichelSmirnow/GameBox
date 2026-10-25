// ---------------------------------------------------------------------------
//  GameBox :: game_spts.cpp — Super Power Training Simulator
//
//  Вкладки:
//    0 ТРЕНИРОВКА — выбор навыка (вглубь — навыки), автотренировка + рывок
//    1 ЗОНЫ       — зоны тренировки (вглубь — зоны, открываются по силе)
//    2 АРЕНА      — автобой с ботами (вглубь — боты), награда — очки
//    3 МАГАЗИН    — множители к навыкам за очки
//    4 СТАТИСТИКА — итоги силы и прогресса
// ---------------------------------------------------------------------------
#include "../core/game.h"
#include "../core/util.h"
#include "../core/ui.h"
#include "../core/storage.h"
#include "../gfx/gfx.h"
#include "../gfx/gfx_their.h"
#include <stdio.h>
#include <string.h>

namespace {

struct SkillDef { const char* name; float weight; uint8_t sprite; };
const uint8_t kSkills = 5;
// Навыки — как в SPTS: сила удара, прочность тела, психика, скорость, прыжок
const SkillDef kSkill[kSkills] = {
  {"Сила удара", 1.00f, 0},
  {"Прочность",  1.25f, 1},
  {"Психика",    1.60f, 4},
  {"Скорость",   0.85f, 2},
  {"Прыжок",     0.75f, 3},
};

// Зоны тренировки: списки взяты из game_spts.cpp автора (порядок и названия
// его). Каждая зона качает РОВНО один навык, следующая зона того же навыка
// даёт вдвое больше (множитель 2^n — формула SPTS).
struct SZoneDef { const char* name; };
const SZoneDef kZonesFist[] = {
  {"Мешок"}, {"Лес"}, {"Стальной столб"}, {"Скала"}, {"Кристалл"},
  {"Синяя звезда"}, {"Токсичная планета"}, {"Магмовая планета"}, {"Багровые кристаллы"},
  {"Молния"}, {"Чёрная дыра"}, {"Стадион драконов"}, {"Меч"}, {"Клетка"},
  {"Верховная планета"}, {"Скопление пустоты"}, {"Ядро зла"}, {"Бесконечная пустота"},
  {"Элементарный метеор"}, {"Небесный пояс"},
};
const SZoneDef kZonesBody[] = {
  {"Холодный душ"}, {"Ледяная ванна"}, {"Огненная ванна"}, {"Айсберг"}, {"Торнадо"},
  {"Вулкан"}, {"Адская яма"}, {"Кислотный бассейн"}, {"Грузовой отсек"}, {"ЛЭП"},
  {"Радиоактивный разлив"}, {"Сжатая материя"}, {"Логово Ктулху"}, {"Кугельблиц"},
  {"Внутренний вулкан"}, {"Ядро преисподней"}, {"Заражённый ил"}, {"Вирус порчи"},
  {"Луч душ"}, {"Разлом пустоты"}, {"Ультимативный торнадо"}, {"Сингулярность"},
  {"Двойные звёзды"}, {"Вспышка сверхновой"}, {"Виртуальная пустота"},
  {"Слияние звёзд"}, {"Остров творца"}, {"Внешняя тьма"},
};
const SZoneDef kZonesPsy[] = {
  {"Медитация"}, {"Лужайка"}, {"Мост"}, {"Водопад"}, {"Ядро храма"},
  {"Эксперимент"}, {"Жизненная аллея"}, {"Крылья дракона"}, {"Некротический айсберг"},
  {"Надзиратель порчи"}, {"Глитч-звезда"}, {"Звезда авроры"}, {"Золотая звезда"},
  {"Синергия"}, {"Галактические жеоды"}, {"Божественный меч"}, {"Искажённое измерение"},
};
const SZoneDef kZonesSpeed[] = {
  {"Беговая дорожка"}, {"100 ФУНТОВ"}, {"1 ТОННА"}, {"10 ТОНН"}, {"100 ТОНН"},
  {"1К ТОНН"}, {"10К ТОНН"}, {"100К ТОНН"}, {"1М ТОНН"}, {"10М ТОНН"},
  {"1МЛРД ТОНН"}, {"100МЛРД ТОНН"}, {"10ТРЛН ТОНН"}, {"1КВ ТОНН"}, {"100КВ ТОНН"},
  {"10КВИ ТОНН"}, {"1СЕКС ТОНН"}, {"100СЕКС ТОНН"}, {"10СЕПТ ТОНН"}, {"10ОКТ ТОНН"},
  {"1НОН ТОНН"},
};
const SZoneDef kZonesJump[] = {
  {"Скакалка"}, {"1 пружина"}, {"10 пружин"}, {"100 пружин"}, {"1К пружин"},
  {"10К пружин"}, {"100К пружин"}, {"1М пружин"}, {"10М пружин"}, {"1МЛРД пружин"},
  {"100МЛРД пружин"}, {"10ТРЛН пружин"}, {"1КВ пружин"}, {"100КВ пружин"},
  {"10КВИ пружин"}, {"1СЕКС пружин"}, {"100СЕКС пружин"}, {"10СЕПТ пружин"},
  {"10ОКТ пружин"}, {"1НОН пружин"},
};

// Таблица «зоны одного навыка»: указатель + количество
struct ZoneSet { const SZoneDef* z; uint8_t n; };
const ZoneSet kZoneSet[kSkills] = {
  { kZonesFist,  sizeof(kZonesFist) / sizeof(SZoneDef) },
  { kZonesBody,  sizeof(kZonesBody) / sizeof(SZoneDef) },
  { kZonesPsy,   sizeof(kZonesPsy) / sizeof(SZoneDef) },
  { kZonesSpeed, sizeof(kZonesSpeed) / sizeof(SZoneDef) },
  { kZonesJump,  sizeof(kZonesJump) / sizeof(SZoneDef) },
};

struct BotDef { const char* name; double power; uint32_t reward; };
const uint8_t kBots = 5;
const BotDef kBot[kBots] = {
  {"Драчун",      50,       5},
  {"Каратист",    1200,     20},
  {"Титан",       60000,    80},
  {"Кулак бури",  3e6,      300},
  {"Владыка",     2e8,      1200},
};

struct SptsState {
  double xp[kSkills];
  uint8_t zskill = 0;       // навык текущей зоны
  uint8_t ztier = 0;        // номер зоны внутри навыка (0..)
  uint8_t mult[kSkills];    // уровень множителя
  uint32_t tokens = 0;
  uint32_t statFights = 0, statWins = 0;
  double statPowerEarned = 0;
  // арена
  uint8_t battleBot = 0;
  uint8_t battlePhase = 0;  // 0 нет, 1 бой, 2 результат
  uint32_t battleStart = 0;
  float pHp = 1, eHp = 1;
  bool battleWin = false;
  uint32_t battleReward = 0;
  // тренировка
  uint32_t lastTrain = 0;
  uint32_t burstReady = 0;
  uint8_t skillSel = 0;
  uint8_t zoneCat = 0;      // выбранная категория зон (навык)
  bool catMenu = false;     // меню выбора категории, как у зелий в рыбалке
  uint8_t catSel = 0;
};

Store sptsStore("gb_spts");

class GameSpts : public Game {
 public:
  const char* title() const override { return "SPTS: тренировка силы"; }
  const char* shortName() const override { return "SPTS"; }
  const Bmp* cover() const override { return &bmpTheirMenu3; }
  const Bmp& banner() const override { return bmpBannerSpts; }
  uint8_t tabs() const override { return 5; }
  const char* tabName(uint8_t t) const override {
    static const char* n[5] = {"ТРЕНИРОВКА", "ЗОНЫ", "АРЕНА", "МАГАЗИН", "ИТОГИ"};
    return n[t < 5 ? t : 0];
  }
  uint8_t vpages(uint8_t t) const override {
    if (t == 0 || t == 3) return kSkills;
    if (t == 1) return kZoneSet[s.zoneCat].n;
    if (t == 2) return kBots;
    return 1;
  }

  void enter() override {
    sptsStore.begin();
    load();
    s.lastTrain = gbMillis();
  }
  void leave() override { save(); }

  void onEvent(Ev e, uint32_t now) override {
    // меню категорий зон — как переключение категорий зелий в рыбалке
    if (s.catMenu) {
      if (e == EV_DOWN && s.catSel + 1 < kSkills) s.catSel++;
      else if (e == EV_UP && s.catSel > 0) s.catSel--;
      else if (e == EV_SELECT) { s.zoneCat = s.catSel; s.catMenu = false; vpage = 0; }
      else if (e == EV_BACK || e == EV_STICK) s.catMenu = false;
      return;
    }
    if (e == EV_STICK && tab == 1) { s.catMenu = true; s.catSel = s.zoneCat; return; }
    switch (tab) {
      case 0: tab0(e, now); break;
      case 1: tab1(e, now); break;
      case 2: tab2(e, now); break;
      case 3: tab3(e, now); break;
      default: break;
    }
  }

  void update(uint32_t dt, uint32_t now) override {
    // автотренировка идёт, пока открыта вкладка «Тренировка»
    if (tab == 0 && s.battlePhase != 1) {
      double add = trainRate() * (dt / 1000.0);
      s.xp[s.zskill] += add;
      s.statPowerEarned += add;
    }
    if (s.battlePhase == 1 && now - s.battleStart > 2600) {
      s.battlePhase = 2;
      if (s.battleWin) {
        s.tokens += s.battleReward;
        s.statWins++;
        ui::beep(3000, 40);
      } else {
        ui::beep(700, 60);
      }
    }
    if (s.battlePhase == 2 && now - s.battleStart > 5200) s.battlePhase = 0;
  }

  void draw(Canvas& c, uint32_t now, int8_t t, int8_t vp) override {
    c.clear();
    switch (t) {
      case 0: drawTrain(c, now, vp); break;
      case 1: drawZones(c, now, vp); break;
      case 2: drawArena(c, now, vp); break;
      case 3: drawShop(c, now, vp); break;
      default: drawStats(c); break;
    }
    if (s.catMenu) drawCatMenu(c);
    ui::noticeDraw(c);
  }

  const char* help() const override {
    return
      "ЦЕЛЬ\n"
      "Развить силу, пройти все\nзоны и побеждать на\nаренах.\n\n"
      "ТРЕНИРОВКА\n"
      "Вверх/вниз — навык. Пока\n"
      "вкладка открыта, навык\n"
      "качается сам. ВЫБОР —\n"
      "рывок (мгновенный\n"
      "прирост).\n\n"
      "ЗОНЫ\n"
      "СТИК — категория навыка\n"
      "(как зелья в рыбалке),\n"
      "вверх/вниз — зона, ВЫБОР —\n"
      "тренироваться здесь.\n"
      "Каждая зона качает только\n"
      "свой навык, следующая\n"
      "даёт вдвое больше (2^n),\n"
      "а открывается нужным\n"
      "уровнем этого навыка.\n\n"
      "АРЕНА\n"
      "Вверх/вниз — бот, ВЫБОР —\n"
      "автобой. Победа даёт очки.\n\n"
      "МАГАЗИН\n"
      "Очки тратятся на\n"
      "множители навыков (до x5).\n\n"
      "СБРОС — вернуться к\n"
      "тренировке. Престижа\n"
      "в этой игре нет.";
  }

 private:
  SptsState s;

  // ---------- зоны ----------
  static double zoneReq(uint8_t tier) {          // нужный уровень навыка
    return 2.0 + 4.0 * tier + (double)tier * tier;
  }
  static double zoneBoost(uint8_t tier) {        // множитель 2^n (формула SPTS)
    return (double)(1UL << (tier > 20 ? 20 : tier));
  }
  const SZoneDef& curZone() const {
    const ZoneSet& zs = kZoneSet[s.zskill < kSkills ? s.zskill : 0];
    uint8_t t = s.ztier < zs.n ? s.ztier : (uint8_t)(zs.n - 1);
    return zs.z[t];
  }
  double trainRate() const {                      // сила в секунду в текущей зоне
    return zoneBoost(s.ztier) * 0.5 * (1.0 + s.mult[s.zskill] * 0.5);
  }

  double power() const {
    double p = 0;
    for (uint8_t i = 0; i < kSkills; i++) {
      double lvl = level(i);
      p += lvl * lvl * kSkill[i].weight * (1.0 + s.mult[i] * 0.5);
    }
    return p;
  }

  double level(uint8_t i) const {
    // уровень = 1 + sqrt(xp / 10)
    double l = 1.0 + sqrt(s.xp[i] / 10.0);
    return l;
  }

  double xpForNext(uint8_t i) const {
    double lv = level(i);
    double nextLv = floor(lv) + 1.0;
    return (nextLv - 1.0) * (nextLv - 1.0) * 10.0;
  }

  void load() {
    sptsStore.getBytes("xp", s.xp, sizeof(s.xp));
    s.zskill = sptsStore.getU8("zsk", 0);
    s.ztier = sptsStore.getU8("ztr", 0);
    s.zoneCat = sptsStore.getU8("zcat", 0);
    sptsStore.getBytes("mult", s.mult, sizeof(s.mult));
    s.tokens = sptsStore.getU32("tok", 0);
    s.statFights = sptsStore.getU32("f", 0);
    s.statWins = sptsStore.getU32("w", 0);
    s.statPowerEarned = sptsStore.getF("pe", 0);
    if (s.zskill >= kSkills) s.zskill = 0;
    if (s.zoneCat >= kSkills) s.zoneCat = 0;
    if (s.ztier >= kZoneSet[s.zskill].n) s.ztier = (uint8_t)(kZoneSet[s.zskill].n - 1);
  }

  void save() {
    sptsStore.setBytes("xp", s.xp, sizeof(s.xp));
    sptsStore.setU8("zsk", s.zskill);
    sptsStore.setU8("ztr", s.ztier);
    sptsStore.setU8("zcat", s.zoneCat);
    sptsStore.setBytes("mult", s.mult, sizeof(s.mult));
    sptsStore.setU32("tok", s.tokens);
    sptsStore.setU32("f", s.statFights);
    sptsStore.setU32("w", s.statWins);
    sptsStore.setF("pe", (float)s.statPowerEarned);
  }

  void tab0(Ev e, uint32_t now) {
    s.skillSel = (uint8_t)vpage;
    if (e == EV_SELECT) {
      if (now < s.burstReady) {
        ui::notice("Рывок ещё не готов", 900);
        return;
      }
      double add = zoneBoost(s.ztier) * 25.0 * (1.0 + s.mult[s.zskill] * 0.5);
      s.xp[s.zskill] += add;
      s.statPowerEarned += add;
      s.burstReady = now + 2500;
      ui::beep(2400, 20);
    }
  }

  void tab1(Ev e, uint32_t /*now*/) {
    if (e != EV_SELECT) return;
    const ZoneSet& zs = kZoneSet[s.zoneCat];
    uint8_t t = (uint8_t)vpage;
    if (t >= zs.n) return;
    if (level(s.zoneCat) < zoneReq(t)) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Нужен уровень %.0f", zoneReq(t));
      ui::notice(buf, 1400);
      return;
    }
    s.zskill = s.zoneCat;
    s.ztier = t;
    char buf[48];
    snprintf(buf, sizeof(buf), "Зона: %s", zs.z[t].name);
    ui::notice(buf, 1100);
    ui::beep(2600, 25);
    save();
  }

  void tab2(Ev e, uint32_t now) {
    uint8_t b = (uint8_t)vpage;
    if (s.battlePhase == 1) return;
    if (e != EV_SELECT) return;
    if (b >= kBots) return;
    double p = power();
    if (p < kBot[b].power * 0.05) {   // слишком слабо
      ui::notice("Вы слишком слабы для этого бота", 1500);
      return;
    }
    s.battleBot = b;
    s.battleWin = gbChance((float)(p / (p + kBot[b].power)));
    s.battleReward = kBot[b].reward;
    s.battlePhase = 1;
    s.battleStart = now;
    s.statFights++;
    ui::beep(1800, 25);
  }

  void tab3(Ev e, uint32_t now) {
    uint8_t k = (uint8_t)vpage;
    if (k >= kSkills || e != EV_SELECT) return;
    if (s.mult[k] >= 5) { ui::notice("Максимальный множитель", 1200); return; }
    uint32_t cost = 10u << s.mult[k];
    if (s.tokens < cost) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Нужно очков: %lu", (unsigned long)cost);
      ui::notice(buf, 1400);
      return;
    }
    s.tokens -= cost;
    s.mult[k]++;
    ui::beep(2600, 30);
    save();
    (void)now;
  }

  static void nameFit(Canvas& c, int x, int y, const char* str, int maxW, uint8_t v) {
    if (c.textW(str, font_big) <= maxW) { c.text(x, y, str, font_big, v); return; }
    char buf[40];
    c.copyFit(buf, sizeof(buf), str, font_small, maxW);
    c.text(x, y + 3, buf, font_small, v);
  }

  void topBar(Canvas& c, const char* label, uint8_t t) {
    c.text(2, 1, label, font_small, 1);
    char b[20], line[28];
    fmtNum(b, sizeof(b), power());
    snprintf(line, sizeof(line), "%s", b);
    c.textR(103, 1, line, font_small, 1);
    ui::tabsDots(c, t, tabs(), 126, 3);
    ui::separator(c, 12);
  }

  void drawTrain(Canvas& c, uint32_t now, int8_t vp) {
    uint8_t k = (uint8_t)vp;
    if (k >= kSkills) k = 0;
    topBar(c, "ТРЕНИРОВКА", 0);
    char buf[48], pg[16];
    snprintf(pg, sizeof(pg), "%u/%u", k + 1, kSkills);
    c.textR(126, 13, pg, font_small, 1);

    c.bitmap(2, 15, spsSkillSprites[kSkill[k].sprite], BMP_COPY);
    nameFit(c, 32, 14, kSkill[k].name, 76, 1);
    double lv = level(k);
    snprintf(buf, sizeof(buf), "ур. %.0f · x%.1f", lv, 1.0 + s.mult[k] * 0.5);
    c.text(32, 31, buf, font_small, 1);
    ui::progress(c, 2, 42, 124, 6, (float)(lv - floor(lv)));

    {
      char zn[32];
      c.copyFit(zn, sizeof(zn), curZone().name, font_small, 62);
      snprintf(buf, sizeof(buf), "%s x%.0f/с", zn, trainRate());
    }
    c.text(2, 52, buf, font_small, 1);
    if (now < s.burstReady) {
      char t[16];
      fmtTime(t, sizeof(t), (s.burstReady - now) / 1000 + 1);
      c.textR(126, 52, t, font_small, 1);
    } else if (ui::blink(900)) {
      c.textR(126, 52, "рывок!", font_small, 1);
    }
  }

  void drawZones(Canvas& c, uint32_t /*now*/, int8_t vp) {
    const ZoneSet& zs = kZoneSet[s.zoneCat];
    uint8_t z = (uint8_t)vp;
    if (z >= zs.n) z = 0;
    char buf[64], hdr[20];
    snprintf(hdr, sizeof(hdr), "ЗОНЫ %u/%u", z + 1, zs.n);
    topBar(c, hdr, 1);

    double lv = level(s.zoneCat);
    bool open = lv >= zoneReq(z);
    c.bitmap(84, 14, spsSkillSprites[kSkill[s.zoneCat].sprite], BMP_COPY);
    if (!open) c.bitmap(94, 18, bmpTheirLock, BMP_MASK);   // замок автора

    c.text(2, 14, kSkill[s.zoneCat].name, font_small, 1);  // навык этой зоны
    nameFit(c, 2, 25, zs.z[z].name, 78, 1);
    snprintf(buf, sizeof(buf), "множитель x%.0f", zoneBoost(z));
    c.text(2, 40, buf, font_small, 1);
    if (open) {
      if (s.zskill == s.zoneCat && s.ztier == z) c.text(2, 52, "Тренируемся здесь", font_small, 1);
      else if (ui::blink(900)) c.text(2, 52, "ВЫБОР — тренироваться", font_small, 1);
    } else {
      snprintf(buf, sizeof(buf), "нужен ур. %.0f (есть %.0f)", zoneReq(z), lv);
      c.text(2, 52, buf, font_small, 1);
    }
  }

  void drawCatMenu(Canvas& c) {
    c.clear();
    c.text(2, 1, "КАТЕГОРИЯ ЗОН", font_small, 1);
    ui::separator(c, 12);
    for (uint8_t i = 0; i < kSkills; i++) {
      int y = 15 + i * 9;
      bool sel = (i == s.catSel);
      if (sel) c.fillRect(0, y - 1, 128, 9, 1);
      c.text(3, y, kSkill[i].name, font_small, sel ? 0 : 1);
      char b[24];
      snprintf(b, sizeof(b), "x%.0f", zoneBoost(0));
      (void)b;
    }
    if (ui::blink(700)) c.textR(126, 56, "СТИК/НАЗАД — закрыть", font_small, 1);
  }

  void drawArena(Canvas& c, uint32_t now, int8_t vp) {
    uint8_t b = (uint8_t)vp;
    if (b >= kBots) b = 0;
    topBar(c, "АРЕНА", 2);
    if (s.battlePhase) {
      uint8_t bot = s.battleBot;
      c.textC(12, kBot[bot].name, font_big, 1);
      c.bitmap(4, 26, spsSkillSprites[0], BMP_COPY);
      c.bitmap(98, 26, spsSkillSprites[1], BMP_COPY);
      float t = (now - s.battleStart) / 2600.0f;
      if (t > 1) t = 1;
      float ph = 1.0f - t * (s.battleWin ? 0.25f : 0.95f);
      float eh = 1.0f - t * (s.battleWin ? 0.95f : 0.2f);
      // полосы здоровья прямо под бойцами (подписи не нужны — стороны очевидны)
      ui::progress(c, 2, 54, 40, 5, ph);
      ui::progress(c, 86, 54, 40, 5, eh);
      if (s.battlePhase == 1) {
        if (ui::blink(200)) c.textC(34, "БОЙ!", font_small, 1);
      } else if (s.battleWin) {
        char buf[40];
        snprintf(buf, sizeof(buf), "ПОБЕДА +%lu", (unsigned long)s.battleReward);
        c.textC(34, buf, font_small, 1);
      } else {
        c.textC(34, "Поражение", font_small, 1);
      }
      return;
    }
    c.bitmap(2, 12, spsSkillSprites[b % 4], BMP_COPY);
    nameFit(c, 32, 12, kBot[b].name, 76, 1);
    char buf[64], n[20], pg[16];
    snprintf(pg, sizeof(pg), "%u/%u", b + 1, kBots);
    c.textR(126, 13, pg, font_small, 1);
    // строка характеристик
    fmtNum(n, sizeof(n), kBot[b].power);
    snprintf(buf, sizeof(buf), "сила %s · +%lu", n, (unsigned long)kBot[b].reward);
    c.text(2, 30, buf, font_small, 1);
    // очки слева, действие справа
    snprintf(buf, sizeof(buf), "у вас %lu", (unsigned long)s.tokens);
    c.text(2, 52, buf, font_small, 1);
    if (ui::blink(1000)) c.textR(126, 40, "ВЫБОР — бой", font_small, 1);
  }

  void drawShop(Canvas& c, uint32_t /*now*/, int8_t vp) {
    uint8_t k = (uint8_t)vp;
    if (k >= kSkills) k = 0;
    topBar(c, "МАГАЗИН", 3);
    c.bitmap(2, 12, spsSkillSprites[kSkill[k].sprite], BMP_COPY);
    nameFit(c, 32, 12, kSkill[k].name, 76, 1);
    char buf[64], n[20], pg[16];
    snprintf(pg, sizeof(pg), "%u/%u", k + 1, kSkills);
    c.textR(126, 13, pg, font_small, 1);
    snprintf(buf, sizeof(buf), "множитель x%.1f", 1.0 + s.mult[k] * 0.5);
    c.text(2, 30, buf, font_small, 1);
    fmtNum(n, sizeof(n), s.tokens);
    snprintf(buf, sizeof(buf), "очки %s", n);
    c.text(2, 52, buf, font_small, 1);
    if (s.mult[k] >= 5) {
      c.textR(126, 40, "максимум", font_small, 1);
    } else {
      snprintf(buf, sizeof(buf), "цена %lu", (unsigned long)(10u << s.mult[k]));
      c.textR(126, 40, buf, font_small, 1);
      if (ui::blink(900)) c.textR(126, 52, "ВЫБОР", font_small, 1);
    }
  }

  void drawStats(Canvas& c) {
    topBar(c, "ИТОГИ", 4);
    char buf[48], n[20];
    int y = 12;
    for (uint8_t i = 0; i < kSkills; i++) {
      snprintf(buf, sizeof(buf), "%s", kSkill[i].name);
      c.text(2, y, buf, font_small, 1);
      char m[24];
      snprintf(m, sizeof(m), "%3.0f ур · x%.1f", level(i), 1.0 + s.mult[i] * 0.5);
      c.textR(126, y, m, font_small, 1);
      y += 9;
    }
    (void)buf;
    (void)n;
  }
};

}  // namespace

Game* makeGameSpts() {
  static GameSpts g;
  return &g;
}
