#!/usr/bin/env python3
"""
GameBox font generator.

Строит компактные 1-битные шрифты с кириллицей для OLED 128x64.
Формат глифа: колоночно-мажорный, ceil(h/8) байт на колонку.

Выход: GameBox/src/core/font_data.h
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT_H = os.path.join(os.path.dirname(__file__), "..", "GameBox", "src", "core", "font_data.h")
OUT_C = os.path.join(os.path.dirname(__file__), "..", "GameBox", "src", "core", "font_data.cpp")

FONTS = [
    # (имя, ttf, размер px, порог)
    ("font_small", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 9, 96),
    ("font_big",   "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13, 96),
]

# Набор символов: ASCII + кириллица + спецсимволы интерфейса
chars = []
chars += [chr(c) for c in range(32, 127)]
chars += [chr(c) for c in range(0x0410, 0x0450)]          # А-Я а-я
chars += list("Ёё№«»—–…•°±×·→←↑↓▲▼◄►★☆✓✔●○◆◇■□₽")
chars += ['’', '‘', '"', '«', '»']
seen = set()
uniq = []
for ch in chars:
    if ch not in seen:
        seen.add(ch)
        uniq.append(ch)


def render(font_path, size, cp):
    """Возвращает (bitmap rows as list of str, w, h, xoff, top) относительно базовой линии."""
    f = ImageFont.truetype(font_path, size)
    ascent, descent = f.getmetrics()
    W, H = size * 3 + 20, ascent + descent + size
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)
    d.text((10, 4), cp, font=f, fill=255)
    bbox = img.getbbox()
    if bbox is None:
        return [], 0, 0, 0, 0
    x0, y0, x1, y1 = bbox
    w, h = x1 - x0, y1 - y0
    crop = img.crop((x0, y0, x1, y1))
    rows = []
    px = crop.load()
    for y in range(h):
        rows.append([1 if px[x, y] > 96 else 0 for x in range(w)])
    xoff = x0 - 10
    top = y0 - 4          # смещение от верхней границы строки (anchor top)
    return rows, w, h, xoff, top


def render_ruble(font_path, size):
    """₽ = «Р» с двумя горизонтальными штрихами."""
    rows, w, h, xoff, top = render(font_path, size, "Р")
    if not rows:
        return [], 0, 0, 0, 0
    # два горизонтальных штриха под чашей буквы «Р»
    stem = 0
    while stem < w and not any(rows[y][stem] for y in range(h)):
        stem += 1
    width = max(2, int(w * 0.78))
    for frac in (0.60, 0.80):
        y = int(h * frac)
        for dy in (0, 1):
            if 0 <= y + dy < h:
                for x in range(stem, min(w, stem + width)):
                    rows[y + dy][x] = 1
    return rows, w, h, xoff, top


def build(name, path, size, thresh):
    f = ImageFont.truetype(path, size)
    ascent, descent = f.getmetrics()
    glyphs = []
    blob = bytearray()
    # таблица должна быть отсортирована по коду: поиск бинарный
    uniq_sorted = sorted(uniq, key=ord)
    for ch in uniq_sorted:
        rows, w, h, xoff, top = render(path, size, ch)
        if ch == "₽" and (w == 0 or h == 0):      # в старых DejaVu знака нет
            rows, w, h, xoff, top = render_ruble(path, size)
        adv = max(1, int(round(f.getlength(ch))))
        if w == 0 or h == 0:
            glyphs.append((ord(ch), 0, 0, 0, 0, adv, len(blob), 0))
            continue
        bytes_per_col = (h + 7) // 8
        off = len(blob)
        for x in range(w):
            for b in range(bytes_per_col):
                v = 0
                for bit in range(8):
                    y = b * 8 + bit
                    if y < h and rows[y][x]:
                        v |= (1 << bit)
                blob.append(v)
        glyphs.append((ord(ch), w, h, xoff, top, adv, off, bytes_per_col))
    # таблица
    lines = []
    lines.append("// Автоматически сгенерировано tools/gen_fonts.py — не редактировать вручную.\n")
    lines.append(f"const uint8_t {name}_bmp[] = {{\n")
    for i in range(0, len(blob), 16):
        lines.append("  " + ",".join(f"0x{b:02X}" for b in blob[i:i+16]) + ",\n")
    lines.append("};\n\n")
    lines.append(f"const Glyph {name}_glyphs[] = {{\n")
    for g in glyphs:
        lines.append("  {%d,%d,%d,%d,%d,%d,%d,%d},\n" % g)
    lines.append("};\n\n")
    table = "".join(lines)
    meta = (f"const Font {name} = {{ {name}_bmp, {name}_glyphs, "
            f"sizeof({name}_glyphs)/sizeof(Glyph), {ascent + descent}, {ascent} }};\n\n")
    return table + meta, len(blob), len(glyphs), ascent, descent


def main():
    parts = ["""// ---------------------------------------------------------------------------
//  GameBox — шрифты (1 bit, UTF-8, кириллица) — определения таблиц
//  Сгенерировано tools/gen_fonts.py из DejaVu (Bitstream Vera license).
// ---------------------------------------------------------------------------
#include "font_data.h"

"""]
    total = 0
    for name, path, size, th in FONTS:
        code, size_bytes, n, asc, desc = build(name, path, size, th)
        parts.append(code)
        total += size_bytes
        print(f"{name}: {n} глифов, {size_bytes} байт битмапа, ascent={asc} lineH={asc+desc}")
    with open(OUT_C, "w") as fh:
        fh.write("".join(parts))

    hdr = """// ---------------------------------------------------------------------------
//  GameBox — шрифты (1 bit, UTF-8, кириллица)
//  Сгенерировано tools/gen_fonts.py. Не редактировать вручную.
// ---------------------------------------------------------------------------
#pragma once
#include <stdint.h>

struct Glyph {
  uint16_t cp;     // кодовая точка UTF-8
  uint8_t  w;      // ширина в пикселях
  uint8_t  h;      // высота
  int8_t   xo;     // смещение по X от пера
  int8_t   yo;     // смещение по Y от верха строки
  uint8_t  adv;    // шаг пера
  uint16_t off;    // смещение в битмапе (байты)
  uint8_t  bpc;    // байт на колонку
};

struct Font {
  const uint8_t* bmp;
  const Glyph*   glyphs;
  uint16_t       count;
  uint8_t        lineH;
  uint8_t        ascent;
};

"""
    for name, _, _, _ in FONTS:
        hdr += f"extern const Font {name};\n"
    hdr += "\n"
    with open(OUT_H, "w") as fh:
        fh.write(hdr)
    print("Всего байт:", total, "->", OUT_C)


if __name__ == "__main__":
    main()
