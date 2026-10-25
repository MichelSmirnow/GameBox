// ---------------------------------------------------------------------------
//  GameBox :: canvas.h — 1-битный кадровый буфер 128x64 + графика + UTF-8 текст
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "font_data.h"

// На ESP32 PROGMEM определён в Arduino.h и означает обычную flash-память.
// Для сборки на ПК (симулятор) подставляем пустое определение.
#ifndef PROGMEM
#define PROGMEM
#endif

// Битмап в формате image2cpp "Horizontal, 1 bit per pixel, MSB first"
struct Bmp {
  uint8_t w;
  uint8_t h;
  const uint8_t* data;
};

// Режимы вывода битмапа
enum BmpMode : uint8_t {
  BMP_COPY = 0,   // непрозрачный: 1-пиксели белые, 0-пиксели чёрные
  BMP_MASK = 1,   // прозрачный: рисуются только единичные пиксели
  BMP_XOR  = 2    // инверсия по маске (для "вырезанного" текста)
};

// ---- отладочный учёт текста (только в симуляторе на ПК) ------------------
#ifndef ARDUINO
#include <string>
#include <vector>
struct TextBox {
  int x, y, w, h;
  std::string s;
};
extern std::vector<TextBox> g_textBoxes;   // заполняется, если g_textLog = true
extern bool g_textLog;
class Canvas;
extern const Canvas* g_logCanvas;          // лог ведётся только для этого буфера
extern int g_textDepth;                    // >0 внутри обводки — не логируем
#endif

class Canvas {
 public:
  uint8_t buf[8][128];

  Canvas() { clear(); }

  void clear(uint8_t v = 0);
  void px(int x, int y, uint8_t v = 1);
  int  get(int x, int y) const;
  void hline(int x, int y, int w, uint8_t v = 1);
  void vline(int x, int y, int h, uint8_t v = 1);
  void line(int x0, int y0, int x1, int y1, uint8_t v = 1);
  void rect(int x, int y, int w, int h, uint8_t v = 1);
  void fillRect(int x, int y, int w, int h, uint8_t v = 1);
  void circle(int cx, int cy, int r, uint8_t v = 1);
  void fillCircle(int cx, int cy, int r, uint8_t v = 1);
  void roundRect(int x, int y, int w, int h, int r, uint8_t v = 1);
  void fillRoundRect(int x, int y, int w, int h, int r, uint8_t v = 1);
  void arc(int cx, int cy, int r, int a0, int a1, uint8_t v = 1);   // углы в градусах
  void ditherRect(int x, int y, int w, int h, uint8_t level, int phase = 0);
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t v = 1);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t v = 1);
  void bitmap(int x, int y, const Bmp& b, uint8_t mode = BMP_COPY);
  void bitmap(int x, int y, int w, int h, const uint8_t* data, uint8_t mode = BMP_COPY);
  void sprite(int x, int y, const Bmp& b) { bitmap(x, y, b, BMP_MASK); }
  void invertRect(int x, int y, int w, int h);

  // ---------------------------- текст (UTF-8) ----------------------------
  int  text(int x, int y, const char* s, const Font& f, uint8_t v = 1);
  int  textW(const char* s, const Font& f) const;
  void textC(int y, const char* s, const Font& f, uint8_t v = 1);            // по центру
  void textR(int xr, int y, const char* s, const Font& f, uint8_t v = 1);    // по правому краю
  void textOutlined(int x, int y, const char* s, const Font& f, uint8_t v = 1);  // кайма
  int  textWrap(int x, int y, int w, const char* s, const Font& f,
                uint8_t v = 1, int maxLines = 32);
  int  linesCount(const char* s, const Font& f, int w) const;
  // Копирует строку в out, обрезая её по ширине maxW по границам символов UTF-8
  // (в отличие от "%.12s", который режет многобайтные буквы и даёт "?").
  void copyFit(char* out, int cap, const char* s, const Font& f, int maxW) const;
};

// Копирование сцены со смещением (для свайпов и слайдов)
// clipOnly == true: пиксели вне области источника не трогаются (оверлей)
void blit(Canvas& dst, const Canvas& src, int dx, int dy, bool clipOnly = false);

// Декодирование UTF-8: возвращает кодовую точку и сдвигает указатель
uint16_t utf8Next(const char*& s);
