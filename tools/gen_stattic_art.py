#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pack the supplied HOME artwork at native resolution, deduplicating rows.

The reference JPGs are design inputs only, never linked into firmware.
One-bit rows and small palette masks need no decoding frame or framebuffer.
"""
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
BOXES = ((63,366,895,699),(75,393,907,703),(69,391,901,703))
HEIGHTS = (84,78,78)
BADGES = ((2,(303,264,438,372),(34,27)),(3,(375,269,572,367),(49,25)),
          (4,(658,136,903,220),(61,21)),(5,(687,122,884,240),(49,30)))

def mono(im):
    im=im.convert('L');w,h=im.size
    return [tuple(sum((x+b<w and im.getpixel((x+b,y))>=112)<<(7-b) for b in range(8)) for x in range(0,w,8)) for y in range(h)]
def crows(rows):
    return '\n'.join(' {'+','.join(f'0x{x:02x}' for x in row)+'},' for row in rows)
def main():
    images=[Image.open(ROOT/f'assets/stattic-home/{n}.jpg').convert('RGB') for n in range(1,6)]
    rows=[];lookup={};indices=[]
    for im,box,height in zip(images,BOXES,HEIGHTS):
        frame=[]
        for row in mono(im.crop(box).resize((208,height),Image.Resampling.NEAREST)):
            if row not in lookup:lookup[row]=len(rows);rows.append(row)
            frame.append(lookup[row])
        indices.append(frame+frame[:84-height])
    assert len(rows)<256
    out=['/* SPDX-License-Identifier: GPL-3.0-only */','/* Generated from the user HOME mockups, assets/stattic-home/. */',
         '#define ST_STATIC_W 208u','#define ST_STATIC_H 84u',f'static const uint8_t ST_STATIC_ROWS[{len(rows)}][26]={{',crows(rows),'};',
         'static const uint8_t ST_STATIC_INDEX[3][84]={',crows(indices),'};']
    for i,(n,box,size) in enumerate(BADGES):
        im=images[n-1].crop(box).resize(size,Image.Resampling.NEAREST)
        white=Image.new('L',size);accent=Image.new('L',size)
        for y in range(size[1]):
            for x in range(size[0]):
                r,g,b=im.getpixel((x,y));theme=g>r*1.3 and g>80 and b>r*1.3
                white.putpixel((x,y),255 if min(r,g,b)>115 and not theme else 0)
                accent.putpixel((x,y),255 if theme else 0)
        for label,mask in (('WHITE',white),('THEME',accent)):
            packed=mono(mask);out += [f'static const uint8_t ST_BADGE_{i}_{label}[{len(packed)}][{len(packed[0])}]={{',crows(packed),'};']
    bar=images[4].crop((395,502,760,568)).resize((92,17),Image.Resampling.NEAREST)
    out += ['static const uint8_t ST_BAR[17][12]={',crows(mono(bar)),'};']
    (ROOT/'firmware/src/stattic_art.h').write_text('\n'.join(out)+'\n')
    print(f'STATTIC static: {len(rows)} unique rows, {len(rows)*26+252} flash bytes + small icon/bar masks')
if __name__=='__main__':main()
