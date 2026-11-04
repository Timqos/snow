/* SPDX-License-Identifier: GPL-3.0-only
 * Snow 0.3: fixed instrument roles, with the complete open synthesis surface. */
#include "snow_params.h"
#include "snow_styles.h"
static const char *const SN_ALGS[] = {"FM PAIR","FM CHAIN","FM PAR","RING","ADDITIVE","FOLD"};
static const char *const SN_WAVES[] = {"SINE","TRI","SAW","SQUARE","PULSE","ORGAN"};
static const char *const SN_RMODELS[] = {"ROOM","HALL","PLATE"};
static const char *const SN_MODES[] = {"HIT","GATED"};
static const char *const SN_FILTERS[] = {"OFF","LP","HP","BP"};
static const char *const SN_FB_NAMES[] = {"MOD","CARRIER","CROSS"};
static const char *const SN_L2_DST[] = {"OFF","TONE","SHAPE","SWEEP","DECAY","MIX","FEEDBACK","CUTOFF","DRIVE","PITCH","AMP"};
static const char *const SN_CONTOUR[] = {"WALK","RISE","FALL","ARCH","MOTIF"};
static const char *const SN_GEN_KIND[] = {"RHYTHM","MELODY","BOTH"};
static const char *const SN_LIVE[] = {"OFF","1 LOOP","2 LOOPS","4 LOOPS"};
static struct { uint32_t phase; int32_t value, fade, rng; } snow_l2[NTRK];
static uint32_t snow_role(const track_t *t) { return (uint32_t)(t-trk)%NTRK; }
static int16_t snow_offset[NTRK][4];
static uint8_t snow_mask=63;
static uint32_t div_samples(uint32_t div);
static int32_t snow_param(const track_t *t,uint32_t k)
{
    int32_t v=t->p[P_E0+k]+(k<4u?snow_offset[snow_role(t)][k]:0);
    if(t->p[SN_L2_DEST]==(int16_t)(k+1u))v+=snow_l2[snow_role(t)].value;
    return clamp(v,0,127);
}
static void snow_block(track_t *t)
{
    uint32_t k=snow_role(t),ph=snow_l2[k].phase,inc=LFO_INC[t->p[SN_L2_RATE]&127];
    int32_t wave,fade=snow_l2[k].fade;
    if(t->p[SN_L2_SYNC]) {
        static const uint8_t divs[11]={0,9,8,7,6,0,1,4,2,5,3};
        uint32_t period=div_samples(divs[(uint32_t)t->p[SN_L2_SYNC]%11u]);
        period=period?period:1u;
        inc=(0xffffffffu/period)*CTL+((0xffffffffu%period)*CTL+CTL)/period;
    }
    snow_l2[k].phase+=inc;
    if(!snow_l2[k].rng)snow_l2[k].rng=(int32_t)(0x293471u+k*977u);
    if(snow_l2[k].phase<ph)(void)noise32(&snow_l2[k].rng);
    ph=snow_l2[k].phase+((uint32_t)t->p[SN_L2_PHASE]<<25);
    switch(t->p[SN_L2_WAVE]) {
    case 1:wave=osc_tri(ph);break;
    case 2:wave=(int32_t)(ph>>16)-32768;break;
    case 3:wave=ph<0x80000000u?32767:-32767;break;
    case 4:wave=(snow_l2[k].rng>>16);break;
    default:wave=sine_i(ph);break;
    }
    fade=clamp(fade+(int32_t)(ENV_LIN[t->p[SN_L2_FADE]&127]>>9),0,32767);
    snow_l2[k].fade=fade;
    snow_l2[k].value=mulq15(wave,fade)*t->p[SN_L2_DEPTH]/32768;
}
static void snow_note_on(track_t *t,voice_t *v)
{
    for(uint32_t i=0;i<10u;i++)v->s[i]=0;
    v->s[2]=v->s[8]=1<<24;
    v->s[4]=(int32_t)((v->note+1u)*0x9e3779b9u);
    v->ph[0]=v->ph[1]=v->ph[2]=0;
    if(!t->p[SN_L2_TRIG]) {snow_l2[snow_role(t)].phase=0;snow_l2[snow_role(t)].fade=0;}
}
static int snow_done(track_t *t,voice_t *v)
{
    return v->s[1]>0 && (!t->p[SN_MODE] || v->s[1]==3) && v->s[0]<4096;
}
static int32_t snow_amp(track_t *t,voice_t *v,int32_t adsr)
{
    const int16_t *p=t->p;
    if(!v->active)return 0;
    int32_t stage=v->s[1],env=v->s[0],sus=p[SN_MODE]?p[P_SUS]<<17:0;
    uint32_t decay=(uint32_t)clamp(p[P_DEC]+snow_param(t,3)-64,0,127);
    (void)adsr;
    if(p[SN_MODE]&&!v->gate)stage=3;
    if(stage==0) {
        env+=(int32_t)ENV_LIN[p[P_ATK]&127];
        if(env>=(1<<24)){env=1<<24;stage=1;}
    }else if(stage==3)env-=mulq16(env,ENV_EXP[p[P_REL]&127]);
    else env+=mulq16(sus-env,ENV_EXP[decay]);
    v->s[0]=clamp(env,0,1<<24);v->s[1]=stage;
    v->s[2]-=mulq16(v->s[2],ENV_EXP[p[SN_PDEC]&127]);
    if(p[SN_FMDEC])v->s[8]-=mulq16(v->s[8],ENV_EXP[p[SN_FMDEC]&127]);
    int32_t amp=mulq15(v->s[0]>>9,v->vel*258);
    if(p[SN_L2_DEST]==10)amp=mulq15(amp,clamp(32767+snow_l2[snow_role(t)].value*258,0,32767));
    return amp;
}
static int32_t snow_wave(uint32_t ph,uint32_t inc,uint32_t wave,uint32_t pw)
{
    inc=inc>0x70000000u?0x70000000u:inc;
    switch(wave) {
    case 1:return osc_tri(ph);
    case 2:return clamp(osc_saw(ph,inc),-32767,32767);
    case 3:return osc_pulse(ph,inc,0x80000000u);
    case 4:return osc_pulse(ph,inc,pw);
    case 5:return (sine_i(ph)*3+sine_i(ph*2u))/4;
    default:return sine_i(ph);
    }
}
static void snow_render(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    const int16_t *p=t->p;
    uint32_t a=v->ph[0],b=v->ph[1],c=v->ph[2],alg=(uint32_t)p[SN_ALG]%6u;
    int32_t tone=clamp(snow_param(t,0)+(m->shape-(64<<8))/256,0,127);
    int32_t pitch=60*16+(m->pitch16-60*16)*p[SN_KEYTRACK]/127+p[SN_TUNE]*16;
    int32_t sweep=p[SN_PAMT]+(snow_param(t,2)-64)/2;
    pitch+=(int32_t)(((int64_t)sweep*16*(v->s[2]>>9))>>15);
    if(p[SN_L2_DEST]==9)pitch+=snow_l2[snow_role(t)].value*2;
    uint32_t inc=pitch_inc((uint32_t)clamp(pitch,0,2047));
    inc+=(uint32_t)((int32_t)(inc>>12)*(p[SN_FINE]*2367/1000+m->fine));
    uint32_t ratio=(uint32_t)clamp(p[SN_RATIO],1,127)*(64u+(uint32_t)snow_param(t,1))/8u;
    uint32_t incm=(uint32_t)(((uint64_t)inc*ratio)>>8);
    uint32_t inc3=(uint32_t)(((uint64_t)inc*(uint32_t)clamp(p[SN_RATIO3],1,127))>>4);
    incm=incm>0x70000000u?0x70000000u:incm;
    inc3=inc3>0x70000000u?0x70000000u:inc3;
    int32_t depth=tone<=64?tone*128:8192+(tone-64)*(tone-64)*32;
    int32_t index=mulq15(depth,v->s[8]>>9),index0=v->s[7],prev=v->s[3],rng=v->s[4];
    int32_t lo=v->s[5],bp=v->s[6],cut=clamp((snow_param(t,6)<<8)+m->cutoff,0,127<<8);
    tsvf_t filter;tsvf_coef(&filter,cut,p[SN_RESO]);
    uint32_t pw=(uint32_t)clamp(p[SN_PW],1,126)<<25;
    int32_t mix=clamp(p[SN_MIX]+snow_param(t,4),0,127);
    for(uint32_t i=0;i<n;i++) {
        int32_t idx=index0+(((index-index0)*(int32_t)(i+1u))>>CTL_LOG2);
        int32_t third=snow_wave(c,inc3,(uint32_t)p[SN_WAVE3],pw);
        int32_t feedback=prev*snow_param(t,5)*128;
        int32_t mod=snow_wave(b+(uint32_t)feedback,incm,(uint32_t)p[SN_WAVE2],pw);
        uint32_t shift=(uint32_t)mod*(uint32_t)idx;
        if(alg==1)mod=snow_wave(b+(uint32_t)third*(uint32_t)idx+(uint32_t)feedback,incm,(uint32_t)p[SN_WAVE2],pw);
        if(alg==1)shift=(uint32_t)mod*(uint32_t)idx;
        if(alg==2)shift=(uint32_t)((mod+third)/2)*(uint32_t)idx;
        if(alg>=3)shift=0;
        int32_t x=snow_wave(a+shift,inc,(uint32_t)p[SN_WAVE1],pw);
        if(alg==3)x=mulq15(x,mod);
        if(alg==4)x=(x*(127-mix)+mod*mix)/127;
        if(alg==5)x=sine_i((uint32_t)x*(uint32_t)(32768+tone*4096));
        if(alg<4 && mix)x=(x*(127-mix)+mod*mix)/127;
        x=(x*(127-p[SN_SUB])+third*p[SN_SUB])/127;
        int32_t noise=(int32_t)(noise32(&rng)>>16)-32768;
        x=(x*(127-p[SN_NOISE])+noise*p[SN_NOISE])/127;
        prev=p[SN_FB_ROUTE]==1?x:p[SN_FB_ROUTE]==2?third:mod;
        if(p[SN_FILTER]) {
            int32_t lp=tsvf_lp(&filter,x,&bp,&lo);
            x=p[SN_FILTER]==1?lp:p[SN_FILTER]==2?x-lp-((bp*(8192-p[SN_RESO]*7600/127))>>12):bp;
        }
        x=soft_knee(x*(128+snow_param(t,7)*3)/128,22000);
        out[i]+=voice_amp(x,m,i);
        a+=inc;b+=incm;c+=inc3;
    }
    v->ph[0]=a;v->ph[1]=b;v->ph[2]=c;
    v->s[3]=prev;v->s[4]=rng;v->s[5]=lo;v->s[6]=bp;v->s[7]=index;
}
#include "snow_presets.h"
static void snow_fill(int16_t *p,uint32_t role,uint32_t preset)
{
    role%=NTRK;preset%=10u;
    for(uint32_t k=0;k<20u;k++)p[SN_ALG+k]=SNOW_DETAIL[role][preset][k];
    p[P_CHRD]=SNOW_CHORD[role][preset];
    if(role<5u)p[P_VOICE]=V_MONO;
}
#define SNOW_MACHINE(role,title) { .name=title,.page_title={"MACROS","FM DETAIL"}, \
 .edit={{"TONE",0,0,127,40,F_PCT,0},{"SHAPE",0,0,127,64,F_PCT,0}, \
 {"SWEEP",0,0,127,64,F_PCT,0},{"DECAY",0,0,127,64,F_PCT,0}, \
 {"MIX",0,0,127,0,F_PCT,0},{"FEEDBACK",0,0,127,0,F_PCT,0}, \
 {"CUTOFF",0,0,127,110,F_CUTOFF,0},{"DRIVE",0,0,127,0,F_PCT,0}}, \
 .presets=SNOW_PATCHES[role],.npresets=10,.knob={P_E0,P_E1,P_E2,P_E3}, \
 .poly=4,.ownenv=1,.keep=0xe0,.note_on=snow_note_on,.render=snow_render, \
 .amp=snow_amp,.done=snow_done,.block=snow_block }
static const engine_t SNOW_ENG[NTRK]={SNOW_MACHINE(0,"KICK"),SNOW_MACHINE(1,"SNARE"),SNOW_MACHINE(2,"HAT"),SNOW_MACHINE(3,"PERC"),SNOW_MACHINE(4,"BASS"),SNOW_MACHINE(5,"CHORD")};
#undef SNOW_MACHINE
