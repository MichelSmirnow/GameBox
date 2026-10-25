// ---------------------------------------------------------------------------
//  GameBox :: host.cpp
// ---------------------------------------------------------------------------
#include "host.h"
#include "util.h"

void Host::open(Game* g_, uint32_t now) {
  g = g_;
  tab_ = vpage_ = 0;
  g->tab = 0;
  g->vpage = 0;
  animDur = 0;
  pending = EV_NONE;
  g->enter();
  (void)now;
}

void Host::close() {
  if (g) g->leave();
  g = nullptr;
}

bool Host::animating(uint32_t now) const {
  return animDur && (now - animStart) < animDur;
}

float Host::progress(uint32_t now) const {
  if (!animDur) return 1.0f;
  float t = (float)(now - animStart) / (float)animDur;
  return t > 1.0f ? 1.0f : t;
}

Ev Host::takePending() {
  Ev e = pending;
  pending = EV_NONE;
  return e;
}

void Host::startAnim(uint8_t dir, uint32_t now) {
  fromTab = tab_;
  fromV = vpage_;
  animDir = dir;
  animStart = now;
  animDur = UI_ANIM_MS;
}

void Host::event(Ev e, uint32_t now) {
  if (!g || e == EV_NONE) return;
  if (g->modal()) { g->onEvent(e, now); return; }   // модальное окно игры
  if (animating(now)) {                // во время свайпа копим одно событие
    if (pending == EV_NONE) pending = e;
    return;
  }
  // Сброс "вглубь страницы" при горизонтальном переходе (по ТЗ)
  switch (e) {
    case EV_LEFT:
      if (tab_ > 0) { tab_--; vpage_ = 0; g->tab = tab_; g->vpage = 0; startAnim(2, now); return; }
      break;
    case EV_RIGHT:
      if (tab_ + 1 < (int)g->tabs()) { tab_++; vpage_ = 0; g->tab = tab_; g->vpage = 0; startAnim(1, now); return; }
      break;
    case EV_DOWN:
      if (vpage_ + 1 < (int)g->vpages((uint8_t)tab_)) {
        vpage_++; g->vpage = vpage_; startAnim(3, now); return;
      }
      break;
    case EV_UP:
      if (vpage_ > 0) { vpage_--; g->vpage = vpage_; startAnim(4, now); return; }
      break;
    default: break;
  }
  g->onEvent(e, now);
}

void Host::back(uint32_t now) {
  if (!g || animating(now)) return;
  if (vpage_ > 0) { vpage_--; g->vpage = vpage_; startAnim(4, now); return; }
  if (tab_ > 0) { tab_ = 0; vpage_ = 0; g->tab = 0; g->vpage = 0; startAnim(2, now); return; }
}

void Host::update(uint32_t dt, uint32_t now) {
  if (!g) return;
  g->update(dt, now);
  if (animDur && (now - animStart) >= animDur) {
    animDur = 0;
    fromTab = tab_;
    fromV = vpage_;
  }
}

void Host::drawPage(Canvas& c, uint32_t now, int8_t tab, int8_t vpage) {
  if (tab < 0 || tab >= (int)g->tabs()) return;
  if (vpage < 0 || vpage >= (int)g->vpages((uint8_t)tab)) return;
  g->draw(c, now, tab, vpage);
}

void Host::draw(Canvas& c, uint32_t now) {
  if (!g) return;
  if (!animating(now)) { drawPage(c, now, tab_, vpage_); return; }

  bool horiz = (animDir == 1 || animDir == 2);
  float p = easeOutCubic(progress(now));
  int span = horiz ? OLED_WIDTH : OLED_HEIGHT;
  int off = (int)((1.0f - p) * (float)span + 0.5f);

  int oldShift, newShift;
  if (animDir == 1)      { newShift =  off; oldShift =  off - span; }   // вправо
  else if (animDir == 2) { newShift = -off; oldShift =  span - off; }   // влево
  else if (animDir == 3) { newShift =  off; oldShift =  off - span; }   // вниз
  else                   { newShift = -off; oldShift =  span - off; }   // вверх

  // 1. старая страница (заполняет фон)
  tmp.clear();
  drawPage(tmp, now, fromTab, fromV);
  blit(c, tmp, horiz ? oldShift : 0, horiz ? 0 : oldShift, false);

  // 2. новая страница поверх, непрозрачно в своей области
  tmp.clear();
  drawPage(tmp, now, tab_, horiz ? 0 : vpage_);
  blit(c, tmp, horiz ? newShift : 0, horiz ? 0 : newShift, true);
}
