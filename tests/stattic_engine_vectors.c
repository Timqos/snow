/* SPDX-License-Identifier: GPL-3.0-only */
/* Same semantic stimuli against frozen old STATTIC and the SLOOP adapter.
 * This tests oscillator/envelope character, not hardware CPU deadlines. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef STATTIC_REFERENCE_SOURCE
#define FELUCCA_SNOW 1
#include STATTIC_REFERENCE_SOURCE
#else
#define FELUCCA_STATTIC 1
#define main native_sloop_hostsim_main
#include "hostsim.c"
#undef main
#endif

static uint64_t hword(uint64_t h, uint32_t v)
{
    for (uint32_t i=0;i<4;i++) { h^=v&255u;h*=UINT64_C(1099511628211);v>>=8; }
    return h;
}

static FILE *drum_dump;
static void vector(uint32_t role,uint32_t preset,uint32_t variant)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);
    memset(snow_l2,0,sizeof snow_l2);memset(snow_lfo_clock,0,sizeof snow_lfo_clock);
    memset(snow_offset,0,sizeof snow_offset);
#ifdef STATTIC_REFERENCE_SOURCE
    track_t *t=&trk[role];t->engine=t->eng_req=ENGI_SNOW+role;
#else
    track_t *t=&trk[role%NPART];t->engine=t->eng_req=ENGI_STATTIC+role;
#endif
    const preset_t *p=&SNOW_PATCHES[role][preset];
    for(uint32_t id=0;id<P_COUNT;id++)t->p[id]=TP[id].def;
    for(uint32_t i=0;i<8;i++)t->p[P_E0+i]=p->e[i];
    t->p[P_ATK]=p->env[0];t->p[P_DEC]=p->env[1];
    t->p[P_SUS]=p->env[2];t->p[P_REL]=p->env[3];
    t->p[SN_L2_RATE]=55;t->p[SN_L2_WAVE]=0;t->p[SN_L2_DEPTH]=0;
    t->p[SN_L2_DEST]=0;t->p[SN_L2_SYNC]=0;t->p[SN_L2_PHASE]=0;
    t->p[SN_L2_TRIG]=1;t->p[SN_L2_FADE]=0;
    snow_fill(t->p,role,preset);song.g[G_BPM]=122;
    voice_t *v=&t->v[0];v->note=60;v->vel=100;v->active=v->gate=1;v->stage=1;
    if(variant){
        uint32_t a=(variant-1u)/6u,w=(variant-1u)%6u;
        t->p[SN_ALG]=(int16_t)a;t->p[SN_WAVE1]=(int16_t)w;
        t->p[SN_WAVE2]=(int16_t)((w+2u)%6u);t->p[SN_WAVE3]=(int16_t)((w+4u)%6u);
        t->p[SN_MIX]=55;t->p[SN_SUB]=31;t->p[SN_NOISE]=19;
        t->p[SN_FB_ROUTE]=variant%3u;t->p[SN_FILTER]=variant%4u;
        t->p[SN_RESO]=variant%2u?102:0;t->p[SN_PW]=variant%2u?28:103;
        t->p[SN_TUNE]=-12;t->p[SN_FINE]=-5;t->p[SN_RATIO3]=47;
    }
    snow_note_on(t,v);
    uint64_t h=UINT64_C(14695981039346656037);
    uint32_t peak=0,frames=role==5u&&!variant?FS*5u:16384u;
    if(drum_dump)frames=FS/2u;
    int32_t last=0;
    for(uint32_t at=0;at<frames;at+=CTL){
        if(at>=4096u)v->gate=0;
        snow_block(t);esync_coef(t);
        vmod_t m={.pitch16=60*16,.shape=64<<8,.amp0=last};
        m.amp1=snow_amp(t,v,0);last=m.amp1;
        int32_t out[CTL]={0};snow_render(t,v,out,CTL,&m);
        for(uint32_t i=0;i<CTL;i++){
            uint32_t a=out[i]<0?0u-(uint32_t)out[i]:(uint32_t)out[i];
            if(a>peak)peak=a;h=hword(h,(uint32_t)out[i]);
            if(drum_dump&&at+i<frames){int16_t sample=(int16_t)clamp(out[i],-32767,32767);fwrite(&sample,2,1,drum_dump);}
        }
    }
    for(uint32_t i=0;i<3;i++)h=hword(h,v->ph[i]);
    for(uint32_t i=0;i<10;i++)h=hword(h,(uint32_t)v->s[i]);
    printf("%u %u %u %016llx %u %u\n",role,preset,variant,(unsigned long long)h,peak,snow_done(t,v));
}

int main(int argc,char **argv)
{
    if(argc==2){
        drum_dump=fopen(argv[1],"wb");if(!drum_dump)return 2;
        for(uint32_t r=0;r<4;r++)for(uint32_t p=0;p<10;p++)vector(r,p,0);
        return fclose(drum_dump)!=0;
    }
    for(uint32_t r=0;r<6;r++)for(uint32_t p=0;p<10;p++)vector(r,p,0);
    for(uint32_t r=0;r<6;r++)for(uint32_t v=1;v<=36;v++)vector(r,0,v);
    return 0;
}
