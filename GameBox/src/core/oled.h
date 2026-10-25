// ---------------------------------------------------------------------------
//  GameBox :: oled.h — минимальный драйвер SSD1306 / SH1106 по I2C
//  Быстрее Adafruit_SSD1306: без лишних абстракций, только разница страниц.
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include "canvas.h"

class Oled {
 public:
  bool begin();
  // Отправляет кадр. Если force == false, посылаются только изменившиеся страницы.
  void flush(Canvas& c, bool force = false);
  void contrast(uint8_t v);
  void invert(bool on);
  bool ready() const { return ok; }
  uint8_t addr() const { return i2cAddr; }

 private:
  bool ok = false;
  uint8_t i2cAddr = 0x3C;
  uint8_t last[8][128];
  uint8_t shadowPages = 0;   // какие страницы реально изменены с прошлого flush
  void cmd(uint8_t c);
  void cmd2(uint8_t a, uint8_t b);
  void pageData(uint8_t page, const uint8_t* data, uint8_t len);
};

extern Oled oled;
