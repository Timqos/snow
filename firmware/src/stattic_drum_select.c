/* SPDX-License-Identifier: GPL-3.0-only */
/* A catalog ID identifies a shot, not a key position. Category checks apply
 * to load, generation and playback; missing user data falls back to factory. */
static int32_t stattic_shot_zone(uint32_t shot,uint32_t category)
{
    static const uint8_t note[4]={36,38,42,60};
    if(shot<ST_FACTORY_SHOTS)return shot/10u==category?(int32_t)shot:-1;
    if(shot<ST_USER_SHOT||shot>=ST_USER_SHOT+64u)return -1;
    uint32_t user=shot-ST_USER_SHOT,slot=user/16u,zone=user%16u;
    if(!stattic_user_catalog[slot]||zone>=usr_nz[slot])return -1;
    const smp_zone_t *z=&usr_zone[slot][zone];
    return z->n>=22u&&z->lo==note[category]&&z->hi==note[category]?(int32_t)(DZ_USR+user):-1;
}
/* (2026-11-09, the owner) DR plays synthesised micro drums (stattic_micro.h): a lane's pick is a seed
 * (0..63, 128..255) the sound is built from, or a user shot (64..127: the Samples page, USR1..4) */
#include "stattic_micro.h"
static int stattic_pick_user(uint32_t pick) { return pick>=ST_USER_SHOT&&pick<ST_USER_SHOT+64u; }
static int stattic_pick_ok(uint32_t pick,uint32_t category)
{
    return !stattic_pick_user(pick)||stattic_shot_zone(pick,category)>=0;
}
static uint32_t st_mix32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
static int32_t st_bits(uint32_t *h,int32_t lo,int32_t hi)    /* lo..hi from the hash, then the next hash */
{
    uint32_t v=*h%(uint32_t)(hi-lo+1);*h=st_mix32(*h+0x9e3779b9u);return lo+(int32_t)v;
}
static uint8_t st_u7(int32_t v){return (uint8_t)clamp(v,0,127);}
/* the sound of lane l for its seed: an archetype of the lane's category, varied in pitch, length,
 * click, filter and noise. The same seed gives the same sound (a project, a snapshot, an undo) */
static uint32_t stattic_micro_make(dsnd_t *d,uint32_t lane,uint32_t pick)
{
    uint32_t cat=ST_LANE_CATEGORY[lane&15u]&3u,h=st_mix32(pick*0x9e3779b1u^(lane+1u)*0x85ebca77u^0x5717a7u);
    uint32_t n=ST_MICRO_START[cat+1u]-ST_MICRO_START[cat],crush=0;
    /* (2026-11-09, the owner: softer, less of a caricature) the chip and glitch families are left out (a square
     * tone or the CHIP noise), and the variations are narrower: pitch, length, click, filter; no lo-fi */
    for(uint32_t k=0;k<n;k++){
        *d=ST_MICRO[ST_MICRO_START[cat]+(h+k)%n];
        if(d->wave!=DW_SQUARE&&(d->src&15u)!=DN_CHIP)break;
    }
    h=st_mix32(h);
    static const int8_t span[4]={3,4,6,7};                 /* pitch: +- semitones */
    int32_t semi=st_bits(&h,-span[cat],span[cat]),len=st_bits(&h,-6,6);
    if(lane==5u)len+=18;                                    /* the open hat: longer, still a hat */
    if(lane==6u)len-=6;                                     /* the pedal hat: shorter */
    if(d->wave)d->pitch=st_u7((int32_t)d->pitch+semi);
    d->fine=(uint8_t)st_bits(&h,0,15);
    if(d->bend)d->bend=(uint8_t)clamp((int32_t)d->bend+st_bits(&h,-4,4),0,96);
    d->btime=st_u7((int32_t)d->btime+st_bits(&h,-4,4));
    d->decay=st_u7((int32_t)d->decay+len);
    d->ndec=st_u7((int32_t)d->ndec+len);
    d->click=st_u7((int32_t)d->click*st_bits(&h,9,15)/16);
    if(d->flt&3u)d->fcut=st_u7((int32_t)d->fcut+st_bits(&h,-5,4)+(d->wave?0:semi));
    if(d->hpf)d->hpf=st_u7((int32_t)d->hpf+st_bits(&h,-4,4)+semi/2);
    if(d->chip)d->chip=st_u7((int32_t)d->chip+semi*2);
    if(d->src&&!d->wave&&(d->src&15u)==DN_METAL)d->pitch=st_u7((int32_t)d->pitch+semi);
    if(d->t2&&st_bits(&h,0,5)==0)d->t2=(uint8_t)clamp((int32_t)d->t2+st_bits(&h,-6,12),33,255);
    (void)crush;                                            /* (no lo-fi any more) */
    return crush;
}
static void stattic_drum_random(uint8_t *picks,uint32_t *state)
{
    for(uint32_t lane=0;lane<16;lane++){
        uint8_t user[64];uint32_t count=0,cat=ST_LANE_CATEGORY[lane];
        for(uint32_t shot=ST_USER_SHOT;shot<ST_USER_SHOT+64u;shot++)if(stattic_shot_zone(shot,cat)>=0)user[count++]=(uint8_t)shot;
        uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x?x:0x6d2b79f5u;
        uint32_t pick;
        if(count&&x%4u==0u){pick=user[(x>>2)%count];if(count>1u&&pick==picks[lane])pick=user[((x>>2)+1u)%count];}
        else{                                               /* a seed, never the one it had */
            do{pick=(x>>3)%192u;pick=pick<64u?pick:pick+64u;x=x*1664525u+1013904223u;}while(pick==picks[lane]);
        }
        picks[lane]=(uint8_t)pick;
    }
}
