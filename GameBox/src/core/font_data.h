// ---------------------------------------------------------------------------
//  GameBox — шрифты (1 bit, UTF-8, кириллица)
//  Сгенерировано tools/gen_fonts.py. Не редактировать вручную.
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>

struct Glyph {
  uint16_t cp;     // кодовая точка UTF-8
  uint8_t  w;      // ширина в пикселях
  uint8_t  h;      // высота
  int8_t   xo;     // смещение по X от пера
  int8_t   yo;     // смещение по Y от верха строки
  uint8_t  adv;    // шаг пера
  uint16_t off;    // смещение в битмапе (байты)
  uint8_t  bpc;    // байт на колонку
};

struct Font {
  const uint8_t* bmp;
  const Glyph*   glyphs;
  uint16_t       count;
  uint8_t        lineH;
  uint8_t        ascent;
};

extern const Font font_small;
extern const Font font_big;

