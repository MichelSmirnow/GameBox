// ---------------------------------------------------------------------------
//  GameBox :: input.cpp
// ---------------------------------------------------------------------------
#include "input.h"
#include "config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const char* kEvNames[] = {"-", "вверх", "вниз", "влево", "вправо",
                                 "выбор", "назад", "сброс", "меню", "стик",
                                 "выбор(удерж.)", "стик(удерж.)"};

const char* evName(Ev e) { return (e < EV_COUNT) ? kEvNames[e] : "?"; }

void Input::push(Ev e) {
  uint8_t next = (uint8_t)((qTail + 1) & 15);
  if (next == qHead) qHead = (uint8_t)((qHead + 1) & 15);  // переполнение: теряем старое
  q[qTail] = e;
  qTail = next;
}

Ev Input::pop() {
  if (qHead == qTail) return EV_NONE;
  Ev e = q[qHead];
  qHead = (uint8_t)((qHead + 1) & 15);
  return e;
}

void Input::clear() { qHead = qTail = 0; }

// ---------------------------- чтение кнопки ---------------------------------
static inline bool readBtn(uint8_t pin) {
#ifdef ARDUINO
  bool v = digitalRead(pin) == HIGH;
  return BTN_ACTIVE_LOW ? !v : v;
#else
  (void)pin;
  return false;
#endif
}

static inline int readJoy(uint8_t pin) {
#ifdef ARDUINO
  return (int)analogReadMilliVolts(pin);
#else
  (void)pin;
  return 1500;
#endif
}

static Input* g_inputSelf = nullptr;
bool gbBtnHeld(Btn b) { return g_inputSelf && g_inputSelf->held(b); }

void Input::begin() {
  g_inputSelf = this;
#ifdef ARDUINO
  pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_MENU, INPUT_PULLUP);
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  pinMode(PIN_JOY_X, INPUT);
  pinMode(PIN_JOY_Y, INPUT);
#endif
  recalibrate();
}

bool Input::recalibrate() {
  // Быстрая калибровка центра: 40 замеров по каждой оси
  int32_t sx = 0, sy = 0;
  for (int i = 0; i < 40; i++) {
    sx += readJoy(PIN_JOY_X);
    sy += readJoy(PIN_JOY_Y);
#ifdef ARDUINO
    delay(2);
#endif
  }
  cx = (int)(sx / 40);
  cy = (int)(sy / 40);
  dirX = dirY = 0;
  return true;
}

void Input::scanStick(uint32_t now) {
  (void)now;
  const int thr = JOY_THRESHOLD_MV;
  const int rel = JOY_THRESHOLD_MV - JOY_HYST_MV;

  int vx = 0, vy = 0;
  for (int i = 0; i < JOY_SAMPLES; i++) { vx += readJoy(PIN_JOY_X); vy += readJoy(PIN_JOY_Y); }
  rawX_ = vx / JOY_SAMPLES;
  rawY_ = vy / JOY_SAMPLES;

  int dx = rawX_ - cx;
  int dy = rawY_ - cy;
#if JOY_INVERT_X
  dx = -dx;
#endif
#if JOY_INVERT_Y
  dy = -dy;
#endif

  // ------------------------------- ось X -----------------------------------
  int8_t nx = dirX;
  if (dirX == 0) {
    if (dx < -thr) nx = -1;
    else if (dx > thr) nx = 1;
  } else if ((dx > -rel && dx < rel)) {
    nx = 0;
  } else if (dirX < 0 && dx > thr) {
    nx = 1;              // перескок в другую сторону без полного возврата
  } else if (dirX > 0 && dx < -thr) {
    nx = -1;
  }
  if (nx != dirX) {
    if (nx < 0) push(EV_LEFT);
    else if (nx > 0) push(EV_RIGHT);
    dirX = nx;
  }

  // ------------------------------- ось Y -----------------------------------
  int8_t ny = dirY;
  if (dirY == 0) {
    if (dy < -thr) ny = -1;
    else if (dy > thr) ny = 1;
  } else if (dy > -rel && dy < rel) {
    ny = 0;
  } else if (dirY < 0 && dy > thr) {
    ny = 1;
  } else if (dirY > 0 && dy < -thr) {
    ny = -1;
  }
  if (ny != dirY) {
    // Вверх по экрану = EV_UP. Знак зависит от ориентации модуля:
    // при стандартном включении "вверх" даёт уменьшение Y (можно инвертировать).
    if (ny < 0) push(EV_UP);
    else if (ny > 0) push(EV_DOWN);
    dirY = ny;
  }
}

void Input::update(uint32_t now) {
  hitFlag = longFlag = 0;
  static const uint8_t pins[BTN_COUNT] = {PIN_BTN_SELECT, PIN_BTN_BACK, PIN_BTN_RESET,
                                          PIN_BTN_MENU, PIN_JOY_SW};
  static const Ev shortEv[BTN_COUNT] = {EV_SELECT, EV_BACK, EV_RESET, EV_MENU, EV_STICK};
  static const Ev longEv[BTN_COUNT] = {EV_SELECT_LONG, EV_NONE, EV_NONE, EV_NONE, EV_STICK_LONG};

  for (uint8_t b = 0; b < BTN_COUNT; b++) {
    bool raw = readBtn(pins[b]);
    if (raw != lastRaw[b]) {
      lastRaw[b] = raw;
      rawChangeMs[b] = now;
    }
    if (raw != stable[b] && (now - rawChangeMs[b]) >= BTN_DEBOUNCE_MS) {
      stable[b] = raw;
      if (raw) {
        hitFlag |= (1u << b);
        heldFlag |= (1u << b);
        heldMs_[b] = 0;
        longFired[b] = false;
        push(shortEv[b]);
      } else {
        heldFlag &= ~(1u << b);
      }
    }
    if (stable[b]) heldMs_[b] = now - rawChangeMs[b];
    if (stable[b] && !longFired[b] && longEv[b] != EV_NONE && heldMs_[b] >= LONG_PRESS_MS) {
      longFired[b] = true;
      longFlag |= (1u << b);
      push(longEv[b]);
    }
  }

  scanStick(now);
}
