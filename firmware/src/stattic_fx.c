/* SPDX-License-Identifier: GPL-3.0-only
 * Snow: six independent effect racks. No shared delay/reverb/chorus returns.
 * Tape and chorus run at FS; delay and reverb use filtered quarter-rate input
 * and interpolate their returns. Buffers/state belong to exactly one channel. */
#define SN_DL_LEN 11264u                         /* 1.02 s at FS/4; exposed time: 10..1000 ms */
#define SN_RV_LEN 1358u
enum { SN_FX_TAPE=1u, SN_FX_CRUSH=2u, SN_FX_CHORUS=4u, SN_FX_DELAY=8u, SN_FX_REVERB=16u };
static const uint16_t SN_RV_LINES[3][4]={{137,193,251,293},{229,307,379,443},{211,271,353,433}};
typedef struct {
    int16_t delay[SN_DL_LEN],chorus[512],tape[256],comb[SN_RV_LEN],ap[236],pre[512];
    uint32_t pos,dw,dt_q8,cho_ph,wow_ph,flutter_ph,pre_w;
    uint16_t ri[4],ai[2];
    int32_t rlp[4],dlp,in1,in2,tone,tape_dc,crush,crush_lp;
    int32_t from_l,from_r,to_l,to_r;
    uint32_t crush_count;
    int32_t side[CTL];
    /* Valid history since each effect was enabled. Disabled effects do no
     * DSP or buffer writes; age guards invalidate old samples in constant
     * time, without clearing 28 KiB inside an audio interrupt. */
    uint16_t delay_fill,chorus_fill,tape_fill,rv_fill;
    uint8_t enabled,rv_model,running;
    uint32_t quiet;
} snow_fx_t;
static snow_fx_t snow_fx[NPART] __attribute__((section(".pool")));   /* (BS SY CH: DR has no rack; ~28 KiB back for .hot_text) */
/* Per-channel performance processing, after the channel's stereo rack and
 * before its final pan. Zero/zero is an exact bypass for existing projects. */
static struct { int32_t filter,width,bp[2],lp[2],wet,wgain;tsvf_t coef; } snow_mix_fx[NTRK];
static int32_t snow_mix_slew(int32_t from,int32_t target)
{
    int32_t d=target-from;return from+(d?snow_div2(d,3)+(d>0?1:-1):0);
}
static int snow_mix_begin(track_t *t)
{
    uint32_t k=(uint32_t)(t-trk);int32_t *f=&snow_mix_fx[k].filter,*w=&snow_mix_fx[k].width;
    *f=snow_mix_slew(*f,t->p[SN_DJ_FILTER]*256);*w=snow_mix_slew(*w,t->p[SN_WIDTH]*256);
    int32_t a=*f<0?-*f:*f;
    snow_mix_fx[k].wet=clamp(a>>1,0,1024);
    snow_mix_fx[k].wgain=*w*4/(*w<0?64:63);
    if(a)tsvf_coef(&snow_mix_fx[k].coef,*f<0?127*256-a*125/64:a*121/63,0);
    return a||*w;
}
static ST_HOT void snow_mix_sample(track_t *t,int32_t *l,int32_t *r)
{
    uint32_t k=(uint32_t)(t-trk);int32_t v[2]={*l,*r};
    for(uint32_t c=0;c<2;c++)if(snow_mix_fx[k].wet){
        int32_t in=snow_div2(v[c],3),before=snow_mix_fx[k].bp[c],lo=tsvf_lp(&snow_mix_fx[k].coef,in,&snow_mix_fx[k].bp[c],&snow_mix_fx[k].lp[c]);
        int32_t out=snow_mix_fx[k].filter<0?lo:in-lo-before-snow_mix_fx[k].bp[c];
        v[c]+=snow_div2((out*8-v[c])*snow_mix_fx[k].wet,10);
    }
    int32_t delta=snow_div2((v[0]-v[1])*snow_mix_fx[k].wgain,11);
    *l=v[0]+delta;*r=v[1]-delta;
}
static inline int32_t snow_fx_mul(int32_t x,int32_t gain) { return snow_div2(x*gain,15); }
static inline __attribute__((always_inline)) void snow_fx_lowpass(int32_t *state,int32_t x,int32_t gain)
{
    int32_t delta=x-*state,step=snow_fx_mul(delta,gain);
    /* Reach exact silence rather than retaining a negative rounding cycle. */
    *state+=step?step:delta>0?1:delta<0?-1:0;
}
static inline __attribute__((always_inline)) int32_t snow_tap(const int16_t *b,uint32_t length,uint32_t wp,uint32_t dq8,uint32_t filled)
{
    uint32_t back=dq8>>8;
    /* Callers bound the tap to this line: one wrap, no per-sample division. */
    uint32_t a=wp+length-back;if(a>=length)a-=length;
    uint32_t z=a?a-1u:length-1u;
    int32_t v=back<=filled?b[a]:0,w=back<filled?b[z]:0;
    return v+(((w-v)*(int32_t)(dq8&255u))>>8);
}
static inline __attribute__((always_inline)) int32_t snow_allpass(snow_fx_t *s,uint32_t stage,int32_t x)
{
    uint32_t len=stage?139u:97u,off=stage?97u:0u,idx=s->ai[stage];
    int32_t delayed=s->rv_fill>=len?s->ap[off+idx]:0,y=delayed-x;
    s->ap[off+idx]=(int16_t)clamp(x+snow_div2(delayed,1),-32767,32767);
    s->ai[stage]=(uint16_t)(idx+1u==len?0u:idx+1u);return y;
}
static uint32_t snow_delay_target(const int16_t *p)
{
    uint32_t target=(uint32_t)p[SN_DTIME]*FS/400u;
    if(p[SN_DSYNC]){
        target=stattic_sync_samples((uint32_t)p[SN_DSYNC])/4u;
    }
    return (uint32_t)clamp((int32_t)target,2,SN_DL_LEN-2u);
}
static inline __attribute__((always_inline)) uint32_t snow_delay_slew(uint32_t from,uint32_t target)
{
    int32_t d=(int32_t)(target<<8)-(int32_t)from;
    return (uint32_t)((int32_t)from+(d?snow_div2(d,6)+(d>0?1:-1):0));
}
/* Examine only entries written this block. After a complete longest-buffer
 * turn of zero writes AND zero scalar state, every active history is empty.
 * No amplitude threshold or fixed tail timeout may cut a quiet sound. */
static int snow_zero_written(const int16_t *b,uint32_t len,uint32_t wp,uint32_t n)
{
    while(n--){wp=wp?wp-1u:len-1u;if(b[wp])return 0;}
    return 1;
}
static uint32_t snow_fx_quiet_span(const snow_fx_t *s,uint32_t enabled,uint32_t n,uint32_t q)
{
    if(s->from_l|s->from_r|s->to_l|s->to_r)return 0;
    uint32_t span=1;
    if(enabled&SN_FX_TAPE){
        if(s->tone||s->tape_dc||!snow_zero_written(s->tape,256,s->pos&255u,n))return 0;
        span=256;
    }
    if((enabled&SN_FX_CRUSH)&&(s->crush||s->crush_lp))return 0;
    if(enabled&SN_FX_CHORUS){
        if(!snow_zero_written(s->chorus,512,s->pos&511u,n))return 0;
        span=512;
    }
    if(enabled&(SN_FX_DELAY|SN_FX_REVERB))if(s->in1||s->in2)return 0;
    if(enabled&SN_FX_REVERB){
        uint32_t off=0;
        for(uint32_t c=0;c<4u;c++){
            uint32_t len=SN_RV_LINES[s->rv_model][c];
            if(s->rlp[c]||!snow_zero_written(s->comb+off,len,s->ri[c],q))return 0;
            off+=len;
        }
        if(!snow_zero_written(s->pre,512,s->pre_w&511u,q)||
           !snow_zero_written(s->ap,97,s->ai[0],q)||!snow_zero_written(s->ap+97,139,s->ai[1],q))return 0;
        span=512u*4u;
    }
    if(enabled&SN_FX_DELAY){
        if(s->dlp||!snow_zero_written(s->delay,SN_DL_LEN,s->dw,q))return 0;
        span=SN_DL_LEN*4u;
    }
    return span;
}
static ST_HOT void snow_fx_core(snow_fx_t *s,const int16_t *p,int32_t *b,uint32_t n);
/* (the owner) SY and CH keep their racks; BS and DR play into the shared one (stattic_bus_run) */
static ST_HOT void snow_fx_process(track_t *t,int32_t *b,uint32_t n)
{
    if(is_drum(t)||t==&trk[0])return;
    snow_fx_core(&snow_fx[(uint32_t)(t-trk)%NPART],t->p,b,n);
}
static ST_HOT void snow_fx_core(snow_fx_t *s,const int16_t *p,int32_t *b,uint32_t n)
{
    int32_t on=!p[SN_FX_BYPASS],cm=on?p[P_CHOR]:0,dm=on?p[P_DLY]:0,rm=on?p[P_REV]:0;
    if(on&&thr_rev)rm=send_thr(rm*258,thr_rev)/258;
    int32_t tm=on?p[SN_TMIX]:0,bm=on?p[SN_BMIX]:0;
    int32_t dfb=p[SN_DFB]*247,dmix=dm*258;
    dmix=send_thr(dmix,thr_dly);
    if(thr_dly&&dfb<22938)dfb+=(int32_t)(((int64_t)(22938-dfb)*thr_dly)>>15);
    dmix=on?clamp(dmix,0,32767):0;dfb=clamp(dfb,0,30500);
    uint32_t enabled=(tm?SN_FX_TAPE:0u)|(bm?SN_FX_CRUSH:0u)|(cm?SN_FX_CHORUS:0u)|
                     (dmix?SN_FX_DELAY:0u)|(rm?SN_FX_REVERB:0u),changed=s->enabled^enabled;
    if(changed&SN_FX_TAPE){s->tape_fill=0;s->tone=s->tape_dc=0;}
    if(changed&SN_FX_CRUSH)s->crush=s->crush_lp=s->crush_count=0;
    if(changed&SN_FX_CHORUS)s->chorus_fill=0;
    if(changed&SN_FX_DELAY){s->delay_fill=0;s->dlp=0;s->dt_q8=0;}
    uint32_t model=(uint32_t)p[SN_RTYPE]%3u;
    if((changed&SN_FX_REVERB)||s->rv_model!=model){
        s->rv_fill=0;s->rv_model=(uint8_t)model;
        for(uint32_t c=0;c<4u;c++)s->rlp[c]=0;
    }
    if(!(s->enabled&(SN_FX_DELAY|SN_FX_REVERB)))s->in1=s->in2=0;
    s->enabled=(uint8_t)enabled;
    uint32_t ci=LFO_INC[p[SN_CRATE]&127]>>CTL_LOG2;
    uint32_t input=0;
    if(enabled)for(uint32_t i=0;i<n;i++)if(b[i]){input=1;break;}
    if(input){s->running=1;s->quiet=0;}
    uint32_t q=(n+3u-((4u-(s->pos&3u))&3u))>>2;
    if(!enabled||!s->running){
        /* Dry and BYPASS really are dry, including the first sample. Keep
         * modulation clocks running with one addition per control block. */
        s->pos+=n;s->cho_ph+=ci*n;s->wow_ph+=34087u*n;s->flutter_ph+=594315u*n;
        s->from_l=s->from_r=s->to_l=s->to_r=0;
        if(!enabled){s->running=0;s->quiet=0;}
        /* Preserve sample/hold phase and delay-time smoothing while a truly
         * empty, enabled rack sleeps; the next note remains sample-identical. */
        if(bm){
            uint32_t rate=(uint32_t)clamp(p[SN_BRATE],1,64),count=s->crush_count;
            s->crush_count=n<=count?count-n:(rate-(n-count)%rate)%rate;
        }
        if(dmix){
            uint32_t target=snow_delay_target(p);
            if(!s->dt_q8)s->dt_q8=target<<8;
            for(uint32_t j=0;j<q;j++)s->dt_q8=snow_delay_slew(s->dt_q8,target);
        }
        for(uint32_t i=0;i<n;i++)s->side[i]=0;
        return;
    }
    int32_t cfb=p[SN_CFB]*180,ck=2048+p[SN_DCOLOR]*230,rk=32000-p[SN_RDAMP]*230;
    uint32_t target=dmix?snow_delay_target(p):0;
    if(dmix&&!s->dt_q8)s->dt_q8=target<<8;
    int32_t rfb=12000+p[SN_RSIZE]*(model==0?135:157);
    uint32_t pre=(uint32_t)p[SN_RPRE]*FS/4000u;
    /* Parameters are fixed between mod_begin/mod_end for this control block.
     * Buffer writes can alias int16_t parameters as far as the size optimiser
     * knows; capture the unchanged controls before the sample loop. */
    int32_t tape_drive=128+p[SN_TDRIVE]*5,tape_tone=2048+p[SN_TTONE]*225;
    int32_t wow=p[SN_TWOW],flutter_depth=p[SN_TFLUT],crush_tone=1500+p[SN_BTONE]*245;
    int32_t chorus_depth=p[SN_CDEPTH];
    uint32_t crush_shift=16u-(uint32_t)clamp(p[SN_BBITS],2,16);
    int32_t quantum=1<<crush_shift;
    uint32_t crush_rate=(uint32_t)clamp(p[SN_BRATE],1,64),delay_width=508u-(uint32_t)p[SN_DWIDTH];
    for(uint32_t i=0;i<n;i++) {
        int32_t dry=b[i],x=clamp(snow_div2(dry,2),-32767,32767);
        if(tm) {
            int32_t sat=soft_knee(snow_div2(x*tape_drive,7),16000);
            snow_fx_lowpass(&s->tone,sat,tape_tone);
            s->tape[s->pos&255u]=(int16_t)clamp(s->tone,-32767,32767);
            int32_t wobble=snow_fx_mul(osc_sine(s->wow_ph),wow);
            int32_t flutter=snow_fx_mul(osc_sine(s->flutter_ph),flutter_depth);
            int32_t lag=96*256+wobble*100+flutter*24;
            int32_t tape=snow_tap(s->tape,256,s->pos&255u,(uint32_t)clamp(lag,256,240*256),s->tape_fill);
            if(s->tape_fill<256u)s->tape_fill++;
            snow_fx_lowpass(&s->tape_dc,tape,32);
            x+=snow_div127((tape-s->tape_dc-x)*tm);dry=x*4;
        }
        if(tm){s->wow_ph+=34087u;s->flutter_ph+=594315u;}
        if(bm) {
            if(!s->crush_count) {
                s->crush=snow_div2(x,crush_shift)*quantum;s->crush_count=crush_rate;
            }
            s->crush_count--;
            snow_fx_lowpass(&s->crush_lp,s->crush,crush_tone);
            x+=snow_div127((s->crush_lp-x)*bm);dry=x*4;
        }
        int32_t cho_l=0,cho_r=0;
        if(cm){
            s->cho_ph+=ci;
            int32_t shift=snow_div2(osc_sine(s->cho_ph)*chorus_depth,7);
            uint32_t cl=(uint32_t)clamp(150*256+shift,256,500*256),cr=(uint32_t)clamp(150*256-shift,256,500*256);
            if(s->chorus_fill>=512u){            /* full line: every tap (<= 500 back) is history */
                uint32_t w=s->pos&511u,ia=(w-(cl>>8))&511u,ib=(w-(cr>>8))&511u;
                int32_t va=s->chorus[ia],vb=s->chorus[ib];
                cho_l=va+(((s->chorus[(ia-1u)&511u]-va)*(int32_t)(cl&255u))>>8);
                cho_r=vb+(((s->chorus[(ib-1u)&511u]-vb)*(int32_t)(cr&255u))>>8);
            }else{
                cho_l=snow_tap(s->chorus,512,s->pos&511u,cl,s->chorus_fill);
                cho_r=snow_tap(s->chorus,512,s->pos&511u,cr,s->chorus_fill);
            }
            s->chorus[s->pos&511u]=(int16_t)clamp(x+snow_fx_mul(snow_div2(cho_l+cho_r,1),cfb),-32767,32767);
            if(s->chorus_fill<512u)s->chorus_fill++;
        }
        if(enabled&(SN_FX_DELAY|SN_FX_REVERB)){
            snow_fx_lowpass(&s->in1,x,14000);snow_fx_lowpass(&s->in2,s->in1,14000);
        }
        uint32_t phase=s->pos&3u;
        if(!phase) {
            int32_t dl=0,dr=0,rev_l=0,rev_r=0;
            if(dmix){
                s->dt_q8=snow_delay_slew(s->dt_q8,target);
                dl=snow_tap(s->delay,SN_DL_LEN,s->dw,s->dt_q8,s->delay_fill);
                /* dt_q8 <= (11264-2)*256; its product with 508 fits uint32_t. */
                uint32_t rt=(uint32_t)snow_div127((int32_t)((s->dt_q8*delay_width)>>2));
                dr=snow_tap(s->delay,SN_DL_LEN,s->dw,rt,s->delay_fill);
                snow_fx_lowpass(&s->dlp,dl,ck);
                s->delay[s->dw]=(int16_t)clamp((dm?s->in2:0)+snow_fx_mul(s->dlp,dfb),-32767,32767);
                if(++s->dw==SN_DL_LEN)s->dw=0;
                if(s->delay_fill<SN_DL_LEN)s->delay_fill++;
            }
            if(rm){
                s->pre[s->pre_w&511u]=(int16_t)s->in2;
                int32_t rin=pre<=s->rv_fill?snow_div2(s->pre[(s->pre_w-pre)&511u],1):0;s->pre_w++;
                int32_t rv[4];uint32_t off=0;
                for(uint32_t c=0;c<4u;c++) {
                    uint32_t len=SN_RV_LINES[model][c],idx=s->ri[c];if(idx>=len)idx%=len;
                    rv[c]=s->rv_fill>=len?s->comb[off+idx]:0;snow_fx_lowpass(&s->rlp[c],rv[c],rk);
                    s->comb[off+idx]=(int16_t)clamp(rin+snow_fx_mul(s->rlp[c],rfb),-32767,32767);
                    s->ri[c]=(uint16_t)(idx+1u==len?0u:idx+1u);off+=len;
                }
                int32_t diffuse=snow_allpass(s,1,snow_allpass(s,0,snow_div2(rv[0]+rv[1]+rv[2]+rv[3],3)));
                rev_l=snow_div2(rv[0]+rv[2],2)+snow_div2(diffuse,1);
                rev_r=snow_div2(rv[1]+rv[3],2)+snow_div2(diffuse,1);
                if(s->rv_fill<512u)s->rv_fill++;
            }
            s->from_l=s->to_l;s->from_r=s->to_r;
            s->to_l=mulq15(dl,dmix)+snow_div127(rev_l*rm);s->to_r=mulq15(dr,dmix)+snow_div127(rev_r*rm);
        }
        int32_t wl=s->from_l+snow_div2((s->to_l-s->from_l)*(int32_t)phase,2);
        int32_t wr=s->from_r+snow_div2((s->to_r-s->from_r)*(int32_t)phase,2);
        if(cm){wl+=snow_div127(cho_l*cm);wr+=snow_div127(cho_r*cm);}
        b[i]=dry+(wl+wr)*2;s->side[i]=(wl-wr)*2;s->pos++;
    }
    if(!tm){s->wow_ph+=34087u*n;s->flutter_ph+=594315u*n;}   /* (the clocks of the effects that are off: */
    if(!cm)s->cho_ph+=ci*n;                                     /* once, not a sample at a time) */
    if(!input){
        uint32_t span=snow_fx_quiet_span(s,enabled,n,q);
        s->quiet=span?s->quiet+n:0;
        if(span&&s->quiet>=span)s->running=0;
    }
}

/* (the owner) The shared effects of BS and DR: one rack (BS's old storage, snow_fx[0]) with DR's
 * FX settings (its DELAY / REVERB / CHORUS / TAPE / BITCRUSH pages: their CHO / DLY / REV are the
 * returns). BS sends its FX SEND (its REV slot) of its mix, DR all of its own (post level, mute, pan:
 * the channel's stereo sum). It runs inside BS's unit, either core, so not on core 0's serial tail;
 * DR's send of the block before is used (written by DR's unit then: 0.7 ms, never read while written). */
static int32_t stattic_dr_send[2][CTL],stattic_bs_send[CTL];
static volatile uint32_t stattic_blk;                    /* blocks rendered (mix_block, before the units) */
static ST_HOT void stattic_bus_run(mixacc_t *mx,uint32_t n)
{
    const int32_t *dr=stattic_dr_send[(stattic_blk&1u)^1u];
    int32_t b[CTL],in[CTL];uint32_t i;
    for(i=0;i<n;i++)b[i]=in[i]=stattic_bs_send[i]+dr[i];
    snow_fx_core(&snow_fx[0],TDRUM->p,b,n);
    const int32_t *side=snow_fx[0].side;
    if(!snow_fx[0].running)return;                       /* (asleep: its input came back unchanged, no side) */
    for(i=0;i<n;i++){
        int32_t wet=b[i]-in[i];                          /* (the rack adds its returns to its input: the returns) */
        mx->l[i]+=wet+side[i];mx->r[i]+=wet-side[i];
    }
}
