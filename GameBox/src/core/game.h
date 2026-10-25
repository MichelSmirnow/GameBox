// ---------------------------------------------------------------------------
//  GameBox :: game.h — базовый класс игры
//
//  Модель страниц: табы по горизонтали (джойстик влево/вправо) и
//  подстраницы по вертикали (джойстик вверх/вниз). Рисование полностью
//  "без состояния": draw(tab, vpage) рисует любую страницу, что позволяет
//  анимировать свайпы между страницами.
// ---------------------------------------------------------------------------
#pragma once
#include "canvas.h"
#include "input.h"

class Game {
 public:
  virtual ~Game() {}

  virtual const char* title() const = 0;        // полное название
  virtual const char* shortName() const = 0;    // для шапки
  virtual const Bmp& banner() const = 0;        // баннер 128x32
  // Обложка для карусели игр (128x46, арт автора). nullptr — рисуем баннер.
  virtual const Bmp* cover() const { return nullptr; }
  virtual uint8_t tabs() const = 0;             // количество горизонтальных страниц
  virtual const char* tabName(uint8_t tab) const = 0;
  virtual uint8_t vpages(uint8_t tab) const { (void)tab; return 1; }

  virtual void enter() {}                       // вход (загрузка прогресса)
  virtual void leave() {}                       // выход (сохранение)

  virtual void onEvent(Ev e, uint32_t now) = 0;             // активная страница
  virtual void update(uint32_t dtMs, uint32_t now) {}
  virtual void draw(Canvas& c, uint32_t now, int8_t tab, int8_t vpage) = 0;

  // true — открыто модальное окно: игра сама обрабатывает все события,
  // включая навигацию джойстиком и кнопку НАЗАД.
  virtual bool modal() const { return false; }
  // только для симулятора: текущая фаза/сцена игры (0, если не нужна)
  virtual uint8_t debugPhase() const { return 0; }

  // Описание управления для вкладки "Настройки"
  virtual const char* helpTitle() const { return title(); }
  virtual const char* help() const = 0;         // многострочный текст

  // Текущая позиция — выставляет Host, игры читают при отрисовке
  int8_t tab = 0, vpage = 0;
};
