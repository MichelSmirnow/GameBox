#!/usr/bin/env python3
"""Собирает картинки для README: макеты приставки с реальными кадрами дисплея
и анимированные GIF-демки. Скриншоты берутся из preview/, ничего не рисуется
«на глаз» — на экране макета всегда настоящий кадр 128x64.

    python3 tools/gen_readme.py
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageChops, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREVIEW = os.path.join(ROOT, "preview")
OUT = os.path.join(ROOT, "docs", "img")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

INK = (11, 14, 20)
PCB = (18, 23, 31)
PCB_HI = (34, 42, 55)
EDGE = (58, 70, 86)
ACCENT = (61, 220, 151)
SCREEN_GLOW = (61, 220, 151)

os.makedirs(OUT, exist_ok=True)


def load(name):
    p = os.path.join(PREVIEW, name)
    im = Image.open(p).convert("L")
    return im.point(lambda v: 255 if v > 127 else 0, "1")


# ---------------------------------------------------------------- макет платы
SS = 4          # суперсэмплинг: рисуем крупно, потом уменьшаем (сглаживание)


def device(screen_name, out_name, scale=2):
    """Корпус приставки с настоящим кадром 128x64 на экране."""
    W, H = 440, 250
    S = SS
    im = Image.new("RGB", (W * S, H * S), INK)
    d = ImageDraw.Draw(im)

    def rr(box, r, fill=None, outline=None, width=1):
        d.rounded_rectangle([c * S for c in box], radius=r * S, fill=fill,
                            outline=outline, width=max(1, int(width * S)))

    # тень
    for i in range(9):
        pad = i * 3
        rr((12 - pad, 20 - pad + 8, 428 + pad, 240 + pad), 26 + pad,
           outline=(12 + i, 16 + i, 23 + i), width=3)

    # корпус
    rr((10, 14, 430, 238), 24, fill=(30, 36, 47))
    rr((12, 16, 428, 236), 22, fill=PCB)
    rr((12, 95, 428, 125), 0, fill=(21, 27, 36))          # мягкая полоса-блик
    rr((12, 118, 428, 236), 22, fill=(16, 21, 29))
    rr((12, 16, 428, 236), 22, outline=EDGE, width=1)
    rr((13, 17, 427, 235), 21, outline=(44, 54, 68), width=1)

    # USB-C
    rr((196, 7, 236, 18), 4, fill=(158, 168, 182))
    rr((200, 9, 232, 16), 3, fill=(52, 60, 72))

    # вентиляционные прорези и индикатор
    for i in range(6):
        rr((30 + i * 9, 30, 34 + i * 9, 40), 2, fill=(11, 15, 20))
    d.ellipse([(404 - 4) * S, (32 - 4) * S, (404 + 4) * S, (32 + 4) * S], fill=(61, 220, 151))
    d.ellipse([(404 - 2) * S, (32 - 2) * S, (404 + 2) * S, (32 + 2) * S], fill=(200, 255, 228))

    # ---- экран
    sx0, sy0, sx1, sy1 = 118, 58, 322, 162
    rr((sx0 - 7, sy0 - 7, sx1 + 7, sy1 + 7), 9, fill=(8, 10, 14))
    rr((sx0 - 7, sy0 - 7, sx1 + 7, sy1 + 7), 9, outline=(56, 66, 80), width=1)

    # свечение вокруг экрана
    glow = Image.new("RGB", im.size, (0, 0, 0))
    ImageDraw.Draw(glow).rectangle(
        [(sx0 + 6) * S, (sy0 + 6) * S, (sx1 - 6) * S, (sy1 - 6) * S], fill=(10, 34, 27))
    glow = glow.filter(ImageFilter.GaussianBlur(11 * S))
    im = ImageChops.add(im, glow)

    w, h = (sx1 - sx0) * S, (sy1 - sy0) * S
    scr = load(screen_name).resize((w, h), Image.NEAREST).convert("RGB")
    px = scr.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (232, 255, 244) if px[x, y][0] > 127 else (9, 14, 17)
    im.paste(scr, (sx0 * S, sy0 * S))
    d = ImageDraw.Draw(im)
    # лёгкие скан-линии: экран читается как настоящий
    veil = Image.new("RGB", im.size, (0, 0, 0))
    vd = ImageDraw.Draw(veil)
    for y in range(sy0 * S, sy1 * S, 2 * S):
        vd.rectangle([sx0 * S, y, sx1 * S, y + S - 2], fill=(0, 0, 0))
    im = Image.composite(Image.blend(im, veil, 0.45), im, veil.convert("L").point(lambda v: 255 if v else 0))
    d = ImageDraw.Draw(im)
    rr((sx0 - 7, sy0 - 7, sx1 + 7, sy1 + 7), 9, outline=(48, 58, 72), width=1)

    # ---- джойстик
    cx, cy, r = 62, 108, 26
    d.ellipse([(cx - r - 6) * S, (cy - r - 6) * S, (cx + r + 6) * S, (cy + r + 6) * S],
              fill=(9, 12, 17))
    for k in range(r, 0, -1):
        t = k / r
        d.ellipse([(cx - k) * S, (cy - k) * S, (cx + k) * S, (cy + k) * S],
                  fill=(int(44 + 74 * (1 - t)), int(50 + 82 * (1 - t)), int(62 + 90 * (1 - t))))
    d.ellipse([(cx - 11) * S, (cy - 11) * S, (cx + 11) * S, (cy + 11) * S], fill=(19, 24, 32))
    d.ellipse([(cx - 7) * S, (cy - 7) * S, (cx + 7) * S, (cy + 7) * S], outline=(132, 146, 164),
              width=S)
    d.line([(cx - r + 3) * S, cy * S, (cx + r - 3) * S, cy * S], fill=(30, 37, 47), width=S)
    d.line([cx * S, (cy - r + 3) * S, cx * S, (cy + r - 3) * S], fill=(30, 37, 47), width=S)
    d.text((cx * S - 8 * S, 145 * S), "СТИК", font=ImageFont.truetype(FONT_B, int(8 * S)),
           fill=(110, 124, 142))

    # ---- кнопки 2x2 (цвет = назначение, как в controls.md)
    btns = [((350, 80), (61, 220, 151)), ((394, 80), (255, 118, 118)),
            ((350, 124), (110, 176, 255)), ((394, 124), (255, 193, 92))]
    for (bx, by), col in btns:
        rr((bx - 18, by - 18, bx + 18, by + 18), 9, fill=(10, 13, 18))
        for k in range(13, 0, -1):
            t = k / 13
            c = tuple(int(v * (0.50 + 0.50 * (1 - t))) for v in col)
            d.ellipse([(bx - k) * S, (by - k) * S, (bx + k) * S, (by + k) * S], fill=c)
        d.ellipse([(bx - 9) * S, (by - 12) * S, (bx + 3) * S, (by - 3) * S],
                  fill=tuple(min(255, int(v * 1.3) + 26) for v in col))
    fb = ImageFont.truetype(FONT_B, int(7 * S))
    fs = ImageFont.truetype(FONT, int(7 * S))
    fsmall = ImageFont.truetype(FONT, int(6.5 * S))
    legend = [[("ВЫБОР", (61, 220, 151)), ("  НАЗАД", (255, 118, 118))],
              [("СБРОС", (110, 176, 255)), ("  МЕНЮ", (255, 193, 92))]]
    ly = 152
    for row in legend:
        lx = 330.0
        for txt, col in row:
            d.text((lx * S, ly * S), txt, font=fsmall, fill=col)
            lx += d.textlength(txt, font=fsmall) / S
        ly += 11

    # ---- пьезо-излучатель
    px_, py_ = 394, 202
    d.ellipse([(px_ - 14) * S, (py_ - 14) * S, (px_ + 14) * S, (py_ + 14) * S], fill=(9, 12, 17))
    d.ellipse([(px_ - 10) * S, (py_ - 10) * S, (px_ + 10) * S, (py_ + 10) * S], fill=(30, 37, 48))
    d.ellipse([(px_ - 3) * S, (py_ - 3) * S, (px_ + 3) * S, (py_ + 3) * S], fill=(7, 9, 13))
    d.text((330 * S, 195 * S), "звук", font=fs, fill=(84, 96, 112))

    # шелкография
    d.text((24 * S, 196 * S), "GameBox", font=ImageFont.truetype(FONT_B, int(11 * S)),
           fill=(122, 138, 158))
    d.text((24 * S, 214 * S), "ESP32-C3 Super Mini · OLED 128×64 · 5 игр",
           font=fs, fill=(84, 96, 112))

    # винты
    for (vx, vy) in ((26, 30), (414, 30), (26, 222), (414, 222)):
        d.ellipse([(vx - 4) * S, (vy - 4) * S, (vx + 4) * S, (vy + 4) * S], fill=(62, 72, 86))
        d.ellipse([(vx - 2) * S, (vy - 2) * S, (vx + 2) * S, (vy + 2) * S], fill=(22, 27, 35))

    out = im.resize((W * scale, H * scale), Image.LANCZOS)
    path = os.path.join(OUT, out_name)
    out.save(path, optimize=True)
    print("%-28s %6.1f КБ" % (out_name, os.path.getsize(path) / 1024))


# ------------------------------------------------------------------- GIF-демки
def demo(frames, out_name, ms=650, scale=2, pad=4, border=ACCENT):
    ims = []
    for i, name in enumerate(frames):
        s = load(name).resize((128 * scale, 64 * scale), Image.NEAREST).convert("RGB")
        px = s.load()
        for y in range(s.height):
            for x in range(s.width):
                px[x, y] = (232, 255, 244) if px[x, y][0] > 127 else (12, 17, 20)
        w, h = s.width + pad * 2, s.height + pad * 2
        f = Image.new("RGB", (w, h), INK)
        f.paste(s, (pad, pad))
        ImageDraw.Draw(f).rectangle([0, 0, w - 1, h - 1], outline=border)
        ims.append(f.quantize(colors=8, method=Image.MEDIANCUT))
    path = os.path.join(OUT, out_name)
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=ms,
                loop=0, optimize=True, disposal=1)
    print("%-28s %6.1f КБ  (%d кадров)" % (out_name, os.path.getsize(path) / 1024, len(ims)))


def main():
    for name, src in [
        ("mock-menu.png", "screen_002_menu.png"),
        ("mock-fish.png", "screen_019_fish_rod.png"),
        ("mock-spts.png", "screen_034_spts_train.png"),
        ("mock-shot.png", "screen_056_shot_play.png"),
        ("mock-rng.png", "screen_064_rng_roll.png"),
        ("mock-life.png", "screen_075_life_sim.png"),
        ("mock-boot.png", "screen_000_boot_phase1.png"),
    ]:
        device(src, name)

    demo(["screen_000_boot_phase1.png", "screen_001_boot_phase2.png",
          "screen_002_menu.png", "screen_013_games_1_fish.png",
          "screen_014_games_2.png", "screen_015_games_3.png",
          "screen_016_games_4.png", "screen_017_games_5.png"],
         "demo-start.gif", 700)

    demo(["screen_018_fish_enter.png", "screen_019_fish_rod.png",
          "screen_020_fish_casting.png", "screen_021_fish_bite.png",
          "screen_022_fish_caught.png", "screen_023_fish_boats.png",
          "screen_027_fish_potions.png", "screen_029_fish_board.png"],
         "demo-fish.gif", 600)

    demo(["screen_033_spts_enter.png", "screen_034_spts_train.png",
          "screen_035_spts_train_2.png", "screen_036_spts_burst_notice.png",
          "screen_037_spts_zones.png", "screen_040_spts_zones_body.png",
          "screen_043_spts_arena.png", "screen_045_spts_battle_result.png"],
         "demo-spts.gif", 600)

    demo(["screen_048_shot_enter.png", "screen_050_shot_intro_bath.png",
          "screen_053_shot_round_shells.png", "screen_054_shot_round_lives.png",
          "screen_056_shot_play.png", "screen_057_shot_items.png",
          "screen_060_shot_fire.png", "screen_061_shot_result.png"],
         "demo-shot.gif", 700)

    demo(["screen_063_rng_enter.png", "screen_064_rng_roll.png",
          "screen_065_rng_anim_1.png", "screen_067_rng_after.png",
          "screen_068_rng_collection.png", "screen_069_rng_potions.png",
          "screen_071_rng_upgrades.png", "screen_072_rng_stats.png"],
         "demo-rng.gif", 600)

    demo(["screen_073_life_enter.png", "screen_074_life_editor.png",
          "screen_075_life_sim.png", "screen_076_life_paused.png",
          "screen_077_life_back_to_editor.png"],
         "demo-life.gif", 800)




# ------------------------------------------------------------------- SVG-рисунки
import base64


def b64(png_name):
    with open(os.path.join(PREVIEW, png_name), "rb") as f:
        return "data:image/png;base64," + base64.b64encode(f.read()).decode()


def svg_write(name, body, w, h):
    path = os.path.join(OUT, name)
    with open(path, "w") as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
                'viewBox="0 0 %d %d" role="img">\n%s\n</svg>\n' % (w, h, w, h, body))
    print("%-28s %6.1f КБ" % (name, os.path.getsize(path) / 1024))


def svg_console(x, y, w, h, screen_png, label_buttons=True):
    """Векторный корпус приставки + настоящий кадр внутри."""
    sw = w * 0.44
    sh = sw / 2.0
    sx, sy = x + w * 0.265, y + (h - sh) / 2.0
    out = []
    a = out.append
    a('<defs>'
      '<linearGradient id="case" x1="0" y1="0" x2="0" y2="1">'
      '<stop offset="0" stop-color="#2b3442"/><stop offset="1" stop-color="#151b24"/></linearGradient>'
      '<linearGradient id="glass" x1="0" y1="0" x2="1" y2="1">'
      '<stop offset="0" stop-color="#0b1116"/><stop offset="1" stop-color="#060a0d"/></linearGradient>'
      '<radialGradient id="g1" cx="0.35" cy="0.3" r="0.75">'
      '<stop offset="0" stop-color="#7dffc4"/><stop offset="1" stop-color="#2a9d6d"/></radialGradient>'
      '<radialGradient id="g2" cx="0.35" cy="0.3" r="0.75">'
      '<stop offset="0" stop-color="#ff9d9d"/><stop offset="1" stop-color="#b34a4a"/></radialGradient>'
      '<radialGradient id="g3" cx="0.35" cy="0.3" r="0.75">'
      '<stop offset="0" stop-color="#9ecbff"/><stop offset="1" stop-color="#4a76b3"/></radialGradient>'
      '<radialGradient id="g4" cx="0.35" cy="0.3" r="0.75">'
      '<stop offset="0" stop-color="#ffe0a0"/><stop offset="1" stop-color="#c19340"/></radialGradient>'
      '<radialGradient id="stick" cx="0.38" cy="0.32" r="0.8">'
      '<stop offset="0" stop-color="#5a6678"/><stop offset="1" stop-color="#1b212b"/></radialGradient>'
      '<filter id="glow" x="-40%" y="-40%" width="180%" height="180%">'
      '<feGaussianBlur stdDeviation="9"/></filter>'
      '</defs>')
    a('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="url(#case)" '
      'stroke="#3a4658"/>' % (x, y, w, h, h * 0.10))
    a('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="none" '
      'stroke="#46536a" opacity="0.7"/>' % (x + 3, y + 3, w - 6, h - 6, h * 0.095))
    # USB-C
    a('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="#96a0ae"/>'
      % (x + w * 0.42, y - 5, w * 0.16, h * 0.045, h * 0.02))
    # экран
    a('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="url(#glass)" '
      'stroke="#4d5b70"/>' % (sx - 6, sy - 6, sw + 12, sh + 12, 8))
    a('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="4" fill="#3ddc97" opacity="0.28" '
      'filter="url(#glow)"/>' % (sx, sy, sw, sh))
    a(screen_rects(screen_png, sx, sy, sw, sh))
    # стик
    cx, cy, r = x + w * 0.15, y + h * 0.47, h * 0.165
    a('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#0a0e13"/>' % (cx, cy, r * 1.18))
    a('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="url(#stick)" stroke="#3d4757"/>' % (cx, cy, r * 1.02))
    a('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#151a22" stroke="#8794a6"/>'
      % (cx, cy, r * 0.42))
    a('<path d="M%.1f %.1f h%.1f M%.1f %.1f v%.1f" stroke="#28313d" stroke-width="2"/>'
      % (cx - r * 0.85, cy, r * 1.7, cx, cy - r * 0.85, r * 1.7))
    # кнопки
    bx, by, br = x + w * 0.82, y + h * 0.45, h * 0.062
    pos = [(-br * 1.55, -br * 1.55, "url(#g1)"), (br * 1.55, -br * 1.55, "url(#g2)"),
           (-br * 1.55, br * 1.55, "url(#g3)"), (br * 1.55, br * 1.55, "url(#g4)")]
    for dx, dy, fill in pos:
        a('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#0a0e13"/>' % (bx + dx, by + dy, br * 1.34))
        a('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s"/>' % (bx + dx, by + dy, br, fill))
        a('<ellipse cx="%.1f" cy="%.1f" rx="%.1f" ry="%.1f" fill="#ffffff" opacity="0.35"/>'
          % (bx + dx - br * 0.28, by + dy - br * 0.35, br * 0.42, br * 0.26))
    if label_buttons:
        a('<text x="%.1f" y="%.1f" font-family="DejaVu Sans, sans-serif" font-size="10" '
          'fill="#8b9bb0" text-anchor="middle">ВЫБОР</text>' % (bx - br * 1.55, by + br * 3.4))
        a('<text x="%.1f" y="%.1f" font-family="DejaVu Sans, sans-serif" font-size="10" '
          'fill="#8b9bb0" text-anchor="middle">НАЗАД</text>' % (bx + br * 1.55, by + br * 3.4))
    return "".join(out)


def screen_rects(png_name, x, y, w, h):
    """Кадр 128x64 в виде векторных прямоугольников — SVG остаётся чистым
    вектором, картинка не «отваливается» при просмотре через <img>."""
    im = load(png_name).convert("L").resize((128, 64), Image.NEAREST).point(
        lambda v: 255 if v > 127 else 0)
    px = im.load()
    cw, ch = w / 128.0, h / 64.0
    parts = []
    for yy in range(64):
        xx = 0
        while xx < 128:
            if px[xx, yy]:
                run = 0
                while xx + run < 128 and px[xx + run, yy]:
                    run += 1
                parts.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f"/>'
                             % (x + xx * cw, y + yy * ch, run * cw + 0.35, ch + 0.35))
                xx += run
            else:
                xx += 1
    return '<g fill="#dcfff0">%s</g>' % "".join(parts)


def svg_masthead():
    w, h = 1200, 420
    a = []
    a.append('<defs><linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">'
             '<stop offset="0" stop-color="#0d1117"/><stop offset="0.55" stop-color="#111a20"/>'
             '<stop offset="1" stop-color="#0a0f14"/></linearGradient>'
             '<radialGradient id="spot" cx="0.78" cy="0.35" r="0.6">'
             '<stop offset="0" stop-color="#3ddc97" stop-opacity="0.22"/>'
             '<stop offset="1" stop-color="#3ddc97" stop-opacity="0"/></radialGradient>'
             '</defs>')
    a.append('<rect width="%d" height="%d" fill="url(#bg)"/>' % (w, h))
    a.append('<rect width="%d" height="%d" fill="url(#spot)"/>' % (w, h))
    a.append('<g opacity="0.16" stroke="#3ddc97">')
    for gx in range(0, w, 24):
        a.append('<path d="M%d 0 V%d" stroke-width="1"/>' % (gx, h))
    for gy in range(0, h, 24):
        a.append('<path d="M0 %d H%d" stroke-width="1"/>' % (gy, w))
    a.append('</g>')
    a.append('<text x="64" y="140" font-family="DejaVu Sans, sans-serif" font-size="76" '
             'font-weight="bold" fill="#eaf6ef" letter-spacing="2">GameBox</text>')
    a.append('<text x="66" y="184" font-family="DejaVu Sans, sans-serif" font-size="24" '
             'fill="#3ddc97">карманная приставка на ESP32-C3</text>')
    a.append('<text x="66" y="228" font-family="DejaVu Sans, sans-serif" font-size="16" '
             'fill="#8b9bb0">OLED 128×64 · джойстик · 4 кнопки · 5 игр · русский интерфейс</text>')
    a.append('<text x="66" y="252" font-family="DejaVu Sans, sans-serif" font-size="16" '
             'fill="#8b9bb0">сохранения в NVS · без сторонних библиотек</text>')
    for i, (t, c) in enumerate([("всё в 1 файле .ino", "#3ddc97"),
                                ("0 библиотек", "#6eb0ff"),
                                ("78 экранов UI", "#ffc15c")]):
        a.append('<rect x="%d" y="286" width="%d" height="34" rx="17" fill="%s" opacity="0.12" '
                 'stroke="%s"/>' % (66 + i * 176, 170, c, c))
        a.append('<text x="%d" y="308" font-family="DejaVu Sans, sans-serif" font-size="14" '
                 'fill="%s">%s</text>' % (66 + i * 176 + 20, c, t))
    a.append(svg_console(690, 52, 460, 258, "screen_019_fish_rod.png"))
    svg_write("masthead.svg", "".join(a), w, h)


def svg_buttons():
    w, h = 1000, 330
    a = []
    a.append('<defs><radialGradient id="bs1" cx="0.35" cy="0.3" r="0.75">'
             '<stop offset="0" stop-color="#7dffc4"/><stop offset="1" stop-color="#2a9d6d"/></radialGradient>'
             '<radialGradient id="bs2" cx="0.35" cy="0.3" r="0.75">'
             '<stop offset="0" stop-color="#ff9d9d"/><stop offset="1" stop-color="#b34a4a"/></radialGradient>'
             '<radialGradient id="bs3" cx="0.35" cy="0.3" r="0.75">'
             '<stop offset="0" stop-color="#9ecbff"/><stop offset="1" stop-color="#4a76b3"/></radialGradient>'
             '<radialGradient id="bs4" cx="0.35" cy="0.3" r="0.75">'
             '<stop offset="0" stop-color="#ffe0a0"/><stop offset="1" stop-color="#c19340"/></radialGradient>'
             '<radialGradient id="bst" cx="0.38" cy="0.32" r="0.8">'
             '<stop offset="0" stop-color="#5a6678"/><stop offset="1" stop-color="#1b212b"/></radialGradient>'
             '</defs>')
    a.append('<rect width="%d" height="%d" rx="18" fill="#0f141b"/>' % (w, h))
    # стик
    a.append('<circle cx="150" cy="150" r="86" fill="#0a0e13"/>')
    a.append('<circle cx="150" cy="150" r="74" fill="url(#bst)" stroke="#3d4757"/>')
    a.append('<circle cx="150" cy="150" r="30" fill="#151a22" stroke="#8794a6" stroke-width="2"/>')
    a.append('<path d="M84 150 H216 M150 84 V216" stroke="#28313d" stroke-width="2"/>')
    a.append('<text x="150" y="272" font-family="DejaVu Sans, sans-serif" font-size="17" '
             'font-weight="bold" fill="#cfe0ef" text-anchor="middle">ДЖОЙСТИК</text>')
    a.append('<text x="150" y="296" font-family="DejaVu Sans, sans-serif" font-size="13" '
             'fill="#7d8ea3" text-anchor="middle">X — GPIO0 · Y — GPIO1 · кнопка — GPIO3</text>')
    # кнопки 2x2
    btns = [(500, 96, "ВЫБОР", "GPIO4", "url(#bs1)", "#3ddc97"),
            (650, 96, "НАЗАД", "GPIO5", "url(#bs2)", "#ff7676"),
            (500, 218, "СБРОС", "GPIO6", "url(#bs3)", "#6eb0ff"),
            (650, 218, "МЕНЮ", "GPIO7", "url(#bs4)", "#ffc15c")]
    for bx, by, name, pin, fill, col in btns:
        a.append('<circle cx="%d" cy="%d" r="34" fill="#0a0e13"/>' % (bx, by))
        a.append('<circle cx="%d" cy="%d" r="26" fill="%s"/>' % (bx, by, fill))
        a.append('<ellipse cx="%d" cy="%d" rx="11" ry="7" fill="#fff" opacity="0.34"/>'
                 % (bx - 7, by - 9))
        a.append('<text x="%d" y="%d" font-family="DejaVu Sans, sans-serif" font-size="15" '
                 'font-weight="bold" fill="%s" text-anchor="middle">%s</text>'
                 % (bx, by + 54, col, name))
        a.append('<text x="%d" y="%d" font-family="DejaVu Sans, sans-serif" font-size="12" '
                 'fill="#6d7d92" text-anchor="middle">%s</text>' % (bx, by + 71, pin))
    a.append('<text x="790" y="120" font-family="DejaVu Sans, sans-serif" font-size="14" '
             'fill="#8b9bb0">все кнопки —</text>')
    a.append('<text x="790" y="141" font-family="DejaVu Sans, sans-serif" font-size="14" '
             'fill="#8b9bb0">пин → GND,</text>')
    a.append('<text x="790" y="162" font-family="DejaVu Sans, sans-serif" font-size="14" '
             'fill="#8b9bb0">INPUT_PULLUP</text>')
    svg_write("buttons.svg", "".join(a), w, h)


def svg_footer():
    w, h = 1200, 200
    a = []
    a.append('<defs><linearGradient id="fbg" x1="0" y1="0" x2="1" y2="0">'
             '<stop offset="0" stop-color="#0d1117"/><stop offset="0.5" stop-color="#122019"/>'
             '<stop offset="1" stop-color="#0d1117"/></linearGradient></defs>')
    a.append('<rect width="%d" height="%d" fill="url(#fbg)"/>' % (w, h))
    a.append('<text x="600" y="72" font-family="DejaVu Sans, sans-serif" font-size="22" '
             'font-weight="bold" fill="#eaf6ef" text-anchor="middle">Собрано на ESP32-C3 Super Mini</text>')
    a.append('<text x="600" y="104" font-family="DejaVu Sans, sans-serif" font-size="15" '
             'fill="#8b9bb0" text-anchor="middle">SSD1306 128×64 · KY-023 · 4 кнопки · NVS</text>')
    a.append('<text x="600" y="140" font-family="DejaVu Sans, sans-serif" font-size="14" '
             'fill="#3ddc97" text-anchor="middle">все картинки в этом README получены '
             'из настоящих кадров симулятора</text>')
    svg_write("footer.svg", "".join(a), w, h)




# --------------------------------------------------- HTML-версия README для просмотра
def build_html_preview():
    """README.md → docs/readme_preview.html с картинками внутри (data URI).
    Нужен, чтобы README можно было читать локально без интернета."""
    import re
    import markdown

    src = open(os.path.join(ROOT, "README.md"), encoding="utf-8").read()
    # бейджи shields.io требуют сети — в локальной версии они не нужны
    src = re.sub(r'<img alt="[^"]*" src="https://img\.shields\.io[^>]*>\n?', "", src)
    body = markdown.markdown(src, extensions=["tables", "fenced_code"])

    # якоря как на GitHub, чтобы ссылки из «Содержания» работали и в HTML
    def gh_slug(text):
        text = re.sub(r"<[^>]+>", "", text)
        text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE).strip().lower()
        return re.sub(r"\s", "-", text)

    def add_id(m):
        tag, attrs, inner = m.group(1), m.group(2), m.group(3)
        return '<%s%s id="%s">%s</%s>' % (tag, attrs, gh_slug(inner), inner, tag)

    body = re.sub(r"<(h[1-3])([^>]*)>(.*?)</\1>", add_id, body, flags=re.S)

    def embed(m):
        path = m.group(1)
        if path.startswith("http"):
            return m.group(0)
        full = os.path.join(ROOT, path)
        if not os.path.exists(full):
            return m.group(0)
        mime = "image/svg+xml" if path.endswith(".svg") else (
            "image/gif" if path.endswith(".gif") else "image/png")
        with open(full, "rb") as f:
            data = base64.b64encode(f.read()).decode()
        return 'src="data:%s;base64,%s"' % (mime, data)

    body = re.sub(r'src="([^"]+)"', embed, body)

    html = """<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>GameBox — README</title>
<style>
  :root { --bg:#0d1117; --fg:#dfe7ef; --dim:#8b9bb0; --acc:#3ddc97; --card:#161c24; }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg);
         font:16px/1.6 -apple-system,"Segoe UI",Roboto,"DejaVu Sans",sans-serif; }
  .wrap { max-width:1080px; margin:0 auto; padding:0 20px 80px; }
  img { max-width:100%; border-radius:8px; }
  h1,h2,h3 { line-height:1.25; }
  h1 { font-size:2.1em; } h2 { font-size:1.55em; margin-top:2em;
       border-bottom:1px solid #253140; padding-bottom:.3em; }
  h3 { font-size:1.2em; color:var(--acc); }
  a { color:#6eb0ff; }
  hr { border:0; border-top:1px solid #1d2632; margin:2.2em 0; }
  table { border-collapse:collapse; width:100%; margin:1em 0; background:var(--card); }
  th,td { border:1px solid #253140; padding:8px 10px; vertical-align:top; text-align:left; }
  th { background:#1b2431; }
  td img { display:block; margin:0 auto; image-rendering:pixelated; }
  code { background:#1b2431; padding:1px 5px; border-radius:4px; font-size:.9em; }
  pre { background:#11171e; border:1px solid #253140; border-radius:8px;
        padding:14px; overflow:auto; }
  pre code { background:none; padding:0; }
  blockquote { margin:1.2em 0; padding:.6em 1em; background:#13201a;
               border-left:3px solid var(--acc); border-radius:0 8px 8px 0; }
  div[align="center"] { text-align:center; }
  sub { color:var(--dim); }
  .foot { color:var(--dim); text-align:center; margin-top:40px; font-size:.9em; }
</style></head>
<body><div class="wrap">
__BODY__
<div class="foot">локальная копия README · картинки встроены, интернет не нужен</div>
</div></body></html>""".replace("__BODY__", body)

    out = os.path.join(ROOT, "docs", "readme_preview.html")
    open(out, "w", encoding="utf-8").write(html)
    print("%-28s %6.1f КБ" % ("readme_preview.html", os.path.getsize(out) / 1024))


def main_all():
    main()
    svg_masthead()
    svg_buttons()
    svg_footer()
    build_html_preview()


if __name__ == "__main__":
    main_all()
