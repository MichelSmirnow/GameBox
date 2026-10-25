// ---------------------------------------------------------------------------
//  GameBox :: game_shot.cpp — Buckshot Roulette
//
//  Управление:
//    ← / →   — выбрать цель (себя / дилера)
//    ↑ / ↓   — выбрать предмет
//    ВЫБОР   — применить предмет
//    СТИК    — выстрелить
//    НАЗАД   — выход, СБРОС — новая партия, МЕНЮ — справка
//
//  Раунды: 1) 3 патрона (2 боевых, 1 холостой), 2) 5 патронов + предметы,
//          3) 7 патронов + предметы. Жизни восстанавливаются каждый раунд.
// ---------------------------------------------------------------------------
#include "../core/game.h"
#include "../core/util.h"
#include "../core/ui.h"
#include "../core/storage.h"
#include "../gfx/gfx.h"
#include <stdio.h>
#include <string.h>

namespace {

enum ItemId : uint8_t {
  IT_NONE = 0, IT_CIG, IT_MAG, IT_BEER, IT_SAW, IT_CUFF, IT_INV, IT_MED, IT_PHONE, IT_COUNT
};
const char* kItemName[IT_COUNT] = {"—", "Сигарета", "Лупа", "Пиво", "Пила",
                                   "Наручники", "Инвертор", "Аптечка", "Телефон"};

enum Phase : uint8_t {
  PH_INTRO = 0, PH_INTRO2, PH_DISC, PH_ROUND, PH_PLAYER, PH_DEALER,
  PH_ROUNDEND, PH_WIN, PH_LOSE
};

struct ShotState {
  uint8_t phase = PH_INTRO;
  uint32_t phaseStart = 0;
  uint8_t introFrame = 0;

  uint8_t round = 1;
  int8_t hpP = 3, hpD = 3;
  int8_t maxHpP = 3, maxHpD = 3;

  uint8_t shells[8];
  uint8_t shellCount = 0, shellPos = 0;
  int8_t known = -1;            // 1 боевой, 0 холостой, -1 неизвестно

  uint8_t itemsP[8], itemsD[8];
  uint8_t nItemsP = 0, nItemsD = 0;
  uint8_t itemSel = 0;

  bool cuffedD = false;         // дилер пропустит ход
  bool sawed = false;           // следующий выстрел x2
  uint8_t target = 0;           // 0 — в дилера, 1 — в себя

  char msg[64] = "";
  uint32_t msgUntil = 0;

  uint32_t dealerTimer = 0;
  uint8_t dealerStep = 0;
  bool gameWin = false;
  uint8_t endings = 0;
  uint32_t shotsFired = 0;
};

Store shotStore("gb_shot");

class GameShot : public Game {
 public:
  const char* title() const override { return "Buckshot Roulette"; }
  const char* shortName() const override { return "Buckshot"; }
  const Bmp& banner() const override { return bmpBannerShot; }
  uint8_t tabs() const override { return 1; }
  const char* tabName(uint8_t) const override { return "ИГРА"; }

  void enter() override {
    shotStore.begin();
    s.endings = shotStore.getU8("end", 0);
    newGame(gbMillis());
  }
  void leave() override { shotStore.setU8("end", s.endings); }

  void onEvent(Ev e, uint32_t now) override {
    switch (s.phase) {
      case PH_INTRO:
      case PH_INTRO2:
      case PH_DISC:
        if (e == EV_SELECT || e == EV_STICK) advanceIntro(now);
        return;
      case PH_ROUNDEND:
        if (e == EV_SELECT) startRound(now, (uint8_t)(s.round + 1));
        return;
      case PH_WIN:
      case PH_LOSE:
        if (e == EV_SELECT) newGame(now);
        return;
      case PH_DEALER:
        return;                                  // ждём дилера
      case PH_PLAYER: {
        if (e == EV_LEFT) { s.target = 1; ui::beep(1500, 8); }
        else if (e == EV_RIGHT) { s.target = 0; ui::beep(1500, 8); }
        else if (e == EV_DOWN) { if (s.nItemsP) s.itemSel = (uint8_t)((s.itemSel + 1) % s.nItemsP); }
        else if (e == EV_UP) { if (s.nItemsP) s.itemSel = (uint8_t)((s.itemSel + s.nItemsP - 1) % s.nItemsP); }
        else if (e == EV_SELECT) usePlayerItem(now);
        else if (e == EV_STICK) shootPlayer(now);
        return;
      }
      default: break;
    }
    if (e == EV_RESET) newGame(now);
  }

  void update(uint32_t dt, uint32_t now) override {
    switch (s.phase) {
      case PH_INTRO:
        if (now - s.phaseStart > 700) { s.introFrame++; s.phaseStart = now;
          if (s.introFrame >= shotIntroCount) { s.phase = PH_DISC; s.phaseStart = now; } }
        break;
      case PH_INTRO2:
        if (now - s.phaseStart > 1200) { s.phase = PH_ROUND; s.phaseStart = now; startRound(now, 1); }
        break;
      case PH_ROUND:
        if (now - s.phaseStart > 1100) { s.phase = PH_PLAYER; s.phaseStart = now; }
        break;
      case PH_DEALER:
        if (now - s.dealerTimer > 780) { s.dealerTimer = now; dealerAct(now); }
        break;
      case PH_ROUNDEND:
        break;
      default: break;
    }
    (void)dt;
    if (s.msg[0] && now > s.msgUntil) s.msg[0] = 0;
  }

  void draw(Canvas& c, uint32_t now, int8_t, int8_t) override {
    c.clear();
    switch (s.phase) {
      case PH_INTRO: drawIntro(c); break;
      case PH_INTRO2:
      case PH_DISC: drawDisclaimer(c); break;
      case PH_ROUND: drawRoundSplash(c); break;
      case PH_ROUNDEND: drawRoundSplash(c, true); break;
      case PH_WIN: drawEnding(c, true); break;
      case PH_LOSE: drawEnding(c, false); break;
      default: drawPlay(c, now); break;
    }
  }

  const char* help() const override {
    return
      "ПРАВИЛА\n"
      "Дробовик заряжается\n"
      "патронами: боевые ранят,\n"
      "холостые — нет. Стреляйте\n"
      "в дилера или в себя\n"
      "(холостой в себя оставляет\n"
      "ход вам).\n\n"
      "ТРИ РАУНДА\n"
      "1) 3 патрона, 2) 5 и\n"
      "предметы, 3) 7 и предметы.\n"
      "Жизни восстанавливаются.\n\n"
      "УПРАВЛЕНИЕ\n"
      "← → цель, ↑ ↓ предмет,\n"
      "ВЫБОР — применить предмет,\n"
      "СТИК — выстрелить.\n"
      "СБРОС — новая партия.\n\n"
      "ПРЕДМЕТЫ\n"
      "Сигарета +1 жизнь, лупа —\n"
      "посмотреть патрон, пиво —\n"
      "выбросить патрон, пила —\n"
      "следующий выстрел x2,\n"
      "наручники — дилер\n"
      "пропустит ход, инвертор —\n"
      "перевернуть патрон,\n"
      "аптечка — лечение или\n"
      "риск, телефон — подсказка\n"
      "о патроне.";
  }

 private:
  ShotState s;

  // ------------------------------------------------------------- партии
  void newGame(uint32_t now) {
    s.phase = PH_INTRO;
    s.phaseStart = now;
    s.introFrame = 0;
    s.round = 1;
    s.gameWin = false;
    s.shotsFired = 0;
    s.msg[0] = 0;
  }

  void advanceIntro(uint32_t now) {
    if (s.phase == PH_INTRO) { s.phase = PH_DISC; s.phaseStart = now; return; }
    s.phase = PH_ROUND;
    s.phaseStart = now;
    startRound(now, 1);
  }

  void startRound(uint32_t now, uint8_t r) {
    s.round = r;
    s.phase = PH_ROUND;
    s.phaseStart = now;
    s.hpP = (r == 1) ? 3 : (r == 2 ? 4 : 5);
    s.hpD = s.hpP;
    s.maxHpP = s.hpP;
    s.maxHpD = s.hpD;
    s.cuffedD = false;
    s.sawed = false;
    s.known = -1;
    s.target = 0;
    s.shellPos = 0;

    // ---- не случайная выдача патронов (заложенный алгоритм) ----
    uint8_t live = 2, blank = 1;
    if (r == 2) { live = 3; blank = 2; }
    if (r == 3) { live = 4; blank = 3; }
    s.shellCount = live + blank;
    // первый раунд: сначала боевой, чтобы игрок понял механику
    uint8_t idx = 0;
    if (r == 1) {
      s.shells[idx++] = 1;
      s.shells[idx++] = 0;
      s.shells[idx++] = 1;
    } else {
      // перемешиваем, но гарантируем чередование в начале
      for (uint8_t i = 0; i < live; i++) s.shells[idx++] = 1;
      for (uint8_t i = 0; i < blank; i++) s.shells[idx++] = 0;
      for (int i = (int)s.shellCount - 1; i > 0; i--) {
        int j = (int)gbRandomRange(0, (uint32_t)i);
        uint8_t t = s.shells[i]; s.shells[i] = s.shells[j]; s.shells[j] = t;
      }
      // хотя бы один холостой в первой половине — чтобы был шанс на тактику
      bool early = false;
      for (uint8_t i = 0; i < s.shellCount / 2; i++) if (!s.shells[i]) early = true;
      if (!early) { uint8_t t = s.shells[0]; s.shells[0] = 0; s.shells[s.shellPos] = t; }
    }

    // ---- предметы ----
    s.nItemsP = 0;
    s.nItemsD = 0;
    if (r >= 2) {
      uint8_t n = (r == 2) ? 2 : 3;
      dealItems(s.itemsP, s.nItemsP, n);
      dealItems(s.itemsD, s.nItemsD, n);
    }
    s.itemSel = 0;
    setMsg(r == 1 ? "Оружие заряжено." : "Предметы выданы.");
    s.dealerTimer = now;
    ui::beep(2200, 25);
  }

  void dealItems(uint8_t* dst, uint8_t& n, uint8_t count) {
    static const uint8_t pool[] = {IT_CIG, IT_MAG, IT_BEER, IT_SAW, IT_CUFF, IT_INV, IT_MED, IT_PHONE};
    n = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t it = pool[gbRandomRange(0, sizeof(pool) - 1)];
      // без дублей в одной руке
      bool dup = false;
      for (uint8_t k = 0; k < n; k++) if (dst[k] == it) dup = true;
      if (dup) { i--; if (gbRandomRange(0, 3) == 0) break; continue; }
      dst[n++] = it;
    }
  }

  void setMsg(const char* m) {
    strncpy(s.msg, m, sizeof(s.msg) - 1);
    s.msg[sizeof(s.msg) - 1] = 0;
    s.msgUntil = gbMillis() + 2600;
  }

  // ------------------------------------------------------------ механика
  uint8_t currentShell() const { return s.shells[s.shellPos]; }
  uint8_t liveLeft() const {
    uint8_t n = 0;
    for (uint8_t i = s.shellPos; i < s.shellCount; i++) n += s.shells[i];
    return n;
  }
  uint8_t shellsLeft() const { return (uint8_t)(s.shellCount - s.shellPos); }

  void consumeShell() {
    s.shellPos++;
    s.known = -1;
    if (s.shellPos >= s.shellCount) {
      // патроны кончились
      if (s.round >= 3) {
        s.gameWin = true;
        s.phase = PH_WIN;
        s.phaseStart = gbMillis();
        if (s.endings < 3) { s.endings++; shotStore.setU8("end", s.endings); }
      } else {
        s.phase = PH_ROUNDEND;
        s.phaseStart = gbMillis();
      }
    }
  }

  void checkDeath() {
    uint32_t now = gbMillis();
    if (s.hpP <= 0) { s.phase = PH_LOSE; s.phaseStart = now; ui::beep(400, 120); }
    else if (s.hpD <= 0) {
      if (s.round >= 3) { s.gameWin = true; s.phase = PH_WIN; s.phaseStart = now;
        if (s.endings < 3) { s.endings++; shotStore.setU8("end", s.endings); } }
      else { s.phase = PH_ROUNDEND; s.phaseStart = now; }
      ui::beep(3000, 60);
    }
  }

  void nextTurn(bool playerKeeps) {
    uint32_t now = gbMillis();
    if (playerKeeps) { s.phase = PH_PLAYER; s.phaseStart = now; s.dealerTimer = now; }
    else if (s.cuffedD) {
      s.cuffedD = false;
      setMsg("Дилер пропускает ход");
      s.phase = PH_PLAYER;
      s.phaseStart = now;
      s.dealerTimer = now;
    } else {
      s.phase = PH_DEALER;
      s.phaseStart = now;
      s.dealerTimer = now;
      s.dealerStep = 0;
    }
  }

  void shootPlayer(uint32_t now) {
    bool self = (s.target == 1);
    uint8_t dmg = s.sawed ? 2 : 1;
    s.sawed = false;
    s.shotsFired++;
    bool live = currentShell() == 1;
    if (self) {
      if (live) { s.hpP -= dmg; setMsg("В себя: боевой!"); }
      else setMsg("Холостой! Ход ваш.");
    } else {
      if (live) { s.hpD -= dmg; setMsg("В дилера: боевой!"); }
      else setMsg("Холостой. Ход дилера.");
    }
    ui::beep(live ? 900 : 1800, live ? 80 : 30);
    consumeShell();
    if (s.phase == PH_PLAYER) checkDeath();
    if (s.phase == PH_PLAYER) nextTurn(self && !live);
    (void)now;
  }

  // ------------------------------------------------------------- предметы
  void usePlayerItem(uint32_t now) {
    if (!s.nItemsP) { setMsg("Предметов нет."); return; }
    uint8_t it = s.itemsP[s.itemSel];
    bool ok = applyItem(it, true);
    if (ok) {
      // удаляем использованный предмет
      for (uint8_t i = s.itemSel; i + 1 < s.nItemsP; i++) s.itemsP[i] = s.itemsP[i + 1];
      s.nItemsP--;
      if (s.itemSel >= s.nItemsP) s.itemSel = s.nItemsP ? (uint8_t)(s.nItemsP - 1) : 0;
      ui::beep(2400, 18);
    }
    (void)now;
  }

  bool applyItem(uint8_t it, bool player) {
    char buf[32];
    switch (it) {
      case IT_CIG:
        if (player) { if (s.hpP < s.maxHpP) s.hpP++; setMsg("Сигарета: +1"); }
        else { if (s.hpD < s.maxHpD) s.hpD++; setMsg("Дилер курит: +1"); }
        return true;
      case IT_MAG:
        s.known = currentShell();
        if (player) setMsg(s.known ? "Лупа: боевой" : "Лупа: холостой");
        else setMsg("Дилер смотрит в ствол");
        return true;
      case IT_BEER: {
        bool live = currentShell() == 1;
        if (player) setMsg(live ? "Выброшен боевой" : "Выброшен холостой");
        else setMsg("Дилер пьёт пиво");
        consumeShell();
        if (s.phase == PH_DEALER) { /* дилер продолжает */ }
        return true;
      }
      case IT_SAW:
        s.sawed = true;
        if (player) setMsg("Пила: урон x2");
        else setMsg("Дилер спилил ствол!");
        return true;
      case IT_CUFF:
        if (player) { s.cuffedD = true; setMsg("Дилер в наручниках"); }
        else setMsg("Дилер надел наручники");
        return true;
      case IT_INV:
        s.shells[s.shellPos] ^= 1;
        s.known = s.shells[s.shellPos];
        if (player) setMsg("Патрон перевёрнут");
        else setMsg("Дилер: инвертор");
        return true;
      case IT_MED:
        if (gbChance(0.5f)) {
          if (player) { s.hpP += 2; if (s.hpP > s.maxHpP + 1) s.hpP = s.maxHpP + 1; setMsg("Аптечка: +2 жизни"); }
          else { s.hpD += 2; if (s.hpD > s.maxHpD + 1) s.hpD = s.maxHpD + 1; setMsg("Дилер лечится"); }
        } else {
          if (player) { s.hpP -= 1; setMsg("Просрочена! -1"); }
          else { s.hpD -= 1; setMsg("Дилеру плохо"); }
        }
        return true;
      case IT_PHONE: {
        // подсказка: сообщает патрон 2-го или 3-го места
        uint8_t pos = s.shellPos + 1 + (uint8_t)gbRandomRange(0, 1);
        if (pos >= s.shellCount) pos = s.shellPos;
        snprintf(buf, sizeof(buf), "%u-й: %s", pos + 1, s.shells[pos] ? "боевой" : "холостой");
        setMsg(buf);
        return true;
      }
      default: return false;
    }
  }

  // ------------------------------------------------------------- ИИ дилера
  void dealerAct(uint32_t now) {
    // 1. Сначала логика предметов
    if (dealerUseItemIfNeeded(now)) return;

    // 2. Решение стрелять
    uint8_t left = shellsLeft();
    if (left == 0) { consumeShell(); if (s.phase == PH_DEALER) nextTurn(false); return; }

    bool knows = s.known >= 0;
    bool liveNow = knows ? (s.known == 1) : false;
    float liveRatio = left ? (float)liveLeft() / left : 0.5f;

    bool shootSelf;
    if (knows) shootSelf = !liveNow;              // знает точно
    else shootSelf = liveRatio < 0.4f;            // эвристика по остатку

    bool live = currentShell() == 1;              // фактический патрон
    uint8_t dmg = s.sawed ? 2 : 1;
    s.sawed = false;
    s.shotsFired++;

    if (shootSelf) {
      if (live) { s.hpD -= dmg; setMsg("Дилер: в себя, боевой"); }
      else setMsg("Дилер: щелчок");
    } else {
      if (live) { s.hpP -= dmg; setMsg("Дилер: в вас, боевой!"); }
      else setMsg("Дилер: в вас, холостой");
    }
    ui::beep(1000, 60);
    consumeShell();
    if (s.phase != PH_DEALER) return;
    checkDeath();
    if (s.phase != PH_DEALER) return;
    // холостой в себя сохраняет ход дилеру
    nextTurn(shootSelf && !live);
  }

  bool dealerUseItemIfNeeded(uint32_t now) {
    if (!s.nItemsD) return false;
    // отложенные действия: один предмет за шаг
    for (uint8_t i = 0; i < s.nItemsD; i++) {
      uint8_t it = s.itemsD[i];
      bool use = false;
      if (it == IT_CIG && s.hpD < s.maxHpD && (s.hpD <= 1 || gbChance(0.5f))) use = true;
      if (it == IT_MED && s.hpD <= s.maxHpD - 2 && gbChance(0.5f)) use = true;
      if (it == IT_MAG && s.known < 0 && shellsLeft() > 1) use = true;
      if (it == IT_BEER && s.known < 0 && shellsLeft() > 2 && gbChance(0.45f)) use = true;
      if (it == IT_CUFF && !s.cuffedD && gbChance(0.7f)) use = true;
      if (it == IT_SAW && s.known == 1 && !s.sawed) use = true;
      if (it == IT_INV && s.known == 0 && !s.sawed) use = true;
      if (it == IT_PHONE && s.known < 0 && shellsLeft() > 1 && gbChance(0.35f)) use = true;
      if (use) {
        applyItem(it, false);
        for (uint8_t k = i; k + 1 < s.nItemsD; k++) s.itemsD[k] = s.itemsD[k + 1];
        s.nItemsD--;
        s.dealerTimer = now;
        return true;
      }
    }
    return false;
  }

  // ------------------------------------------------------------ отрисовка
  void drawIntro(Canvas& c) {
    const Bmp& f = shotIntroFrames[s.introFrame < shotIntroCount ? s.introFrame : 0];
    c.bitmap(0, 0, f, BMP_COPY);
    c.textOutlined(3, 52, "GameBox", font_small, 1);
  }

  void drawDisclaimer(Canvas& c) {
    c.bitmap(0, 0, bmpDisclaimer, BMP_MASK);
    c.text(8, 5, "ОТКАЗ ОТ", font_small, 1);
    c.text(8, 17, "ОТВЕТСТВЕННОСТИ", font_small, 1);
    c.text(8, 32, "Игра о выживании, 18+", font_small, 1);
    if (ui::blink()) c.textC(46, "ВЫБОР — принять", font_small, 1);
  }

  void drawRoundSplash(Canvas& c, bool roundEnd = false) {
    char buf[40];
    if (roundEnd) {
      snprintf(buf, sizeof(buf), "РАУНД %u", s.round);
      c.textC(8, buf, font_big, 1);
      c.textC(26, "ПРОЙДЕН", font_small, 1);
      c.textC(38, "Жизни восстановлены", font_small, 1);
      if (ui::blink()) c.textC(52, "ВЫБОР — дальше", font_small, 1);
      return;
    }
    snprintf(buf, sizeof(buf), "РАУНД %u", s.round);
    c.textC(8, buf, font_big, 1);
    c.textC(26, "Оружие заряжено", font_small, 1);
    snprintf(buf, sizeof(buf), "Патронов: %u", s.shellCount);
    c.textC(38, buf, font_small, 1);
    if (s.round >= 2)
      snprintf(buf, sizeof(buf), "Жизней: %u · предметы", (unsigned)s.hpP);
    else
      snprintf(buf, sizeof(buf), "Жизней: %u", (unsigned)s.hpP);
    c.textC(50, buf, font_small, 1);
  }

  void drawEnding(Canvas& c, bool win) {
    if (win) {
      c.bitmap(0, 0, bmpShotMoney, BMP_MASK);
      c.textC(6, "ВЫ ЖИВЫ", font_big, 1);
      c.textC(26, "Дилер повержен", font_small, 1);
      char buf[40];
      snprintf(buf, sizeof(buf), "Концовок: %u из 3", s.endings);
      c.textC(36, buf, font_small, 1);
      if (ui::blink()) c.textC(52, "ВЫБОР — новая партия", font_small, 1);
    } else {
      c.textC(8, "ВЫ МЕРТВЫ", font_big, 1);
      c.bitmap(0, 0, bmpShotDead, BMP_MASK);
      c.textC(30, "Дилер снова победил", font_small, 1);
      if (ui::blink()) c.textC(52, "ВЫБОР — попробовать снова", font_small, 1);
    }
  }


  // ------------------------------------------------------------------ игровая сцена
  const Bmp& itemIcon(uint8_t it) {
    switch (it) {
      case IT_CIG: return bmpIconCig;
      case IT_MAG: return bmpIconMag;
      case IT_BEER: return bmpIconBeer;
      case IT_SAW: return bmpIconSaw;
      case IT_CUFF: return bmpIconCuff;
      case IT_INV: return bmpIconInv;
      case IT_MED: return bmpIconMed;
      case IT_PHONE: return bmpIconPhone;
      default: return bmpIconStar;
    }
  }

  void drawPlay(Canvas& c, uint32_t now) {
    char buf[48];

    // ---------- строка состояния ----------
    snprintf(buf, sizeof(buf), "Р%u", s.round);
    c.text(2, 1, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "%u/%u", shellsLeft(), s.shellCount);
    c.text(20, 1, buf, font_small, 1);
    c.textR(126, 1, (s.phase == PH_DEALER) ? "дилер" : "вы", font_small, 1);
    if (ui::blink(300)) c.fillCircle(92, 5, 2, 1);
    ui::separator(c, 11);

    // ---------- дилер ----------
    c.bitmap(2, 14, bmpDealerFace, BMP_MASK);
    for (int i = 0; i < s.maxHpD; i++) {
      if (i < s.hpD) c.fillRect(2 + i * 5, 40, 4, 3, 1);
      else c.rect(2 + i * 5, 40, 4, 3, 1);
    }
    // ---------- игрок ----------
    c.bitmap(94, 16, bmpShotgun, BMP_MASK);
    for (int i = 0; i < s.maxHpP; i++) {
      if (i < s.hpP) c.fillRect(126 - 3 - i * 5, 40, 4, 3, 1);
      else c.rect(126 - 3 - i * 5, 40, 4, 3, 1);
    }

    // ---------- патрон в центре ----------
    c.roundRect(56, 14, 16, 24, 3, 1);
    if (s.known >= 0) c.bitmap(59, 16, s.known ? bmpShotShellLive : bmpShotShellBlank, BMP_COPY);
    else c.textC(20, "?", font_big, 1);
    if (s.sawed) c.text(74, 16, "x2", font_small, 1);
    if (s.cuffedD) c.text(74, 26, "нар", font_small, 1);

    // ---------- прицел ----------
    if (s.phase == PH_PLAYER) {
      if (s.target == 0) {
        c.text(22, 38, "в дилера", font_small, 1);
        if (ui::blink(420)) ui::triangle(c, 16, 40, 3, 3, true);
      } else {
        c.text(84, 38, "в себя", font_small, 1);
        if (ui::blink(420)) ui::triangle(c, 106, 40, 1, 3, true);
      }
    }

    // ---------- сообщение ----------
    if (s.msg[0]) c.textC(50, s.msg, font_small, 1);

    // ---------- предметы игрока: 8 ячеек по 15 px ----------
    for (int slot = 0; slot < 8; slot++) {
      int x = 2 + slot * 15;
      bool has = (slot < (int)s.nItemsP);
      bool sel = has && (s.phase == PH_PLAYER) && (slot == (int)s.itemSel);
      if (sel) {
        c.fillRect(x, 52, 13, 11, 0);
        c.rect(x, 52, 13, 11, 1);
        c.bitmap(x + 3, 53, itemIcon(s.itemsP[slot]), BMP_COPY);
      } else if (has) {
        c.bitmap(x + 3, 53, itemIcon(s.itemsP[slot]), BMP_MASK);
      }
    }
    // название выбранного предмета
    if (s.phase == PH_PLAYER && s.nItemsP) {
      const char* nm = kItemName[s.itemsP[s.itemSel]];
      c.textOutlined(2, 40, nm, font_small, 1);
    }
    (void)now;
  }
};

}  // namespace

Game* makeGameShot() {
  static GameShot g;
  return &g;
}
