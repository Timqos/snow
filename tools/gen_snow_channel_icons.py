#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Adapt the five owner-supplied GIFs to a fixed 33x33, two-ink pixel grid.

A single crop/scale is shared by every pose of each icon. The outer pixel is
always empty. 33 evenly spaced source-frame poses follow parameter position,
not an animation clock. Zero-run/literal XOR deltas need only 273 scratch bytes
in the device main loop; the original GIF files never enter the firmware.
"""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
SIDE, POSES = 33, 33
NAMES = ('KK', 'SD', 'HH', 'PR', 'BS')
BYTES = (SIDE * SIDE + 3) // 4


def encode(delta):
    out = bytearray(); i = 0
    while i < len(delta):
        if delta[i] == 0:
            start = i
            while i < len(delta) and delta[i] == 0 and i - start < 128: i += 1
            out.append(128 + i - start - 1)
        else:
            start = i; i += 1
            while i < len(delta) and i - start < 128:
                if delta[i] == 0 and i + 1 < len(delta) and delta[i + 1] == 0: break
                i += 1
            out.append(i - start - 1); out.extend(delta[start:i])
    return out


def pack(roles):
    out = bytearray(BYTES)
    for i, v in enumerate(roles): out[i // 4] |= v << (6 - 2 * (i % 4))
    return bytes(out)


def main():
    stream = bytearray(); offsets = [0]; meta = []; reference = bytearray()
    for name in NAMES:
        path = ROOT / 'assets/snow-animation' / (name + '.gif')
        im = Image.open(path); n = im.n_frames; box = None
        for i in range(n):
            im.seek(i); r, g, b = im.convert('RGB').split()
            ink = ImageChops.lighter(ImageChops.lighter(ImageChops.invert(r), ImageChops.invert(g)), ImageChops.invert(b))
            bound = ink.point(lambda p: 255 if p > 32 else 0).getbbox()
            if bound: box = bound if box is None else (min(box[0],bound[0]),min(box[1],bound[1]),max(box[2],bound[2]),max(box[3],bound[3]))
        w, h = box[2] - box[0], box[3] - box[1]
        size = (max(1,w*31//max(w,h)),max(1,h*31//max(w,h)))
        origin = ((SIDE-size[0])//2,(SIDE-size[1])//2)
        im.seek(0)
        candidates = [(count,c) for count,c in im.convert('RGB').getcolors(1000000) if c[1]-c[0]>60 and abs(c[1]-c[2])<8]
        colour = max(candidates)[1]
        indices = [round(i*(n-1)/(POSES-1)) for i in range(POSES)]
        old = bytes(BYTES); poses = []; bounds = []
        begin = len(stream)
        for index in indices:
            im.seek(index); small = im.convert('RGB').crop(box).resize(size,Image.Resampling.BOX)
            roles = [0]*(SIDE*SIDE)
            for y in range(size[1]):
                for x in range(size[0]):
                    r,g,b = small.getpixel((x,y))
                    accent = max(0,min(1,(min(g,b)-r)/(colour[1]-colour[0])))
                    base = max(0,min(1,(255-g-accent*(255-colour[1]))/255))
                    role = 2 if accent>=.28 and accent>=base*.85 else 1 if base>=.28 else 0
                    roles[(origin[1]+y)*SIDE+origin[0]+x] = role
            xs = [i%SIDE for i,v in enumerate(roles) if v]; ys = [i//SIDE for i,v in enumerate(roles) if v]
            assert xs and min(xs)>=1 and max(xs)<SIDE-1 and min(ys)>=1 and max(ys)<SIDE-1
            bounds.append([min(xs),min(ys),max(xs)+1,max(ys)+1]); poses.append(pack(roles)); reference.extend(poses[-1])
            stream.extend(encode(bytes(a^b for a,b in zip(poses[-1],old)))); old = poses[-1]
        offsets.append(len(stream))
        # Verify every adapted pose, not only the first and last.
        restored = bytearray(BYTES); cursor = begin
        for expected in poses:
            at = 0
            while at < BYTES:
                op = stream[cursor]; cursor += 1; length = (op&127)+1
                assert at+length<=BYTES
                if not op&128:
                    for j in range(length): restored[at+j] ^= stream[cursor+j]
                    cursor += length
                at += length
            assert bytes(restored)==expected
        assert cursor==offsets[-1]
        meta.append(dict(channel=name,source=path.name,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),source_size=list(im.size),source_frames=n,source_frame_indices=indices,crop=list(box),fit_size=list(size),origin=list(origin),poses=POSES,distinct_poses=len(set(poses)),bounds=bounds,encoded_bytes=len(stream)-begin))
    assert len(stream)<65536
    lines=['/* SPDX-License-Identifier: GPL-3.0-only */','/* Generated from the owner-supplied GIFs; see assets/snow-animation/manifest.json. */',f'#define SN_CHANNEL_SIDE {SIDE}u',f'#define SN_CHANNEL_POSES {POSES}u',f'#define SN_CHANNEL_BYTES {BYTES}u','static const uint16_t SN_CHANNEL_OFFSET[6]={'+','.join(map(str,offsets))+'};',f'static const uint8_t SN_CHANNEL_DATA[{len(stream)}]={{']
    for i in range(0,len(stream),20): lines.append(' '+','.join(f'0x{b:02x}' for b in stream[i:i+20])+',')
    lines.append('};')
    (ROOT/'firmware/src/snow_channel_icons.h').write_text('\n'.join(lines)+'\n')
    (ROOT/'assets/snow-animation/reference.frames').write_bytes(reference)
    manifest=dict(side=SIDE,poses=POSES,palette={'0':'background','1':'light contour','2':'system theme'},codec='zero-run/literal XOR deltas',scratch_bytes=BYTES,encoded_bytes=len(stream),main_zone={'x':13,'y':115,'width':66,'height':66},icons=meta)
    (ROOT/'assets/snow-animation/manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Snow channel icons: {len(stream)} flash bytes, {BYTES} stack scratch bytes; all {len(NAMES)*POSES} poses decoded losslessly')


if __name__=='__main__': main()
