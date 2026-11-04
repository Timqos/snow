/* SPDX-License-Identifier: GPL-3.0-only
 * Snow: six independent effect racks. No shared delay/reverb/chorus returns.
 * Tape and chorus run at FS; delay and reverb use filtered quarter-rate input
 * and interpolate their returns. Buffers/state belong to exactly one channel. */
#define SN_DL_LEN 11264u                         /* 1.02 s at FS/4; exposed time: 10..1000 ms */
#define SN_RV_LEN 1358u
static const uint16_t SN_RV_LINES[3][4]={{137,193,251,293},{229,307,379,443},{211,271,353,433}};
typedef struct {
    int16_t delay[SN_DL_LEN],chorus[512],tape[256],comb[SN_RV_LEN],ap[236],pre[512];
    uint32_t pos,dw,dt_q8,cho_ph,wow_ph,flutter_ph,pre_w;
    uint16_t ri[4],ai[2];
    int32_t rlp[4],dlp,in1,in2,tone,tape_dc,crush,crush_lp;
    int32_t from_l,from_r,to_l,to_r;
    uint32_t crush_count;
    int32_t side[CTL];
} snow_fx_t;
static snow_fx_t snow_fx[NTRK] __attribute__((section(".pool")));
static int32_t snow_fx_mul(int32_t x,int32_t gain) { return (x*gain)/32768; }
static void snow_fx_lowpass(int32_t *state,int32_t x,int32_t gain)
{
    int32_t delta=x-*state,step=snow_fx_mul(delta,gain);
    /* Reach exact silence rather than retaining a negative rounding cycle. */
    *state+=step?step:delta>0?1:delta<0?-1:0;
}
static int32_t snow_tap(const int16_t *b,uint32_t length,uint32_t wp,uint32_t dq8)
{
    uint32_t back=dq8>>8;
    /* Callers bound the tap to this line: one wrap, no per-sample division. */
    uint32_t a=wp+length-back;if(a>=length)a-=length;
    uint32_t z=a?a-1u:length-1u;
    return b[a]+(((b[z]-b[a])*(int32_t)(dq8&255u))>>8);
}
static int32_t snow_allpass(snow_fx_t *s,uint32_t stage,int32_t x)
{
    uint32_t len=stage?139u:97u,off=stage?97u:0u,idx=s->ai[stage];
    int32_t delayed=s->ap[off+idx],y=delayed-x;
    s->ap[off+idx]=(int16_t)clamp(x+delayed/2,-32767,32767);
    s->ai[stage]=(uint16_t)(idx+1u==len?0u:idx+1u);return y;
}
static void snow_fx_process(track_t *t,int32_t *b,uint32_t n)
{
    snow_fx_t *s=&snow_fx[(uint32_t)(t-trk)%NTRK];const int16_t *p=t->p;
    int32_t on=!p[SN_FX_BYPASS],cm=on?p[P_CHOR]:0,dm=on?p[P_DLY]:0,rm=on?p[P_REV]:0;
    int32_t tm=on?p[SN_TMIX]:0,bm=on?p[SN_BMIX]:0;
    int32_t cfb=p[SN_CFB]*180,ck=2048+p[SN_DCOLOR]*230,rk=32000-p[SN_RDAMP]*230;
    uint32_t target=(uint32_t)p[SN_DTIME]*FS/400u;
    if(p[SN_DSYNC]) {
        static const uint8_t divs[11]={0,9,8,7,6,0,1,4,2,5,3};
        target=div_samples(divs[(uint32_t)p[SN_DSYNC]%11u])/4u;
    }
    target=(uint32_t)clamp((int32_t)target,2,SN_DL_LEN-2u);
    if(!s->dt_q8)s->dt_q8=target<<8;
    uint32_t ci=LFO_INC[p[SN_CRATE]&127]/CTL,model=(uint32_t)p[SN_RTYPE]%3u;
    int32_t dfb=p[SN_DFB]*247,dmix=dm*258;
    perf_dub_dly(&dfb,&dmix);
    dmix=on?clamp(dmix,0,32767):0;dfb=clamp(dfb,0,30500);
    int32_t rfb=12000+p[SN_RSIZE]*(model==0?135:157);
    uint32_t pre=(uint32_t)p[SN_RPRE]*FS/4000u;
    for(uint32_t i=0;i<n;i++) {
        int32_t dry=b[i],x=clamp(dry/4,-32767,32767);
        if(tm) {
            int32_t sat=soft_knee(x*(128+p[SN_TDRIVE]*5)/128,16000);
            snow_fx_lowpass(&s->tone,sat,2048+p[SN_TTONE]*225);
            s->tape[s->pos&255u]=(int16_t)clamp(s->tone,-32767,32767);
            int32_t wobble=(osc_sine(s->wow_ph)*p[SN_TWOW])/32768;
            int32_t flutter=(osc_sine(s->flutter_ph)*p[SN_TFLUT])/32768;
            int32_t lag=96*256+wobble*100+flutter*24;
            int32_t tape=snow_tap(s->tape,256,s->pos&255u,(uint32_t)clamp(lag,256,240*256));
            snow_fx_lowpass(&s->tape_dc,tape,32);
            x+=(tape-s->tape_dc-x)*tm/127;dry=x*4;
        }
        s->wow_ph+=34087u;s->flutter_ph+=594315u;
        if(bm) {
            if(!s->crush_count) {
                int32_t q=1<<(16-clamp(p[SN_BBITS],2,16));
                s->crush=(x/q)*q;s->crush_count=(uint32_t)clamp(p[SN_BRATE],1,64);
            }
            s->crush_count--;
            snow_fx_lowpass(&s->crush_lp,s->crush,1500+p[SN_BTONE]*245);
            x+=(s->crush_lp-x)*bm/127;dry=x*4;
        }
        s->cho_ph+=ci;
        int32_t shift=(osc_sine(s->cho_ph)*p[SN_CDEPTH])/128;
        uint32_t cl=(uint32_t)clamp(150*256+shift,256,500*256),cr=(uint32_t)clamp(150*256-shift,256,500*256);
        int32_t cho_l=snow_tap(s->chorus,512,s->pos&511u,cl),cho_r=snow_tap(s->chorus,512,s->pos&511u,cr);
        s->chorus[s->pos&511u]=(int16_t)clamp((cm?x:0)+snow_fx_mul((cho_l+cho_r)/2,cfb),-32767,32767);
        snow_fx_lowpass(&s->in1,x,14000);snow_fx_lowpass(&s->in2,s->in1,14000);
        uint32_t phase=s->pos&3u;
        if(!phase) {
            int32_t d=(int32_t)(target<<8)-(int32_t)s->dt_q8;
            s->dt_q8=(uint32_t)((int32_t)s->dt_q8+(d?d/64+(d>0?1:-1):0));
            int32_t dl=snow_tap(s->delay,SN_DL_LEN,s->dw,s->dt_q8);
            uint32_t rt=(uint32_t)((uint64_t)s->dt_q8*(uint32_t)(508-p[SN_DWIDTH])/508u);
            int32_t dr=snow_tap(s->delay,SN_DL_LEN,s->dw,rt);
            snow_fx_lowpass(&s->dlp,dl,ck);
            s->delay[s->dw]=(int16_t)clamp((dm?s->in2:0)+snow_fx_mul(s->dlp,dfb),-32767,32767);
            if(++s->dw==SN_DL_LEN)s->dw=0;
            s->pre[s->pre_w&511u]=(int16_t)(rm?s->in2:0);
            int32_t rin=s->pre[(s->pre_w-pre)&511u]/2;s->pre_w++;
            int32_t rv[4];uint32_t off=0;
            for(uint32_t c=0;c<4u;c++) {
                uint32_t len=SN_RV_LINES[model][c],idx=s->ri[c];if(idx>=len)idx%=len;
                rv[c]=s->comb[off+idx];snow_fx_lowpass(&s->rlp[c],rv[c],rk);
                s->comb[off+idx]=(int16_t)clamp(rin+snow_fx_mul(s->rlp[c],rfb),-32767,32767);
                s->ri[c]=(uint16_t)(idx+1u==len?0u:idx+1u);off+=len;
            }
            int32_t diffuse=snow_allpass(s,1,snow_allpass(s,0,(rv[0]+rv[1]+rv[2]+rv[3])/8));
            int32_t rev_l=(rv[0]+rv[2])/4+diffuse/2,rev_r=(rv[1]+rv[3])/4+diffuse/2;
            s->from_l=s->to_l;s->from_r=s->to_r;
            s->to_l=mulq15(dl,dmix)+rev_l*rm/127;s->to_r=mulq15(dr,dmix)+rev_r*rm/127;
        }
        int32_t wl=s->from_l+(s->to_l-s->from_l)*(int32_t)phase/4+cho_l*cm/127;
        int32_t wr=s->from_r+(s->to_r-s->from_r)*(int32_t)phase/4+cho_r*cm/127;
        b[i]=dry+(wl+wr)*2;s->side[i]=(wl-wr)*2;s->pos++;
    }
}
