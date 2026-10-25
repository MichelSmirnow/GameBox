// ---------------------------------------------------------------------------
//  GameBox :: storage.h — обёртка над NVS (встроенная flash ESP32-C3)
//
//  Зачем: EEPROM на ESP8266 изнашивалась и требовала ручной раскладки.
//  NVS делает выравнивание износа само, хранит ключи по namespace.
//  Правило: пишем в NVS только по событиям (покупка, выход, смена настроек),
//  а не каждый кадр — так ресурс flash расходуется разумно.
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include <stddef.h>

class Store {
 public:
  explicit Store(const char* ns);
  bool begin();                       // чтение-запись
  void end();
  bool ok() const { return opened; }

  uint32_t getU32(const char* k, uint32_t def = 0);
  void     setU32(const char* k, uint32_t v);
  int32_t  getI32(const char* k, int32_t def = 0) { return (int32_t)getU32(k, (uint32_t)def); }
  void     setI32(const char* k, int32_t v) { setU32(k, (uint32_t)v); }
  float    getF(const char* k, float def = 0);
  void     setF(const char* k, float v);
  bool     getB(const char* k, bool def = false) { return getU32(k, def ? 1 : 0) != 0; }
  void     setB(const char* k, bool v) { setU32(k, v ? 1 : 0); }
  uint8_t  getU8(const char* k, uint8_t def = 0) { return (uint8_t)getU32(k, def); }
  void     setU8(const char* k, uint8_t v) { setU32(k, v); }

  // Массивы (инвентарь, счётчики)
  size_t getBytes(const char* k, void* dst, size_t len);
  void   setBytes(const char* k, const void* src, size_t len);
  void   remove(const char* k);
  bool   has(const char* k);

 private:
  const char* ns;
  bool opened = false;
  void* handle = nullptr;
};
