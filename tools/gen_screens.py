#!/usr/bin/env python3
# ---------------------------------------------------------------------------
#  GameBox — сборка симулятора и экспорт экранов в preview/
#    python3 tools/gen_screens.py            # сборка + рендер всех кадров
#    python3 tools/gen_screens.py --no-build # только рендер (бинарь готов)
# ---------------------------------------------------------------------------
import os
import subprocess
import sys
import glob
import shutil

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                      # .../GameBox
PREVIEW = os.path.join(ROOT, "preview")
FRAMES = "/tmp/gbshots"
SIM = "/tmp/gbsim_gamebox"

SOURCES = [
    os.path.join(HERE, "sim", "host_main.cpp"),
]
for sub in ("core", "games", "gfx"):
    SOURCES += sorted(glob.glob(os.path.join(ROOT, "GameBox", "src", sub, "*.cpp")))

INCLUDES = [
    "-I", os.path.join(ROOT, "GameBox", "src", "core"),
    "-I", os.path.join(ROOT, "GameBox", "src"),
]


def build():
    cmd = ["g++", "-std=c++17", "-O1", "-o", SIM] + INCLUDES + SOURCES
    print("сборка симулятора…")
    subprocess.run(cmd, check=True)
    print("ok:", SIM)


def render():
    os.makedirs(PREVIEW, exist_ok=True)
    for f in glob.glob(os.path.join(PREVIEW, "screen_*.png")):
        os.remove(f)
    shutil.rmtree(FRAMES, ignore_errors=True)
    subprocess.run([SIM, FRAMES], check=True)
    files = sorted(glob.glob(os.path.join(FRAMES, "*.ppm")))
    shots = []
    for f in files:
        name = os.path.basename(f)[:-4]
        num, _, label = name.partition("_")
        out = os.path.join(PREVIEW, f"screen_{name}.png")
        im = Image.open(f).convert("L")
        im.resize((512, 256), Image.NEAREST).save(out)     # x4, читаемо
        shots.append((label or name, im))
    print(f"кадров: {len(shots)} -> {PREVIEW}")
    return shots


def contact(shots, cols=6):
    if not shots:
        return
    cellw, cellh = 264, 148
    rows = (len(shots) + cols - 1) // cols
    sheet = Image.new("L", (cols * cellw, rows * cellh), 40)
    from PIL import ImageDraw
    d = ImageDraw.Draw(sheet)
    for i, (label, im) in enumerate(shots):
        x = (i % cols) * cellw
        y = (i // cols) * cellh
        sheet.paste(im.resize((256, 128), Image.NEAREST), (x + 4, y + 4))
        d.text((x + 4, y + 134), label, fill=230)
    out = os.path.join(PREVIEW, "all_screens.png")
    sheet.save(out)
    print("контактный лист:", out)


if __name__ == "__main__":
    if "--no-build" not in sys.argv or not os.path.exists(SIM):
        build()
    contact(render())
