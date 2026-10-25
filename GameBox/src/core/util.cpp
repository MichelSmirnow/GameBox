// ---------------------------------------------------------------------------
//  GameBox :: util.cpp
// ---------------------------------------------------------------------------
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_system.h>
static uint32_t g_rngState = 0;
void gbRandomSeed(uint32_t seed) { g_rngState = seed ? seed : 1; }
uint32_t gbRandomU32() {
  // xorshift32 + аппаратный энтропийный источник
  uint32_t x = g_rngState ? g_rngState : (esp_random() | 1u);
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_rngState = x;
  return x ^ esp_random();
}
#else
static uint32_t g_rngState = 12345;
void gbRandomSeed(uint32_t seed) { g_rngState = seed ? seed : 1; }
uint32_t gbRandomU32() {
  uint32_t x = g_rngState;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_rngState = x;
  return x;
}
#endif

uint32_t gbRandomRange(uint32_t lo, uint32_t hi) {
  if (hi <= lo) return lo;
  uint32_t span = hi - lo + 1;
  return lo + (gbRandomU32() % span);
}

bool gbChance(float p) {
  if (p <= 0.f) return false;
  if (p >= 1.f) return true;
  return (gbRandomU32() % 100000u) < (uint32_t)(p * 100000.f);
}

float gbRandomFloat() { return (gbRandomU32() & 0xFFFFFF) / 16777216.0f; }

uint32_t gbMillis() {
#ifdef ARDUINO
  return millis();
#else
  return g_simMillis;
#endif
}

// ------------------------------ форматирование ------------------------------
static const char* kSuffix[] = {"", "К", "М", "Б", "Т", "Кв", "Кн", "Ск", "Сп"};

void fmtNum(char* out, size_t n, double v) {
  bool neg = v < 0;
  if (neg) v = -v;
  int g = 0;
  while (v >= 1000.0 && g < 8) { v /= 1000.0; g++; }
  char tmp[24];
  if (g == 0) {
    if (v < 10 && v != (double)(long)v) snprintf(tmp, sizeof(tmp), "%.1f", v);
    else snprintf(tmp, sizeof(tmp), "%ld", (long)(v + 0.5));
  } else if (v < 10) {
    snprintf(tmp, sizeof(tmp), "%.2f", v);
  } else if (v < 100) {
    snprintf(tmp, sizeof(tmp), "%.1f", v);
  } else {
    snprintf(tmp, sizeof(tmp), "%ld", (long)(v + 0.5));
  }
  snprintf(out, n, "%s%s%s", neg ? "-" : "", tmp, kSuffix[g]);
}

void fmtMoney(char* out, size_t n, double v) {
  char buf[24];
  fmtNum(buf, sizeof(buf), v);
  snprintf(out, n, "%s₽", buf);
}

void fmtShort(char* out, size_t n, double v, int decimals) {
  char fmt[8];
  snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
  snprintf(out, n, fmt, v);
}

void fmtTime(char* out, size_t n, uint32_t sec) {
  uint32_t h = sec / 3600, m = (sec / 60) % 60, s = sec % 60;
  if (h) snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  else   snprintf(out, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

const char* rarityMark(int idx) {
  static const char* marks[] = {"F", "E", "D", "C", "B", "A", "S", "L", "M"};
  if (idx < 0) idx = 0;
  if (idx > 8) idx = 8;
  return marks[idx];
}
