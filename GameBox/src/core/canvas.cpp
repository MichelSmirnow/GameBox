// ---------------------------------------------------------------------------
//  GameBox :: canvas.cpp
// ---------------------------------------------------------------------------
#include "canvas.h"
#include <algorithm>
#include <string.h>
#include <math.h>

uint16_t utf8Next(const char*& s) {
  uint8_t c = (uint8_t)*s;
  if (c < 0x80) { s++; return c; }
  if ((c & 0xE0) == 0xC0 && s[1]) {
    uint16_t cp = ((uint16_t)(c & 0x1F) << 6) | (s[1] & 0x3F);
    s += 2; return cp;
  }
  if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
    uint16_t cp = ((uint16_t)(c & 0x0F) << 12) | ((uint16_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    s += 3; return cp;
  }
  if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
    uint32_t cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
                  ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    s += 4; return (uint16_t)cp;
  }
  s++;
  return '?';
}

// ------------------------------ поиск глифа ---------------------------------
static const Glyph* findGlyph(const Font& f, uint16_t cp) {
  int lo = 0, hi = (int)f.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint16_t c = f.glyphs[mid].cp;
    if (c == cp) return &f.glyphs[mid];
    if (c < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

void Canvas::clear(uint8_t v) {
#ifndef ARDUINO
  if (g_textLog && !v && (!g_logCanvas || g_logCanvas == this)) g_textBoxes.clear();
#endif
  memset(buf, v ? 0xFF : 0x00, sizeof(buf));
}

void Canvas::px(int x, int y, uint8_t v) {
  if ((unsigned)x >= 128u || (unsigned)y >= 64u) return;
  if (v) buf[y >> 3][x] |= (uint8_t)(1u << (y & 7));
  else   buf[y >> 3][x] &= (uint8_t)~(1u << (y & 7));
}

int Canvas::get(int x, int y) const {
  if ((unsigned)x >= 128u || (unsigned)y >= 64u) return 0;
  return (buf[y >> 3][x] >> (y & 7)) & 1;
}

void Canvas::hline(int x, int y, int w, uint8_t v) {
  if (y < 0 || y >= 64) return;
  int x0 = x < 0 ? 0 : x;
  int x1 = x + w; if (x1 > 128) x1 = 128;
  for (int i = x0; i < x1; i++) px(i, y, v);
}

void Canvas::vline(int x, int y, int h, uint8_t v) {
  if (x < 0 || x >= 128) return;
  int y0 = y < 0 ? 0 : y, y1 = y + h; if (y1 > 64) y1 = 64;
  for (int i = y0; i < y1; i++) px(x, i, v);
}

void Canvas::line(int x0, int y0, int x1, int y1, uint8_t v) {
  int dx = x1 - x0, dy = y1 - y0;
  int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
  dx = dx > 0 ? dx : -dx; dy = dy > 0 ? dy : -dy;
  int err = dx - dy;
  while (true) {
    px(x0, y0, v);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err << 1;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx)  { err += dx; y0 += sy; }
  }
}

void Canvas::rect(int x, int y, int w, int h, uint8_t v) {
  if (w <= 0 || h <= 0) return;
  hline(x, y, w, v); hline(x, y + h - 1, w, v);
  vline(x, y, h, v); vline(x + w - 1, y, h, v);
}

void Canvas::fillRect(int x, int y, int w, int h, uint8_t v) {
#ifndef ARDUINO
  // чёрная заливка стирает старые надписи в своей области
  if (g_textLog && v == 0 && (!g_logCanvas || g_logCanvas == this)) {
    if (x <= 0 && y <= 0 && w >= 128 && h >= 64) {
      g_textBoxes.clear();
    } else {
      for (size_t i = 0; i < g_textBoxes.size();) {
        const TextBox& b = g_textBoxes[i];
        int ix = std::min(b.x + b.w, x + w) - std::max(b.x, x);
        int iy = std::min(b.y + b.h, y + h) - std::max(b.y, y);
        if (ix > 0 && iy > 0) g_textBoxes.erase(g_textBoxes.begin() + (long)i);
        else i++;
      }
    }
  }
#endif
  for (int j = 0; j < h; j++) hline(x, y + j, w, v);
}

void Canvas::invertRect(int x, int y, int w, int h) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) px(x + i, y + j, !get(x + i, y + j));
}

void Canvas::circle(int cx, int cy, int r, uint8_t v) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    px(cx + x, cy + y, v); px(cx + y, cy + x, v);
    px(cx - y, cy + x, v); px(cx - x, cy + y, v);
    px(cx - x, cy - y, v); px(cx - y, cy - x, v);
    px(cx + y, cy - x, v); px(cx + x, cy - y, v);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

void Canvas::fillCircle(int cx, int cy, int r, uint8_t v) {
  for (int y = -r; y <= r; y++) {
    int dx = 0;
    while ((dx + 1) * (dx + 1) + y * y <= r * r) dx++;
    hline(cx - dx, cy + y, 2 * dx + 1, v);
  }
}

void Canvas::roundRect(int x, int y, int w, int h, int r, uint8_t v) {
  if (r <= 0) { rect(x, y, w, h, v); return; }
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  hline(x + r, y, w - 2 * r, v);
  hline(x + r, y + h - 1, w - 2 * r, v);
  vline(x, y + r, h - 2 * r, v);
  vline(x + w - 1, y + r, h - 2 * r, v);
  arc(x + r, y + r, r, 180, 270, v);
  arc(x + w - 1 - r, y + r, r, 270, 360, v);
  arc(x + w - 1 - r, y + h - 1 - r, r, 0, 90, v);
  arc(x + r, y + h - 1 - r, r, 90, 180, v);
}

void Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint8_t v) {
  if (r <= 0) { fillRect(x, y, w, h, v); return; }
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  for (int j = 0; j < h; j++) {
    int k = -1;
    if (j < r) k = r - 1 - j;
    else if (j >= h - r) k = j - (h - r);
    int inset = 0;
    if (k >= 0) {
      float s2 = (float)(r * r - k * k);
      if (s2 < 0) s2 = 0;
      inset = r - (int)(sqrtf(s2) + 0.5f);
    }
    hline(x + inset, y + j, w - 2 * inset, v);
  }
}

void Canvas::arc(int cx, int cy, int r, int a0, int a1, uint8_t v) {
  // a0..a1 по часовой стрелке, 0° — вправо, 90° — вниз
  if (a1 < a0) { int t = a0; a0 = a1; a1 = t; }
  for (int a = a0; a <= a1; a++) {
    float rad = a * 0.01745329f;
    int x = cx + (int)(r * cosf(rad) + 0.5f);
    int y = cy + (int)(r * sinf(rad) + 0.5f);
    px(x, y, v);
  }
}

void Canvas::ditherRect(int x, int y, int w, int h, uint8_t level, int phase) {
  // level 0..16 — плотность заполнения (шахматка 4x4)
  static const uint8_t m4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int v = m4[((j + phase) & 3) * 4 + ((i + phase) & 3)];
      if (v < level) px(x + i, y + j, 1);
    }
}

void Canvas::triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t v) {
  line(x0, y0, x1, y1, v); line(x1, y1, x2, y2, v); line(x2, y2, x0, y0, v);
}

void Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t v) {
  int xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  for (int i = 0; i < 2; i++)
    for (int j = i + 1; j < 3; j++)
      if (ys[j] < ys[i]) {
        int t = ys[i]; ys[i] = ys[j]; ys[j] = t;
        t = xs[i]; xs[i] = xs[j]; xs[j] = t;
      }
  int ymin = ys[0], ymax = ys[2];
  for (int y = ymin; y <= ymax; y++) {
    int xa = 127, xb = -1;
    for (int e = 0; e < 3; e++) {
      int n = (e + 1) % 3;
      int ya = ys[e], yb2 = ys[n];
      if (ya == yb2) {
        if (y == ya) { if (xs[e] < xa) xa = xs[e]; if (xs[n] > xb) xb = xs[n]; }
        continue;
      }
      if (y >= (ya < yb2 ? ya : yb2) && y <= (ya > yb2 ? ya : yb2)) {
        int xc = xs[e] + (xs[n] - xs[e]) * (y - ya) / (yb2 - ya);
        if (xc < xa) xa = xc;
        if (xc > xb) xb = xc;
      }
    }
    if (xb >= xa) hline(xa, y, xb - xa + 1, v);
  }
}

void Canvas::bitmap(int x, int y, const Bmp& b, uint8_t mode) {
  bitmap(x, y, b.w, b.h, b.data, mode);
}

void Canvas::bitmap(int x, int y, int w, int h, const uint8_t* data, uint8_t mode) {
  int stride = (w + 7) / 8;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byte = data[j * stride + (i >> 3)];  // PROGMEM на ESP32 = обычная память
      if (byte & (0x80 >> (i & 7))) {
        if (mode == BMP_COPY) px(x + i, y + j, 1);
        else if (mode == BMP_MASK) px(x + i, y + j, 1);
        else px(x + i, y + j, !get(x + i, y + j));
      } else if (mode == BMP_COPY) {
        px(x + i, y + j, 0);
      }
    }
  }
}

// =================================== ТЕКСТ ==================================
#ifndef ARDUINO
std::vector<TextBox> g_textBoxes;
bool g_textLog = false;
const Canvas* g_logCanvas = nullptr;
int g_textDepth = 0;
#endif

int Canvas::text(int x, int y, const char* s, const Font& f, uint8_t v) {
#ifndef ARDUINO
  if (g_textLog && g_textDepth == 0 && (!g_logCanvas || g_logCanvas == this) && s && *s) {
    TextBox b;
    b.x = x;
    b.y = y;
    b.w = textW(s, f);
    b.h = f.ascent;
    b.s = s;
    g_textBoxes.push_back(b);
  }
#endif
  int penX = x;
  const char* p = s;
  while (*p) {
    uint16_t cp = utf8Next(p);
    if (cp == '\n') { y += f.lineH; penX = x; continue; }
    const Glyph* g = findGlyph(f, cp);
    if (!g) g = findGlyph(f, '?');
    if (!g) continue;
    if (g->w && g->h) {
      for (int c = 0; c < g->w; c++) {
        int gx = penX + g->xo + c;
        if (gx < 0 || gx >= 128) continue;
        for (int b = 0; b < g->bpc; b++) {
          uint8_t bits = f.bmp[g->off + c * g->bpc + b];
          if (!bits) continue;
          for (int bit = 0; bit < 8; bit++) {
            if (bits & (1 << bit)) px(gx, y + g->yo + b * 8 + bit, v);
          }
        }
      }
    }
    penX += g->adv;
  }
  return penX - x;
}

int Canvas::textW(const char* s, const Font& f) const {
  int w = 0, lineW = 0;
  const char* p = s;
  while (*p) {
    uint16_t cp = utf8Next(p);
    if (cp == '\n') { if (lineW > w) w = lineW; lineW = 0; continue; }
    const Glyph* g = findGlyph(f, cp);
    if (!g) g = findGlyph(f, '?');
    if (g) lineW += g->adv;
  }
  return lineW > w ? lineW : w;
}

void Canvas::textC(int y, const char* s, const Font& f, uint8_t v) {
  text((128 - textW(s, f)) / 2, y, s, f, v);
}

void Canvas::textR(int xr, int y, const char* s, const Font& f, uint8_t v) {
  text(xr - textW(s, f), y, s, f, v);
}

void Canvas::textOutlined(int x, int y, const char* s, const Font& f, uint8_t v) {
#ifndef ARDUINO
  g_textDepth++;                       // обводка не участвует в проверке наложений
#endif
  // Кайма: сначала чёрная обводка вокруг глифов, затем белый текст.
  // Никакой подложки — фон под текстом остаётся видимым.
  if (v) {
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++)
        if (dx || dy) text(x + dx, y + dy, s, f, 0);
  }
  text(x, y, s, f, v);
#ifndef ARDUINO
  g_textDepth--;
#endif
}

// Ширина отрезка UTF-8 строки длиной bytes
[[maybe_unused]] static int segWidth(const Font& f, const char* s, int bytes) {
  int w = 0, i = 0;
  while (i < bytes && s[i]) {
    const char* q = s + i;
    uint16_t cp = utf8Next(q);
    const Glyph* g = findGlyph(f, cp);
    if (!g) g = findGlyph(f, '?');
    if (g) w += g->adv;
    i += (int)(q - (s + i));
  }
  return w;
}

int Canvas::textWrap(int x, int y, int w, const char* s, const Font& f,
                     uint8_t v, int maxLines) {
  char buf[96];
  int line = 0, cy = y;
  const char* p = s;
  while (*p && line < maxLines) {
    int n = 0, cand = -1, px = 0;
    const char* q = p;
    while (*q) {
      const char* q2 = q;
      uint16_t cp = utf8Next(q2);
      if (cp == '\n') break;
      int bl = (int)(q2 - q);
      const Glyph* g = findGlyph(f, cp);
      if (!g) g = findGlyph(f, '?');
      int adv = g ? g->adv : 4;
      if (n + bl >= (int)sizeof(buf) - 1) break;
      if (px + adv > w && n > 0) break;
      for (int k = 0; k < bl; k++) buf[n++] = q[k];
      if (cp == ' ') cand = n;
      px += adv;
      q = q2;
    }
    buf[n] = 0;
    if (*q && *q != '\n' && cand > 0) {   // перенос по последнему пробелу
      q = p + cand;
      n = cand;
      while (n > 0 && buf[n - 1] == ' ') n--;
      buf[n] = 0;
    }
    while (n > 0 && buf[n - 1] == ' ') { buf[--n] = 0; }
    text(x, cy, buf, f, v);
    line++;
    cy += f.lineH;
    p = q;
    while (*p == ' ') p++;
    if (*p == '\n') p++;
    if (!*p) break;
  }
  return line;
}

void Canvas::copyFit(char* out, int cap, const char* s, const Font& f, int maxW) const {
  if (!out || cap < 2) return;
  int n = 0;
  int px = 0;
  const char* p = s;
  while (*p) {
    const char* q = p;
    uint16_t cp = utf8Next(q);
    int bl = (int)(q - p);
    if (n + bl >= cap - 1) break;
    const Glyph* g = findGlyph(f, cp);
    int adv = g ? g->adv : 4;
    if (px + adv > maxW) break;
    for (int i = 0; i < bl; i++) out[n++] = p[i];
    px += adv;
    p = q;
  }
  out[n] = 0;
}

int Canvas::linesCount(const char* s, const Font& f, int w) const {
  int line = 0;
  const char* p = s;
  while (*p) {
    int n = 0, cand = -1, px = 0;
    const char* q = p;
    while (*q) {
      const char* q2 = q;
      uint16_t cp = utf8Next(q2);
      if (cp == '\n') break;
      const Glyph* g = findGlyph(f, cp);
      if (!g) g = findGlyph(f, '?');
      int adv = g ? g->adv : 4;
      if (px + adv > w && n > 0) break;
      if (cp == ' ') cand = (int)(q2 - p);
      px += adv;
      n = (int)(q2 - p);
      q = q2;
    }
    if (*q && *q != '\n' && cand > 0) q = p + cand;
    line++;
    p = q;
    while (*p == ' ') p++;
    if (*p == '\n') p++;
    if (!*p) break;
  }
  return line ? line : 1;
}

void blit(Canvas& dst, const Canvas& src, int dx, int dy, bool clipOnly) {
  if (dx == 0 && dy == 0) {
    if (!clipOnly) { memcpy(dst.buf, src.buf, sizeof(dst.buf)); return; }
  }
  for (int y = 0; y < 64; y++) {
    int sy = y - dy;
    if (sy < 0 || sy >= 64) { if (!clipOnly) dst.hline(0, y, 128, 0); continue; }
    const uint8_t* srow = src.buf[sy >> 3];
    for (int x = 0; x < 128; x++) {
      int sx = x - dx;
      if (sx < 0 || sx >= 128) { if (!clipOnly) dst.px(x, y, 0); continue; }
      dst.px(x, y, (srow[sx] >> (sy & 7)) & 1);
    }
  }
}
