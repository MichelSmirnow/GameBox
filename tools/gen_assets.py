#!/usr/bin/env python3
"""
GameBox asset generator.

Рисует всю 1-битную графику (без полутонов, только чёрное/белое) и
экспортирует её в GameBox/src/gfx/gfx_data.cpp в формате image2cpp
("Horizontal, 1 bit per pixel, MSB first").

Запуск:  python3 tools/gen_assets.py
"""
import math
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "GameBox", "src", "gfx", "gfx_data.cpp")
PREVIEW = os.path.join(os.path.dirname(__file__), "..", "preview")
os.makedirs(PREVIEW, exist_ok=True)

assets = {}      # name -> (w, h, bytes)


# --------------------------------------------------------------------------- #
#  Примитивы
# --------------------------------------------------------------------------- #
def new(w, h, bg=0):
    return Image.new("1", (w, h), bg)


def save(name, img):
    w, h = img.size
    stride = (w + 7) // 8
    data = bytearray(stride * h)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                data[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
    assets[name] = (w, h, bytes(data))
    return img


def P(d, pts, fill=1):
    d.polygon(pts, fill=fill)


def waves(img, y0, amp=1, period=10, step=1, fill=1, phase=0, height=1):
    d = ImageDraw.Draw(img)
    w, h = img.size
    for x in range(w):
        y = y0 + int(round(amp * math.sin((x + phase) * 2 * math.pi / period)))
        for k in range(height):
            if 0 <= y + k < h:
                d.point((x, y + k), fill)


def dither(img, box, level=8, phase=0):
    """level 0..16 — плотность; рисует прямо в 1-битное изображение."""
    x0, y0, x1, y1 = box
    px = img.load()
    m = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    w, h = img.size
    for y in range(max(0, y0), min(h, y1)):
        for x in range(max(0, x0), min(w, x1)):
            if m[(y + phase) & 3][(x + phase) & 3] < level:
                px[x, y] = 1


def hatch(img, box, step=3, fill=1, diag=True):
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    if diag:
        for i in range(x0 - (y1 - y0), x1, step):
            d.line([(i, y1), (i + (y1 - y0), y0)], fill=fill)
    else:
        for y in range(y0, y1, step):
            d.line([(x0, y), (x1, y)], fill=fill)


def star(d, cx, cy, r1, r2, n=5, fill=1, rot=-90):
    pts = []
    for i in range(n * 2):
        r = r1 if i % 2 == 0 else r2
        a = math.radians(rot + i * 180.0 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, fill=fill)


def burst(img, cx, cy, r0, r1, n=12, fill=1, thick=1):
    d = ImageDraw.Draw(img)
    for i in range(n):
        a = 2 * math.pi * i / n
        for t in range(thick):
            x0 = cx + (r0 + t) * math.cos(a)
            y0 = cy + (r0 + t) * math.sin(a)
            x1 = cx + r1 * math.cos(a)
            y1 = cy + r1 * math.sin(a)
            d.line([(x0, y0), (x1, y1)], fill=fill)


# --------------------------------------------------------------------------- #
#  Логотип 48x40
# --------------------------------------------------------------------------- #
def gen_logo():
    W, H = 48, 40
    img = new(W, H)
    d = ImageDraw.Draw(img)
    # корпус геймпада
    d.rounded_rectangle([3, 8, 44, 33], radius=11, outline=1, width=1)
    d.ellipse([5, 22, 19, 36], outline=1, width=1)
    d.ellipse([29, 22, 43, 36], outline=1, width=1)
    # крестовина
    d.rectangle([12, 15, 15, 24], fill=1)
    d.rectangle([8, 18, 19, 21], fill=1)
    # кнопки
    d.ellipse([30, 16, 35, 21], fill=1)
    d.ellipse([36, 19, 41, 24], fill=1)
    # "антенна" — всплеск
    d.line([(24, 8), (24, 2)], fill=1)
    d.line([(20, 4), (24, 1)], fill=1)
    d.line([(28, 4), (24, 1)], fill=1)
    save("bmpLogo", img)


# --------------------------------------------------------------------------- #
#  Баннеры игр 128x32
# --------------------------------------------------------------------------- #
def banner_fish():
    img = new(128, 32)
    d = ImageDraw.Draw(img)
    # вода
    for base, amp, per in ((20, 1, 14), (24, 1, 9), (28, 1, 7)):
        waves(img, base, amp, per, height=1)
    # лодка
    P(d, [(88, 19), (120, 19), (114, 24), (94, 24)], 1)
    d.line([(104, 18), (104, 8)], fill=1)
    d.line([(104, 8), (112, 16)], fill=1)
    P(d, [(104, 9), (112, 17), (104, 17)], 1)
    # линия удочки
    d.line([(120, 10), (126, 4), (126, 18)], fill=1)
    # рыба, выпрыгнувшая из воды
    d.ellipse([26, 13, 54, 24], outline=1, width=1)
    P(d, [(26, 18), (17, 10), (19, 18), (17, 26)], 1)
    d.ellipse([44, 16, 46, 18], fill=1)     # глаз
    d.line([(28, 13), (34, 11)], fill=1)    # плавник
    # всплеск
    d.line([(24, 26), (20, 30)], fill=1)
    d.line([(30, 26), (32, 31)], fill=1)
    d.line([(18, 28), (14, 29)], fill=1)
    # пузыри
    for (x, y) in ((60, 24), (66, 21), (71, 26)):
        d.ellipse([x, y, x + 2, y + 2], outline=1)
    save("bmpBannerFish", img)


def banner_spts():
    img = new(128, 32)
    d = ImageDraw.Draw(img)
    burst(img, 30, 16, 10, 15, n=16)
    # качок-силуэт
    d.ellipse([25, 4, 35, 14], fill=1)          # голова
    P(d, [(19, 15), (41, 15), (37, 30), (23, 30)], 1)   # торс
    # руки
    P(d, [(20, 16), (14, 11), (11, 13), (18, 20)], 1)
    P(d, [(40, 16), (46, 11), (49, 13), (42, 20)], 1)
    # кулаки
    d.ellipse([7, 8, 15, 16], fill=1)
    d.ellipse([45, 8, 53, 16], fill=1)
    # молния силы
    P(d, [(66, 2), (74, 12), (69, 12), (78, 24), (64, 14), (70, 14)], 1)
    # текст-полоски шкалы
    for i in range(5):
        d.rectangle([86, 6 + i * 5, 86 + 8 + i * 7, 8 + i * 5], fill=1)
    save("bmpBannerSpts", img)


def banner_shot():
    img = new(128, 32)
    d = ImageDraw.Draw(img)
    dither(img, (0, 0, 128, 32), 4)
    # стол
    d.rectangle([0, 24, 127, 25], fill=1)
    # ружьё
    P(d, [(18, 18), (92, 18), (92, 21), (18, 21)], 1)
    P(d, [(18, 14), (36, 18), (36, 21), (18, 21)], 1)
    P(d, [(92, 17), (120, 15), (120, 16), (92, 20)], 1)
    d.rectangle([44, 21, 58, 24], fill=1)
    # патроны
    for i, x in enumerate((66, 74, 82)):
        d.rectangle([x, 6, x + 5, 16], fill=1)
        d.rectangle([x, 6, x + 5, 9], fill=1)
        d.rectangle([x + 1, 10, x + 4, 15], fill=0)
        d.rectangle([x + 1, 10, x + 4, 15], outline=1)
    # блик
    d.line([(24, 12), (34, 16)], fill=1)
    save("bmpBannerShot", img)


def banner_rng():
    img = new(128, 32)
    d = ImageDraw.Draw(img)
    burst(img, 30, 16, 6, 15, n=10)
    star(d, 30, 16, 13, 5, n=5, fill=1)
    star(d, 30, 16, 8, 3, n=5, fill=0)
    # кубик
    d.rounded_rectangle([62, 8, 82, 28], radius=3, outline=1, width=1)
    for (x, y) in ((66, 12), (78, 12), (72, 18), (66, 24), (78, 24)):
        d.rectangle([x - 1, y - 1, x + 1, y + 1], fill=1)
    # искры
    for (x, y, r) in ((92, 8, 2), (108, 18, 3), (98, 26, 2), (120, 8, 2)):
        d.line([(x - r, y), (x + r, y)], fill=1)
        d.line([(x, y - r), (x, y + r)], fill=1)
    save("bmpBannerRng", img)


def banner_life():
    img = new(128, 32)
    d = ImageDraw.Draw(img)
    cell = 5                       # 5x5 px на клетку -> 25x6 клеток
    g = [(1, 0), (2, 1), (0, 2), (1, 2), (2, 2)]     # планер
    cells = set()
    for (gx, gy) in ((1, 1), (7, 3), (13, 0), (18, 4), (10, 4), (4, 5), (21, 2)):
        for (cx, cy) in g:
            X, Y = gx + cx, gy + cy
            if 0 <= X < 25 and 0 <= Y < 6:
                cells.add((X, Y))
    # точки сетки (лёгкий фон)
    for cy in range(6):
        for cx in range(25):
            if (cx, cy) not in cells:
                d.point((cx * cell, cy * cell + 2), fill=1)
    for (cx, cy) in cells:
        d.rectangle([cx * cell + 1, cy * cell + 1, cx * cell + 4, cy * cell + 4], fill=1)
    save("bmpBannerLife", img)


# --------------------------------------------------------------------------- #
#  Рыбы: параметрические силуэты 13 px высотой
# --------------------------------------------------------------------------- #
class Fish:
    """Силуэт рыбы: длина, высота тела, форма хвоста, плавники, особенности."""

    def __init__(self, w, hbody, tail="fork", dorsal=True, eye=True, fin=0,
                 bill=False, whiskers=False, round_=False, spikes=False,
                 long_=False, flat=False, teeth=False, sail=False, glow=False,
                 stripes=0, spots=0, ink=0):
        self.w = w
        self.hb = hbody
        self.tail = tail
        self.dorsal = dorsal
        self.eye = eye
        self.bill = bill
        self.whiskers = whiskers
        self.round_ = round_
        self.spikes = spikes
        self.long_ = long_
        self.flat = flat
        self.teeth = teeth
        self.sail = sail
        self.glow = glow
        self.fin = fin
        self.stripes = stripes
        self.spots = spots
        self.ink = ink


def draw_fish(f: Fish):
    H = 13
    W = f.w
    img = new(W, H)
    d = ImageDraw.Draw(img)
    cy = H // 2
    hb = f.hb
    body_x0 = 0
    body_x1 = W - 7          # место под хвост
    if f.bill:
        body_x1 = W - 8
    # тело
    if f.round_:
        r = min(hb, (body_x1 - 3) // 2)
        cx = body_x1 // 2 + 1
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=1)
        body_x0, body_x1 = cx - r, cx + r
    else:
        pts_top, pts_bot = [], []
        n = body_x1 - body_x0 + 1
        for i in range(n):
            t = i / max(1, n - 1)
            if f.long_:
                prof = 0.35 + 0.55 * math.sin(math.pi * min(1.0, t * 2.2)) if t < 0.5 else 0.9 - 0.4 * (t - 0.5)
            else:
                prof = math.sin(math.pi * (0.12 + 0.88 * t)) ** (0.55 if f.flat else 1.1)
            hh = max(0.5, hb * prof)
            pts_top.append((body_x0 + i, cy - hh + 0.5))
            pts_bot.append((body_x0 + i, cy + hh - 0.5))
        P(d, pts_top + pts_bot[::-1], 1)
    # хвост
    tx = body_x1
    if f.tail == "fork":
        P(d, [(tx - 1, cy), (W - 1, 0), (W - 3, cy), (W - 1, H - 1)], 1)
    elif f.tail == "round":
        d.ellipse([tx - 2, cy - 4, W - 1, cy + 4], fill=1)
    elif f.tail == "fan":
        P(d, [(tx - 1, cy), (W - 1, 1), (W - 2, cy), (W - 1, H - 2)], 1)
        P(d, [(tx - 1, cy - 1), (W - 1, 1), (W - 1, cy - 2)], 1)
    elif f.tail == "whip":
        d.line([(tx, cy), (W - 1, cy)], fill=1)
        d.line([(tx, cy), (W - 3, cy - 4)], fill=1)
    # спинной плавник
    if f.dorsal and not f.round_:
        mx = body_x0 + (body_x1 - body_x0) // 2
        P(d, [(mx - 4, cy - hb + 1), (mx + 3, cy - hb - 2), (mx + 5, cy - hb + 1)], 1)
    if f.sail:
        mx = body_x0 + (body_x1 - body_x0) // 3
        P(d, [(mx - 3, cy - hb + 1), (mx + 4, 0), (mx + 8, cy - hb + 1)], 1)
    # брюшной плавник
    if not f.round_:
        P(d, [(body_x0 + (body_x1 - body_x0) // 2 - 1, cy + hb - 1),
              (body_x0 + (body_x1 - body_x0) // 2 + 2, cy + hb + 2),
              (body_x0 + (body_x1 - body_x0) // 2 + 3, cy + hb - 1)], 1)
    # меч / клюв
    if f.bill:
        d.line([(body_x0, cy), (0, cy)], fill=1)
        d.line([(body_x0, cy + 1), (2, cy + 1)], fill=1)
    # усы
    if f.whiskers:
        d.line([(body_x0 + 1, cy + 1), (0, cy + 5)], fill=1)
        d.line([(body_x0 + 2, cy + 2), (1, cy + 6)], fill=1)
    # шипы
    if f.spikes:
        for i in range(4):
            x = body_x0 + 3 + i * max(2, (body_x1 - body_x0 - 4) // 4)
            d.line([(x, cy - hb + 1), (x + 1, cy - hb - 3)], fill=1)
    # зубы
    if f.teeth:
        for i in range(3):
            x = body_x0 + 2 + i * 2
            d.line([(x, cy - 1), (x + 1, cy + 1)], fill=1)
    # ---------- текстура ----------
    top = cy - int(hb) + 1
    bot = cy + int(hb) - 1
    if f.stripes:
        for i in range(f.stripes):
            x = body_x0 + 5 + i * 3
            d.line([(x, top + (1 if i % 2 else 0)), (x, bot - (1 if i % 2 else 0))], fill=0)
    if f.spots:
        for i in range(f.spots):
            x = body_x0 + 5 + (i * 3) % max(4, (body_x1 - body_x0 - 6))
            y = cy - 2 + (i % 3) * 2 - 1
            d.point((x, y), 0)
            if i % 2 == 0:
                d.point((x + 1, y), 0)
    if f.ink:
        # заливка "плотная" — тело остаётся, но с инверсией нижней части
        d.line([(body_x0 + 2, cy + 1), (body_x1 - 1, cy + 1)], fill=0)
    # глаз
    if f.eye:
        ex = body_x0 + 2 if not f.round_ else body_x0 + 3
        d.rectangle([ex, cy - 2, ex + 1, cy - 1], fill=0)
        d.rectangle([ex, cy - 2, ex, cy - 1], fill=0)
        d.point((ex, cy - 2), 0)
        d.point((ex + 1, cy - 1), 0)
        d.rectangle([ex - 1, cy - 3, ex + 2, cy], fill=1)
        d.rectangle([ex, cy - 2, ex + 1, cy - 1], fill=0)
    return img


#        имя (файл)              ширина  высота  особенности
FISH_ART = [
    ("fishBoot",    Fish(22, 2.5, "round", dorsal=False, fin=0)),
    ("fishNavaga",  Fish(26, 3.0, "fork", spots=4)),
    ("fishPollock", Fish(30, 3.0, "fork", spikes=True)),
    ("fishLemon",   Fish(30, 3.5, "fan", sail=True)),
    ("fishFlounder", Fish(26, 4.0, "fan", dorsal=False, flat=True)),
    ("fishHake",    Fish(28, 3.0, "fork")),
    ("fishCod",     Fish(34, 3.2, "round", whiskers=True, spots=6)),
    ("fishPerch",   Fish(28, 4.0, "fork", spikes=True, stripes=4)),
    ("fishVendace", Fish(26, 2.8, "fork")),
    ("fishHumpback", Fish(28, 4.0, "fan")),
    ("fishCrucian", Fish(26, 4.2, "fan")),
    ("fishTrout",   Fish(30, 3.4, "fork", spikes=True, spots=7)),
    ("fishOmul",    Fish(32, 3.0, "fork")),
    ("fishDorado",  Fish(30, 4.0, "fan", spots=5)),
    ("fishMuksun",  Fish(32, 3.0, "fork")),
    ("fishCongrio", Fish(36, 2.2, "whip", long_=True)),
    ("fishSalmon",  Fish(34, 3.4, "fork", spots=6)),
    ("fishSquid",   Fish(28, 2.4, "whip", long_=True, dorsal=False)),
    ("fishTurbot",  Fish(28, 4.4, "fan", flat=True, dorsal=False, spots=8)),
    ("fishPike",    Fish(36, 3.0, "fan", spikes=True, teeth=True, spots=9)),
    ("fishSturgeon", Fish(38, 3.4, "fork", whiskers=True)),
    ("fishCarp",    Fish(32, 4.4, "fan", whiskers=True, stripes=3)),
    ("fishCatfish", Fish(38, 3.2, "round", whiskers=True)),
    ("fishGarfish", Fish(38, 1.8, "fork", long_=True, bill=True)),
    ("fishEel",     Fish(38, 1.8, "whip", long_=True, dorsal=False)),
    ("fishCottidae", Fish(28, 4.0, "round", spikes=True)),
    ("fishBeluga",  Fish(42, 3.2, "fork", whiskers=True)),
    ("fishParrot",  Fish(32, 4.0, "fan", teeth=True)),
    ("fishTuna",    Fish(36, 4.0, "fork", sail=True, stripes=3)),
    ("fishPaddle",  Fish(38, 2.6, "whip", bill=True, long_=True)),
    ("fishGuasa",   Fish(34, 3.2, "fork", teeth=True, spikes=True)),
    ("fishPuffer",  Fish(24, 5.5, "round", round_=True, spikes=True)),
    ("fishSword",   Fish(38, 3.0, "fork", bill=True, sail=True)),
    ("fishStingray", Fish(30, 3.0, "whip", flat=True, dorsal=False)),
    ("fishShark",   Fish(40, 3.4, "fork", sail=True, teeth=True, ink=1)),
    ("fishArowana", Fish(38, 3.0, "fan", long_=True, stripes=5)),
    ("fishLatimeria", Fish(36, 4.0, "round", sail=True)),
    ("fishCoelacanth", Fish(36, 4.0, "fork", sail=True, spikes=True)),
    ("fishAngler",  Fish(26, 4.5, "round", teeth=True, round_=True)),
    ("fishManta",   Fish(34, 3.4, "whip", flat=True, dorsal=False, sail=True)),
    ("fishOarfish", Fish(40, 1.6, "whip", long_=True, dorsal=False)),
    ("fishGhost",   Fish(30, 3.4, "fan", glow=True)),
    ("fishTitan",   Fish(44, 4.6, "fork", spikes=True, teeth=True, sail=True)),
    ("fishVoid",    Fish(40, 5.0, "fork", round_=True, teeth=True, spikes=True, sail=True)),
]


# --------------------------------------------------------------------------- #
#  Лодки 8 видов (40..56 x 26)
# --------------------------------------------------------------------------- #
def boat(kind):
    W, H = 52, 26
    img = new(W, H)
    d = ImageDraw.Draw(img)
    hull_y = 16
    if kind == 0:      # надувная лодка
        d.ellipse([6, 12, 46, 24], outline=1, width=1)
        d.line([(8, 18), (44, 18)], fill=1)
        d.line([(26, 18), (26, 8)], fill=1)
        d.line([(26, 9), (34, 16)], fill=1)
    elif kind == 1:    # деревянная вёсельная
        P(d, [(6, 14), (46, 14), (40, 23), (12, 23)], 1)
        d.rectangle([6, 14, 46, 15], fill=0)
        d.line([(26, 14), (26, 6)], fill=1)
        d.line([(26, 6), (36, 13)], fill=1)
        d.line([(14, 10), (18, 20)], fill=1)
        d.line([(38, 10), (34, 20)], fill=1)
    elif kind == 2:    # моторная лодка
        P(d, [(4, 13), (50, 13), (44, 22), (10, 22)], 1)
        d.rectangle([16, 6, 34, 13], fill=1)
        d.rectangle([20, 8, 24, 11], fill=0)
        d.rectangle([28, 8, 32, 11], fill=0)
        d.rectangle([46, 14, 50, 20], fill=1)
    elif kind == 3:    # парусник
        P(d, [(6, 15), (48, 15), (42, 23), (12, 23)], 1)
        d.line([(26, 15), (26, 2)], fill=1)
        P(d, [(27, 3), (40, 14), (27, 14)], 1)
        P(d, [(25, 5), (25, 14), (16, 14)], 1)
    elif kind == 4:    # катер с каютой
        P(d, [(4, 14), (50, 14), (45, 23), (9, 23)], 1)
        d.rectangle([14, 5, 38, 14], fill=1)
        d.rectangle([18, 8, 24, 12], fill=0)
        d.rectangle([28, 8, 34, 12], fill=0)
        d.line([(40, 5), (40, 14)], fill=1)
    elif kind == 5:    # яхта
        P(d, [(6, 16), (48, 16), (40, 24), (14, 24)], 1)
        d.polygon([(26, 16), (26, 1), (24, 16)], fill=1)
        P(d, [(24, 3), (10, 15), (24, 15)], 1)
        d.line([(26, 4), (26, 15)], fill=1)
        P(d, [(28, 5), (42, 15), (28, 15)], 1)
    elif kind == 6:    # траулер
        P(d, [(2, 12), (50, 12), (46, 24), (6, 24)], 1)
        d.rectangle([14, 2, 36, 12], fill=1)
        for x in range(17, 35, 4):
            d.rectangle([x, 5, x + 1, 9], fill=0)
        d.line([(38, 2), (38, 12)], fill=1)
        d.line([(40, 3), (48, 10)], fill=1)
        d.line([(10, 16), (44, 16)], fill=0)
    else:              # научный катамаран / лодка будущего
        P(d, [(2, 13), (50, 13), (46, 24), (6, 24)], 1)
        d.rounded_rectangle([16, 3, 36, 13], radius=3, outline=0, width=1)
        d.rectangle([16, 3, 36, 13], fill=1)
        d.rectangle([18, 5, 34, 11], fill=0)
        d.rectangle([22, 6, 30, 10], fill=1)
        d.line([(40, 8), (46, 8)], fill=1)
        d.ellipse([42, 3, 50, 11], outline=1)
    return img


# --------------------------------------------------------------------------- #
#  Зоны (пропуска) 44x26
# --------------------------------------------------------------------------- #
def zone(kind):
    W, H = 44, 26
    img = new(W, H)
    d = ImageDraw.Draw(img)
    if kind == 0:      # пруд
        d.rectangle([0, 16, 43, 25], fill=1)
        for x in range(0, 44, 7):
            d.line([(x, 16), (x, 25)], fill=0)
        d.ellipse([14, 12, 24, 18], fill=0)
        d.line([(30, 6), (34, 12)], fill=1)
        d.line([(34, 12), (38, 8)], fill=1)
        d.line([(30, 6), (34, 3)], fill=1)
        for x in (6, 12, 38):
            d.line([(x, 9), (x, 14)], fill=1)
        d.ellipse([4, 5, 10, 9], fill=1)
    elif kind == 1:    # река
        d.line([(0, 24), (10, 10), (22, 16), (34, 6), (43, 12)], fill=1)
        d.line([(0, 25), (10, 11), (22, 17), (34, 7), (43, 13)], fill=1)
        d.line([(0, 20), (12, 6)], fill=1)
        d.line([(24, 4), (36, 2)], fill=1)
    elif kind == 2:    # озеро
        d.ellipse([2, 12, 42, 25], outline=1)
        d.line([(8, 18), (36, 18)], fill=1)
        d.ellipse([30, 2, 40, 10], fill=1)
        d.line([(0, 12), (6, 8)], fill=1)
        d.line([(0, 13), (5, 16)], fill=1)
    elif kind == 3:    # море
        for base, per in ((18, 8), (21, 6), (24, 4)):
            waves(img, base, 1, per, height=1)
        P(d, [(8, 16), (20, 16), (16, 20), (11, 20)], 1)
        d.line([(14, 15), (14, 6)], fill=1)
        P(d, [(15, 7), (22, 15), (15, 15)], 1)
        d.ellipse([28, 2, 34, 7], outline=1)
    elif kind == 4:    # океан
        for base, per in ((17, 10), (21, 7), (24, 5)):
            waves(img, base, 1, per, height=1)
        d.line([(0, 10), (6, 4)], fill=1)
        d.line([(38, 8), (43, 3)], fill=1)
        P(d, [(16, 14), (30, 14), (26, 19), (20, 19)], 1)
        d.line([(22, 13), (22, 5)], fill=1)
        P(d, [(23, 6), (30, 13), (23, 13)], 1)
    else:              # бездна
        dither(img, (0, 0, 44, 26), 3)
        d.line([(2, 2), (10, 8), (6, 16), (16, 22)], fill=0)
        d.ellipse([18, 8, 34, 22], outline=1)
        d.ellipse([22, 12, 26, 16], fill=1)
        d.ellipse([30, 12, 34, 16], fill=0)
        for i in range(6):
            d.line([(20 + i * 2, 22), (18 + i * 3, 25)], fill=1)
        d.line([(38, 4), (42, 2)], fill=1)
        d.line([(36, 6), (40, 8)], fill=1)
    return img


# --------------------------------------------------------------------------- #
#  Навыки SPTS 26x26
# --------------------------------------------------------------------------- #
def skill(kind):
    W = 26
    img = new(W, W)
    d = ImageDraw.Draw(img)
    if kind == 0:      # кулак
        d.rounded_rectangle([6, 8, 20, 21], radius=3, fill=1)
        for x in (8, 11, 14, 17):
            d.line([(x, 8), (x, 4)], fill=1)
        d.rectangle([4, 11, 7, 17], fill=1)
        d.line([(8, 15), (18, 15)], fill=0)
    elif kind == 1:    # тело
        d.ellipse([9, 2, 17, 10], fill=1)
        P(d, [(5, 11), (21, 11), (18, 24), (8, 24)], 1)
        d.line([(11, 13), (15, 13)], fill=0)
        d.line([(11, 16), (15, 16)], fill=0)
        d.line([(11, 19), (15, 19)], fill=0)
    elif kind == 2:    # скорость
        for i, y in enumerate((6, 12, 18)):
            d.line([(3 + i, y), (14 + i, y)], fill=1)
        P(d, [(12, 4), (24, 13), (12, 22)], 1)
    elif kind == 3:    # прыжок
        d.ellipse([6, 2, 14, 10], fill=1)
        P(d, [(8, 10), (16, 12), (18, 20), (10, 18)], 1)
        d.line([(10, 20), (7, 25)], fill=1)
        d.line([(16, 20), (20, 25)], fill=1)
        d.line([(2, 22), (6, 22)], fill=1)
        d.line([(18, 8), (23, 4)], fill=1)
    else:              # психика
        d.ellipse([4, 4, 22, 22], outline=1)
        d.ellipse([9, 9, 17, 17], outline=1)
        d.ellipse([12, 12, 14, 14], fill=1)
        for a in range(0, 360, 45):
            r = math.radians(a)
            d.line([(13 + 9 * math.cos(r), 13 + 9 * math.sin(r)),
                    (13 + 12 * math.cos(r), 13 + 12 * math.sin(r))], fill=1)
    return img


# --------------------------------------------------------------------------- #
#  Buckshot: сцены 128x64
# --------------------------------------------------------------------------- #
def shot_scene(kind):
    W, H = 128, 64
    img = new(W, H)
    d = ImageDraw.Draw(img)
    if kind == "dealer":
        # фигура дилера в капюшоне
        dither(img, (0, 0, 128, 64), 3, phase=0)
        P(d, [(44, 64), (48, 26), (60, 12), (68, 12), (80, 26), (84, 64)], 1)
        d.ellipse([52, 14, 76, 38], fill=0)
        d.ellipse([52, 14, 76, 38], outline=1)
        d.ellipse([56, 22, 62, 28], fill=1)
        d.ellipse([66, 22, 72, 28], fill=1)
        d.line([(58, 33), (70, 33)], fill=1)
        d.rectangle([40, 40, 88, 64], fill=1)
        dither(img, (40, 40, 88, 64), 6, phase=2)
        d.line([(40, 40), (88, 40)], fill=0)
    elif kind == "dealer_gun":
        shot_scene("dealer")
        d.rectangle([24, 44, 104, 50], fill=1)
        d.rectangle([24, 44, 104, 50], outline=0)
        P(d, [(24, 40), (44, 44), (44, 50), (24, 50)], 1)
        d.rectangle([70, 50, 84, 56], fill=1)
        d.line([(104, 42), (120, 40)], fill=1)
        d.line([(104, 43), (120, 41)], fill=1)
    elif kind == "dealer_shoot":
        shot_scene("dealer_gun")
        burst(img, 122, 40, 2, 16, n=12, thick=1)
        for a in range(0, 360, 30):
            r = math.radians(a)
            d.line([(112 + 10 * math.cos(r), 40 + 10 * math.sin(r)),
                    (124 + 16 * math.cos(r), 40 + 16 * math.sin(r))], fill=1)
    elif kind == "player_shoot":
        # ствол, направленный на игрока (вид от первого лица)
        dither(img, (0, 0, 128, 64), 2)
        d.ellipse([44, 8, 84, 60], outline=1)
        d.ellipse([48, 12, 80, 56], outline=1)
        d.ellipse([54, 20, 74, 48], fill=1)
        d.ellipse([58, 24, 70, 40], fill=0)
        d.line([(44, 8), (30, 66)], fill=1)
        d.line([(84, 8), (98, 66)], fill=1)
    elif kind == "dead":
        dither(img, (0, 0, 128, 64), 4)
        # череп
        d.ellipse([44, 8, 84, 48], fill=1)
        d.ellipse([50, 18, 62, 32], fill=0)
        d.ellipse([66, 18, 78, 32], fill=0)
        P(d, [(60, 36), (68, 36), (66, 44), (62, 44)], 0)
        for x in (54, 60, 66, 72):
            d.line([(x, 48), (x, 56)], fill=1)
        d.line([(50, 56), (78, 56)], fill=1)
    elif kind == "table":
        dither(img, (0, 0, 128, 64), 3)
        d.rectangle([0, 46, 127, 48], fill=1)
        d.rectangle([0, 49, 127, 63], fill=0)
        P(d, [(16, 38), (108, 38), (108, 42), (16, 42)], 1)
        P(d, [(16, 32), (36, 38), (36, 42), (16, 42)], 1)
        d.rectangle([60, 42, 76, 46], fill=1)
        for i, x in enumerate((84, 92, 100)):
            d.rectangle([x, 22, x + 6, 36], fill=1)
            d.rectangle([x + 1, 25, x + 5, 34], fill=0)
            d.rectangle([x + 1, 25, x + 5, 34], outline=1)
    elif kind == "rounds":
        img = new(W, H)
        d = ImageDraw.Draw(img)
        # три гильзы в ряд
        for i, x in enumerate((20, 52, 84)):
            d.rounded_rectangle([x, 16, x + 24, 50], radius=3, fill=1)
            d.rounded_rectangle([x + 3, 20, x + 21, 34], radius=2, fill=0)
            d.rounded_rectangle([x + 3, 20, x + 21, 34], radius=2, outline=1)
            for y in range(38, 48, 3):
                d.line([(x + 2, y), (x + 22, y)], fill=0)
    elif kind == "money":
        d.rectangle([10, 18, 118, 46], outline=1, width=1)
        d.rectangle([14, 22, 114, 42], outline=1, width=1)
        d.ellipse([54, 22, 74, 42], outline=1)
        d.line([(58, 32), (70, 32)], fill=1)
        d.line([(64, 26), (64, 38)], fill=1)
        for x in (22, 100):
            d.rectangle([x, 26, x + 8, 38], outline=1)
    elif kind == "magnifier":
        W2 = 32
        img = new(W2, W2)
        d = ImageDraw.Draw(img)
        d.ellipse([3, 3, 21, 21], outline=1, width=2)
        d.line([(20, 20), (29, 29)], fill=1)
        d.line([(21, 21), (30, 30)], fill=1)
        d.line([(25, 29), (29, 25)], fill=1)
    elif kind == "shell_live":
        W2, H2 = 10, 20
        img = new(W2, H2)
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([1, 1, 8, 19], radius=2, fill=1)
        d.rectangle([2, 1, 7, 6], fill=0)
    elif kind == "shell_blank":
        W2, H2 = 10, 20
        img = new(W2, H2)
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([1, 1, 8, 19], radius=2, outline=1)
        d.line([(2, 18), (7, 2)], fill=1)
    elif kind == "disclaimer":
        # рамка + акценты; тексты рисует игра
        d.rectangle([1, 1, 126, 62], outline=1, width=1)
        d.rectangle([4, 4, 123, 59], outline=1, width=1)
        # предупреждающая штриховка-полоса
        for x in range(7, 121, 2):
            d.line([(x, 26), (x + 1, 26)], fill=1)
            d.line([(x, 30), (x + 1, 30)], fill=1)
        # череп в правом верхнем углу
        d.ellipse([106, 5, 122, 21], fill=1)
        d.ellipse([109, 9, 113, 14], fill=0)
        d.ellipse([115, 9, 119, 14], fill=0)
        d.rectangle([112, 16, 116, 18], fill=0)
    elif kind == "night":
        dither(img, (0, 0, 128, 64), 5)
        d.ellipse([88, 6, 112, 30], fill=1)
        d.ellipse([82, 2, 106, 26], fill=0)
        d.rectangle([0, 44, 127, 63], fill=0)
        # здание
        d.rectangle([36, 20, 92, 63], fill=1)
        for y in range(26, 58, 8):
            for x in range(42, 88, 10):
                d.rectangle([x, y, x + 5, y + 5], fill=0)
        d.rectangle([58, 52, 70, 63], fill=0)
        d.line([(36, 20), (92, 20)], fill=0)
    elif kind == "go_rounds":
        dither(img, (0, 0, 128, 64), 3)
        d.line([(0, 52), (127, 52)], fill=1)
        for x in range(0, 128, 6):
            d.line([(x, 52), (x - 4, 62)], fill=1)
        P(d, [(20, 34), (104, 34), (96, 50), (28, 50)], 1)
        d.rectangle([42, 16, 82, 34], fill=1)
        d.rectangle([46, 20, 78, 30], fill=0)
        d.rectangle([44, 24, 80, 36], fill=0)
    return img


# --------------------------------------------------------------------------- #
#  Мелкие иконки
# --------------------------------------------------------------------------- #
def dealer_face():
    W, H = 24, 24
    img = new(W, H)
    d = ImageDraw.Draw(img)
    # капюшон
    P(d, [(2, 23), (4, 8), (12, 2), (20, 8), (22, 23)], 1)
    # лицо — тёмный овал внутри капюшона
    d.ellipse([6, 7, 18, 21], fill=0)
    d.ellipse([6, 7, 18, 21], outline=1)
    # глаза
    d.ellipse([8, 11, 10, 13], fill=1)
    d.ellipse([14, 11, 16, 13], fill=1)
    # ухмылка
    d.line([(8, 17), (16, 17)], fill=1)
    d.line([(9, 18), (15, 18)], fill=1)
    save("bmpDealerFace", img)


def item_icons():
    names = []
    # сигарета
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.rectangle([0, 3, 5, 4], fill=1); d.rectangle([5, 3, 7, 4], fill=0)
    d.line([(6, 3), (6, 4)], fill=1)
    save("bmpIconCig", img); names.append("bmpIconCig")
    # лупа
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.ellipse([0, 0, 5, 5], outline=1); d.line([(5, 5), (7, 7)], fill=1)
    save("bmpIconMag", img); names.append("bmpIconMag")
    # пиво
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.rectangle([1, 1, 5, 7], fill=1); d.point((3, 3), 0); d.point((4, 4), 0)
    d.rectangle([6, 2, 7, 5], fill=1)
    save("bmpIconBeer", img); names.append("bmpIconBeer")
    # пила
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.line([(0, 6), (7, 1)], fill=1)
    for i in range(4):
        d.line([(2 + i * 2, 6 - i), (1 + i * 2, 4 - i)], fill=1)
    save("bmpIconSaw", img); names.append("bmpIconSaw")
    # наручники
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.ellipse([0, 2, 3, 7], outline=1); d.ellipse([4, 2, 7, 7], outline=1)
    d.line([(3, 4), (5, 4)], fill=1)
    save("bmpIconCuff", img); names.append("bmpIconCuff")
    # инвертор
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.line([(1, 1), (6, 6)], fill=1); d.line([(1, 6), (6, 1)], fill=1)
    save("bmpIconInv", img); names.append("bmpIconInv")
    # аптечка
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.rectangle([0, 2, 7, 7], outline=1)
    d.line([(3, 3), (3, 6)], fill=1); d.line([(2, 4), (5, 4)], fill=1)
    save("bmpIconMed", img); names.append("bmpIconMed")
    # телефон
    img = new(8, 8); d = ImageDraw.Draw(img)
    d.rounded_rectangle([1, 0, 6, 7], radius=2, outline=1)
    d.point((3, 5), 1)
    save("bmpIconPhone", img); names.append("bmpIconPhone")
    save("bmpItemIcon0", new(8, 8))  # пустышка
    return names


def icons():
    # холодильник 14x20
    img = new(14, 20)
    d = ImageDraw.Draw(img)
    d.rectangle([1, 0, 12, 19], outline=1)
    d.line([(1, 7), (12, 7)], fill=1)
    d.line([(3, 2), (3, 5)], fill=1)
    d.line([(3, 9), (3, 14)], fill=1)
    save("bmpFridge", img)

    # зелье 12x16
    img = new(12, 16)
    d = ImageDraw.Draw(img)
    d.rectangle([4, 0, 7, 3], fill=1)
    d.line([(4, 3), (2, 7)], fill=1)
    d.line([(7, 3), (9, 7)], fill=1)
    d.ellipse([1, 6, 10, 15], outline=1)
    d.ellipse([3, 9, 8, 14], fill=1)
    d.point((6, 4), 1)
    save("bmpPotion", img)

    # удочка 20x20
    img = new(20, 20)
    d = ImageDraw.Draw(img)
    d.line([(2, 18), (16, 2)], fill=1)
    d.line([(3, 18), (17, 2)], fill=1)
    d.line([(16, 2), (18, 14)], fill=1)
    d.ellipse([16, 14, 19, 17], outline=1)
    d.line([(4, 12), (8, 16)], fill=1)
    save("bmpRod", img)

    # замок 10x12
    img = new(10, 12)
    d = ImageDraw.Draw(img)
    d.rectangle([1, 5, 8, 11], fill=1)
    d.arc([2, 0, 7, 8], 180, 360, fill=1)
    d.rectangle([4, 7, 5, 9], fill=0)
    save("bmpIconLock", img)

    # сердце 9x8
    img = new(9, 8)
    d = ImageDraw.Draw(img)
    d.ellipse([0, 1, 4, 5], fill=1)
    d.ellipse([4, 1, 8, 5], fill=1)
    P(d, [(0, 3), (8, 3), (4, 7)], 1)
    save("bmpIconHeart", img)

    # патрон 6x12
    img = new(6, 12)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([1, 0, 4, 11], radius=1, fill=1)
    d.rectangle([2, 0, 3, 3], fill=0)
    save("bmpIconShell", img)

    # звезда 9x9
    img = new(9, 9)
    d = ImageDraw.Draw(img)
    star(d, 4, 4, 4, 2, n=5, fill=1)
    save("bmpIconStar", img)

    # монета 9x9
    img = new(9, 9)
    d = ImageDraw.Draw(img)
    d.ellipse([0, 0, 8, 8], outline=1)
    d.line([(4, 2), (4, 6)], fill=1)
    d.line([(3, 3), (6, 3)], fill=1)
    d.line([(3, 5), (6, 5)], fill=1)
    save("bmpIconCoin", img)

    # дробовик 32x12 (для HUD), ствол смотрит вправо
    img = new(32, 12)
    d = ImageDraw.Draw(img)
    d.rectangle([6, 3, 27, 5], fill=1)          # ствол
    d.rectangle([6, 6, 20, 7], fill=1)          # цевьё
    d.polygon([(0, 2), (8, 4), (8, 8), (2, 11)], fill=1)   # приклад
    d.rectangle([9, 7, 12, 9], fill=1)          # спусковая скоба
    d.rectangle([27, 2, 31, 6], fill=1)         # дульный срез
    d.point((8, 2), 1)                          # мушка
    save("bmpShotgun", img)

    # курсор-рука/указатель не нужен: используется треугольник


def main():
    gen_logo()
    banner_fish(); banner_spts(); banner_shot(); banner_rng(); banner_life()

    fish_names = []
    for name, f in FISH_ART:
        assets[name] = None
        img = draw_fish(f)
        save(name, img)
        fish_names.append(name)

    boats = [boat(i) for i in range(8)]
    for i, b in enumerate(boats):
        save(f"boat{i}", b)

    for i in range(6):
        save(f"zone{i}", zone(i))

    for i in range(5):
        save(f"skill{i}", skill(i))

    shot_names = ["dealer", "dealer_gun", "dealer_shoot", "player_shoot", "dead", "table",
                  "rounds", "money", "magnifier", "shell_live", "shell_blank",
                  "disclaimer", "night", "go_rounds"]
    shot_map = {
        "dealer": "bmpShotDealer", "dealer_gun": "bmpShotDealerGun",
        "dealer_shoot": "bmpShotDealerShoot", "player_shoot": "bmpShotPlayerShoot",
        "dead": "bmpShotDead", "table": "bmpShotTable", "rounds": "bmpShotRounds",
        "money": "bmpShotMoney", "magnifier": "bmpShotMagnifier",
        "shell_live": "bmpShotShellLive", "shell_blank": "bmpShotShellBlank",
        "disclaimer": "bmpDisclaimer", "night": "bmpShotNight", "go_rounds": "bmpShotGoRounds",
    }
    for k in shot_names:
        save(shot_map[k], shot_scene(k))

    icons()
    dealer_face()
    _itemNames = item_icons()

    # ---------------------------- запись C++ ---------------------------------
    lines = ["""// ---------------------------------------------------------------------------
//  GameBox — 1-битные изображения (сгенерировано tools/gen_assets.py)
//  Формат: image2cpp, Horizontal, 1 bit per pixel, MSB first.
// ---------------------------------------------------------------------------
#include "gfx.h"

"""]
    for name, (w, h, data) in assets.items():
        if data is None:
            continue
        lines.append(f"// {name}, {w}x{h}\n")
        lines.append(f"static const uint8_t {name}_data[] PROGMEM = {{\n")
        for i in range(0, len(data), 16):
            lines.append("  " + ",".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",\n")
        lines.append("};\n")
        lines.append(f"const Bmp {name} = {{ {w}, {h}, {name}_data }};\n\n")

    # массивы
    def arr(cname, ctype, names):
        out = [f"const Bmp {cname}[] = {{\n"]
        for n in names:
            out.append(f"  {n},\n")
        out.append("};\n")
        out.append(f"const uint8_t {ctype} = {len(names)};\n\n")
        return "".join(out)

    lines.append(arr("shotIntroFrames", "shotIntroCount",
                     ["bmpShotNight", "bmpShotGoRounds", "bmpShotTable", "bmpShotDealer",
                      "bmpShotDealerGun", "bmpShotDealerShoot"]))
    lines.append(arr("fishSprites", "fishSpriteCount", fish_names))
    lines.append(arr("boatSprites", "boatSpriteCount", [f"boat{i}" for i in range(8)]))
    lines.append(arr("spsZoneSprites", "spsZoneCount", [f"zone{i}" for i in range(6)]))
    lines.append(arr("spsSkillSprites", "spsSkillCount", [f"skill{i}" for i in range(5)]))

    with open(OUT, "w") as fh:
        fh.write("".join(lines))

    # ---------------------------- превью ------------------------------------
    total = sum(len(v[2]) for v in assets.values() if v)
    print(f"ассетов: {len([v for v in assets.values() if v])}, байт: {total}, файл: {OUT}")

    # контактный лист
    names = [n for n, v in assets.items() if v]
    cols = 6
    cellw, cellh = 136, 74
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("L", (cols * cellw, rows * cellh), 90)
    dd = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        w, h, data = assets[n]
        im = Image.new("1", (w, h), 0)
        px = im.load()
        stride = (w + 7) // 8
        for y in range(h):
            for x in range(w):
                if data[y * stride + (x >> 3)] & (0x80 >> (x & 7)):
                    px[x, y] = 1
        x0 = (i % cols) * cellw
        y0 = (i // cols) * cellh
        sheet.paste(im.convert("L"), (x0 + 2, y0 + 2))
        dd.text((x0 + 2, y0 + h + 4), f"{n} {w}x{h}", fill=255)
    sheet.save(os.path.join(PREVIEW, "contact_sheet.png"))


if __name__ == "__main__":
    main()
