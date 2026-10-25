// ---------------------------------------------------------------------------
//  GameBox :: host.h — навигация по страницам игры + анимации переходов
// ---------------------------------------------------------------------------
#pragma once
#include "game.h"
#include "config.h"

class Host {
 public:
  void open(Game* g, uint32_t now);
  void close();
  Game* game() const { return g; }

  void event(Ev e, uint32_t now);         // обрабатывает навигацию и передаёт игре
  void update(uint32_t dt, uint32_t now);
  void draw(Canvas& c, uint32_t now);

  int tab() const { return (int)tab_; }
  int vpage() const { return (int)vpage_; }
  bool animating(uint32_t now) const;      // идёт ли переход
  bool atRoot() const { return tab_ == 0 && vpage_ == 0; }
  void back(uint32_t now);                 // шаг назад по страницам
  Ev takePending();                        // событие, отложенное из-за анимации

 private:
  Game* g = nullptr;
  int8_t tab_ = 0, vpage_ = 0;
  int8_t fromTab = 0, fromV = 0;
  uint32_t animStart = 0, animDur = 0;
  uint8_t animDir = 0;    // 1 = вправо, 2 = влево, 3 = вниз, 4 = вверх
  Canvas tmp;
  Ev pending = EV_NONE;

  void startAnim(uint8_t dir, uint32_t now);
  float progress(uint32_t now) const;
  void drawPage(Canvas& c, uint32_t now, int8_t tab, int8_t vpage);
};
