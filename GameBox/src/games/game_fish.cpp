// ---------------------------------------------------------------------------
//  GameBox :: game_fish.cpp — «Симулятор рыбалки»
//
//  Вкладки (джойстик влево/вправо):
//    0 УДОЧКА        — ручная рыбалка; вглубь (вниз) — прокачка удочки
//    1 ЛОДКИ         — автоловля, по одной лодке на страницу
//    2 ХОЛОДИЛЬНИК   — продажа рыбы (стик — продать всё)
//    3 ЗЕЛЬЯ         — варка и применение эффектов (стик — категории)
//    4 ЗОНЫ          — пропуска на новые водоёмы
//    5 ПРЕСТИЖ       — сброс прогресса за постоянный бонус
//
//  Управление: ВЫБОР — действие, НАЗАД — шаг назад/выход,
//              СБРОС — сбросить сессию ловли, МЕНЮ — справка,
//              СТИК — табло (в удержании на вкладке удочки).
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

// ============================== ДАННЫЕ =====================================
enum FishId : uint8_t {
  F_SHOE, F_NAVAGA, F_POLLOCK, F_LEMON, F_FLOUNDER, F_HAKE, F_COD, F_PERCH,
  F_VENDACE, F_HUMPBACK, F_CRUCIAN, F_TROUT, F_OMUL, F_DORADO, F_MUKSUN,
  F_CONGRio, F_SALMON, F_SQUID, F_TURBOT, F_PIKE, F_STURGEON, F_CARP,
  F_CATFISH, F_GARFISH, F_EEL, F_COTTIDAE, F_BELUGA, F_PARROT, F_TUNA,
  F_PADDLE, F_GUASA, F_PUFFER, F_SWORD, F_STINGRAY, F_SHARK, F_AROWANA,
  F_LATIMERIA, F_COELACANTH, F_ANGLER, F_MANTA, F_OARFISH, F_GHOST,
  F_TITAN, F_VOID, FISH_COUNT
};

struct FishDef {
  const char* name;
  uint32_t rarity;    // «1 из N» на единицу удачи
  double price;
  uint8_t zone;       // минимальная зона
  uint8_t rod;        // минимальный уровень удочки
  uint8_t sprite;     // индекс в fishSprites
  bool trash;
};

const FishDef kFish[FISH_COUNT] = {
  {"Старый башмак",     3,        1,     0, 0,  0, true},
  {"Навага",            6,        4,     0, 0,  1, false},
  {"Минтай",            10,       7,     0, 0,  2, false},
  {"Лимонема",          18,       12,    0, 0,  3, false},
  {"Камбала",           25,       15,    0, 1,  4, false},
  {"Хек",               35,       20,    0, 1,  5, false},
  {"Треска",            50,       30,    0, 2,  6, false},
  {"Окунь",             70,       45,    0, 2,  7, false},
  {"Ряпушка",           90,       60,    1, 3,  8, false},
  {"Горбуша",           120,      80,    1, 4,  9, false},
  {"Карась",            40,       25,    0, 1, 10, false},
  {"Форель",            150,      110,   1, 5, 11, false},
  {"Омуль",             200,      150,   1, 5, 12, false},
  {"Дорадо",            260,      200,   2, 6, 13, false},
  {"Муксун",            320,      250,   1, 7, 14, false},
  {"Конгер",            400,      330,   2, 7, 15, false},
  {"Лосось",            500,      420,   1, 8, 16, false},
  {"Кальмар",           650,      560,   3, 9, 17, false},
  {"Тюрбо",             800,      700,   2, 9, 18, false},
  {"Щука",              1000,     900,   1, 10, 19, false},
  {"Осётр",             1300,     1200,  2, 10, 20, false},
  {"Сазан",             1700,     1600,  2, 11, 21, false},
  {"Сом",               2200,     2100,  2, 11, 22, false},
  {"Сарган",            3000,     2900,  3, 12, 23, false},
  {"Угорь",             4000,     3900,  2, 12, 24, false},
  {"Подкаменщик",       5500,     5200,  3, 13, 25, false},
  {"Белуга",            7000,     7000,  3, 13, 26, false},
  {"Рыба-попугай",      9000,     9500,  3, 14, 27, false},
  {"Тунец",             12000,    13000, 3, 14, 28, false},
  {"Веслонос",          16000,    18000, 4, 15, 29, false},
  {"Гуаса",             22000,    25000, 4, 15, 30, false},
  {"Рыба-ёж",           30000,    34000, 4, 16, 31, false},
  {"Меч-рыба",          40000,    46000, 4, 16, 32, false},
  {"Скат",              55000,    62000, 4, 17, 33, false},
  {"Акула",             75000,    85000, 4, 17, 34, false},
  {"Аравана",           100000,   115000,4, 18, 35, false},
  {"Латимерия",         140000,   160000,5, 18, 36, false},
  {"Целакант",          200000,   230000,5, 19, 37, false},
  {"Удильщик",          280000,   320000,5, 19, 38, false},
  {"Манта",             400000,   460000,5, 20, 39, false},
  {"Сельдяной король",  600000,   700000,5, 20, 40, false},
  {"Призрачная рыба",   900000,   1000000, 5, 20, 41, false},
  {"Титанида",          2000000,  2500000, 5, 20, 42, false},
  {"Пустотный кит",     5000000,  7000000, 5, 20, 43, false},
};

const uint8_t kRodMax = 20;
uint32_t rodCost(uint8_t lvl) {                 // цена улучшения до lvl+1
  double c = 45.0;
  for (uint8_t i = 1; i < lvl; i++) c *= 2.35;
  return (uint32_t)c;
}
float rodLuck(uint8_t lvl) { return 1.0f + (lvl - 1) * 0.35f; }
uint32_t boatLuckCost(uint8_t lvl) {          // удача лодок дешевле удочки
  return (uint32_t)(rodCost(lvl) * 0.6);
}

struct BoatDef {
  const char* name;
  double price;
  float luck;
  uint8_t amount;      // рыб за тик
  uint8_t sprite;
};
const uint8_t kBoatCount = 8;
const BoatDef kBoat[kBoatCount] = {
  {"Плот",             0,          1.0f,  1, 0},
  {"Надувная",         2500,       1.6f,  1, 1},
  {"Моторка",          30000,      2.5f,  2, 2},
  {"Парусник",         450000,     4.0f,  3, 3},
  {"Катер",            6000000,    6.5f,  5, 4},
  {"Яхта",             90000000,   10.0f, 8, 5},
  {"Траулер",          1500000000, 16.0f, 12, 6},
  {"Катамаран",        30000000000.0, 25.0f, 20, 7},
};

struct ZoneDef {
  const char* name;
  double price;
  float luck;
};
const uint8_t kZoneCount = 6;
const ZoneDef kZone[kZoneCount] = {
  {"Пруд",         0,           1.0f},
  {"Река",         1200,        1.35f},
  {"Озеро",        30000,       1.8f},
  {"Море",         750000,      2.5f},
  {"Океан",        20000000,    3.4f},
  {"Бездна",       500000000,   5.0f},
};

// типы эффектов
enum PotType : uint8_t { POT_HASTE, POT_LUCK, POT_SELL };
struct PotionDef {
  const char* name;
  uint8_t cat;        // 0 скорость, 1 удача, 2 продажа, 3 эликсиры
  uint8_t type;
  float val;          // множитель
  uint16_t dur;       // секунды
  uint8_t reqFish;
  uint8_t reqCount;
};
const uint8_t kPotCount = 16;
const PotionDef kPot[kPotCount] = {
  {"Настойка скорости I",  0, POT_HASTE, 1.25f,  60,  F_NAVAGA,    3},
  {"Настойка скорости II", 0, POT_HASTE, 1.6f,   90,  F_PERCH,     5},
  {"Настойка скорости III",0, POT_HASTE, 2.4f,   120, F_SALMON,    3},
  {"Ярость щуки",          0, POT_HASTE, 3.2f,   60,  F_PIKE,      2},
  {"Приманка удачи I",     1, POT_LUCK,  1.2f,   60,  F_FLOUNDER,  4},
  {"Приманка удачи II",    1, POT_LUCK,  1.5f,   90,  F_OMUL,      3},
  {"Приманка удачи III",   1, POT_LUCK,  2.2f,   120, F_TUNA,      2},
  {"Дар глубин",           1, POT_LUCK,  3.0f,   90,  F_SHARK,     1},
  {"Торговый настой I",    2, POT_SELL,  1.3f,   60,  F_POLLOCK,   5},
  {"Торговый настой II",   2, POT_SELL,  1.8f,   90,  F_CARP,      3},
  {"Торговый настой III",  2, POT_SELL,  2.5f,   120, F_STURGEON,  2},
  {"Золотая чешуя",        2, POT_SELL,  4.0f,   90,  F_BELUGA,    1},
  {"Эликсир рыболова",     3, POT_LUCK,  1.9f,   180, F_LATIMERIA, 1},
  {"Кровавый эликсир",     3, POT_SELL,  3.0f,   180, F_COELACANTH,1},
  {"Штормовой эликсир",    3, POT_HASTE, 2.8f,   180, F_MANTA,     1},
  {"Абсолют",              3, POT_SELL,  6.0f,   300, F_TITAN,     1},
};
const char* kCatName[4] = {"СКОРОСТЬ", "УДАЧА", "ПРОДАЖА", "ЭЛИКСИРЫ"};
const char* kPrestigeTitle[6] = {"", "Новичок", "Рыбак", "Мастер", "Легенда", "Владыка глубин"};

// ============================== СОСТОЯНИЕ ==================================
struct FishState {
  double money = 0;
  uint8_t rod = 1;
  uint8_t zone = 0;
  uint8_t boats = 0x01;      // маска купленных
  uint8_t boatLuck = 1;      // прокачка удачи лодок (отдельно от удочки)
  uint8_t activeBoat = 0;
  uint8_t prestige = 0;
  uint16_t inv[FISH_COUNT];
  uint8_t potOwned[kPotCount];      // сколько сварено/в запасе
  float effHaste = 1, effLuck = 1, effSell = 1;
  uint32_t effUntil[3] = {0, 0, 0};

  // сессия ловли
  uint8_t fishPhase = 0;      // 0 покой, 1 заброс, 2 ожидание, 3 поклёвка, 4 результат
  uint32_t phaseStart = 0;
  uint32_t waitMs = 0;
  uint8_t caught = 0;         // FishId
  bool caughtIsTrash = false;
  // автоловля
  uint32_t lastTick = 0;
  double sessionEarned = 0;
  uint32_t totalCaught = 0;
};

Store fishStore("gb_fish");

// ============================== ЛОГИКА =====================================
class GameFish : public Game {
 public:
  const char* title() const override { return "Симулятор рыбалки"; }
  const char* shortName() const override { return "Рыбалка"; }
  const Bmp* cover() const override { return &bmpTheirMenu1; }
  const Bmp& banner() const override { return bmpBannerFish; }
  uint8_t tabs() const override { return 6; }

  const char* tabName(uint8_t t) const override {
    static const char* n[6] = {"УДОЧКА", "ЛОДКИ", "ХОЛОДИЛЬНИК", "ЗЕЛЬЯ", "ЗОНЫ", "ПРЕСТИЖ"};
    return n[t < 6 ? t : 0];
  }

  uint8_t vpages(uint8_t t) const override {
    switch (t) {
      case 0: return 2;                 // ловля / прокачка
      case 1: return kBoatCount + 1;    // по лодке на страницу + прокачка удачи
      case 4: return kZoneCount;        // по зоне на страницу
      default: return 1;
    }
  }

  bool modal() const override { return catMenu; }
  uint8_t debugPhase() const override { return (uint8_t)(1 + s.fishPhase + (stickBoard ? 10 : 0)); }

  void enter() override {
    fishStore.begin();
    load();
    s.fishPhase = 0;
    s.lastTick = gbMillis();
  }

  void leave() override { save(); }

  void onEvent(Ev e, uint32_t now) override {
    // Баннер «не хватает денег» закрывается повторным нажатием
    if (ui::noticeActive()) { ui::noticeDismiss(); if (e == EV_SELECT) return; }
    // Сначала гасим оверлеи
    if (catMenu) {
      if (e == EV_UP && catSel > 0) catSel--;
      else if (e == EV_DOWN && catSel < 3) catSel++;
      else if (e == EV_SELECT) { potCat = catSel; catMenu = false; potSel = 0; }
      else if (e == EV_BACK || e == EV_STICK) catMenu = false;
      return;
    }
    // Табло (деньги, рыба, эффекты) — с любой страницы, кнопкой СБРОС
    // СБРОС — табло (переключатель), любая другая кнопка его закрывает
    if (stickBoard) { stickBoard = false; if (e != EV_RESET) return; }
    if (e == EV_RESET) { stickBoard = true; return; }
    if (e == EV_STICK) {
      if (tab == 2) { sellAll(); return; }                       // холодильник: продать всё
      if (tab == 3) { catMenu = true; catSel = potCat; return; } // зелья: категории
      return;
    }
    switch (tab) {
      case 0: tab0(e, now); break;
      case 1: tab1(e, now); break;
      case 2: tab2(e, now); break;
      case 3: tab3(e, now); break;
      case 4: tab4(e, now); break;
      case 5: tab5(e, now); break;
      default: break;
    }
  }

  void update(uint32_t dt, uint32_t now) override {
    // эффекты зелий
    for (int i = 0; i < 3; i++)
      if (s.effUntil[i] && now >= s.effUntil[i]) {
        s.effUntil[i] = 0;
        if (i == 0) s.effHaste = 1;
        if (i == 1) s.effLuck = 1;
        if (i == 2) s.effSell = 1;
      }
    // автоловля лодкой: тик раз в 1.8 c (ускоряется зельями)
    uint32_t tick = (uint32_t)(1800.0f / totalHaste());
    if (tick < 300) tick = 300;
    if (s.boats && s.fishPhase != 1 && now - s.lastTick >= tick) {
      s.lastTick = now;
      boatTick();
    }
    // фазы ручной ловли
    if (s.fishPhase == 1 && now - s.phaseStart > 320) {
      s.fishPhase = 2;
      s.phaseStart = now;
      s.waitMs = (uint32_t)((900 + gbRandomRange(0, 3200)) / totalHaste());
      if (s.waitMs < 300) s.waitMs = 300;
    } else if (s.fishPhase == 2 && now - s.phaseStart > s.waitMs) {
      s.fishPhase = 3;                 // поклёвка
      s.phaseStart = now;
      ui::beep(2600, 40);
    } else if (s.fishPhase == 3 && now - s.phaseStart > 1300) {
      s.fishPhase = 0;                 // сорвалась
      s.phaseStart = now;
      ui::notice("Рыба сорвалась!", 900);
    } else if (s.fishPhase == 4 && now - s.phaseStart > 1500) {
      s.fishPhase = 0;
      s.phaseStart = now;
    }
    (void)dt;
  }

  void draw(Canvas& c, uint32_t now, int8_t t, int8_t vp) override {
    c.clear();
    if (stickBoard) { drawBoard(c); return; }
    switch (t) {
      case 0: drawRod(c, now, vp); break;
      case 1: drawBoats(c, now, vp); break;
      case 2: drawFridge(c, now); break;
      case 3: drawPotions(c, now); break;
      case 4: drawZones(c, now, vp); break;
      case 5: drawPrestige(c, now); break;
    }
    if (catMenu) drawCatMenu(c);
    ui::noticeDraw(c);
    (void)now;
  }

  const char* help() const override {
    return
      "РЫБАЛКА (удочка)\n"
      "ВЫБОР — забросить. Когда\nпоплавок дёрнется, ВЫБОР\nснова — подсечь.\n"
      "СБРОС — табло: деньги,\nрыба, эффекты (с любой\nстраницы).\n"
      "Джойстик вниз — прокачка\nудочки, влево/вправо —\nдругие вкладки.\n\n"
      "ЛОДКИ\n"
      "Лодки рыбачат сами, пока\nигра открыта. Вверх/вниз —\nвыбрать лодку, ВЫБОР —\nкупить. Цена растёт, как и\nудача с уловом.\n\n"
      "ХОЛОДИЛЬНИК\n"
      "Вверх/вниз — вид рыбы,\nВЫБОР — продать вид,\nСТИК — продать всю рыбу.\n\n"
      "УДАЧА\n"
      "Удочка и лодки качаются\nотдельно: на последней\nстранице «ЛОДКИ» — удача\nлодок. Удочка в той же\nзоне ловит лучше всегда.\n\n"
      "ЗЕЛЬЯ\n"
      "Вверх/вниз — зелье,\nВЫБОР — варить и сразу\nприменять, СТИК — выбор\nкатегории. Эффекты\nскладываются по времени.\n\n"
      "ЗОНЫ\n"
      "Пропуск открывает более\nредкую рыбу. Вверх/вниз —\nзона, ВЫБОР — купить.\n\n"
      "ПРЕСТИЖ\n"
      "Доступен на последней зоне\nпри больших деньгах.\nСбрасывает прогресс, даёт\n+50% к продаже навсегда\nи новое звание (до 5).";
  }

 private:
  // название крупным шрифтом, если влезает, иначе мелким
  static void nameFit(Canvas& c, int x, int y, const char* str, int maxW, uint8_t v) {
    if (c.textW(str, font_big) <= maxW) { c.text(x, y, str, font_big, v); return; }
    char buf[40];
    c.copyFit(buf, sizeof(buf), str, font_small, maxW);
    c.text(x, y + 3, buf, font_small, v);
  }

  FishState s;
  bool stickBoard = false;
  bool catMenu = false;
  uint8_t catSel = 0, potCat = 0, potSel = 0;
  uint8_t fridgeSel = 0;

  // ------------------------------------------------------------------ утиль
  float totalHaste() const { return s.effHaste * (1.0f + s.prestige * 0.1f); }
  // Общая удача: зелья, престиж и зона — действуют и на удочку, и на лодки.
  float commonLuck() const { return s.effLuck * kZone[s.zone].luck * (1.0f + s.prestige * 0.1f); }
  // Удача удочки — своя прокачка (вкладка «УДОЧКА»).
  float rodQuality() const { return rodLuck(s.rod); }
  // Удача лодок — ОТДЕЛЬНАЯ прокачка (вкладка «ЛОДКИ», последняя страница).
  // Она не наследует удачу удочки и всегда ниже: удочка в той же зоне ловит
  // более редкую рыбу, лодки берут количеством.
  float boatQuality() const {
    float own = 1.0f + (s.boatLuck > 0 ? (s.boatLuck - 1) * 0.25f : 0.0f);
    float cap = rodQuality() * 0.75f;
    return own < cap ? own : cap;
  }
  float totalLuck() const { return rodQuality() * commonLuck(); }
  float totalBoatLuck() const { return boatQuality() * commonLuck(); }

  // Спрайт рыбы: 38 видов — графика автора, новые виды — мои спрайты
  static const Bmp& fishArt(uint8_t id) {
    uint8_t sp = kFish[id].sprite;
    if (sp < 38) return theirFish[sp];
    return fishSprites[sp];
  }
  float sellMult() const { return (1.0f + s.prestige * 0.5f) * s.effSell; }
  double fishPrice(uint8_t id) const { return kFish[id].price * sellMult(); }
  uint32_t invTotal() const {
    uint32_t n = 0;
    for (uint8_t i = 0; i < FISH_COUNT; i++) n += s.inv[i];
    return n;
  }
  double invValue() const {
    double v = 0;
    for (uint8_t i = 0; i < FISH_COUNT; i++) v += s.inv[i] * fishPrice(i);
    return v;
  }

  // ------------------------------------------------------------- сохранение
  void load() {
    s.money = fishStore.getF("money", 0);
    s.rod = fishStore.getU8("rod", 1);
    s.zone = fishStore.getU8("zone", 0);
    s.boats = fishStore.getU8("boats", 1);
    s.activeBoat = fishStore.getU8("aboat", 0);
    s.boatLuck = fishStore.getU8("bluck", 1);
    if (s.boatLuck < 1) s.boatLuck = 1;
    if (s.boatLuck > kRodMax) s.boatLuck = kRodMax;
    s.prestige = fishStore.getU8("prest", 0);
    s.totalCaught = fishStore.getU32("tc", 0);
    fishStore.getBytes("inv", s.inv, sizeof(s.inv));
    fishStore.getBytes("pot", s.potOwned, sizeof(s.potOwned));
    if (s.rod < 1) s.rod = 1;
    if (s.rod > kRodMax) s.rod = kRodMax;
    if (s.zone >= kZoneCount) s.zone = 0;
    if (s.activeBoat >= kBoatCount) s.activeBoat = 0;
  }

  void save() {
    fishStore.setF("money", (float)s.money);
    fishStore.setU8("rod", s.rod);
    fishStore.setU8("zone", s.zone);
    fishStore.setU8("boats", s.boats);
    fishStore.setU8("aboat", s.activeBoat);
    fishStore.setU8("bluck", s.boatLuck);
    fishStore.setU8("prest", s.prestige);
    fishStore.setU32("tc", s.totalCaught);
    fishStore.setBytes("inv", s.inv, sizeof(s.inv));
    fishStore.setBytes("pot", s.potOwned, sizeof(s.potOwned));
  }

  // ------------------------------------------------------------------ ловля
  uint8_t pickFish(bool boatMode) {
    // Идём от самых редких: проверяем шанс = luck * 100 / rarity
    float luck = boatMode ? totalBoatLuck() : totalLuck();
    for (int i = FISH_COUNT - 1; i >= 0; i--) {
      const FishDef& f = kFish[i];
      if (f.zone > s.zone) continue;
      if (f.rod > s.rod) continue;
      float p = luck * 100.0f / (float)f.rarity;
      if (p > 0.95f) p = 0.95f;
      if (gbChance(p)) return (uint8_t)i;
    }
    // ничего не поймали — самое частое из разрешённых
    for (uint8_t i = 0; i < FISH_COUNT; i++) {
      if (kFish[i].zone <= s.zone && kFish[i].rod <= s.rod) return i;
    }
    return F_SHOE;
  }

  void catchFish(uint8_t id, bool silent) {
    if (s.inv[id] < 65000) s.inv[id]++;
    s.totalCaught++;
    s.caught = id;
    if (!silent) ui::beep(3000, 25);
  }

  void boatTick() {
    uint8_t n = kBoat[s.activeBoat].amount;
    for (uint8_t i = 0; i < n; i++) catchFish(pickFish(true), true);
    // лодки тоже тратят "тик" на удачу — сохраняем прогресс изредка
    static uint16_t counter = 0;
    if (++counter > 40) { counter = 0; save(); }
  }

  void sellAll() {
    double v = invValue();
    if (v <= 0) { ui::notice("Холодильник пуст", 1200); return; }
    s.money += v;
    s.sessionEarned += v;
    memset(s.inv, 0, sizeof(s.inv));
    char buf[48];
    char m[20];
    fmtNum(m, sizeof(m), v);
    snprintf(buf, sizeof(buf), "Продано всё: +%s ₽", m);
    ui::notice(buf, 1400);
    save();
  }

  void sellOne(uint8_t id) {
    if (!s.inv[id]) { ui::notice("Такой рыбы нет", 1000); return; }
    double v = fishPrice(id) * s.inv[id];
    s.money += v;
    s.sessionEarned += v;
    char buf[64], m[20];
    fmtNum(m, sizeof(m), v);
    snprintf(buf, sizeof(buf), "%s x%u → +%s ₽", kFish[id].name, s.inv[id], m);
    s.inv[id] = 0;
    ui::notice(buf, 1200);
    save();
  }

  // -------------------------------------------------------------- вкладки
  void tab0(Ev e, uint32_t now) {
    if (vpage == 1) {   // прокачка удочки
      if (e == EV_SELECT) {
        if (s.rod >= kRodMax) { ui::notice("Удочка максимальна", 1200); return; }
        uint32_t cost = rodCost(s.rod);
        if (s.money < cost) { needMoney(cost - (uint32_t)s.money); return; }
        s.money -= cost;
        s.rod++;
        ui::beep(2400, 20);
        char buf[48];
        snprintf(buf, sizeof(buf), "Удочка: уровень %u", s.rod);
        ui::notice(buf, 1200);
        save();
      }
      return;
    }
    if (e == EV_SELECT) {
      switch (s.fishPhase) {
        case 0: s.fishPhase = 1; s.phaseStart = now; ui::beep(1500, 15); break;
        case 3: {                        // подсекли
          uint8_t id = pickFish(false);
          catchFish(id, false);
          s.fishPhase = 4;
          s.phaseStart = now;
          break;
        }
        case 1: case 2: s.fishPhase = 0; break;   // отмена
        default: break;
      }
    }
  }

  void tab1(Ev e, uint32_t /*now*/) {
    uint8_t idx = (uint8_t)vpage;
    if (idx == kBoatCount) {                    // страница «удача лодок»
      if (e != EV_SELECT) return;
      if (s.boatLuck >= kRodMax) { ui::notice("Удача лодок максимальна", 1200); return; }
      uint32_t cost = boatLuckCost(s.boatLuck);
      if (s.money < cost) { needMoney(cost - (uint32_t)s.money); return; }
      s.money -= cost;
      s.boatLuck++;
      ui::beep(2400, 20);
      char buf[48];
      snprintf(buf, sizeof(buf), "Удача лодок x%.2f", boatQuality());
      ui::notice(buf, 1200);
      save();
      return;
    }
    if (idx >= kBoatCount) idx = 0;
    if (e == EV_SELECT) {
      bool owned = s.boats & (1 << idx);
      if (owned) {
        s.activeBoat = idx;
        char buf[48];
        snprintf(buf, sizeof(buf), "Активна: %s", kBoat[idx].name);
        ui::notice(buf, 1000);
        save();
      } else {
        uint32_t price = (uint32_t)kBoat[idx].price;
        if (s.money < kBoat[idx].price) { needMoney((uint32_t)(kBoat[idx].price - s.money)); return; }
        s.money -= kBoat[idx].price;
        s.boats |= (uint8_t)(1 << idx);
        s.activeBoat = idx;
        char buf[48];
        snprintf(buf, sizeof(buf), "Куплена: %s", kBoat[idx].name);
        ui::notice(buf, 1300);
        ui::beep(2600, 30);
        save();
        (void)price;
      }
    }
  }

  void tab2(Ev e, uint32_t /*now*/) {
    // список видов, которые уже попадались
    if (e == EV_UP) { if (fridgeSel > 0) fridgeSel--; }
    else if (e == EV_DOWN) {
      uint8_t maxIdx = 0;
      for (uint8_t i = 0; i < FISH_COUNT; i++) if (s.inv[i]) maxIdx = i;
      if (fridgeSel < maxIdx) fridgeSel++;
    } else if (e == EV_SELECT) sellOne(fridgeSel);
  }

  void tab3(Ev e, uint32_t now) {
    // список зелий текущей категории
    uint8_t first = 0, n = 0;
    uint8_t ids[kPotCount];
    for (uint8_t i = 0; i < kPotCount; i++)
      if (kPot[i].cat == potCat) ids[n++] = i;
    (void)first;
    if (n == 0) return;
    if (potSel >= n) potSel = n - 1;
    if (e == EV_UP && potSel > 0) potSel--;
    else if (e == EV_DOWN && potSel + 1 < n) potSel++;
    else if (e == EV_SELECT) brew(ids[potSel], now);
  }

  void tab4(Ev e, uint32_t /*now*/) {
    uint8_t idx = (uint8_t)vpage;
    if (idx >= kZoneCount) return;
    if (e != EV_SELECT) return;
    if (idx <= s.zone) {
      s.zone = idx;
      char buf[40];
      snprintf(buf, sizeof(buf), "Зона: %s", kZone[idx].name);
      ui::notice(buf, 1000);
      save();
      return;
    }
    if (idx != s.zone + 1) { ui::notice("Сначала купите предыдущую зону", 1500); return; }
    if (s.money < kZone[idx].price) { needMoney((uint32_t)(kZone[idx].price - s.money)); return; }
    s.money -= kZone[idx].price;
    s.zone = idx;
    ui::beep(2800, 35);
    char buf[40];
    snprintf(buf, sizeof(buf), "Открыто: %s", kZone[idx].name);
    ui::notice(buf, 1400);
    save();
  }

  void tab5(Ev e, uint32_t now) {
    if (e != EV_SELECT) return;
    if (s.prestige >= 5) { ui::notice("Максимум престижа", 1200); return; }
    double need = prestigeNeed();
    if (s.zone < kZoneCount - 1) { ui::notice("Нужна зона «Бездна»", 1400); return; }
    if (s.money < need) { needMoney((uint32_t)(need - s.money)); return; }
    s.prestige++;
    s.money = 0;
    s.rod = 1;
    s.zone = 0;
    s.boats = 0x01;
    s.activeBoat = 0;
    memset(s.inv, 0, sizeof(s.inv));
    memset(s.potOwned, 0, sizeof(s.potOwned));
    s.effHaste = s.effLuck = s.effSell = 1;
    s.effUntil[0] = s.effUntil[1] = s.effUntil[2] = 0;
    char buf[48];
    snprintf(buf, sizeof(buf), "Престиж %u: %s", s.prestige, kPrestigeTitle[s.prestige]);
    ui::notice(buf, 1800);
    ui::beep(3200, 60);
    save();
    (void)now;
  }

  double prestigeNeed() const {
    static const double k[5] = {1e6, 1e9, 1e12, 1e15, 1e18};
    return k[s.prestige < 5 ? s.prestige : 4];
  }

  void needMoney(uint32_t diff) {
    char buf[64], m[20];
    fmtNum(m, sizeof(m), diff);
    snprintf(buf, sizeof(buf), "Не хватает %s ₽", m);
    ui::notice(buf, 0, true);       // висит, пока не нажмёте ВЫБОР ещё раз
  }

  void brew(uint8_t id, uint32_t now) {
    const PotionDef& p = kPot[id];
    if (s.inv[p.reqFish] < p.reqCount) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Нужно: %s x%u", kFish[p.reqFish].name, p.reqCount);
      ui::notice(buf, 1600);
      return;
    }
    s.inv[p.reqFish] -= p.reqCount;
    if (s.potOwned[id] < 250) s.potOwned[id]++;
    applyPotion(id, now);
    char buf[48];
    snprintf(buf, sizeof(buf), "Приготовлено: %s", p.name);
    ui::notice(buf, 1300);
    save();
  }

  void applyPotion(uint8_t id, uint32_t now) {
    const PotionDef& p = kPot[id];
    switch (p.type) {
      case POT_HASTE: s.effHaste = p.val; s.effUntil[0] = now + p.dur * 1000UL; break;
      case POT_LUCK:  s.effLuck = p.val;  s.effUntil[1] = now + p.dur * 1000UL; break;
      case POT_SELL:  s.effSell = p.val;  s.effUntil[2] = now + p.dur * 1000UL; break;
    }
  }

  // ------------------------------------------------------------- отрисовка
  void topBar(Canvas& c, const char* tabLabel, uint8_t t) {
    c.text(2, 1, tabLabel, font_small, 1);
    char b[20], money[28];
    fmtNum(b, sizeof(b), s.money);
    snprintf(money, sizeof(money), "%s ₽", b);
    c.textR(103, 1, money, font_small, 1);
    ui::tabsDots(c, t, tabs(), 126, 3);
    ui::separator(c, 12);
  }

  void drawRod(Canvas& c, uint32_t now, int8_t vp) {
    char buf[48];
    if (vp != 0) {
      topBar(c, "УДОЧКА: ПРОКАЧКА", 0);
      snprintf(buf, sizeof(buf), "Уровень %u из %u", s.rod, kRodMax);
      c.text(2, 15, buf, font_small, 1);
      ui::progress(c, 2, 26, 124, 8, (float)s.rod / kRodMax);
      snprintf(buf, sizeof(buf), "Удача удочки x%.2f", rodLuck(s.rod));
      c.text(2, 38, buf, font_small, 1);
      if (s.rod < kRodMax) {
        char cost[20];
        fmtNum(cost, sizeof(cost), rodCost(s.rod));
        snprintf(buf, sizeof(buf), "Цена %s ₽", cost);
        c.text(2, 47, buf, font_small, 1);
        if (ui::blink()) c.textR(126, 52, "ВЫБОР — улучшить", font_small, 1);
      } else {
        c.textC(52, "Максимальный уровень", font_small, 1);
      }
      return;
    }

    topBar(c, "УДОЧКА", 0);

    // ---------- строка зоны (в момент поимки её заменяет результат) ----------
    if (s.fishPhase != 4) {
      snprintf(buf, sizeof(buf), "%s", kZone[s.zone].name);
      c.text(2, 13, buf, font_small, 1);
      snprintf(buf, sizeof(buf), "удача x%.1f", totalLuck());
      c.textR(126, 13, buf, font_small, 1);
    }

    // ---------- вода ----------
    const int waterY = 34;
    // небо: солнце (ниже строки текста, чтобы не задевать надписи)
    c.circle(113, 28, 3, 1);
    for (int i = 0; i < 3; i++) c.px(113 + (i - 1) * 6, 28, 1);
    // горизонт
    c.hline(0, waterY, 128, 1);
    // вода: редкие штрихи-волны, ниже — чистая полоса для подсказки
    for (int i = 0; i < 4; i++) {
      int wy = waterY + 2 + i * 3;
      for (int x = (i % 2) * 5; x < 128; x += 12) c.hline(x, wy, 4, 1);
    }
    for (int y = waterY + 1; y < waterY + 12; y += 2)
      for (int x = ((y / 2) % 3); x < 128; x += 9) c.px(x, y, 1);

    // ---------- рыбак ----------
    c.fillCircle(6, 26, 2, 1);                 // голова
    c.line(6, 28, 6, waterY - 1, 1);           // тело
    c.line(4, waterY - 1, 8, waterY - 1, 1);   // ноги

    // ---------- снасть ----------
    const int tipX = 30, tipY = 25;
    c.line(8, 31, tipX, tipY, 1);              // удилище
    c.line(9, 32, tipX + 1, tipY + 1, 1);
    int bobX = 62, bobY = waterY + 1;
    switch (s.fishPhase) {
      case 0:
        c.line(tipX, tipY, tipX + 3, tipY + 8, 0);
        break;
      case 1: {                                 // заброс
        int t = (int)((now - s.phaseStart) * 20 / 320);
        if (t > 20) t = 20;
        c.line(tipX, tipY, tipX + 3 + t * 2, tipY + 8 - t, 1);
        break;
      }
      case 3:                                   // поклёвка
        bobY += 2;
        c.line(tipX, tipY, bobX, bobY - 2, 1);
        c.fillCircle(bobX, bobY, 2, 1);
        for (int i = 0; i < 3; i++) {
          int r = 4 + i * 3;
          c.circle(bobX, bobY, r, 1);
        }
        break;
      default:                                  // ожидание
        c.line(tipX, tipY, bobX, bobY - 2, 1);
        c.fillCircle(bobX, bobY, 2, 1);
        break;
    }

    // ---------- пойманная рыба ----------
    if (s.fishPhase == 4) {
      const Bmp& im = fishArt(s.caught);
      int fx = 66;
      int fy = waterY - im.h - 4;
      if (fy < 15) fy = 15;
      c.bitmap(fx, fy, im, BMP_COPY);
      char pr[20], p2[26];
      fmtNum(pr, sizeof(pr), fishPrice(s.caught));
      snprintf(p2, sizeof(p2), "+%s ₽", pr);
      char nm[40];
      c.copyFit(nm, sizeof(nm), kFish[s.caught].name, font_small, 72);
      c.textOutlined(2, 13, nm, font_small, 1);
      c.textR(126, 13, p2, font_small, 1);
      // всплеск
      for (int i = 0; i < 5; i++) c.px(fx - 4 + i * 4, waterY - 2 - (i % 2) * 3, 1);
    }

    // ---------- подсказка ----------
    const char* hint = "ВЫБОР — забросить";
    if (s.fishPhase == 2) hint = "Ждём поклёвку...";
    if (s.fishPhase == 3) hint = "ВЫБОР — подсечь!";
    if (s.fishPhase == 4) hint = "ВЫБОР — ещё раз";
    if (ui::blink() || s.fishPhase == 2) c.textC(52, hint, font_small, 1);
  }

  void drawBoats(Canvas& c, uint32_t now, int8_t vp) {
    uint8_t idx = (uint8_t)vp;
    if (idx == kBoatCount) {                    // прокачка удачи лодок
      char buf[48], cost[20];
      topBar(c, "УДАЧА", 1);
      c.bitmap(70, 13, bmpTheirBoat, BMP_MASK);
      snprintf(buf, sizeof(buf), "ур. %u/%u", s.boatLuck, kRodMax);
      c.text(2, 15, buf, font_small, 1);
      ui::progress(c, 2, 25, 62, 8, (float)s.boatLuck / kRodMax);
      snprintf(buf, sizeof(buf), "удача x%.2f", boatQuality());
      c.text(2, 36, buf, font_small, 1);
      if (s.boatLuck < kRodMax) {
        fmtNum(cost, sizeof(cost), boatLuckCost(s.boatLuck));
        snprintf(buf, sizeof(buf), "цена %s ₽", cost);
        c.text(2, 48, buf, font_small, 1);
        if (ui::blink()) c.textR(126, 48, "ВЫБОР — +", font_small, 1);
      } else {
        c.text(2, 48, "максимум", font_small, 1);
      }
      (void)now;
      return;
    }
    if (idx >= kBoatCount) idx = 0;
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "ЛОДКИ %u/%u", idx + 1, kBoatCount);
    topBar(c, hdr, 1);
    bool owned = s.boats & (1 << idx);
    const BoatDef& b = kBoat[idx];
    char buf[64];

    // арт автора (судно 58x37) слева, характеристики справа
    const Bmp& bm = (idx == 0) ? bmpTheirBoat
                              : ((b.sprite < boatSpriteCount) ? boatSprites[b.sprite] : boatSprites[0]);
    int by = 26 - bm.h / 2;
    if (by < 14) by = 14;
    c.bitmap(2, by, bm, BMP_COPY);
    for (int x = 0; x < bm.w; x += 4) c.px(2 + x + ((x / 4) % 2), by + bm.h + 1, 1);

    char nm[40];
    c.copyFit(nm, sizeof(nm), b.name, font_small, 60);
    c.text(64, 15, nm, font_small, 1);
    snprintf(buf, sizeof(buf), "удача x%.1f", b.luck);
    c.text(64, 27, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "%u рыб/тик", b.amount);
    c.text(64, 38, buf, font_small, 1);

    if (!owned) {
      char money[20];
      fmtNum(money, sizeof(money), b.price);
      snprintf(buf, sizeof(buf), "цена %s ₽", money);
      c.text(2, 52, buf, font_small, 1);
      if (ui::blink(900)) c.textR(126, 52, "ВЫБОР — купить", font_small, 1);
    } else if (idx == s.activeBoat) {
      c.text(2, 52, "рыбачит сейчас", font_small, 1);
    } else if (ui::blink(900)) {
      c.text(2, 52, "ВЫБОР — активировать", font_small, 1);
    } else {
      c.text(2, 52, "лодка в доке", font_small, 1);
    }
    (void)now;
  }

  void drawFridge(Canvas& c, uint32_t now) {
    topBar(c, "ХОЛОД.", 2);
    c.bitmap(98, 14, bmpTheirFridge, BMP_MASK);
    char buf[64], val[20];
    fmtNum(val, sizeof(val), invValue());

    if (invTotal() == 0) {
      c.textC(22, "Пусто", font_big, 1);
      c.textC(42, "Забросьте удочку —", font_small, 1);
      c.textC(52, "рыба сама придёт.", font_small, 1);
      return;
    }
    // шапка: сумма
    snprintf(buf, sizeof(buf), "Всего %u · %s ₽", invTotal(), val);
    c.text(2, 13, buf, font_small, 1);

    uint8_t lastIdx = 0;
    for (uint8_t i = 0; i < FISH_COUNT; i++) if (s.inv[i]) lastIdx = i;
    if (fridgeSel > lastIdx) fridgeSel = lastIdx;
    uint8_t first = (fridgeSel > 2) ? (uint8_t)(fridgeSel - 2) : 0;
    int y = 25;
    uint8_t shown = 0;
    for (uint8_t i = first; i < FISH_COUNT && shown < 3; i++) {
      if (!s.inv[i]) continue;
      bool sel = (i == fridgeSel);
      if (sel) c.fillRoundRect(1, y - 1, 126, 12, 3, 1);
      c.text(4, y + 1, kFish[i].name, font_small, sel ? 0 : 1);
      char pr[20], cnt[10];
      fmtNum(pr, sizeof(pr), fishPrice(i));
      snprintf(cnt, sizeof(cnt), "x%u", s.inv[i]);
      c.textR(104, y + 1, pr, font_small, sel ? 0 : 1);
      c.textR(124, y + 1, cnt, font_small, sel ? 0 : 1);
      y += 12;
      shown++;
    }
    if (shown == 0) c.text(2, 30, "Нет подходящей рыбы", font_small, 1);
    // подсказка занимает строку шапки на втором такте мигания
    if (ui::blink(2400)) {
      c.fillRect(0, 13, 128, 12, 0);
      c.text(2, 13, ui::blink(1200) ? "ВЫБОР — продать вид" : "СТИК — продать всё", font_small, 1);
    }
    (void)now;
  }

  void drawPotions(Canvas& c, uint32_t now) {
    topBar(c, "ЗЕЛЬЯ", 3);
    char buf[64], pg[16];

    uint8_t ids[kPotCount], n = 0;
    for (uint8_t i = 0; i < kPotCount; i++) if (kPot[i].cat == potCat) ids[n++] = i;
    if (n == 0) return;
    if (potSel >= n) potSel = (uint8_t)(n - 1);

    // категория (СТИК переключает) + номер
    c.fillRoundRect(2, 13, 124, 11, 3, 1);
    c.text(6, 14, kCatName[potCat], font_small, 0);
    snprintf(pg, sizeof(pg), "%u/%u", potSel + 1, n);
    c.textR(122, 14, pg, font_small, 0);

    const PotionDef& p = kPot[ids[potSel]];
    char nm[40];
    c.copyFit(nm, sizeof(nm), p.name, font_small, 124);
    c.text(2, 26, nm, font_small, 1);
    snprintf(buf, sizeof(buf), "рецепт: %s %u/%u", kFish[p.reqFish].name,
             s.inv[p.reqFish], p.reqCount);
    c.text(2, 38, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "эффект x%.1f на %u с", p.val, p.dur);
    c.text(2, 50, buf, font_small, 1);
    if (ui::blink(2000)) {                       // подсказка поверх строки эффекта
      c.fillRect(0, 50, 128, 12, 0);
      c.text(2, 50, ui::blink(1000) ? "ВЫБОР — варить" : "СТИК — категория", font_small, 1);
    }
    (void)now;
  }

  void drawCatMenu(Canvas& c) {
    // модальное окно на весь экран
    c.clear();
    c.text(2, 2, "КАТЕГОРИИ ЗЕЛИЙ", font_small, 1);
    ui::separator(c, 13);
    for (int i = 0; i < 4; i++) {
      int y = 17 + i * 12;
      if (i == catSel) {
        c.fillRoundRect(3, y, 122, 11, 3, 1);
        c.text(7, y + 1, kCatName[i], font_small, 0);
      } else {
        c.text(7, y + 1, kCatName[i], font_small, 1);
      }
    }
  }

  void drawZones(Canvas& c, uint32_t now, int8_t vp) {
    uint8_t idx = (uint8_t)vp;
    if (idx >= kZoneCount) idx = 0;
    topBar(c, "ЗОНЫ", 4);
    char pg[16], buf[48];
    snprintf(pg, sizeof(pg), "%u/%u", idx + 1, kZoneCount);
    c.textR(126, 13, pg, font_small, 1);          // номер — в шапке
    c.bitmap(82, 14, spsZoneSprites[idx], BMP_COPY);   // 44x26 справа
    nameFit(c, 2, 14, kZone[idx].name, 76, 1);
    snprintf(buf, sizeof(buf), "редкость x%.2f", kZone[idx].luck);
    c.text(2, 29, buf, font_small, 1);

    bool open = idx <= s.zone;
    if (open) {
      c.text(2, 40, idx == s.zone ? "Вы ловите здесь" : "Пропуск куплен", font_small, 1);
      if (idx != s.zone && ui::blink(900)) c.text(2, 52, "ВЫБОР — перейти", font_small, 1);
    } else {
      char money[20];
      fmtNum(money, sizeof(money), kZone[idx].price);
      snprintf(buf, sizeof(buf), "Пропуск %s ₽", money);
      c.text(2, 40, buf, font_small, 1);
      if (idx == s.zone + 1) {
        if (ui::blink(900)) c.text(2, 52, "ВЫБОР — купить", font_small, 1);
      } else {
        c.text(2, 52, "Сначала прошлая зона", font_small, 1);
      }
    }
    (void)now;
  }

  void drawPrestige(Canvas& c, uint32_t now) {
    topBar(c, "ПРЕСТИЖ", 5);
    char buf[64], money[20];
    if (s.prestige) snprintf(buf, sizeof(buf), "Звание: %s", kPrestigeTitle[s.prestige]);
    else snprintf(buf, sizeof(buf), "Звание: нет");
    c.text(2, 13, buf, font_small, 1);
    snprintf(buf, sizeof(buf), "Уровень %u из 5 · +%u%%", s.prestige, s.prestige * 50);
    c.text(2, 25, buf, font_small, 1);
    if (s.prestige >= 5) {
      c.text(2, 37, "Максимальный престиж", font_small, 1);
      c.text(2, 49, "Доход +250% навсегда", font_small, 1);
      return;
    }
    double need = prestigeNeed();
    fmtNum(money, sizeof(money), need);
    snprintf(buf, sizeof(buf), "Нужно %s ₽", money);
    c.text(2, 37, buf, font_small, 1);
    bool zoneOk = s.zone >= kZoneCount - 1;
    bool moneyOk = s.money >= need;
    if (!zoneOk) c.text(2, 49, "Сначала зона «Бездна»", font_small, 1);
    else if (!moneyOk) c.text(2, 49, "Не хватает денег", font_small, 1);
    else if (ui::blink()) c.text(2, 49, "ВЫБОР — новый путь", font_small, 1);
    (void)now;
  }

  void drawBoard(Canvas& c) {
    c.clear();
    c.text(2, 1, "ТАБЛО", font_small, 1);
    ui::tabsDots(c, 0, 1, 126, 3);
    ui::separator(c, 12);
    char b[20], line[64];
    fmtNum(b, sizeof(b), s.money);
    snprintf(line, sizeof(line), "Баланс: %s ₽", b);
    c.text(2, 15, line, font_small, 1);
    snprintf(line, sizeof(line), "Рыбы: %u · всего: %lu", invTotal(), (unsigned long)s.totalCaught);
    c.text(2, 25, line, font_small, 1);
    snprintf(line, sizeof(line), "Удочка %u · %s", s.rod, kZone[s.zone].name);
    c.text(2, 35, line, font_small, 1);
    // эффекты
    uint32_t now = gbMillis();
    int x = 2;
    if (s.effUntil[0] > now) { snprintf(line, sizeof(line), "СК x%.1f", s.effHaste); c.text(x, 43, line, font_small, 1); x += 40; }
    if (s.effUntil[1] > now) { snprintf(line, sizeof(line), "УД x%.1f", s.effLuck); c.text(x, 43, line, font_small, 1); x += 40; }
    if (s.effUntil[2] > now) { snprintf(line, sizeof(line), "ПР x%.1f", s.effSell); c.text(x, 43, line, font_small, 1); }
    if (x == 2) c.text(2, 43, "Эффектов нет", font_small, 1);
    snprintf(line, sizeof(line), "Звание: %s", s.prestige ? kPrestigeTitle[s.prestige] : "—");
    c.text(2, 54, line, font_small, 1);
  }
};

}  // namespace

Game* makeGameFish() {
  static GameFish g;
  return &g;
}
