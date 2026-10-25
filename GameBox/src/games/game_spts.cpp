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
#include <stdio.h>
#include <string.h>

namespace {

struct SkillDef { const char* name; float weight; uint8_t sprite; };
const uint8_t kSkills = 5;
const SkillDef kSkill[kSkills] = {
  {"Кулак",    1.00f, 0},
  {"Тело",     1.25f, 1},
  {"Скорость", 0.85f, 2},
  {"Прыжок",   0.75f, 3},
  {"Психика",  1.60f, 4},
};

struct ZoneDef { const char* name; double needPower; float mult; uint8_t sprite; };
const uint8_t kZones = 6;
const ZoneDef kZone[kZones] = {
  {"Пустырь",    0,      1.0f,  0},
  {"Спортзал",   100,    2.5f,  1},
  {"Кратер",     5000,   7.0f,  2},
  {"Ледник",     250000, 20.0f, 3},
  {"Пекло",      2e7,    60.0f, 4},
  {"Бездна",     5e9,    200.0f,5},
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
  uint8_t zone = 0;         // текущая зона тренировки
  uint8_t unlocked = 0;     // открыто зон (индекс последней)
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
};

Store sptsStore("gb_spts");

class GameSpts : public Game {
 public:
  const char* title() const override { return "SPTS: тренировка силы"; }
  const char* shortName() const override { return "SPTS"; }
  const Bmp& banner() const override { return bmpBannerSpts; }
  uint8_t tabs() const override { return 5; }
  const char* tabName(uint8_t t) const override {
    static const char* n[5] = {"ТРЕНИРОВКА", "ЗОНЫ", "АРЕНА", "МАГАЗИН", "ИТОГИ"};
    return n[t < 5 ? t : 0];
  }
  uint8_t vpages(uint8_t t) const override {
    if (t == 0 || t == 3) return kSkills;
    if (t == 1) return kZones;
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
      double rate = kZone[s.zone].mult * (1.0 + s.mult[s.skillSel] * 0.5);
      double add = rate * (dt / 1000.0);
      s.xp[s.skillSel] += add;
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
    // разблокировка зон по силе
    while (s.unlocked + 1 < kZones && power() >= kZone[s.unlocked + 1].needPower) {
      s.unlocked++;
      ui::notice("Новая зона тренировки!", 1500);
      save();
    }
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
      "Вверх/вниз — зона, ВЫБОР —\n"
      "тренироваться здесь. Чем\n"
      "выше зона, тем больше силы\n"
      "за секунду. Зоны\n"
      "открываются по силе.\n\n"
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
    s.zone = sptsStore.getU8("zone", 0);
    s.unlocked = sptsStore.getU8("unl", 0);
    sptsStore.getBytes("mult", s.mult, sizeof(s.mult));
    s.tokens = sptsStore.getU32("tok", 0);
    s.statFights = sptsStore.getU32("f", 0);
    s.statWins = sptsStore.getU32("w", 0);
    s.statPowerEarned = sptsStore.getF("pe", 0);
    if (s.zone > s.unlocked) s.zone = s.unlocked;
  }

  void save() {
    sptsStore.setBytes("xp", s.xp, sizeof(s.xp));
    sptsStore.setU8("zone", s.zone);
    sptsStore.setU8("unl", s.unlocked);
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
      double add = kZone[s.zone].mult * 25.0 * (1.0 + s.mult[s.skillSel] * 0.5);
      s.xp[s.skillSel] += add;
      s.statPowerEarned += add;
      s.burstReady = now + 2500;
      ui::beep(2400, 20);
    }
  }

  void tab1(Ev /*e*/, uint32_t /*now*/) {
    uint8_t z = (uint8_t)vpage;
    if (z <= s.unlocked) {
      s.zone = z;
      char buf[40];
      snprintf(buf, sizeof(buf), "Зона: %s", kZone[z].name);
      ui::notice(buf, 900);
      save();
    }
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

    snprintf(buf, sizeof(buf), "%s x%.1f/с", kZone[s.zone].name, kZone[s.zone].mult);
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
    uint8_t z = (uint8_t)vp;
    if (z >= kZones) z = 0;
    topBar(c, "ЗОНЫ", 1);
    char buf[48], n[20], pg[16];
    snprintf(pg, sizeof(pg), "%u/%u", z + 1, kZones);
    c.textR(126, 13, pg, font_small, 1);
    c.bitmap(82, 14, spsZoneSprites[kZone[z].sprite], BMP_COPY);
    nameFit(c, 2, 14, kZone[z].name, 76, 1);
    snprintf(buf, sizeof(buf), "скорость x%.2f", kZone[z].mult);
    c.text(2, 29, buf, font_small, 1);
    if (z <= s.unlocked) {
      c.text(2, 40, z == s.zone ? "Тренируемся здесь" : "Зона открыта", font_small, 1);
      if (z != s.zone && ui::blink(900)) c.text(2, 52, "ВЫБОР — перейти", font_small, 1);
    } else {
      fmtNum(n, sizeof(n), kZone[z].needPower);
      snprintf(buf, sizeof(buf), "Нужна сила %s", n);
      c.text(2, 40, buf, font_small, 1);
      c.text(2, 52, "Зона закрыта", font_small, 1);
    }
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
