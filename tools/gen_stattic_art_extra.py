#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Draw the extra HOME pictures (2026-11-08, after the owner's mood board).

Original procedural drawings in the spirit of the references (rounded-square
quadtrees, geometric glyph tiles, a pixel-icon checker, glitch smears, a
dithered field with a checker cloud, Swiss-style letter tiles). Nothing is
copied from the references. Each is 208 x 84, one bit, and wraps vertically
(HOME scrolls it), written to assets/stattic-home/extra/NN-name.png for
gen_stattic_art.py. Seeded: the same files every run.
"""
import math, random
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
W, H = 208, 84
OUT = ROOT / 'assets/stattic-home/extra'


def canvas():
    # drawn three times as tall, the middle third kept: shapes that cross
    # the top or bottom edge continue on the other side (vertical wrap)
    im = Image.new('1', (W, H * 3), 0)
    return im, ImageDraw.Draw(im)


def tiled(draw_cell, cell, rnd):
    """draw_cell(d, x, y, size, rnd) over a grid, repeated above and below"""
    im, d = canvas()
    cells = [(x, y) for y in range(0, H, cell) for x in range(0, W, cell)]
    plan = [(x, y, rnd.random()) for x, y in cells]
    for oy in (0, H, 2 * H):
        r = random.Random(7)
        for x, y, v in plan:
            r.seed(int(v * 1e9))
            draw_cell(d, x, y + oy, cell, r)
    return im.crop((0, H, W, 2 * H))


def rrect(d, x, y, w, h, r, fill=1):
    d.rounded_rectangle((x, y, x + w - 1, y + h - 1), radius=r, fill=fill)


# 1. QUAD: rounded squares split into quarters, big to tiny
def quad(rnd):
    im, d = canvas()
    def split(x, y, s, depth):
        if s > 4 and (depth == 0 or rnd.random() < 0.55):
            h = s // 2
            for dx, dy in ((0, 0), (h, 0), (0, h), (h, h)):
                split(x + dx, y + dy, h, depth + 1)
        elif rnd.random() < (0.72 if s <= 8 else 0.5):
            g = 1 if s <= 4 else 2
            rrect(d, x + g // 2, y + g // 2, s - g, s - g, max(1, s // 6))
    for y in range(0, H, 28):              # (84 = 3 x 28: no block crosses the wrap)
        for x in range(0, W, 28):
            split(x, y + H, 28, 0)
    return im.crop((0, H, W, 2 * H))


# 2. GLYPH: one geometric glyph per tile
def glyph_cell(d, x, y, s, r):
    k = r.randrange(12); c = s // 2; m = 1
    if k == 0: d.ellipse((x + m, y + m, x + s - 1 - m, y + s - 1 - m), fill=1)
    elif k == 1:
        d.ellipse((x + m, y + m, x + s - 1 - m, y + s - 1 - m), fill=1)
        d.ellipse((x + 4, y + 4, x + s - 5, y + s - 5), fill=0)
    elif k == 2:
        d.rectangle((x + m, y + m, x + s - 1 - m, y + s - 1 - m), fill=1)
        d.rectangle((x + 4, y + 4, x + s - 5, y + s - 5), fill=0)
    elif k == 3:
        d.rectangle((x + c - 2, y + m, x + c + 1, y + s - 1 - m), fill=1)
        d.rectangle((x + m, y + c - 2, x + s - 1 - m, y + c + 1), fill=1)
    elif k == 4:
        for i in range(2):
            for j in range(2):
                d.ellipse((x + 1 + i * c, y + 1 + j * c, x + c - 1 + i * c, y + c - 1 + j * c), fill=1)
    elif k == 5:
        for i in range(3):
            for j in range(3):
                d.rectangle((x + 1 + i * 4, y + 1 + j * 4, x + 3 + i * 4, y + 3 + j * 4), fill=1)
    elif k == 6:
        d.line((x + 1, y + 1, x + s - 2, y + s - 2), fill=1, width=3)
        d.line((x + s - 2, y + 1, x + 1, y + s - 2), fill=1, width=3)
    elif k == 7:
        d.pieslice((x - s + 1, y - s + 1, x + s - 1, y + s - 1), 0, 90, fill=1)
        d.pieslice((x - s + 5, y - s + 5, x + s - 5, y + s - 5), 0, 90, fill=0)
    elif k == 8:
        d.ellipse((x + c - 2, y + c - 2, x + c + 1, y + c + 1), fill=1)
    elif k == 9:
        for i in range(0, s, 3):
            d.line((x + i, y + 1, x + i, y + s - 2), fill=1)
    # 10, 11: empty


def glyph(rnd):
    return tiled(glyph_cell, 13, rnd)


# 3. ICONS: a big two-tone checker with small pixel icons, inverted on the light squares
ICONS = [
    ["0111110", "1000001", "1010101", "1000001", "1011101", "1000001", "0111110"],   # face
    ["0001000", "0011100", "0111110", "1111111", "0111110", "0011100", "0001000"],   # diamond
    ["0001000", "0011100", "0111110", "1111111", "1111111", "0101010", "0011100"],   # spade
    ["0011100", "0100010", "1001001", "1011101", "1001001", "0100010", "0011100"],   # eye
    ["1100011", "0111110", "1101011", "0111110", "1101011", "0111110", "1000001"],   # bug
    ["1010101", "0101010", "1010101", "0101010", "1010101", "0101010", "1010101"],   # check
    ["0011100", "0011100", "1111111", "1111111", "1111111", "0011100", "0011100"],   # plus
    ["1110111", "1000001", "1011101", "0010100", "1011101", "1000001", "1110111"],   # target
]


def icons(rnd):
    im, d = canvas()
    cols = [rnd.choice((12, 16, 20, 24, 28)) for _ in range(20)]
    xs = []; x = 0
    for w in cols:
        if x >= W: break
        xs.append((x, w)); x += w
    rows = []; y = 0
    while y < H:
        h = rnd.choice((12, 14, 21)); h = min(h, H - y); rows.append((y, h)); y += h
    cells = []
    for j, (y, h) in enumerate(rows):
        for i, (x, w) in enumerate(xs):
            cells.append((x, y, w, h, (i + j) & 1, rnd.random() < 0.35, rnd.randrange(len(ICONS)), rnd.random() < 0.18))
    for oy in (0, H, 2 * H):
        for x, y, w, h, light, ico, which, noise in cells:
            if light: d.rectangle((x, y + oy, x + w - 1, y + oy + h - 1), fill=1)
            if noise:
                for yy in range(h):
                    for xx in range(0, w, 2):
                        d.point((x + xx + (yy & 1), y + oy + yy), fill=0 if light else 1)
            if ico and w >= 9 and h >= 9:
                g = ICONS[which]; ox = x + (w - 7) // 2; oyy = y + oy + (h - 7) // 2
                for r, line in enumerate(g):
                    for c, b in enumerate(line):
                        if b == '1': d.point((ox + c, oyy + r), fill=0 if light else 1)
    return im.crop((0, H, W, 2 * H))


# 4. GLITCH: smeared bands, vertical streaks, hard blocks
def glitch(rnd):
    im = Image.new('1', (W, H), 0); px = im.load()
    base = [[0] * W for _ in range(H)]
    # a soft field of drifting waves, thresholded with ordered noise
    for y in range(H):
        ph = rnd.random() * 6
        for x in range(W):
            v = math.sin(x / 9.0 + math.sin(y / 7.0) * 2 + ph * 0.1) + math.sin((x + y * 1.7) / 15.0)
            base[y][x] = v
    for y in range(H):
        for x in range(W):
            t = ((x * 7 + y * 13) % 11) / 11.0 * 2 - 1
            px[x, y] = 1 if base[y][x] * 0.6 > t else 0
    # horizontal smears: copy one row across a band, shifted
    for _ in range(9):
        y0 = rnd.randrange(H); h = rnd.randrange(2, 9); sh = rnd.randrange(-30, 30)
        src = [px[x, y0] for x in range(W)]
        for y in range(y0, y0 + h):
            for x in range(W):
                px[x, y % H] = src[(x + sh) % W]
    # vertical streaks
    for _ in range(26):
        x0 = rnd.randrange(W); y0 = rnd.randrange(H); l = rnd.randrange(8, 40)
        for y in range(y0, y0 + l):
            if rnd.random() < 0.8: px[x0, y % H] = 1 - px[x0, y % H]
    # blocks
    d = ImageDraw.Draw(im)
    for _ in range(14):
        w = rnd.choice((6, 10, 14, 20, 28)); h = rnd.choice((4, 6, 10, 16)); x = rnd.randrange(W - w); y = rnd.randrange(H)
        f = rnd.random() < 0.5
        d.rectangle((x, y, x + w - 1, min(H - 1, y + h - 1)), fill=1 if f else 0)
    return im


# 5. DITHER: a noisy dithered light, a checker cloud, solid blocks, a few code-like lines
def dither(rnd):
    im = Image.new('1', (W, H), 0); px = im.load()
    cx, cy = rnd.randrange(30, 90), rnd.randrange(20, 60)
    for y in range(H):
        for x in range(W):
            dy = min(abs(y - cy), H - abs(y - cy))
            v = max(0.0, 1.0 - math.hypot((x - cx) / 70.0, dy / 40.0))
            if y > 56 and x < 120: v = max(v, 0.85 - abs(y - 64) / 20.0)
            px[x, y] = 1 if rnd.random() < v * v * 0.9 else 0
    d = ImageDraw.Draw(im)
    # checker cloud
    ox, oy = rnd.randrange(110, 150), rnd.randrange(10, 40)
    for j in range(-9, 10):
        for i in range(-9, 12):
            if math.hypot(i / 1.2, j) + rnd.random() * 3 < 10 and (i + j) & 1:
                x = ox + i * 4; y = (oy + j * 4) % H
                d.rectangle((x, y, x + 3, y + 3), fill=1)
    # blocks
    for _ in range(4):
        w = rnd.choice((16, 22, 30)); h = rnd.choice((10, 16, 20)); x = rnd.randrange(W - w); y = rnd.randrange(H - h)
        d.rectangle((x, y, x + w - 1, y + h - 1), fill=1)
    # code-like dashes
    y = rnd.randrange(4, 20)
    for line in range(5):
        x = 8 + rnd.randrange(0, 12)
        for _ in range(rnd.randrange(3, 7)):
            l = rnd.randrange(2, 8)
            for k in range(l):
                if rnd.random() < 0.85: d.rectangle((x + k * 3, y, x + k * 3 + 1, y + 2), fill=1)
            x += l * 3 + 3
        y += 5
    return im


# 6. SWISS: tiles of letter fragments, arcs, rings and hatching in three sizes
def swiss_cell(d, x, y, s, r):
    k = r.randrange(11); t = max(2, s // 5)
    if k == 0:
        d.arc((x, y, x + 2 * s - 1, y + 2 * s - 1), 180, 270, fill=1, width=t)
    elif k == 1:
        d.ellipse((x + 1, y + 1, x + s - 2, y + s - 2), fill=1)
        d.ellipse((x + t + 1, y + t + 1, x + s - t - 2, y + s - t - 2), fill=0)
    elif k == 2:
        for i in range(-s, s, max(3, s // 4)):
            d.line((x + i, y + s - 1, x + i + s, y), fill=1, width=1)
        d.rectangle((x - 1, y, x - 1, y + s), fill=0)
    elif k == 3:      # L
        d.rectangle((x + 1, y + 1, x + t, y + s - 2), fill=1); d.rectangle((x + 1, y + s - 1 - t, x + s - 2, y + s - 2), fill=1)
    elif k == 4:      # Y
        c = s // 2
        d.line((x + 1, y + 1, x + c, y + c), fill=1, width=t); d.line((x + s - 2, y + 1, x + c, y + c), fill=1, width=t)
        d.rectangle((x + c - t // 2, y + c, x + c + (t - 1) // 2, y + s - 2), fill=1)
    elif k == 5:      # slash
        d.line((x + 1, y + s - 2, x + s - 2, y + 1), fill=1, width=t)
    elif k == 6:      # square ring
        d.rectangle((x + 1, y + 1, x + s - 2, y + s - 2), fill=1); d.rectangle((x + t + 1, y + t + 1, x + s - t - 2, y + s - t - 2), fill=0)
    elif k == 7:      # half disc
        d.pieslice((x + 1, y + 1, x + s - 2, y + 2 * s - 3), 180, 360, fill=1)
    elif k == 8:      # zero with stroke
        d.ellipse((x + 2, y + 1, x + s - 3, y + s - 2), outline=1, width=max(1, t - 1)); d.line((x + 2, y + s - 2, x + s - 3, y + 1), fill=1)
    # 9, 10: empty


def swiss(rnd):
    im, d = canvas()
    plan = []
    for by in range(0, H, 28):
        for bx in range(0, W, 28):
            mode = rnd.random()
            if mode < 0.22: plan.append((bx, by, 28, rnd.random()))
            elif mode < 0.55:
                for j in (0, 14):
                    for i in (0, 14): plan.append((bx + i, by + j, 14, rnd.random()))
            else:
                for j in range(0, 28, 7):
                    for i in range(0, 28, 7): plan.append((bx + i, by + j, 7, rnd.random()))
    for oy in (0, H, 2 * H):
        r = random.Random()
        for x, y, s, v in plan:
            r.seed(int(v * 1e9)); swiss_cell(d, x, y + oy, s, r)
    return im.crop((0, H, W, 2 * H))


ARTS = [('quad', quad, 3), ('glyph', glyph, 5), ('icons', icons, 8), ('glitch', glitch, 4), ('dither', dither, 9), ('swiss', swiss, 6)]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for i, (name, fn, seed) in enumerate(ARTS):
        im = fn(random.Random(seed)).convert('1')
        assert im.size == (W, H)
        im.save(OUT / f'{i + 1:02d}-{name}.png')
    print(f'{len(ARTS)} extra HOME pictures in {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
