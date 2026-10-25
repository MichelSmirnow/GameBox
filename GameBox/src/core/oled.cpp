// ---------------------------------------------------------------------------
//  GameBox :: oled.cpp
// ---------------------------------------------------------------------------
#include "oled.h"
#include "config.h"
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

Oled oled;

#ifdef ARDUINO
static void i2cWrite(uint8_t addr, const uint8_t* data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  Wire.endTransmission();
}
#endif

bool Oled::begin() {
#ifdef ARDUINO
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_HZ);
  Wire.setTimeOut(20);
  i2cAddr = OLED_ADDR;
  Wire.beginTransmission(i2cAddr);
  if (Wire.endTransmission() != 0) {
    i2cAddr = (OLED_ADDR == 0x3C) ? 0x3D : 0x3C;   // частая альтернатива
    Wire.beginTransmission(i2cAddr);
    if (Wire.endTransmission() != 0) { ok = false; return false; }
  }
#ifdef DISPLAY_SH1106
  cmd(0xAE);
  cmd(0xD5); cmd(0x80);
  cmd(0xA8); cmd(0x3F);
  cmd(0xD3); cmd(0x00);
  cmd(0x40);
  cmd(0xAD); cmd(0x8B);
  cmd(0x32);
  cmd(0xA1); cmd(0xC8);
  cmd(0xD9); cmd(0x1F);
  cmd(0xDB); cmd(0x40);
  cmd(0xA4); cmd(0xA6);
  cmd(0xAF);
#else
  cmd(0xAE);              // display off
  cmd(0xD5); cmd(0xF0);   // clock
  cmd(0xA8); cmd(0x3F);   // multiplex 1/64
  cmd(0xD3); cmd(0x00);   // offset
  cmd(0x40);              // start line 0
  cmd(0x8D); cmd(0x14);   // charge pump on
  cmd(0x20); cmd(0x02);   // page addressing mode
  cmd(0xA1);              // seg remap
  cmd(0xC8);              // com scan dec
  cmd(0xDA); cmd(0x12);
  cmd(0x81); cmd(0xCF);   // contrast
  cmd(0xD9); cmd(0xF1);
  cmd(0xDB); cmd(0x40);
  cmd(0xA4);
  cmd(0xA6);
  cmd(0xAF);
#endif
  memset(last, 0xFF, sizeof(last));   // заставит первый flush отправить всё
  for (uint8_t p = 0; p < 8; p++) {
    // очистка экрана
    uint8_t zero[128] = {0};
    pageData(p, zero, 128);
  }
  ok = true;
  return true;
#else
  ok = true;
  memset(last, 0xFF, sizeof(last));
  return true;
#endif
}

void Oled::cmd(uint8_t c) {
#ifdef ARDUINO
  uint8_t b[2] = {0x00, c};
  i2cWrite(i2cAddr, b, 2);
#endif
}

void Oled::cmd2(uint8_t a, uint8_t b) {
#ifdef ARDUINO
  uint8_t d[3] = {0x00, a, b};
  i2cWrite(i2cAddr, d, 3);
#else
  (void)a; (void)b;
#endif
}

void Oled::pageData(uint8_t page, const uint8_t* data, uint8_t len) {
#ifdef ARDUINO
  // режим адресации по страницам (0x20, 0x02 выставлен в begin())
  cmd((uint8_t)(0xB0 | page));
  cmd((uint8_t)(0x00 | (OLED_XOFFSET & 0x0F)));
  cmd((uint8_t)(0x10 | ((OLED_XOFFSET >> 4) & 0x0F)));
  // Передаём блоками, чтобы не переполнять буфер Wire
  for (uint8_t off = 0; off < len; off += 32) {
    uint8_t chunk = (len - off > 32) ? 32 : (uint8_t)(len - off);
    uint8_t tmp[33];
    tmp[0] = 0x40;   // data mode
    memcpy(tmp + 1, data + off, chunk);
    i2cWrite(i2cAddr, tmp, chunk + 1);
  }
#else
  (void)page; (void)data; (void)len;
#endif
}

void Oled::flush(Canvas& c, bool force) {
  if (!ok) return;
  for (uint8_t p = 0; p < 8; p++) {
    if (!force && memcmp(last[p], c.buf[p], 128) == 0) continue;
    pageData(p, c.buf[p], 128);
    memcpy(last[p], c.buf[p], 128);
  }
}

void Oled::contrast(uint8_t v) { cmd2(0x81, v); }
void Oled::invert(bool on) { cmd(on ? 0xA7 : 0xA6); }
