#!/usr/bin/env python3
"""Склейка кадров f_0000.png, f_0001.png … (atoms.exe --shot … rec=ПАПКА) в зацикленный GIF.

    python tools/gif.py ПАПКА out.gif [--width 720] [--fps 12.5] [--colors 160]

Палитра общая для всего ролика (по нескольким кадрам) — так фон не мерцает, а файл меньше.
"""
import argparse
import glob
import hashlib
import os
import sys

from PIL import Image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("frames")
    ap.add_argument("out")
    ap.add_argument("--width", type=int, default=720)
    ap.add_argument("--fps", type=float, default=12.5)
    ap.add_argument("--colors", type=int, default=160)
    a = ap.parse_args()

    files = sorted(glob.glob(os.path.join(a.frames, "f_*.png")))
    if not files:
        raise SystemExit(f"нет кадров в {a.frames}")
    # окно, перекрытое другим, иногда отдаёт один и тот же кадр — такой ролик лучше переснять
    distinct = len({hashlib.md5(open(f, "rb").read()).hexdigest() for f in files})
    if distinct < len(files) // 4:
        print(f"{a.frames}: различных кадров {distinct} из {len(files)}")
        sys.exit(3)
    frames = []
    for f in files:
        im = Image.open(f).convert("RGB")
        if im.width > a.width:
            im = im.resize((a.width, round(im.height * a.width / im.width)), Image.LANCZOS)
        frames.append(im)

    # общая палитра: несколько кадров из разных частей ролика в одну полосу
    pick = frames[:: max(1, len(frames) // 6)][:6]
    strip = Image.new("RGB", (pick[0].width, pick[0].height * len(pick)))
    for k, im in enumerate(pick):
        strip.paste(im, (0, k * im.height))
    pal = strip.quantize(colors=a.colors, method=Image.Quantize.MEDIANCUT)

    out = [im.quantize(palette=pal, dither=Image.Dither.NONE) for im in frames]
    out[0].save(a.out, save_all=True, append_images=out[1:], duration=round(1000 / a.fps), loop=0, optimize=True, disposal=1)
    print(f"{a.out}: {len(out)} кадров, {os.path.getsize(a.out) / 1e6:.1f} МБ")


if __name__ == "__main__":
    main()
