// ---------------------------------------------------------------------------
//  GameBox :: input.h — джойстик + 4 кнопки, события с антидребезгом
//
//  Основной принцип джойстика (по ТЗ):
//    * событие направления выдаётся ОДИН раз при выходе за порог;
//    * повторное событие в ту же сторону возможно только после возврата
//      стика в нейтраль или ухода в другую сторону (гистерезис);
//    * аналоговый сигнал меньше порога нажатием не считается.
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>

enum Btn : uint8_t { BTN_SELECT = 0, BTN_BACK, BTN_RESET, BTN_MENU, BTN_STICK, BTN_COUNT };

enum Ev : uint8_t {
  EV_NONE = 0,
  EV_UP, EV_DOWN, EV_LEFT, EV_RIGHT,   // джойстик, по одному на отклонение
  EV_SELECT, EV_BACK, EV_RESET, EV_MENU, EV_STICK,   // короткие нажатия
  EV_SELECT_LONG, EV_STICK_LONG,       // удержания
  EV_COUNT
};

const char* evName(Ev e);

// Глобальный доступ к состоянию кнопок (для игр, которым нужен «отпуск»)
bool gbBtnHeld(Btn b);

class Input {
 public:
  void begin();
  void update(uint32_t nowMs);

  bool hit(Btn b) const { return hitFlag & (1u << b); }        // нажатие в этом кадре
  bool hitLong(Btn b) const { return longFlag & (1u << b); }
  bool held(Btn b) const { return heldFlag & (1u << b); }
  uint32_t heldMs(Btn b) const { return heldMs_[b]; }

  // Очередь событий (направления + кнопки), разбирается игрой/меню
  Ev pop();
  bool empty() const { return qHead == qTail; }
  void clear();

  // Внешняя инъекция события (используется симулятором на ПК)
  void inject(Ev e) { push(e); }

  // Сырые значения (для отладки и для калибровки)
  int rawX() const { return rawX_; }
  int rawY() const { return rawY_; }
  int centerX() const { return cx; }
  int centerY() const { return cy; }
  bool recalibrate();      // откалибровать центр (стик должен быть отпущен)

 private:
  uint32_t hitFlag = 0, longFlag = 0, heldFlag = 0;
  uint32_t heldMs_[BTN_COUNT] = {0};
  bool lastRaw[BTN_COUNT] = {false, false, false, false, false};
  uint32_t rawChangeMs[BTN_COUNT] = {0};
  bool stable[BTN_COUNT] = {false, false, false, false, false};
  bool longFired[BTN_COUNT] = {false, false, false, false, false};
  int rawX_ = 2048, rawY_ = 2048, cx = 2048, cy = 2048;
  int8_t dirX = 0, dirY = 0;
  Ev q[16];
  uint8_t qHead = 0, qTail = 0;
  void push(Ev e);
  void scanStick(uint32_t now);
};
