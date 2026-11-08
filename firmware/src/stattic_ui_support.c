/* SPDX-License-Identifier: GPL-3.0-only */
#include "snow_font.h"
#define T_BG 0u
#define T_TEXT 0xffffu
#define T_THEME C_WHITE
#define T_INK 0u
#define T_MID RGB(176,172,168)
enum { SN_HOME_SYNTH,SN_HOME_GEN,SN_HOME_MIX,SN_HOME_PAT,SN_HOME_MS,SN_HOME_COUNT };
static void cv_frame(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t ink)
{
    cv_rect(x,y,w,1,ink);cv_rect(x,y+h-1,w,1,ink);cv_rect(x,y,1,h,ink);cv_rect(x+w-1,y,1,h,ink);
}
static int32_t snow_text(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{
    for(uint32_t n=0;*s&&n<max;n++,x+=6){
        uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(uint32_t r=0;r<7;r++)for(uint32_t b=0;b<5;b++)if((SN_FONT[c-32][r]>>(4-b))&1u)cv_rect(x+b,y+r,1,1,ink);
    }
    return x;
}
static uint32_t stattic_param_position(const track_t *t,uint32_t id)
{
    const param_desc_t *d=track_desc(t,id);int32_t v=stattic_base_value(t,id);
    return d->max>d->min?(uint32_t)clamp((v-d->min)*1000/(d->max-d->min),0,1000):0;
}
static uint32_t stattic_pattern_pending(uint32_t k)
{
    return stattic_change[k].armed&&(stattic_change[k].what&ST_CHANGE_PATTERN)?stattic_change[k].slot:ST_PATTERNS;
}
static uint32_t stattic_pattern_current(uint32_t k)
{
    uint32_t p=stattic_pattern_pending(k);return p<ST_PATTERNS?p:stattic_current[k];
}
static uint32_t stattic_pending_mute_bits(void)
{
    uint32_t now=0;for(uint32_t k=0;k<NTRK;k++)now|=(uint32_t)!!trk[k].p[P_MUTE]<<k;
    return stattic_ms_pending?now^stattic_ms_mute:0;
}
static uint32_t stattic_pending_solo_bits(void) { return stattic_ms_pending?song.solo^stattic_ms_solo:0; }
static void stattic_sound_name(const track_t *t,char *b)
{
    if(is_drum(t)){str_cpy(b,DRUM_KIT_NAMES[drum_kit()],20);return;}
    if(t->user&&up_used(t->user-1u)){up_name(t->user-1u,b);return;}
    const engine_t *e=ENGINES[t->eng_req%NENGINES];str_cpy(b,e->presets[t->preset%e->npresets].name,20);
}
