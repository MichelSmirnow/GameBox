#!/usr/bin/env python3
# ---------------------------------------------------------------------------
#  GameBox — схема подключения (docs/wiring.png)
#  Каждый контакт платы — горизонтальная линия к подписи назначения.
#  Цвет линии = модуль. Пересечений и наложений нет.
# ---------------------------------------------------------------------------
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle, Circle

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "docs", "wiring.png")

BG = "#0f1318"
BOARD = "#1d6b47"
PAD = "#f2c14e"
TXT = "#e9eff3"
DIM = "#8b9aa4"

C = {
    "oled": "#54b8ff",
    "joy": "#ffb454",
    "btn": "#7ee08a",
    "buz": "#c79bff",
    "pwr": "#e05c5c",
    "gnd": "#a8b0b8",
    "none": "#4c555d",
}

plt.rcParams["font.family"] = ["DejaVu Sans"]

# (силк на плате, GPIO, назначение, модуль)
LEFT = [
    ("5V",   None,     "не подключён",                      "none"),
    ("G",    "GND",    "джойстик GND · кнопки GND · OLED GND", "gnd"),
    ("3V3",  "3V3",    "джойстик VCC · OLED VCC",            "pwr"),
    ("4",    "GPIO4",  "кнопка ВЫБОР",                       "btn"),
    ("3",    "GPIO3",  "джойстик SW (нажатие стика)",         "joy"),
    ("2",    None,     "свободен (strapping)",               "none"),
    ("1",    "GPIO1",  "джойстик VRy — ось Y",                "joy"),
    ("0",    "GPIO0",  "джойстик VRx — ось X",                "joy"),
]

RIGHT = [
    ("5",    "GPIO5",  "кнопка НАЗАД",                       "btn"),
    ("6",    "GPIO6",  "кнопка СБРОС",                       "btn"),
    ("7",    "GPIO7",  "кнопка МЕНЮ",                        "btn"),
    ("8",    "GPIO8",  "OLED SDA",                           "oled"),
    ("9",    "GPIO9",  "OLED SCL",                           "oled"),
    ("10",   "GPIO10", "пьезо +",                            "buz"),
    ("20",   None,     "свободен (UART RX)",                 "none"),
    ("21",   None,     "свободен (UART TX)",                 "none"),
]


def main():
    fig, ax = plt.subplots(figsize=(12.4, 7.2), dpi=170)
    ax.set_xlim(0, 12.4)
    ax.set_ylim(0, 7.2)
    ax.axis("off")
    ax.add_patch(Rectangle((0, 0), 12.4, 7.2, fc=BG, ec="none"))

    ax.text(0.30, 6.92, "GameBox — схема подключения", color=TXT, fontsize=17, weight="bold")
    ax.text(0.30, 6.58, "ESP32-C3 Super Mini · OLED SSD1306 128×64 (I²C 0x3C) · джойстик KY-023 · 4 кнопки · пьезо",
            color=DIM, fontsize=9.5)

    bx, by, bw, bh = 5.30, 1.52, 2.85, 4.60
    ax.add_patch(FancyBboxPatch((bx, by), bw, bh, boxstyle="round,pad=0.02,rounding_size=0.10",
                                fc=BOARD, ec="#0a2a1b", lw=1.6))
    # антенна
    ax.add_patch(Rectangle((bx + 0.45, by + bh - 0.30), 1.95, 0.16, fc="#0a2a1b", ec="none"))
    # USB-C снизу
    ax.add_patch(FancyBboxPatch((bx + 1.05, by - 0.18), 0.75, 0.20,
                                boxstyle="round,pad=0.01,rounding_size=0.05",
                                fc="#c7cbd1", ec="#6d7178", lw=1))
    ax.text(bx + 1.42, by - 0.28, "USB-C", color=DIM, fontsize=7, ha="center", va="top")
    # чип
    ax.add_patch(Rectangle((bx + 0.92, by + 2.05), 1.00, 1.00, fc="#11161b", ec="#3b4147", lw=1))
    ax.text(bx + 1.42, by + 2.55, "ESP32-\nC3", color="#7fe0b0", fontsize=8.5,
            ha="center", va="center", linespacing=1.15)
    for dx, lab in ((0.55, "RST"), (2.30, "BOOT")):
        ax.add_patch(Circle((bx + dx, by + 0.50), 0.10, fc="#c7cbd1", ec="#6d7178", lw=1))
        ax.text(bx + dx, by + 0.26, lab, color=DIM, fontsize=6.5, ha="center")

    ax.text(bx + bw / 2, by + bh + 0.16, "ESP32-C3 Super Mini", color=TXT,
            fontsize=12, ha="center", va="bottom", weight="bold")

    n = len(LEFT)
    top = by + bh - 0.55
    step = (bh - 1.15) / (n - 1)
    ys = [top - i * step for i in range(n)]

    def row(items, left_side):
        for i, (silk, gpio, label, mod) in enumerate(items):
            y = ys[i]
            color = C[mod]
            if left_side:
                xp = bx + 0.02
                ax.add_patch(Circle((xp, y), 0.055, fc=PAD, ec="#8a6a15", lw=0.8))
                ax.text(xp + 0.15, y, silk, color="#0d1b13", fontsize=8.2,
                        ha="left", va="center", weight="bold")
                ax.plot([xp - 0.34, 2.62], [y, y], color=color, lw=1.7, alpha=0.95)
                ax.text(2.56, y, label, color=TXT if mod != "none" else DIM,
                        fontsize=9.3, ha="right", va="center")
                if gpio:
                    ax.text(2.56, y - 0.20, gpio, color=color, fontsize=7.4,
                            ha="right", va="center")
            else:
                xp = bx + bw - 0.02
                ax.add_patch(Circle((xp, y), 0.055, fc=PAD, ec="#8a6a15", lw=0.8))
                ax.text(xp - 0.15, y, silk, color="#0d1b13", fontsize=8.2,
                        ha="right", va="center", weight="bold")
                ax.plot([xp + 0.34, 10.02], [y, y], color=color, lw=1.7, alpha=0.95)
                ax.text(10.08, y, label, color=TXT if mod != "none" else DIM,
                        fontsize=9.3, ha="left", va="center")
                if gpio:
                    ax.text(10.08, y - 0.20, gpio, color=color, fontsize=7.4,
                            ha="left", va="center")

    row(LEFT, True)
    row(RIGHT, False)

    ax.text(1.45, by + bh + 0.06, "левая сторона", color=DIM, fontsize=8.4, ha="center")
    ax.text(10.9, by + bh + 0.05, "правая сторона", color=DIM, fontsize=8.4, ha="center")

    # ---------------- легенда модулей ----------------
    ax.add_patch(FancyBboxPatch((0.30, 0.20), 11.8, 0.86,
                                boxstyle="round,pad=0.03,rounding_size=0.06",
                                fc="#151b21", ec="#2c3a45", lw=1))
    items = [("oled", "OLED SSD1306"), ("joy", "джойстик KY-023"), ("btn", "кнопки ВЫБОР / НАЗАД / СБРОС / МЕНЮ"),
             ("buz", "пьезо (необязательно)"), ("pwr", "питание 3V3"), ("gnd", "земля GND")]
    x, yl = 0.50, 0.86
    for i, (mod, name) in enumerate(items):
        if i == 3:
            x, yl = 0.50, 0.62
        ax.plot([x, x + 0.30], [yl, yl], color=C[mod], lw=2.4, solid_capstyle="round")
        ax.text(x + 0.38, yl, name, color=DIM, fontsize=8.6, va="center")
        x += 0.38 + len(name) * 0.092 + 0.55
    ax.text(0.50, 0.36,
            "Логика 3.3 В · кнопки и кнопка джойстика замыкают пин на GND (INPUT_PULLUP) · "
            "провода I²C короткие · 5V с 3V3 не соединять",
            color="#6f7c85", fontsize=8.2, va="center")

    fig.tight_layout(pad=0.15)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    fig.savefig(OUT, facecolor=BG)
    fig.savefig(OUT.replace(".png", ".svg"), facecolor=BG)
    print("сохранено:", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
