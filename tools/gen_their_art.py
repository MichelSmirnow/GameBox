#!/usr/bin/env python3
"""Импорт графики автора GameBox (Michail Smirnow) в наш проект.

Источники (архив Google Drive, распакован во временный каталог):
  * /tmp/a2/BuckshotRoulette.ino     — его image2cpp-массивы 128x64;
  * /tmp/bs/*.txt                    — те же кадры, выгруженные отдельными txt;
  * /tmp/bs/*.png                    — кадры, которых нет массивами;
  * "Запроганные картинки/..."       — PNG/JPG рыбы 56x13, иконки UI, меню.

Скрипт пишет GameBox/src/gfx/gfx_their.h/.cpp. В рабочем каталоге
картинки не хранятся: они сразу превращаются в 1-битные массивы.
"""
import os, re, glob
from PIL import Image

SRC1 = glob.glob('/tmp/a1/GameBox*')[0]
SRC2 = '/tmp/a2'
BS = '/tmp/bs'
OUT_H = 'GameBox/src/gfx/gfx_their.h'
OUT_C = 'GameBox/src/gfx/gfx_their.cpp'


def norm(s):
    # в архиве имена папок/файлов в виде #U041a#U043e..., приводим к тексту
    s = re.sub(r'#U([0-9a-fA-F]{4})', lambda m: chr(int(m.group(1), 16)), s)
    return re.sub(r'[^0-9a-zа-яё]', '', s.lower())


def walk_png(prefix):
    """Каталог, в имени которого есть prefix (обход мохнатых имён архива)."""
    for dp, dn, fn in os.walk(SRC1):
        if any(p.lower() in os.path.basename(dp).lower().split('#')[-1] + ' '
               for p in [prefix]) or prefix.lower() in ' '.join(
                   re.findall(r'#U[0-9a-f]{4}', os.path.basename(dp))).lower():
            if any(f.lower().endswith(('.png', '.jpg', '.jpeg')) for f in fn):
                return dp
    raise SystemExit('нет каталога ' + prefix)


# --------------------------------------------------------------- источники
def load_sources():
    src = {}
    txt = open(os.path.join(SRC2, 'BuckshotRoulette.ino'), encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'const unsigned char (\w+)\s*\[\]\s*PROGMEM\s*=\s*\{(.*?)\};', txt, re.S):
        data = [int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', m.group(2))]
        if len(data) >= 1000:
            src[norm(m.group(1))] = (data + [0] * 1024)[:1024]
    for p in glob.glob(os.path.join(BS, '*.txt')):
        body = open(p, encoding='utf-8', errors='replace').read()
        data = [int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', body)]
        if len(data) == 1024:                    # txt-выгрузки полные — приоритет
            src[norm(os.path.basename(p)[:-4])] = data
            src[norm('myBitmap' + os.path.basename(p)[:-4])] = data
    for p in glob.glob(os.path.join(BS, '*.png')):
        src['png:' + norm(os.path.basename(p)[:-4])] = p
    return src


def png_bits(path, w, h):
    im = Image.open(path).convert('RGBA')
    if im.size != (w, h):
        im = im.resize((w, h), Image.NEAREST)
    px = im.load()
    rows = []
    for y in range(h):
        rows.append([1 if (px[x, y][3] > 100 and
                           (px[x, y][0] * 299 + px[x, y][1] * 587 + px[x, y][2] * 114) // 1000 > 60)
                     else 0 for x in range(w)])
    return rows


def pack(rows, w, h):
    out = []
    for y in range(h):
        for bx in range(0, w, 8):
            byte = 0
            for bit in range(8):
                x = bx + bit
                if x < w and rows[y][x]:
                    byte |= 0x80 >> bit
            out.append(byte)
    return out


def emit_array(f, name, data, w, h):
    f.write('// %s, %dx%d\nstatic const uint8_t %s_data[] PROGMEM = {\n' % (name, w, h, name))
    for i in range(0, len(data), 16):
        f.write('  ' + ','.join('0x%02X' % b for b in data[i:i + 16]) + ',\n')
    f.write('};\n')


# --------------------------------------------------------------- списки
FISH_ORDER = ["Old Shoe", "Navaga", "Pollock", "Lemongrass", "Flounder", "Hake", "Cod",
              "Perch", "Vendace", "Humpback", "Crucian", "Trout", "Omul", "Dorado",
              "Muksun", "Congrio", "Salmon", "Squid", "Turbot", "Pike", "Sturgeon",
              "Carp", "Catfish", "Garfish", "Eel", "Cottidae", "Beluga", "Parrotfish",
              "Tuna", "Paddlefish", "Guasa", "Pufferfish", "Swordfish", "Stingray",
              "Shark", "Arowana", "Latimeriya", "Coelacant"]

UI_ORDER = [('boat', 'судно', 'Boat'), ('fridge', 'холодильник', 'Fridge'),
            ('beer', 'пиво', 'Beer'), ('potion', 'potion', 'Potion'),
            ('sold', 'sold', 'Sold'), ('lock', 'замок', 'Lock'),
            ('vines', 'Лианы', 'Vines'), ('road0', 'Road stable', 'Road0'),
            ('road1', 'Road_Anim1', 'Road1'), ('road2', 'Road_Anim2', 'Road2')]

SHOT = [
    ('shot_intro', 'introBitmap_intro', None),
    ('shot_toilet1', 'myBitmapToilet1', 'Toilet1'),
    ('shot_toilet2', 'myBitmapToilet2', 'Toilet2'),
    ('shot_toilet3', 'myBitmapToilet3', 'Toilet3'),
    ('shot_toilet4', 'myBitmapToilet4', 'Toilet4'),
    ('shot_toilet5', 'myBitmapToilet5', 'Toilet5'),
    ('shot_toilet6', 'myBitmapToilet6', 'Toilet6'),
    ('shot_toilet7', 'myBitmapToilet7', 'Toilet7'),
    ('shot_dealerNoEffect', 'myBitmapDealerNoEffect', 'DealerNoEffect'),
    ('shot_table', 'myBitmapTableWithShotgun', 'tableWithShotgun'),
    ('shot_dealer', 'myBitmapDealerWithShotgun', 'DilerWithShotgun'),
    ('shot_toDealer', 'myBitmapToDealerShoot', 'ToDealerShot'),
    ('shot_toPlayer', 'myBitmapToPlayerShoot', 'ToPlayerShoot'),
    ('shot_self', 'myBitmapSelfShoot', 'SelfShoot'),
    ('shot_dealerSelf', 'myBitmapDealerSelfShoot', 'DealerSelfShoot'),
    ('shot_blank', 'myBitmapBlank', 'Blank'),
    ('shot_dead', 'myBitmapDead', 'Dead'),
    ('shot_afterShoot', 'epd_bitmapAfterShoot', 'AfterShoot'),
    ('shot_lifeRound', 'myBitmapLifeRound', 'LifeRound'),
    ('shot_seeRounds', 'myBitmapSeeRounds', 'SeeRounds'),
    ('shot_goRounds', 'myBitmapGoToRounds', 'GoToRounds'),
    ('shot_roundCounter', 'myBitmapRoundCounter', 'RoundCounter'),
    ('shot_nullItems', 'myBitmapNullItems', 'NullItems'),
    ('shot_money', 'myBitmapMoney', 'Money'),
    ('shot_night', 'myBitmapTheNightIsYoung', 'TheNightIsYoung'),
]


def cname(alias):
    return 'bmp' + ''.join(w[:1].upper() + w[1:] for w in alias.split('_'))


def main():
    src = load_sources()
    h = open(OUT_H, 'w', encoding='utf-8')
    c = open(OUT_C, 'w', encoding='utf-8')
    h.write('// ---------------------------------------------------------------------------\n'
            '//  GameBox :: gfx_their.h — графика автора (Michail Smirnow).\n'
            '//  Сгенерировано tools/gen_their_art.py; вручную не править.\n'
            '// ---------------------------------------------------------------------------\n'
            '#pragma once\n#include "../core/canvas.h"\n\n')
    c.write('#include "gfx_their.h"\n\n')

    # --- Buckshot ---------------------------------------------------------
    h.write('// Buckshot Roulette: кадры 128x64 (его массивы)\n')
    ok = []
    for alias, name, hint in SHOT:
        data = src.get(norm(name)) or (src.get(norm(hint)) if hint else None)
        if data is None and hint and ('png:' + norm(hint)) in src:
            data = pack(png_bits(src['png:' + norm(hint)], 128, 64), 128, 64)
        if data is None:
            print('!! нет кадра', name)
            continue
        h.write('extern const Bmp %s;\n' % cname(alias))
        emit_array(c, alias, data, 128, 64)
        ok.append(alias)
    c.write('\n')
    for alias in ok:
        c.write('const Bmp %s = { 128, 64, %s_data };\n' % (cname(alias), alias))

    # --- рыбы -------------------------------------------------------------
    fdir = None
    for dp, dn, fn in os.walk(SRC1):
        if any(' - ' in x and x.lower().endswith(('.png', '.jpg', '.jpeg')) for x in fn):
            fdir = dp
            break
    files = {}
    for f in os.listdir(fdir):
        if ' - ' in f:
            files[norm(f.split(' - ', 1)[1].rsplit('.', 1)[0])] = os.path.join(fdir, f)
    h.write('\n// Рыбы автора: 38 спрайтов, высота 13\n')
    c.write('\n')
    for i, name in enumerate(FISH_ORDER):
        p = files.get(norm(name))
        if not p:
            print('!! нет рыбы', name)
            continue
        im = Image.open(p)
        w, hgt = min(im.size[0], 62), 13
        rows = png_bits(p, im.size[0], im.size[1])[:hgt]
        while len(rows) < hgt:
            rows.append([0] * im.size[0])
        rows = [r[:w] for r in rows]
        emit_array(c, 'fish%d' % i, pack(rows, w, hgt), w, hgt)
        c.write('// %s\n' % name)
    c.write('\nconst Bmp theirFish[38] = {\n')
    for i, name in enumerate(FISH_ORDER):
        im = Image.open(files[norm(name)])
        c.write('  { %d, 13, fish%d_data },\n' % (min(im.size[0], 62), i))
    c.write('};\n')
    h.write('extern const Bmp theirFish[38];\n')

    # --- UI рыбалки -------------------------------------------------------
    udir = None
    for dp, dn, fn in os.walk(SRC1):
        if os.path.basename(dp).endswith('UI') or os.path.basename(dp).endswith('UI#'):
            udir = dp
            break
    if udir is None:
        for dp, dn, fn in os.walk(SRC1):
            if any(norm(x) == 'roadstable' for x in fn):
                udir = dp
                break
    ufiles = {norm(f.rsplit('.', 1)[0]): os.path.join(udir, f) for f in os.listdir(udir)}
    h.write('\n// UI рыбалки автора\n')
    c.write('\n')
    for alias, key, nm in UI_ORDER:
        p = ufiles.get(norm(key))
        if not p:
            print('!! нет UI', key)
            continue
        im = Image.open(p)
        w, hgt = im.size
        emit_array(c, 'their_' + alias, pack(png_bits(p, w, hgt), w, hgt), w, hgt)
        c.write('const Bmp bmpTheir%s = { %d, %d, their_%s_data };\n' % (nm, w, hgt, alias))
        h.write('extern const Bmp bmpTheir%s;   // %dx%d %s\n' % (nm, w, hgt, key))

    # --- меню -------------------------------------------------------------
    mdir = None
    for dp, dn, fn in os.walk(SRC1):
        if len([x for x in fn if norm(x.rsplit('.', 1)[0]).startswith('menu')]) >= 3:
            mdir = dp
            break
    h.write('\n// Меню автора 128x46\n')
    for i in range(1, 5):
        p = [os.path.join(mdir, f) for f in os.listdir(mdir) if norm(f.rsplit('.', 1)[0]) == 'menu%d' % i]
        if not p:
            print('!! нет меню', i)
            continue
        im = Image.open(p[0])
        w, hgt = im.size
        emit_array(c, 'their_menu%d' % i, pack(png_bits(p[0], w, hgt), w, hgt), w, hgt)
        c.write('const Bmp bmpTheirMenu%d = { %d, %d, their_menu%d_data };\n' % (i, w, hgt, i))
        h.write('extern const Bmp bmpTheirMenu%d;\n' % i)

    h.close()
    c.close()
    print('готово:', OUT_H, OUT_C)


if __name__ == '__main__':
    main()
