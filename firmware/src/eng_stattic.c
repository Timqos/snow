/* SPDX-License-Identifier: GPL-3.0-only
 * STATTIC instruments on the SLOOP engine API.
 * DSP/presets ported from Timqos/snow b0cb3a5 (eng_snow.c, snow_presets.h).
 * Keep synthesis arithmetic stable while adapting ownership and scheduling.
 * No graphics, random generator or flash operation runs in this module.
 */
#define SN_CH_VOICES STATTIC_CH_VOICES
#define SN_CH_RELEASE STATTIC_CH_RELEASE
#define SNOW_RENDER_RAM ST_HOT __attribute__((flatten))
#define SNOW_WAVE_RAM ST_HOT
static uint32_t stattic_env_coef(const track_t *t, uint32_t i);
/* Exact division by 2^bits, including C's truncation of negative samples.
 * JieLi's size optimiser otherwise leaves hardware divisions in hot loops. */
static inline int32_t snow_div2(int32_t x,uint32_t bits)
{
    return (x+((x>>31)&(int32_t)((1u<<bits)-1u)))>>bits;
}
/* Exact C division towards zero. The target's size optimiser otherwise
 * emits hardware divides in sample loops. Floor reciprocal + one correction
 * works over the entire int32 range, including INT32_MIN. */
static inline int32_t snow_div127(int32_t x)
{
    uint32_t u=x<0?0u-(uint32_t)x:(uint32_t)x;
    uint32_t q=(uint32_t)(((uint64_t)u*33818640u)>>32);
    q+=u-q*127u>=127u;
    return x<0?-(int32_t)q:(int32_t)q;
}
static const char *const SN_ALGS[] = {"FM PAIR","FM CHAIN","FM PAR","RING","ADDITIVE","FOLD"};
static const char *const SN_WAVES[] = {"SINE","TRI","SAW","SQUARE","PULSE","ORGAN"};
static const char *const SN_RMODELS[] = {"ROOM","HALL","PLATE"};
static const char *const SN_MODES[] = {"HIT","GATED"};
static const char *const SN_GKINDS[] = {"NOTES","MELODY","ALL","AMBIENT"};   /* (SN_G_KIND, EVOLVE > GENERATE) */
static const char *const SN_FILTERS[] = {"OFF","LP","HP","BP"};
static const char *const SN_FB_NAMES[] = {"MOD","CARRIER","CROSS"};
static const char *const SN_L2_DST[] = {"OFF","TONE","SHAPE","SWEEP","DECAY","MIX","FEEDBACK","CUTOFF","DRIVE","PITCH","AMP"};
static struct { uint32_t phase; int32_t value, fade, rng; } snow_l2[NTRK];
static int16_t snow_offset[NTRK][4];
static uint32_t snow_role(const track_t *t)
{
    return (uint32_t)(t->engine - ENGI_STATTIC) % 7u;
}
static uint32_t stattic_part(const track_t *t) { return (uint32_t)(t-trk) % NTRK; }
/* The old LFO division order includes four bars; SLOOP's native step order
 * differs. Convert the sync selection explicitly rather than reusing an ID. */
static uint32_t stattic_sync_samples(uint32_t sync)
{
    static const uint16_t pulses[11] = {24,384,192,96,48,24,12,8,6,4,3};
    uint32_t beat=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);
    return (uint32_t)((uint64_t)beat*pulses[sync%11u]/24u);
}
/* A synchronized LFO's period changes only with its tempo/division. Reuse
 * the exact increment between changes instead of dividing for every block.
 * The two clocks have independent cache entries, never shared phase/state. */
static struct { uint32_t period,inc; } snow_lfo_clock[NTRK][2];
static __attribute__((noinline)) uint32_t snow_sync_inc(const track_t *t,uint32_t clock,uint32_t period)
{
    period=period?period:1u;
    uint32_t k=(uint32_t)(t-trk);
    if(snow_lfo_clock[k][clock].period!=period){
        uint32_t q=0xffffffffu/period,r=0xffffffffu%period;
        snow_lfo_clock[k][clock].inc=q*CTL+(r*CTL+CTL)/period;
        snow_lfo_clock[k][clock].period=period;
    }
    return snow_lfo_clock[k][clock].inc;
}
static int32_t snow_param(const track_t *t,uint32_t k)
{
    int32_t v=t->p[P_E0+k]+(k<4u?snow_offset[stattic_part(t)][k]:0);
    if(t->p[SN_L2_DEST]==(int16_t)(k+1u))v+=snow_l2[stattic_part(t)].value;
    return clamp(v,0,127);
}
static int32_t stattic_drum_macro(const track_t *t,uint32_t knob)
{
    uint32_t k=stattic_part(t);int32_t v=t->p[P_E1+(knob&3u)]+snow_offset[k][knob&3u];
    if(t->p[SN_L2_DEST]==(int16_t)(knob+1u))v+=snow_l2[k].value;
    return clamp(v,0,127);
}
static void snow_block(track_t *t)
{
    uint32_t k=stattic_part(t),ph=snow_l2[k].phase,inc=LFO_INC[t->p[SN_L2_RATE]&127];
    int32_t wave,fade=snow_l2[k].fade;
    if(t->p[SN_L2_SYNC]) {
        uint32_t period=stattic_sync_samples((uint32_t)t->p[SN_L2_SYNC]);
        inc=snow_sync_inc(t,1u,period);
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
    if(!t->p[SN_L2_TRIG]) {snow_l2[stattic_part(t)].phase=0;snow_l2[stattic_part(t)].fade=0;}
}
static int snow_done(track_t *t,voice_t *v)
{
    return v->s[1]>0 && (!t->p[SN_MODE] || v->s[1]==3) && v->s[0]<4096;
}
static SNOW_RENDER_RAM int32_t snow_amp(track_t *t,voice_t *v,int32_t adsr)
{
    const int16_t *p=t->p;
    if(!v->active)return 0;
    int32_t stage=v->s[1],env=v->s[0],sus=p[SN_MODE]?p[P_SUS]<<17:0;
    uint32_t decay=(uint32_t)clamp(p[P_DEC]+snow_param(t,3)-64,0,127);
    (void)adsr;
    if(p[SN_MODE]&&!v->gate)stage=3;
    if(stage==0) {
        env+=(int32_t)(snow_role(t)==5u&&p[P_ESYNC]?stattic_env_coef(t,0):ENV_LIN[p[P_ATK]&127]);
        if(env>=(1<<24)){env=1<<24;stage=1;}
    }else if(stage==3)env-=mulq16(env,snow_role(t)==5u?stattic_env_coef(t,2):ENV_EXP[p[P_REL]&127]);
    else env+=mulq16(sus-env,snow_role(t)==5u&&p[P_ESYNC]?stattic_env_coef(t,1):ENV_EXP[decay]);
    v->s[0]=clamp(env,0,1<<24);v->s[1]=stage;
    v->s[2]-=mulq16(v->s[2],ENV_EXP[p[SN_PDEC]&127]);
    if(p[SN_FMDEC])v->s[8]-=mulq16(v->s[8],ENV_EXP[p[SN_FMDEC]&127]);
    int32_t amp=mulq15(v->s[0]>>9,v->vel*258);
    if(p[SN_L2_DEST]==10)amp=mulq15(amp,clamp(32767+snow_l2[stattic_part(t)].value*258,0,32767));
    return amp;
}
static SNOW_WAVE_RAM int32_t snow_wave(uint32_t ph,uint32_t inc,uint32_t wave,uint32_t pw)
{
    inc=inc>0x70000000u?0x70000000u:inc;
    switch(wave) {
    case 1:return osc_tri(ph);
    case 2:return clamp(osc_saw(ph,inc),-32767,32767);
    case 3:return osc_pulse(ph,inc,0x80000000u);
    case 4:return osc_pulse(ph,inc,pw);
    case 5:return snow_div2(sine_i(ph)*3+sine_i(ph*2u),2);
    default:return sine_i(ph);
    }
}
/* Sine/triangle are common in KK/BS/CH. Keep their exact arithmetic in the
 * caller so each sample avoids the waveform switch and helper call.
 * Other shapes retain the complete original waveform path. */
static inline __attribute__((always_inline)) int32_t snow_wave_fast(uint32_t ph,uint32_t inc,uint32_t wave,uint32_t pw)
{
    if(wave<2u)return wave?osc_tri(ph):sine_i(ph);
    return snow_wave(ph,inc,wave,pw);
}
static struct { tsvf_t coef;int32_t cut,reso;uint8_t valid; } stattic_filter_cache[NTRK];
/* KEYS (SY and CH, the owner's choice, after Elektron Model:Cycles' TONE/CHORD):
 * a two-operator FM voice for electric pianos, organs and glassy keys.
 *   carrier: WAVE 1 (SINE TRI SAW SQUARE PULSE ORGAN), PW for PULSE
 *   modulator: a sine at RATIO x (64 + SHAPE) / 2048 of the note (RATIO 16, SHAPE 64: 1:1)
 *   index: TONE's depth, x velocity (VELO: how much a soft note darkens), and
 *          the hammer: SWEEP sets how much of it falls with HAMMER (the FM DECAY time)
 *   hiss: NOISE (steady) + AIR (a burst falling with N.DECAY, the P DECAY slot)
 *   BRIGHT: a one-pole low-pass (CUTOFF slot, its LFO/ENV modulation too), DRIVE
 * The old three-operator algorithms, feedback, SUB, the resonant filter and the
 * pitch sweep are not part of it: about half the cost of the STATTIC voice. */
/* (2026-11-09, the owner) SY only: 8-bit / FX sounds in the same KEYS voice, per block (no per-sample cost):
 *   P AMOUNT  a pitch sweep in semitones, falling to the note with P DECAY (the AIR envelope: v->s[2])
 *   CHIP ARP  (the ALG slot) the note stepped through scale degrees of the track's own scale, as a chip arpeggio;
 *             ARP RATE (the RATIO 3 slot): 8 + RATE blocks a step (~6..98 ms). v->s[9] counts blocks, v->s[3]
 *             holds the step's offset (1/16 semitones). 0 / 0 (every KEYS preset, every CH sound): as before. */
static uint32_t scale_mask(const track_t *t);
static int32_t snow_degree(const track_t *t,int32_t base,int32_t degree);
static const int8_t ST_CHIP_ARP[5][8]={{0,2,4,-1},{0,4,7,-1},{0,2,4,7,-1},{7,4,2,0,-1},{0,4,1,6,2,7,3,5}};
static SNOW_RENDER_RAM void keys_render(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    const int16_t *p=t->p;
    uint32_t a=v->ph[0],b=v->ph[1],wave=(uint32_t)p[SN_WAVE1]%6u;
    int32_t tone=clamp(snow_param(t,0)+snow_div2(m->shape-(64<<8),8),0,127);
    int32_t pitch=60*16+(m->pitch16-60*16)*p[SN_KEYTRACK]/127+p[SN_TUNE]*16;
    if(p[SN_L2_DEST]==9)pitch+=snow_l2[stattic_part(t)].value*2;
    if(t==&trk[1]&&(p[SN_PAMT]|p[SN_ALG])){                 /* (SY: sweep, chip arp) */
        if(p[SN_PAMT])pitch+=(p[SN_PAMT]*16*(v->s[2]>>9))>>15;
        uint32_t pat=(uint32_t)p[SN_ALG];
        if(pat&&pat<=5u){
            uint32_t len=8u+(uint32_t)clamp(p[SN_RATIO3],1,127),age=(uint32_t)v->s[9]++;
            if(age%len==0u){
                const int8_t *seq=ST_CHIP_ARP[pat-1u];uint32_t cnt=0;while(cnt<8u&&seq[cnt]>=0)cnt++;
                int32_t deg=seq[(age/len)%cnt];
                v->s[3]=deg?(snow_degree(t,v->note,deg)-(int32_t)v->note)*16:0;
            }
            pitch+=v->s[3];
        }
    }
    uint32_t inc=pitch_inc((uint32_t)clamp(pitch,0,2047));
    inc+=(uint32_t)((int32_t)(inc>>12)*(p[SN_FINE]*2367/1000+m->fine));
    uint32_t ratio=(uint32_t)clamp(p[SN_RATIO],1,127)*(64u+(uint32_t)snow_param(t,1))/8u;
    uint32_t incm=(uint32_t)(((uint64_t)inc*ratio)>>8);
    incm=incm>0x70000000u?0x70000000u:incm;
    int32_t depth=tone<=64?tone*128:8192+(tone-64)*(tone-64)*32;
    int32_t velo=snow_param(t,4),hit=snow_param(t,2)*258;                /* Q15 */
    depth=mulq15(depth,32767-velo*(127-(int32_t)v->vel)*13/8);           /* (VELO 127, velocity 1: a fifth of it) */
    int32_t index=mulq15(depth,(32767-hit)+mulq15(hit,v->s[8]>>9)),index0=v->s[7];
    int32_t noise_amt=p[SN_NOISE]*128+mulq15(snow_param(t,5)*258,v->s[2]>>9);   /* Q15 */
    int32_t rng=v->s[4],lp=v->s[5];
    int32_t cut=clamp((snow_param(t,6)<<8)+m->cutoff,0,127<<8)>>8;
    int32_t k=cut>=127?32767:400+cut*cut*2;                              /* one pole, Q15 (~490 Hz at 30, ~2 kHz at 64) */
    int32_t drive_gain=128+snow_param(t,7)*3;
    uint32_t pw=(uint32_t)clamp(p[SN_PW],1,126)<<25;
    for(uint32_t i=0;i<n;i++){
        int32_t idx=index0+(((index-index0)*(int32_t)(i+1u))>>CTL_LOG2);
        int32_t x=snow_wave_fast(a+(uint32_t)sine_i(b)*(uint32_t)idx*6u,inc,wave,pw);   /* (x6: TONE 64 ~ index 2.4) */
        if(noise_amt)x+=(((int32_t)(noise32(&rng)>>16)-32768)*noise_amt)>>15;
        if(k<32767){lp+=((x-lp)*k)>>15;x=lp;}
        x=soft_knee(snow_div2(x*drive_gain,7),22000);
        out[i]+=voice_amp(x,m,i);
        a+=inc;b+=incm;
    }
    v->ph[0]=a;v->ph[1]=b;v->s[4]=rng;v->s[5]=lp;v->s[7]=index;
}
static SNOW_RENDER_RAM void snow_render(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    if(snow_role(t)>=5u){keys_render(t,v,out,n,m);return;}   /* SY, CH */
    const int16_t *p=t->p;
    uint32_t a=v->ph[0],b=v->ph[1],c=v->ph[2],alg=(uint32_t)p[SN_ALG]%6u;
    int32_t tone=clamp(snow_param(t,0)+snow_div2(m->shape-(64<<8),8),0,127);
    int32_t pitch=60*16+(m->pitch16-60*16)*p[SN_KEYTRACK]/127+p[SN_TUNE]*16;
    int32_t sweep=p[SN_PAMT]+snow_div2(snow_param(t,2)-64,1);
    pitch+=(int32_t)(((int64_t)sweep*16*(v->s[2]>>9))>>15);
    if(p[SN_L2_DEST]==9)pitch+=snow_l2[stattic_part(t)].value*2;
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
    tsvf_t filter;
    if(p[SN_FILTER]){
        uint32_t part=stattic_part(t);
        if(!stattic_filter_cache[part].valid||stattic_filter_cache[part].cut!=cut||stattic_filter_cache[part].reso!=p[SN_RESO]){
            tsvf_coef(&stattic_filter_cache[part].coef,cut,p[SN_RESO]);
            stattic_filter_cache[part].cut=cut;stattic_filter_cache[part].reso=p[SN_RESO];stattic_filter_cache[part].valid=1;
        }
        filter=stattic_filter_cache[part].coef;
    }
    uint32_t pw=(uint32_t)clamp(p[SN_PW],1,126)<<25;
    int32_t mix=clamp(p[SN_MIX]+snow_param(t,4),0,127);
    /* A control block keeps these values fixed. Resolving the track index,
     * LFO destination and bounds twice per sample needlessly starved the UI
     * and audio deadline on the FM-1, though a desktop rendered it easily. */
    int32_t feedback_gain=snow_param(t,5)*128,drive_gain=128+snow_param(t,7)*3;
    int32_t damping=8192-p[SN_RESO]*7600/127;
    uint32_t third_on=alg==1u||alg==2u||p[SN_SUB]||p[SN_FB_ROUTE]==2;
    for(uint32_t i=0;i<n;i++) {
        int32_t idx=index0+(((index-index0)*(int32_t)(i+1u))>>CTL_LOG2);
        int32_t third=third_on?snow_wave_fast(c,inc3,(uint32_t)p[SN_WAVE3],pw):0;
        int32_t feedback=prev*feedback_gain;
        /* FM CHAIN changes the modulator's phase, not its waveform count.
         * Evaluate it once; the old first result was discarded each sample. */
        uint32_t modph=b+(uint32_t)feedback;
        if(alg==1u)modph+=(uint32_t)third*(uint32_t)idx;
        int32_t mod=snow_wave_fast(modph,incm,(uint32_t)p[SN_WAVE2],pw);
        uint32_t shift=(uint32_t)mod*(uint32_t)idx;
        if(alg==2)shift=(uint32_t)snow_div2(mod+third,1)*(uint32_t)idx;
        if(alg>=3)shift=0;
        int32_t x=snow_wave_fast(a+shift,inc,(uint32_t)p[SN_WAVE1],pw);
        if(alg==3)x=mulq15(x,mod);
        if(alg==4&&mix)x=snow_div127(x*(127-mix)+mod*mix);
        if(alg==5)x=sine_i((uint32_t)x*(uint32_t)(32768+tone*4096));
        if(alg<4 && mix)x=snow_div127(x*(127-mix)+mod*mix);
        if(p[SN_SUB])x=snow_div127(x*(127-p[SN_SUB])+third*p[SN_SUB]);
        int32_t noise=(int32_t)(noise32(&rng)>>16)-32768;
        if(p[SN_NOISE])x=snow_div127(x*(127-p[SN_NOISE])+noise*p[SN_NOISE]);
        prev=p[SN_FB_ROUTE]==1?x:p[SN_FB_ROUTE]==2?third:mod;
        if(p[SN_FILTER]) {
            int32_t lp=tsvf_lp(&filter,x,&bp,&lo);
            x=p[SN_FILTER]==1?lp:p[SN_FILTER]==2?x-lp-((bp*damping)>>12):bp;
        }
        x=soft_knee(snow_div2(x*drive_gain,7),22000);
        out[i]+=voice_amp(x,m,i);
        a+=inc;b+=incm;c+=inc3;
    }
    v->ph[0]=a;v->ph[1]=b;v->ph[2]=c;
    v->s[3]=prev;v->s[4]=rng;v->s[5]=lo;v->s[6]=bp;v->s[7]=index;
}
#include "stattic_presets.h"
static void snow_fill(int16_t *p,uint32_t role,uint32_t preset)
{
    uint32_t synth=role==STATTIC_SY_ENGINE;
    role%=STATTIC_SOUND_ROLES;preset%=synth?20u:role>=4u?15u:10u;
    if(synth)role=5u;
    if(synth&&preset>=15u){                         /* (SY's 8-bit / FX presets) */
        for(uint32_t k=0;k<20u;k++)p[SN_ALG+k]=SNOW_SY_FX_DETAIL[preset-15u][k];
        p[SN_CHORD]=0;
    }else{
        for(uint32_t k=0;k<20u;k++)p[SN_ALG+k]=SNOW_DETAIL[role][preset][k];
        p[SN_CHORD]=SNOW_CHORD[role][preset];
    }
    if(role==5u&&!synth)p[P_REL]=SN_CH_RELEASE;
    if(role<5u)p[P_VOICE]=V_MONO;
    if(role==3u)p[SN_KEYTRACK]=127; /* percussion follows the generated scale notes */
}
#define SNOW_MACHINE(role,title) { .name=title,.page_title={"MACROS","FM DETAIL"}, \
 .edit={{.label="TONE",.fmt=F_PCT,.min=0,.max=127,.def=40},{.label="SHAPE",.fmt=F_PCT,.min=0,.max=127,.def=64}, \
 {.label="SWEEP",.fmt=F_PCT,.min=0,.max=127,.def=64},{.label="DECAY",.fmt=F_PCT,.min=0,.max=127,.def=64}, \
 {.label="MIX",.fmt=F_PCT,.min=0,.max=127,.def=0},{.label="FEEDBACK",.fmt=F_PCT,.min=0,.max=127,.def=0}, \
 {.label="CUTOFF",.fmt=F_CUTOFF,.min=0,.max=127,.def=110},{.label="DRIVE",.fmt=F_PCT,.min=0,.max=127,.def=0}}, \
 .presets=SNOW_PATCHES[role],.npresets=(role>=4?15:10),.macro={P_E0,P_E1,P_E2,P_E3}, \
 .poly=(role==5?SN_CH_VOICES:4),.ownenv=1,.note_on=snow_note_on,.render=snow_render, \
 .amp=snow_amp,.done=snow_done,.block=snow_block }
#define SNOW_KEYS(nvoice,title,pr_,np_) { .name=title,.page_title={"MACROS","KEYS"}, \
 .edit={{.label="TONE",.fmt=F_PCT,.min=0,.max=127,.def=40},{.label="SHAPE",.fmt=F_PCT,.min=0,.max=127,.def=64}, \
 {.label="HAMMER",.fmt=F_PCT,.min=0,.max=127,.def=64},{.label="DECAY",.fmt=F_PCT,.min=0,.max=127,.def=64}, \
 {.label="VELO",.fmt=F_PCT,.min=0,.max=127,.def=48},{.label="AIR",.fmt=F_PCT,.min=0,.max=127,.def=0}, \
 {.label="BRIGHT",.fmt=F_CUTOFF,.min=0,.max=127,.def=110},{.label="DRIVE",.fmt=F_PCT,.min=0,.max=127,.def=0}}, \
 .presets=(pr_),.npresets=(np_),.macro={P_E0,P_E1,P_E2,P_E3}, \
 .poly=(nvoice),.ownenv=1,.note_on=snow_note_on,.render=snow_render, \
 .amp=snow_amp,.done=snow_done,.block=snow_block }
static const engine_t SNOW_ENG[7]={SNOW_MACHINE(0,"KICK"),SNOW_MACHINE(1,"SNARE"),SNOW_MACHINE(2,"HAT"),SNOW_MACHINE(3,"PERC"),SNOW_MACHINE(4,"BASS"),SNOW_KEYS(SN_CH_VOICES,"CHORD",SNOW_PATCHES[5],15),SNOW_KEYS(4,"SYNTH",SNOW_SY_PATCHES,20)};
#undef SNOW_MACHINE
#undef SNOW_KEYS
