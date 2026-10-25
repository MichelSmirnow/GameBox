// ---------------------------------------------------------------------------
//  GameBox :: util.h — математика, форматирование чисел, случайности
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>

template <typename T> inline T clamp(T v, T a, T b) { return v < a ? a : (v > b ? b : v); }
template <typename T> inline T maxv(T a, T b) { return a > b ? a : b; }
template <typename T> inline T minv(T a, T b) { return a < b ? a : b; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float easeOutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
inline float easeInOutCubic(float t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}
inline float easeOutBack(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1.0f;
  return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

extern uint32_t g_simMillis;   // определяется в симуляторе (tools/sim)
// Текущее время, мс (на устройстве — millis(), в симуляторе — g_simMillis)
uint32_t gbMillis();

// ------------------------------- случайности --------------------------------
void gbRandomSeed(uint32_t seed);
uint32_t gbRandomU32();
uint32_t gbRandomRange(uint32_t lo, uint32_t hi);   // [lo, hi]
bool gbChance(float p);                              // вероятность 0..1
float gbRandomFloat();

// ------------------------------ форматирование ------------------------------
// 1234567 -> "1.23М"; суффиксы: К тыс, М млн, Б млрд, Т трлн, Кв, Кн
void fmtNum(char* out, size_t n, double v);
// То же, но с явным знаком валюты: "1.23М ₽"
void fmtMoney(char* out, size_t n, double v);
// 3.25 -> "3.25", 300 -> "300"
void fmtShort(char* out, size_t n, double v, int decimals = 2);
// Секунды -> "12:05" или "1:02:05"
void fmtTime(char* out, size_t n, uint32_t sec);
// Римские / уровни
const char* rarityMark(int idx);
