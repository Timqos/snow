#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(2026-11-09, the owner) STATTIC DR: synthesised micro drums instead of the sampled shots.

Ambient IDM / "pop micro drums": short, dry clicks rather than drums. Each category (KICK SNARE
HAT PERC) has a set of archetypes written in musical units with gen_drumkits.S (the same
drum_synth.c voice the SLOOP kits use). The firmware (stattic_drum_select.c stattic_micro_make)
draws one archetype from a lane's shot seed and varies its pitch, length, click and filter, so
every K turn gives new sounds and two seeds rarely sound alike.

LEVEL: written by tools/level_stattic_micro (tests/stattic_micro_level.c renders each archetype;
the levels make every archetype of a category as loud as the others, the categories at the
targets below, quieter than the old samples: clicks, not drums).

  tools/gen_stattic_micro.py firmware/src/stattic_micro.h [levels.json]
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gen_drumkits import S, enc  # noqa: E402

# (name, sound). S(wave, hz, bend, bt, hold, decay, tlev, t2, t2lev, click, src, nlev, nhold, ndec, filt, ...)
KICK = [
    ("PIP", S("sine", 56, 14, 12, 2, 90, 120, click=40)),
    ("TOCK", S("sine", 78, 24, 6, 0, 45, 120, 1.6, 40, click=70)),
    ("SUB DOT", S("sine", 47, 7, 25, 6, 170, 124, click=18)),
    ("POP", S("sine", 115, 30, 4, 0, 32, 120, click=28)),
    ("WOOD", S("tri", 66, 18, 8, 0, 70, 118, click=50, src="white", nlev=24, ndec=8, filt=("bp", 900, .3))),
    ("FM THUD", S("fm", 62, 12, 10, 2, 60, 116, click=40)),
    ("FELT", S("sine", 52, 5, 30, 4, 120, 122, src="white", nlev=26, ndec=10, filt=("lp", 420, .1, "all"))),
    ("CHIP", S("square", 62, 30, 8, 0, 40, 100, filt=("lp", 650, .2, "all"))),
    ("KNUCKLE", S("sine", 92, 12, 3, 0, 26, 124, 2.4, 50, click=90)),
    ("DUB DOT", S("sine", 44, 10, 18, 10, 240, 120, click=12, filt=("lp", 300, .2, "all"))),
]
SNARE = [
    ("SNAP", S("tri", 220, 0, 5, 0, 20, 40, click=110, src="white", nlev=90, ndec=25, hpf=2000)),
    ("GLASS RIM", S("bell", 450, 0, 5, 0, 25, 70, click=60, filt=("bp", 2500, .4, "all"))),
    ("PAPER", S(None, click=50, src="white", nlev=84, ndec=40, filt=("bp", 1500, .3))),
    ("MICRO CLAP", S(None, src="clap", nlev=90, ndec=30, filt=("bp", 1200, .35))),
    ("TICK SN", S("sine", 330, 12, 3, 0, 15, 70, click=90, src="white", nlev=70, ndec=18, hpf=3000)),
    ("BRUSH", S(None, src="white", nlev=64, nhold=2, ndec=70, filt=("lp", 2600, .1))),
    ("CHIP SN", S("square", 200, 6, 4, 0, 15, 50, src="chip", nlev=80, ndec=30, chip=4000)),
    ("FM SNAP", S("fm", 260, 20, 4, 0, 18, 80, src="white", nlev=54, ndec=20, hpf=1500)),
    ("CARD", S(None, click=127, src="white", nlev=50, ndec=9, filt=("bp", 3200, .5))),
    ("DUST SN", S("tri", 180, 4, 6, 0, 28, 50, src="white", nlev=72, ndec=45, filt=("bp", 1800, .2), hpf=900)),
]
HAT = [
    ("TICK", S(None, click=120, src="white", nlev=40, ndec=6, hpf=6000)),
    ("DUST", S(None, src="white", nlev=56, ndec=12, filt=("bp", 9000, .3), hpf=7000)),
    ("METAL TK", S(None, 4000, src="metal", nlev=70, ndec=15, hpf=5000)),
    ("CHIP HAT", S(None, src="chip", nlev=60, ndec=10, chip=9000, hpf=4000)),
    ("GRAIN", S(None, src="white", nlev=54, nhold=4, ndec=25, filt=("bp", 6000, .3))),
    ("AIR", S(None, src="white", nlev=40, ndec=35, hpf=8000)),
    ("GLASS", S("bell", 5200, 0, 5, 0, 12, 50, click=60)),
    ("CRACKLE", S(None, click=127, src="white", nlev=20, ndec=5, hpf=9000)),
    ("SIZZLE", S(None, 300, src="cym", nlev=60, ndec=20, filt=("bp", 8000, .35), hpf=5000)),
    ("NEEDLE", S("sine", 6200, 0, 5, 0, 8, 60, click=40)),
]
PERC = [
    ("BLIP", S("sine", 1200, 0, 5, 0, 15, 120, click=10)),
    ("DROP", S("sine", 900, 24, 20, 0, 40, 120)),
    ("WOOD", S("tri", 800, 2, 3, 0, 25, 110, click=50, filt=("bp", 1100, .3, "all"))),
    ("BELL", S("bell", 1400, 0, 5, 0, 120, 70)),
    ("FM PING", S("fm", 700, 0, 5, 0, 60, 110)),
    ("CHIP BLIP", S("square", 1000, 0, 5, 0, 20, 80, filt=("lp", 3000, .2, "all"))),
    ("GLITCH", S(None, src="chip", nlev=70, ndec=10, chip=2500)),
    ("KNOCK", S("sine", 400, 8, 5, 0, 30, 116, 2.3, 50, click=80)),
    ("TINE", S("sine", 2000, 0, 5, 0, 200, 90, 3.0, 50)),
    ("BUBBLE", S("sine", 520, 7, 15, 0, 50, 120)),
    ("MARIMBA", S("sine", 660, 0, 5, 0, 90, 116, 4.0, 40, click=24)),
    ("CLAVE", S("tri", 2400, 0, 5, 0, 20, 100, click=30)),
]
CATS = [("KICK", KICK), ("SNARE", SNARE), ("HAT", HAT), ("PERC", PERC)]


def main():
    out = Path(sys.argv[1])
    lv = json.loads(Path(sys.argv[2]).read_text()) if len(sys.argv) > 2 and Path(sys.argv[2]).exists() else {}
    rows, starts, names = [], [], []
    for cat, arch in CATS:
        starts.append(len(rows))
        for name, s in arch:
            trim = lv.get(cat + "/" + name, 0.0)
            rows.append(enc(s, trim))
            names.append(cat[0] + " " + name)
    starts.append(len(rows))
    L = ["/* SPDX-License-Identifier: GPL-3.0-only */",
         "/* Generated by tools/gen_stattic_micro.py: STATTIC DR micro drum archetypes (dsnd_t, drum_synth.c) */",
         "#ifndef STATTIC_MICRO_H", "#define STATTIC_MICRO_H",
         "#define ST_MICRO_N %du" % len(rows),
         "static const uint8_t ST_MICRO_START[5]={%s};" % ",".join(map(str, starts)),
         "static const dsnd_t ST_MICRO[ST_MICRO_N]={"]
    for v, n in zip(rows, names):
        L.append("    {%s},   /* %s */" % (",".join(str(x) for x in v), n))
    L += ["};", "#endif", ""]
    out.write_text("\n".join(L))
    print(len(rows), "archetypes ->", out)


if __name__ == "__main__":
    main()
