// ---------------------------------------------------------------------------
//  GameBox :: game_life.cpp — «Жизнь» (Conway's Game of Life)
//
//  Поле 32x20 клеток слева (96x60 px), справа панель данных.
//  Режим редактирования: джойстик двигает курсор, ВЫБОР ставит/убирает
//  клетку, СТИК запускает симуляцию. В симуляции: ВЫБОР — пауза,
//  вверх/вниз — скорость, влево/вправо — шаг (на паузе),
//  удержание ВЫБОР (долгое нажатие) — зум, СБРОС — очистить поле.
// ---------------------------------------------------------------------------
#include "../core/game.h"
#include "../core/util.h"
#include "../core/ui.h"
#include "../core/storage.h"
#include "../gfx/gfx.h"
#include <stdio.h>
#include <string.h>

namespace {

const uint8_t GW = 28, GH = 20;
const uint8_t CELL = 3;          // размер клетки в пикселях (зум 1)
const int FIELD_W = GW * CELL;   // 84 px
const int FIELD_X = 0, FIELD_Y = 2;

const uint16_t kDelays[] = {400, 250, 160, 100, 60, 35, 20, 10, 5};
const uint8_t kDelaysCount = 9;

struct LifeState {
  uint8_t grid[GH][GW / 8];
  uint8_t next[GH][GW / 8];
  uint16_t gen = 0;
  uint32_t startMs = 0;
  uint32_t elapsedMs = 0;
  uint8_t delayIdx = 4;
  bool paused = true;
  bool running = false;      // режим симуляции (иначе — редактор)
  uint8_t cx = GW / 2, cy = GH / 2;
  uint8_t zoom = 1;
  uint32_t lastStep = 0;
  uint16_t popCount = 0;
};

Store lifeStore("gb_life");

class GameLife : public Game {
 public:
  const char* title() const override { return "Игра «Жизнь»"; }
  const char* shortName() const override { return "Жизнь"; }
  const Bmp& banner() const override { return bmpBannerLife; }
  uint8_t tabs() const override { return 1; }
  const char* tabName(uint8_t) const override { return "ЖИЗНЬ"; }

  void enter() override {
    lifeStore.begin();
    s.running = false;
    s.paused = true;
    s.gen = 0;
    s.elapsedMs = 0;
    s.startMs = gbMillis();
    s.lastStep = gbMillis();
    // стартовый узор — «планер» и «пульсар», чтобы сразу было видно жизнь
    memset(s.grid, 0, sizeof(s.grid));
    seedDemo();                 // пустое поле скучное — сразу показываем узор
    lifeStore.setB("seen", true);
    popCount();
  }
  void leave() override {}

  void onEvent(Ev e, uint32_t now) override {
    if (!s.running) {
      // ---------- редактор ----------
      switch (e) {
        case EV_UP: if (s.cy > 0) s.cy--; break;
        case EV_DOWN: if (s.cy + 1 < GH) s.cy++; break;
        case EV_LEFT: if (s.cx > 0) s.cx--; break;
        case EV_RIGHT: if (s.cx + 1 < GW) s.cx++; break;
        case EV_SELECT: setCell(s.cx, s.cy, !getCell(s.cx, s.cy)); popCount(); break;
        case EV_STICK:
          if (empty()) { ui::notice("Поставьте хотя бы одну клетку", 1400); break; }
          s.running = true; s.paused = false; s.gen = 0;
          s.elapsedMs = 0; s.startMs = now; s.lastStep = now;
          ui::beep(2200, 20);
          break;
        case EV_RESET:
          memset(s.grid, 0, sizeof(s.grid)); s.gen = 0; popCount();
          break;
        case EV_SELECT_LONG:
          s.zoom = s.zoom == 1 ? 2 : 1;
          break;
        default: break;
      }
      return;
    }
    // ---------- симуляция ----------
    switch (e) {
      case EV_SELECT: s.paused = !s.paused; ui::beep(1600, 15); break;
      case EV_UP:
        if (s.delayIdx + 1 < kDelaysCount) s.delayIdx++;      // быстрее
        break;
      case EV_DOWN:
        if (s.delayIdx > 0) s.delayIdx--;                     // медленнее
        break;
      case EV_LEFT:
        if (s.paused) { step(); }                             // шаг назад невозможен — просто шаг
        break;
      case EV_RIGHT:
        if (s.paused) { step(); }
        break;
      case EV_SELECT_LONG: s.zoom = s.zoom == 1 ? 2 : 1; break;
      case EV_STICK: s.running = false; s.paused = true; break;   // назад в редактор
      case EV_RESET:
        memset(s.grid, 0, sizeof(s.grid));
        s.gen = 0; s.elapsedMs = 0; s.startMs = now; popCount();
        break;
      default: break;
    }
  }

  void update(uint32_t dt, uint32_t now) override {
    if (!s.running || s.paused) return;
    s.elapsedMs += dt;
    uint32_t d = kDelays[s.delayIdx];
    if (now - s.lastStep >= d) {
      s.lastStep = now;
      step();
    }
  }

  void draw(Canvas& c, uint32_t now, int8_t, int8_t) override {
    c.clear();
    drawField(c);
    drawPanel(c, now);
    ui::noticeDraw(c);
  }

  const char* help() const override {
    return
      "ПРАВИЛА\n"
      "Клетка живёт, если у неё 2\n"
      "или 3 соседа. Мёртвая\n"
      "клетка рождается при ровно\n"
      "3 соседях.\n\n"
      "РЕДАКТОР\n"
      "Джойстик — курсор,\n"
      "ВЫБОР — поставить/убрать\n"
      "клетку, СТИК — запустить,\n"
      "СБРОС — очистить поле.\n\n"
      "СИМУЛЯЦИЯ\n"
      "ВЫБОР — пауза,\n"
      "вверх — быстрее, вниз —\n"
      "медленнее, влево/вправо —\n"
      "один шаг на паузе,\n"
      "СТИК — назад в редактор.\n\n"
      "УДЕРЖАНИЕ ВЫБОР — зум x2.\n\n"
      "Справа: поколение, время,\n"
      "задержка между\n"
      "поколениями, население и\n"
      "статус.";
  }

 private:
  LifeState s;

  bool getCell(int x, int y) const {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return false;
    return (s.grid[y][x >> 3] >> (x & 7)) & 1;
  }
  void setCell(int x, int y, bool v) {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return;
    if (v) s.grid[y][x >> 3] |= (uint8_t)(1u << (x & 7));
    else s.grid[y][x >> 3] &= (uint8_t)~(1u << (x & 7));
  }
  bool empty() const {
    for (uint8_t y = 0; y < GH; y++)
      for (uint8_t b = 0; b < GW / 8; b++)
        if (s.grid[y][b]) return false;
    return true;
  }
  void popCount() {
    uint16_t n = 0;
    for (uint8_t y = 0; y < GH; y++)
      for (uint8_t x = 0; x < GW; x++) if (getCell(x, y)) n++;
    s.popCount = n;
  }

  void step() {
    for (uint8_t y = 0; y < GH; y++) {
      for (uint8_t x = 0; x < GW; x++) {
        uint8_t n = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            n += getCell(x + dx, y + dy) ? 1 : 0;
          }
        bool alive = getCell(x, y);
        bool nv = alive ? (n == 2 || n == 3) : (n == 3);
        if (nv) s.next[y][x >> 3] |= (uint8_t)(1u << (x & 7));
        else s.next[y][x >> 3] &= (uint8_t)~(1u << (x & 7));
      }
    }
    memcpy(s.grid, s.next, sizeof(s.grid));
    s.gen++;
    popCount();
  }

  void seedDemo() {
    // глайдер, летящий вправо-вниз
    setCell(3, 2, 1); setCell(4, 3, 1); setCell(2, 4, 1); setCell(3, 4, 1); setCell(4, 4, 1);
    // мигалка
    setCell(9, 3, 1); setCell(10, 3, 1); setCell(11, 3, 1);
    // пульсар 13x13
    const int px = 8, py = 8;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        setCell(px + 2 + i * 4 + j, py, 1);
        setCell(px + 2 + i * 4 + j, py + 12, 1);
      }
    }
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        setCell(px, py + 2 + i * 4 + j, 1);
        setCell(px + 12, py + 2 + i * 4 + j, 1);
      }
    }
    // пара блоков справа
    setCell(24, 3, 1); setCell(25, 3, 1); setCell(24, 4, 1); setCell(25, 4, 1);
  }

  // ------------------------------------------------------------ отрисовка
  void drawField(Canvas& c) {
    uint8_t z = s.zoom;
    int cell = CELL * z;
    int visibleW = FIELD_W / cell;
    int visibleH = 62 / cell;
    int ox = 0, oy = 0;
    if (z > 1) {
      // центрируем на курсоре
      ox = (int)s.cx - visibleW / 2; if (ox < 0) ox = 0;
      oy = (int)s.cy - visibleH / 2; if (oy < 0) oy = 0;
      if (ox > GW - visibleW) ox = GW - visibleW;
      if (oy > GH - visibleH) oy = GH - visibleH;
    }
    for (int y = 0; y < visibleH; y++) {
      for (int x = 0; x < visibleW; x++) {
        int gx = ox + x, gy = oy + y;
        if (getCell(gx, gy)) {
          c.fillRect(FIELD_X + x * cell, FIELD_Y + y * cell, cell - (z == 1 ? 0 : 1), cell - (z == 1 ? 0 : 1), 1);
        }
      }
    }
    // сетка (только при зуме 2 — иначе слишком шумно)
    // курсор
    if (!s.running || s.paused) {
      if (ui::blink(400)) {
        int px = FIELD_X + ((int)s.cx - ox) * cell;
        int py = FIELD_Y + ((int)s.cy - oy) * cell;
        if (px >= 0 && px + cell <= FIELD_W) c.rect(px, py, cell, cell, 1);
      }
    }
    // разделитель панели
    c.vline(FIELD_W + 1, 0, 64, 1);
  }

  void drawPanel(Canvas& c, uint32_t now) {
    int x = FIELD_W + 4;
    // в шапке панели — статус (название игры видно в карусели и в справке)
    char buf[16], v[12];
    const char* st = !s.running ? "правка" : (s.paused ? "пауза" : "идёт");
    c.text(x, 1, st, font_small, 1);
    if (s.running && !s.paused) { if (ui::blink(600)) c.fillCircle(122, 5, 2, 1); }

    // поколение
    snprintf(v, sizeof(v), "%u", s.gen);
    c.text(x, 13, "пок", font_small, 1);
    c.textR(126, 13, v, font_small, 1);
    // время
    uint32_t sec = s.elapsedMs / 1000;
    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    c.text(x, 25, "вр", font_small, 1);
    c.textR(126, 25, buf, font_small, 1);
    // шаг
    snprintf(v, sizeof(v), "%u", (unsigned)kDelays[s.delayIdx]);
    c.text(x, 37, "шаг", font_small, 1);
    c.textR(126, 37, v, font_small, 1);
    // население
    snprintf(v, sizeof(v), "%u", s.popCount);
    c.text(x, 49, "жив", font_small, 1);
    c.textR(126, 49, v, font_small, 1);
    (void)now;
  }
};

}  // namespace

Game* makeGameLife() {
  static GameLife g;
  return &g;
}
