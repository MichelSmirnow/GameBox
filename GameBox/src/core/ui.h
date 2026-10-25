// ---------------------------------------------------------------------------
//  GameBox :: ui.h — общие элементы интерфейса (128x64, только ч/б)
// ---------------------------------------------------------------------------
#pragma once
#include "canvas.h"
#include "input.h"

namespace ui {

// Заголовок: название слева, точки-табы справа, линия-разделитель.
void header(Canvas& c, const char* title, int tab = -1, int tabs = 0);
// Точки-индикаторы текущей вкладки (правый верхний угол)
void tabsDots(Canvas& c, int active, int count, int rightX = 126, int y = 3);
// Вертикальный скроллбар
void scrollbar(Canvas& c, int x, int y, int h, int total, int visible, int first);
// Треугольник-указатель (dir: 0 вверх, 1 вправо, 2 вниз, 3 влево)
void triangle(Canvas& c, int x, int y, int dir, int size = 4, bool filled = true);
// Шеврон (стрелка-галочка)
void chevron(Canvas& c, int x, int y, int dir, int size = 4);
// Рамка с текстом (для кнопок/категорий)
void chip(Canvas& c, int x, int y, const char* text, bool active);
// Прогресс-бар
void progress(Canvas& c, int x, int y, int w, int h, float frac, const char* label = nullptr);
// Горизонтальный разделитель
void separator(Canvas& c, int y, int x0 = 0, int x1 = 127);
// Полоска "нехватает/ошибка/покупка" внизу экрана
void notice(const char* text, uint16_t ms, bool waiter = false);
bool noticeActive();
bool noticeDismiss();                 // true если был активен и закрыт
void noticeDraw(Canvas& c);
// Мигающий курсор (используется в селекторах)
bool blink(uint32_t periodMs = 520);
// Короткий звук (если подключён пьезо)
void beep(uint16_t freq = 2000, uint16_t ms = 12);

}  // namespace ui
