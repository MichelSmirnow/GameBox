// ---------------------------------------------------------------------------
//  GameBox :: game_shot.cpp — Buckshot Roulette
//
//  Графика, интро и правила — автора GameBox (Michail Smirnow): кадры 128x64
//  из его BuckshotRoulette.ino (интро в ванной, стол с дробовиком, выстрелы,
//  экраны патронов/жизней/раундов). Механика дополнена правилами оригинала:
//  жизни 2/4/6, патроны по формуле автора, предметы 2 за раунд на игрока.
//
//  Управление:
//    ← / →   — цель (в дилера / в себя)
//    ↑ / ↓   — предмет
//    ВЫБОР   — применить предмет
//    СТИК    — выстрелить
//    НАЗАД   — выход, СБРОС — новая партия, МЕНЮ — справка
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

enum ItemId : uint8_t {
  IT_NONE = 0, IT_CIG, IT_MAG, IT_BEER, IT_SAW, IT_CUFF, IT_INV, IT_MED, IT_PHONE, IT_COUNT
};
const char* kItemName[IT_COUNT] = {"—", "Сигарета", "Лупа", "Пиво", "Пила",
                                   "Наручники", "Инвертор", "Аптечка", "Телефон"};

enum Phase : uint8_t {
  PH_INTRO = 0, PH_DISC, PH_RINTRO, PH_PLAYER, PH_DEALER, PH_ANIM,
  PH_ROUNDEND, PH_WIN, PH_LOSE
};

// Кадры интро автора: заставка, ванная (7 кадров), дилер в комнате
struct IntroStep { const Bmp* bmp; uint16_t ms; };
const IntroStep kIntro[] = {
  { &bmpShotIntro,         3600 },
  { &bmpShotToilet1,       1100 },
  { &bmpShotToilet2,       1100 },
  { &bmpShotToilet3,       1100 },
  { &bmpShotToilet4,       1100 },
  { &bmpShotToilet5,       1100 },
  { &bmpShotToilet6,        900 },
  { &bmpShotToilet7,        900 },
  { &bmpShotDealerNoEffect,1800 },
};
const uint8_t kIntroCount = sizeof(kIntro) / sizeof(kIntro[0]);

struct ShotState {
  uint8_t phase = PH_INTRO;
  uint32_t phaseStart = 0;
  uint8_t introStep = 0;

  uint8_t round = 1;
  int8_t hpP = 2, hpD = 2;
  int8_t maxHpP = 2, maxHpD = 2;

  uint8_t shells[12];
  uint8_t shellCount = 0, shellPos = 0;
  int8_t known = -1;

  uint8_t itemsP[8], itemsD[8];
  uint8_t nItemsP = 0, nItemsD = 0;
  uint8_t itemSel = 0;

  bool itemMenu = false;          // открыт список предметов
  bool cuffedD = false;
  bool sawed = false;
  uint8_t target = 0;            // 0 — в дилера, 1 — в себя

  // анимация выстрела
  uint8_t anim = 0;              // 0 — оружие, 1 — результат
  bool animDealer = false, animSelf = false, animLive = false;
  uint32_t animStart = 0;

  // пошаговое интро раунда: 0 goRounds, 1 seeRounds, 2 lifeRound, 3 roundCounter
  uint8_t rintroStep = 0;

  uint16_t money = 0;
  uint32_t shotsFired = 0;
  uint8_t wins = 0, losses = 0;
  bool gameWin = false;

  char msg[48] = "";
  uint32_t msgUntil = 0;

  uint32_t dealerTimer = 0;
};

Store shotStore("gb_shot");

class GameShot : public Game {
 public:
  const char* title() const override { return "Buckshot Roulette"; }
  const char* shortName() const override { return "Buckshot"; }
  const Bmp* cover() const override { return &bmpTheirMenu4; }
  const Bmp& banner() const override { return bmpBannerShot; }
  uint8_t tabs() const override { return 1; }
  uint8_t debugPhase() const override { return s.phase; }   // для симулятора
  const char* tabName(uint8_t) const override { return "ИГРА"; }

  void enter() override {
    shotStore.begin();
    s.wins = shotStore.getU8("win", 0);
    s.losses = shotStore.getU8("lose", 0);
    s.money = (uint16_t)shotStore.getU32("cash", 0);
    newGame(gbMillis());
  }
  void leave() override { save(); }

  void onEvent(Ev e, uint32_t now) override {
    switch (s.phase) {
      case PH_INTRO:
        if (e == EV_SELECT || e == EV_STICK || e == EV_MENU) skipIntro(now);
        return;
      case PH_DISC:
        if (e == EV_SELECT || e == EV_STICK) { s.phase = PH_RINTRO; beginRoundIntro(now); }
        return;
      case PH_RINTRO:
      case PH_ANIM:
        return;                                    // идёт анимация
      case PH_ROUNDEND:
        if (e == EV_SELECT) { if (s.round >= 3) winGame(now); else startRound(now, (uint8_t)(s.round + 1)); }
        return;
      case PH_WIN:
      case PH_LOSE:
        if (e == EV_SELECT) newGame(now);
        return;
      case PH_DEALER:
        return;
      case PH_PLAYER: {
        if (s.itemMenu) {
          if (e == EV_DOWN) { if (s.nItemsP) s.itemSel = (uint8_t)((s.itemSel + 1) % s.nItemsP); }
          else if (e == EV_UP) { if (s.nItemsP) s.itemSel = (uint8_t)((s.itemSel + s.nItemsP - 1) % s.nItemsP); }
          else if (e == EV_SELECT) usePlayerItem(now);
          else if (e == EV_BACK || e == EV_MENU) s.itemMenu = false;
          return;
        }
        if (e == EV_LEFT)  { s.target = 1; ui::beep(1500, 8); }
        else if (e == EV_RIGHT) { s.target = 0; ui::beep(1500, 8); }
        else if (e == EV_SELECT) s.itemMenu = true;
        else if (e == EV_STICK) playerShoot(now);
        return;
      }
      default: break;
    }
    if (e == EV_RESET) newGame(now);
  }

  void update(uint32_t dt, uint32_t now) override {
    switch (s.phase) {
      case PH_INTRO:
        if (now - s.phaseStart > kIntro[s.introStep].ms) {
          s.introStep++;
          s.phaseStart = now;
          if (s.introStep >= kIntroCount) { s.phase = PH_DISC; s.phaseStart = now; s.introStep = 0; }
        }
        break;
      case PH_RINTRO: {
        const uint16_t dur[4] = { 500, 1400, 900, 900 };
        if (now - s.phaseStart > dur[s.rintroStep]) {
          s.rintroStep++;
          s.phaseStart = now;
          if (s.rintroStep >= 4) { s.phase = PH_PLAYER; s.phaseStart = now; s.dealerTimer = now; }
        }
        break;
      }
      case PH_ANIM:
        if (now - s.animStart > (s.anim == 0 ? 620u : 900u)) stepAnim(now);
        break;
      case PH_DEALER:
        if (now - s.dealerTimer > 780) { s.dealerTimer = now; dealerAct(now); }
        break;
      default: break;
    }
    (void)dt;
    if (s.msg[0] && now > s.msgUntil) s.msg[0] = 0;
  }

  void draw(Canvas& c, uint32_t now, int8_t, int8_t) override {
    c.clear();
    switch (s.phase) {
      case PH_INTRO:    c.bitmap(0, 0, *kIntro[s.introStep < kIntroCount ? s.introStep : 0].bmp, BMP_COPY);
                        if (ui::blink(600)) c.textOutlined(3, 55, "ВЫБОР — пропустить", font_small, 1);
                        break;
      case PH_DISC:     drawDisclaimer(c); break;
      case PH_RINTRO:   drawRoundIntro(c); break;
      case PH_ROUNDEND: drawRoundEnd(c); break;
      case PH_WIN:      drawEnding(c, true); break;
      case PH_LOSE:     drawEnding(c, false); break;
      case PH_ANIM:     drawAnim(c); break;
      case PH_PLAYER:   if (s.itemMenu) { drawItemMenu(c); break; } drawPlay(c, now); break;
      default:          drawPlay(c, now); break;
    }
  }

  const char* help() const override {
    return
      "BUCКSHOT ROULETTE\\n"
      "Дробовик заряжен патронами\\n"
      "вперемешку: боевые ранят,\\n"
      "холостые — нет. Стреляйте в\\n"
      "дилера или в себя: холостой\\n"
      "в себя оставляет ход вам.\\n\\n"
      "ТРИ РАУНДА\\n"
      "Жизней: 2, затем 4 и 6.\\n"
      "Патронов от 2 до 10 — чем\\n"
      "дальше, тем больше.\\n\\n"
      "УПРАВЛЕНИЕ\\n"
      "← → цель, ↑ ↓ предмет,\\n"
      "ВЫБОР — предмет,\\n"
      "СТИК — выстрелить.\\n"
      "СБРОС — новая партия.\\n\\n"
      "ПРЕДМЕТЫ\\n"
      "Сигарета +1 жизнь, лупа —\\n"
      "посмотреть патрон, пиво —\\n"
      "выбросить его, пила — урон\\n"
      "x2, наручники — дилер\\n"
      "пропустит ход, инвертор —\\n"
      "перевернуть патрон,\\n"
      "аптечка — лечение или риск,\\n"
      "телефон — подсказка.";
  }

 private:
  ShotState s;

  // ------------------------------------------------------------- партии
  void newGame(uint32_t now) {
    s.phase = PH_INTRO;
    s.phaseStart = now;
    s.introStep = 0;
    s.round = 1;
    s.gameWin = false;
    s.shotsFired = 0;
    s.msg[0] = 0;
    s.phase = PH_INTRO;
  }

  void skipIntro(uint32_t now) {
    s.introStep = (uint8_t)(kIntroCount - 1);
    s.phaseStart = now - kIntro[kIntroCount - 1].ms;
    s.phase = PH_DISC;
    s.phaseStart = now;
    s.introStep = 0;
  }

  void save() {
    shotStore.setU8("win", s.wins);
    shotStore.setU8("lose", s.losses);
    shotStore.setU32("cash", s.money);
  }

  void winGame(uint32_t now) {
    s.gameWin = true;
    s.phase = PH_WIN;
    s.phaseStart = now;
    if (s.wins < 250) s.wins++;
    save();
    ui::beep(2600, 120);
  }

  void beginRoundIntro(uint32_t now) {
    startRound(now, 1);
    s.phase = PH_RINTRO;
    s.rintroStep = 0;
    s.phaseStart = now;
  }

  void startRound(uint32_t now, uint8_t r) {
    s.round = r;
    s.hpP = (int8_t)(2 * r);
    s.hpD = s.hpP;
    s.maxHpP = s.hpP;
    s.maxHpD = s.hpD;
    s.cuffedD = false;
    s.sawed = false;
    s.known = -1;
    s.target = 0;
    s.shellPos = 0;
    s.anim = 0;

    // формула автора: RoundsNumber = 2 .. (10 - (4 - Battle))
    uint8_t hi = (uint8_t)(10 - (4 - r));
    if (hi > 10) hi = 10;
    if (hi < 2) hi = 2;
    s.shellCount = (uint8_t)gbRandomRange(2, hi);
    if (s.shellCount > 12) s.shellCount = 12;
    for (uint8_t i = 0; i < s.shellCount; i++) s.shells[i] = (gbRandomRange(1, 3) == 1) ? 0 : 1;
    // гарантия: хотя бы по одному патрону каждого вида
    if (s.shellCount >= 2) {
      bool live = false, blank = false;
      for (uint8_t i = 0; i < s.shellCount; i++) (s.shells[i] ? live : blank) = true;
      if (!live) s.shells[gbRandomRange(0, s.shellCount - 1)] = 1;
      if (!blank) s.shells[gbRandomRange(0, s.shellCount - 1)] = 0;
    }

    // предметы: по 2 за раунд каждой стороне (как у автора), максимум 8
    s.nItemsP = 0;
    s.nItemsD = 0;
    uint8_t n = (uint8_t)(r * 2);
    if (n > 8) n = 8;
    dealItems(s.itemsP, s.nItemsP, n);
    dealItems(s.itemsD, s.nItemsD, n);
    s.itemSel = 0;

    s.phase = PH_RINTRO;
    s.phaseStart = now;
    s.dealerTimer = now;
    ui::beep(2200, 25);
  }

  void dealItems(uint8_t* dst, uint8_t& n, uint8_t count) {
    static const uint8_t pool[] = { IT_CIG, IT_MAG, IT_BEER, IT_SAW, IT_CUFF, IT_INV, IT_MED, IT_PHONE };
    n = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t it = pool[gbRandomRange(0, sizeof(pool) - 1)];
      bool dup = false;
      for (uint8_t k = 0; k < n; k++) if (dst[k] == it) dup = true;
      if (dup) { i--; if (gbRandomRange(0, 3) == 0) break; continue; }
      dst[n++] = it;
      if (n >= 8) break;
    }
  }

  void setMsg(const char* m) {
    strncpy(s.msg, m, sizeof(s.msg) - 1);
    s.msg[sizeof(s.msg) - 1] = 0;
    s.msgUntil = gbMillis() + 2400;
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
      s.phase = PH_ROUNDEND;
      s.phaseStart = gbMillis();
      s.money = (uint16_t)(s.money + 1);
    }
  }

  void checkDeath() {
    uint32_t now = gbMillis();
    if (s.hpP <= 0) {
      s.phase = PH_LOSE;
      s.phaseStart = now;
      if (s.losses < 250) s.losses++;
      save();
      ui::beep(400, 160);
    } else if (s.hpD <= 0) {
      if (s.round >= 3) winGame(now);
      else { s.phase = PH_ROUNDEND; s.phaseStart = now; s.money = (uint16_t)(s.money + 1); }
      ui::beep(3000, 60);
    }
  }

  void nextTurn(bool playerKeeps) {
    uint32_t now = gbMillis();
    s.anim = 0;
    s.itemMenu = false;
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
    }
  }

  // запуск анимации выстрела: 0 — кадр оружия, дальше кадр результата
  void beginShotAnim(bool byDealer, bool atSelf, bool live, bool sawed) {
    s.anim = 0;
    s.animDealer = byDealer;
    s.animSelf = atSelf;
    s.animLive = live;
    s.animStart = gbMillis();
    s.phase = PH_ANIM;
    s.phaseStart = s.animStart;
    s.sawed = false;
    s.shotsFired++;
    ui::beep(live ? 900 : 1800, live ? 90 : 30);
    (void)sawed;
  }

  void stepAnim(uint32_t now) {
    if (s.anim == 0) {          // оружие -> результат
      s.anim = 1;
      s.animStart = now;
      uint8_t dmg = 1;
      if (s.animLive) {
        if (s.animDealer) { if (!s.animSelf) s.hpP = (int8_t)(s.hpP - dmg); else s.hpD = (int8_t)(s.hpD - dmg); }
        else { if (!s.animSelf) s.hpD = (int8_t)(s.hpD - dmg); else s.hpP = (int8_t)(s.hpP - dmg); }
      }
      consumeShell();
      return;
    }
    // результат показан — решаем, что дальше
    if (s.hpP <= 0 || s.hpD <= 0) { checkDeath(); return; }
    if (s.phase == PH_ROUNDEND) return;
    bool keeps = s.animSelf && !s.animLive;      // холостой в себя — ход остаётся
    if (s.animDealer) nextTurn(keeps);           // дилер стрелял: keeps -> дилер продолжает
    else nextTurn(keeps);
  }

  void playerShoot(uint32_t now) {
    bool self = (s.target == 1);
    bool live = currentShell() == 1;
    beginShotAnim(false, self, live, s.sawed);
    (void)now;
  }

  // ------------------------------------------------------------- предметы
  void usePlayerItem(uint32_t now) {
    if (!s.nItemsP) { setMsg("Предметов нет"); return; }
    uint8_t it = s.itemsP[s.itemSel];
    if (applyItem(it, true)) {
      if (s.nItemsP == 0) s.itemMenu = false;
      for (uint8_t i = s.itemSel; i + 1 < s.nItemsP; i++) s.itemsP[i] = s.itemsP[i + 1];
      s.nItemsP--;
      if (s.itemSel >= s.nItemsP) s.itemSel = s.nItemsP ? (uint8_t)(s.nItemsP - 1) : 0;
      ui::beep(2400, 18);
      if (s.itemMenu && it != IT_BEER && it != IT_MAG) s.itemMenu = false;
    }
    (void)now;
  }

  bool applyItem(uint8_t it, bool player) {
    char buf[32];
    switch (it) {
      case IT_CIG:
        if (player) { if (s.hpP < s.maxHpP) s.hpP++; setMsg("Сигарета: +1 жизнь"); }
        else { if (s.hpD < s.maxHpD) s.hpD++; setMsg("Дилер курит: +1"); }
        return true;
      case IT_MAG:
        s.known = currentShell();
        if (player) setMsg(s.known ? "Лупа: боевой" : "Лупа: холостой");
        else setMsg("Дилер смотрит в ствол");
        return true;
      case IT_BEER: {
        bool live = currentShell() == 1;
        if (player) setMsg(live ? "Пиво: выброшен боевой" : "Пиво: выброшен холостой");
        else setMsg("Дилер пьёт пиво");
        consumeShell();
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
          if (player) { s.hpP += 2; if (s.hpP > s.maxHpP + 1) s.hpP = (int8_t)(s.maxHpP + 1); setMsg("Аптечка: +2"); }
          else { s.hpD += 2; if (s.hpD > s.maxHpD + 1) s.hpD = (int8_t)(s.maxHpD + 1); setMsg("Дилер лечится"); }
        } else {
          if (player) { s.hpP -= 1; setMsg("Просрочена! -1"); }
          else { s.hpD -= 1; setMsg("Дилеру плохо"); }
        }
        return true;
      case IT_PHONE: {
        uint8_t pos = (uint8_t)(s.shellPos + 1 + gbRandomRange(0, 1));
        if (pos >= s.shellCount) pos = s.shellPos;
        snprintf(buf, sizeof(buf), "%u-й: %s", (unsigned)(pos + 1),
                 s.shells[pos] ? "боевой" : "холостой");
        setMsg(buf);
        return true;
      }
      default: return false;
    }
  }

  // ------------------------------------------------------------- ИИ дилера
  void dealerAct(uint32_t now) {
    if (s.shellPos >= s.shellCount) { consumeShell(); return; }
    if (dealerUseItemIfNeeded(now)) return;

    uint8_t left = shellsLeft();
    bool knows = s.known >= 0;
    float liveRatio = left ? (float)liveLeft() / (float)left : 0.5f;

    bool shootSelf;
    if (knows) shootSelf = (s.known == 0);
    else if (liveRatio > 0.6f) shootSelf = false;
    else if (liveRatio < 0.4f) shootSelf = true;
    else shootSelf = gbChance(0.5f);

    bool live = currentShell() == 1;
    beginShotAnim(true, shootSelf, live, s.sawed);
  }

  bool dealerUseItemIfNeeded(uint32_t now) {
    if (!s.nItemsD) return false;
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
  // Сердечко автора: 4 линии в рамке 6x7
  // Сердечко 5x5: пустое — контуром, целое — залитое
  static void heart(Canvas& c, int x, int y, bool filled) {
    static const uint8_t full[5] = { 0x0A, 0x1F, 0x1F, 0x0E, 0x04 };
    static const uint8_t empty[5] = { 0x0A, 0x11, 0x11, 0x0A, 0x04 };
    const uint8_t* pat = filled ? full : empty;
    for (int r = 0; r < 5; r++)
      for (int b = 0; b < 5; b++)
        if (pat[r] & (0x10 >> b)) c.px(x + b, y + r, 1);
  }

  void drawHp(Canvas& c) {
    for (int i = 0; i < s.maxHpP; i++) heart(c, 4 + i * 7, 57, i < s.hpP);
    for (int i = 0; i < s.maxHpD; i++) heart(c, 117 - i * 7, 57, i < s.hpD);
  }

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

  void drawDisclaimer(Canvas& c) {
    c.bitmap(0, 0, bmpShotTable, BMP_COPY);
    c.fillRect(0, 10, 128, 44, 0);
    c.rect(0, 10, 128, 44, 1);
    c.textC(13, "18+", font_big, 1);
    c.textC(30, "Игра о выживании:", font_small, 1);
    c.textC(40, "азарт, дробовик, дилер.", font_small, 1);
    if (ui::blink(600)) c.textC(50, "ВЫБОР — принять", font_small, 1);
  }

  void drawRoundIntro(Canvas& c) {
    char buf[40];
    switch (s.rintroStep) {
      case 0: c.bitmap(0, 0, bmpShotGoRounds, BMP_COPY); break;
      case 1: c.bitmap(0, 0, bmpShotSeeRounds, BMP_COPY);
              snprintf(buf, sizeof(buf), "%u патронов", (unsigned)s.shellCount);
              c.textOutlined(2, 52, buf, font_small, 1);
              break;
      case 2: c.bitmap(0, 0, bmpShotLifeRound, BMP_COPY);
              c.fillRect(0, 45, 128, 19, 0);
              drawHp(c);
              snprintf(buf, sizeof(buf), "Жизней: %u", (unsigned)s.hpP);
              c.text(2, 46, buf, font_small, 1);
              break;
      default: c.bitmap(0, 0, bmpShotRoundCounter, BMP_COPY);
              snprintf(buf, sizeof(buf), "РАУНД %u", (unsigned)s.round);
              c.textOutlined(2, 54, buf, font_small, 1);
              break;
    }
  }

  void drawRoundEnd(Canvas& c) {
    char buf[40];
    c.bitmap(0, 0, bmpShotMoney, BMP_COPY);
    c.textOutlined(2, 2, s.hpD <= 0 ? "Дилер повержен" : "Патроны кончились", font_small, 1);
    if (s.round < 3) {
      snprintf(buf, sizeof(buf), "РАУНД %u ПРОЙДЕН", (unsigned)s.round);
      c.textC(50, buf, font_small, 1);
      if (ui::blink()) c.textC(56, "ВЫБОР — дальше", font_small, 1);
    } else {
      c.textC(50, "ВЫБОР — финал", font_small, 1);
    }
  }

  void drawEnding(Canvas& c, bool win) {
    char buf[40];
    if (win) {
      c.bitmap(0, 0, bmpShotNight, BMP_COPY);
      c.textOutlined(2, 2, "ВЫ ВЫЖИЛИ", font_big, 1);
      snprintf(buf, sizeof(buf), "Побед %u · смертей %u", (unsigned)s.wins, (unsigned)s.losses);
      c.textOutlined(2, 50, buf, font_small, 1);
      if (ui::blink(600)) c.textOutlined(2, 58, "ВЫБОР — заново", font_small, 1);
    } else {
      // как у автора: до финала — «просыпаетесь» в ванной, в финале — Dead
      c.bitmap(0, 0, s.round >= 3 ? bmpShotDead : bmpShotNight, BMP_COPY);
      c.textOutlined(2, 2, "ВЫ МЕРТВЫ", font_big, 1);
      snprintf(buf, sizeof(buf), "Раунд %u · побед %u", (unsigned)s.round, (unsigned)s.wins);
      c.textOutlined(2, 50, buf, font_small, 1);
      if (ui::blink(600)) c.textOutlined(2, 58, "ВЫБОР — заново", font_small, 1);
    }
  }

  void drawAnim(Canvas& c) {
    const Bmp* f = nullptr;
    if (s.anim == 0) {
      if (!s.animDealer) f = s.animSelf ? &bmpShotSelf : &bmpShotToDealer;
      else f = s.animSelf ? &bmpShotDealerSelf : &bmpShotToPlayer;
    } else {
      if (!s.animLive) f = &bmpShotBlank;
      else if (s.hpP <= 0 || s.hpD <= 0) f = &bmpShotDead;
      else f = &bmpShotAfterShoot;
    }
    if (f) c.bitmap(0, 0, *f, BMP_COPY);
    if (s.anim == 1 && !s.animLive && (s.hpP > 0 && s.hpD > 0))
      c.textOutlined(2, 54, "Холостой", font_small, 1);
  }

  // HUD: верхняя и нижняя чёрные панели (как в оригинале — у автора подписи
  // лежат на чёрных плашках), арт занимает середину экрана.
  void drawPlay(Canvas& c, uint32_t now) {
    char buf[48];
    c.bitmap(0, 0, bmpShotTable, BMP_COPY);

    // ---------- верхняя панель ----------
    c.fillRect(0, 0, 128, 12, 0);
    c.hline(0, 12, 128, 1);
    snprintf(buf, sizeof(buf), "Р%u %u/%u", (unsigned)s.round,
             (unsigned)shellsLeft(), (unsigned)s.shellCount);
    c.text(2, 2, buf, font_small, 1);
    if (s.nItemsP) {
      snprintf(buf, sizeof(buf), "предм %u", (unsigned)s.nItemsP);
      c.textR(126, 2, buf, font_small, 1);
    }

    // ---------- нижняя панель: жизни и цель ----------
    c.fillRect(0, 52, 128, 12, 0);
    c.hline(0, 52, 128, 1);
    for (int i = 0; i < s.maxHpP; i++) heart(c, 2 + i * 6, 55, i < s.hpP);
    for (int i = 0; i < s.maxHpD; i++) heart(c, 117 - i * 6, 55, i < s.hpD);
    if (s.phase == PH_PLAYER) {
      c.textC(54, s.target == 0 ? "дилер" : "себя", font_small, 1);
      if (ui::blink(420)) {
        if (s.target == 0) ui::triangle(c, 64, 43, 0, 3, true);   // вверх — в дилера
        else ui::triangle(c, 64, 49, 2, 3, true);                 // вниз — в себя
      }
    } else if (ui::blink(400)) {
      c.textC(54, "дилер", font_small, 1);
    }

    // ---------- сообщение поверх арта ----------
    if (s.msg[0]) {
      char n[40];
      c.copyFit(n, sizeof(n), s.msg, font_small, 100);
      int w = c.textW(n, font_small);
      int x = 64 - w / 2;
      if (x < 2) x = 2;
      c.fillRect(x - 2, 36, w + 4, 12, 0);
      c.text(x, 38, n, font_small, 1);
    }
    (void)now;
  }

  // Меню предметов (как ItemsMenu автора): список, ВЫБОР — применить
  void drawItemMenu(Canvas& c) {
    c.clear();
    c.fillRect(0, 0, 128, 10, 1);
    c.text(2, 1, "ПРЕДМЕТЫ", font_small, 0);
    c.textR(126, 1, "НАЗАД", font_small, 0);
    for (int i = 0; i < (int)s.nItemsP && i < 6; i++) {
      int y = 13 + i * 8;
      bool sel = (i == (int)s.itemSel);
      if (sel) c.fillRect(0, y - 1, 128, 9, 1);
      c.text(3, y, kItemName[s.itemsP[i]], font_small, sel ? 0 : 1);
    }
    if (!s.nItemsP) c.textC(30, "Предметов нет", font_small, 1);
  }
};

}  // namespace

Game* makeGameShot() {
  static GameShot g;
  return &g;
}
