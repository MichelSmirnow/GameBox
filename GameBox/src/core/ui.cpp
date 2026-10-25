// ---------------------------------------------------------------------------
//  GameBox :: ui.cpp
// ---------------------------------------------------------------------------
#include "ui.h"
#include "config.h"
#include "font_data.h"
#include "util.h"
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif


namespace ui {

static char noticeText[48];
static uint32_t noticeUntil = 0;
static bool noticeWait = false;
static bool noticeOn = false;

static inline uint32_t nowMs() { return gbMillis(); }

void header(Canvas& c, const char* title, int tab, int tabs) {
  c.text(2, 1, title, font_small, 1);
  if (tabs > 1) tabsDots(c, tab, tabs, 126, 3);
  separator(c, 12);
}

void tabsDots(Canvas& c, int active, int count, int rightX, int y) {
  if (count <= 0) return;
  int w = count * 4 - 1;
  int x = rightX - w + 1;
  for (int i = 0; i < count; i++) {
    int xx = x + i * 4;
    if (i == active) c.fillRect(xx, y, 3, 3, 1);
    else { c.px(xx + 1, y + 1, 1); }
  }
}

void scrollbar(Canvas& c, int x, int y, int h, int total, int visible, int first) {
  if (total <= visible) return;
  c.vline(x, y, h, 1);
  int barH = h * visible / total;
  if (barH < 4) barH = 4;
  int maxFirst = total - visible;
  int barY = y + (h - barH) * first / (maxFirst ? maxFirst : 1);
  c.vline(x, barY, barH, 1);
}

void triangle(Canvas& c, int x, int y, int dir, int size, bool filled) {
  for (int i = 0; i <= size; i++) {
    int half = size - i;
    switch (dir) {
      case 1:   // вправо
        if (filled) c.vline(x + i, y - half, 2 * half + 1, 1);
        else { c.px(x + i, y - half, 1); c.px(x + i, y + half, 1); }
        break;
      case 3:   // влево
        if (filled) c.vline(x - i, y - half, 2 * half + 1, 1);
        else { c.px(x - i, y - half, 1); c.px(x - i, y + half, 1); }
        break;
      case 0:   // вверх
        if (filled) c.hline(x - half, y - i, 2 * half + 1, 1);
        else { c.px(x - half, y - i, 1); c.px(x + half, y - i, 1); }
        break;
      default:  // вниз
        if (filled) c.hline(x - half, y + i, 2 * half + 1, 1);
        else { c.px(x - half, y + i, 1); c.px(x + half, y + i, 1); }
        break;
    }
  }
}

void chevron(Canvas& c, int x, int y, int dir, int size) {
  // dir: 0 вверх, 1 вправо, 2 вниз, 3 влево
  for (int i = 0; i <= size; i++) {
    switch (dir) {
      case 0: c.px(x - size + i, y + size - i, 1); c.px(x + size - i, y + size - i, 1); break;
      case 2: c.px(x - size + i, y + i, 1); c.px(x + size - i, y + i, 1); break;
      case 1: c.px(x + i, y - size + i, 1); c.px(x + i, y + size - i, 1); break;
      default: c.px(x + size - i, y - size + i, 1); c.px(x + size - i, y + size - i, 1); break;
    }
  }
}

void chip(Canvas& c, int x, int y, const char* text, bool active) {
  int w = c.textW(text, font_small) + 8;
  c.roundRect(x, y, w, 13, 3, 1);
  if (active) c.textOutlined(x + 4, y + 2, text, font_small, 1);
  else c.text(x + 4, y + 2, text, font_small, 1);
}

void progress(Canvas& c, int x, int y, int w, int h, float frac, const char* label) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  c.rect(x, y, w, h, 1);
  int inner = (int)((w - 2) * frac);
  if (inner > 0) c.fillRect(x + 1, y + 1, inner, h - 2, 1);
  if (label) c.textOutlined(x + 2, y - 1, label, font_small, 1);
}

void separator(Canvas& c, int y, int x0, int x1) { c.hline(x0, y, x1 - x0 + 1, 1); }

void notice(const char* text, uint16_t ms, bool waiter) {
  strncpy(noticeText, text, sizeof(noticeText) - 1);
  noticeText[sizeof(noticeText) - 1] = 0;
  noticeUntil = nowMs() + ms;
  noticeWait = waiter;
  noticeOn = true;
}

bool noticeActive() { return noticeOn; }

bool noticeDismiss() {
  if (!noticeOn) return false;
  noticeOn = false;
  return true;
}

void noticeDraw(Canvas& c) {
  if (!noticeOn) return;
  if (!noticeWait && nowMs() > noticeUntil) { noticeOn = false; return; }
  // Баннер: рамка + текст с чёрной каймой (фон не заливается)
  int lines = c.linesCount(noticeText, font_small, 112);
  if (lines < 1) lines = 1;
  int th = lines * font_small.lineH;
  int boxH = th + 6;
  int y = 32 - boxH / 2;
  c.fillRoundRect(4, y, 120, boxH, 3, 0);   // чистая область, чтобы текст не смешивался
  c.roundRect(4, y, 120, boxH, 3, 1);
  c.textWrap(10, y + 3, 108, noticeText, font_small, 1, lines);
  if (noticeWait) {
    // мигающий маркер "нажмите выбор"
    if (blink()) c.px(120, y + boxH - 4, 1);
  }
}

bool blink(uint32_t periodMs) {
  return ((nowMs() / (periodMs / 2)) & 1) == 0;
}

void beep(uint16_t freq, uint16_t ms) {
#ifdef ARDUINO
#if USE_BUZZER
  tone(PIN_BUZZER, freq, ms);
#else
  (void)freq; (void)ms;
#endif
#else
  (void)freq; (void)ms;
#endif
}

}  // namespace ui
