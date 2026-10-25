#include "canvas.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
extern uint32_t g_simMillis;
int main() {
  Canvas c;
  c.clear(0);
  ui::header(c, "ИГРЫ", 0, 3);
  c.text(2, 16, "Симулятор рыбалки", font_small);
  c.text(2, 28, "Удочка: 12 ур.", font_small);
  c.textOutlined(2, 40, "123.45К ₽", font_big);
  c.roundRect(4, 52, 60, 10, 3, 1);
  c.fillRoundRect(6, 54, 30, 6, 2, 1);
  ui::tabsDots(c, 1, 5, 126, 3);
  ui::triangle(c, 120, 58, 1, 4, true);
  char buf[32]; fmtNum(buf, sizeof(buf), 1234567); printf("%s\n", buf);
  fmtMoney(buf, sizeof(buf), 98765432100.0); printf("%s\n", buf);
  FILE* f = fopen("/tmp/core.ppm", "wb");
  fprintf(f, "P5\n128 64\n255\n");
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++) fputc(c.get(x, y) ? 255 : 0, f);
  fclose(f);
  return 0;
}
