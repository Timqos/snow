/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef STATTIC_REFERENCE_SOURCE
#define FELUCCA_SNOW 1
#include STATTIC_REFERENCE_SOURCE
#else
#define FELUCCA_STATTIC 1
#define main unused_native_host_main
#include "hostsim.c"
#undef main
#endif
static uint32_t seed=0x41547721u;
static uint32_t draw(void){seed=seed*1664525u+1013904223u;return seed;}
static uint64_t hash(uint64_t h,const void *raw,uint32_t n){const uint8_t *p=raw;for(uint32_t i=0;i<n;i++)h=(h^p[i])*UINT64_C(1099511628211);return h;}
int main(void)
{
    static const uint8_t size[]={1,3,7,16,31,32};
    for(uint32_t scene=0;scene<64;scene++){
        track_t *t=&trk[0];memset(t,0,sizeof *t);memset(snow_fx,0,sizeof snow_fx);
        for(uint32_t p=0;p<P_COUNT;p++)t->p[p]=TP[p].def;
        uint64_t h=UINT64_C(1469598103934665603);
        for(uint32_t block=0;block<320;block++){
            song.g[G_BPM]=(int16_t)(60+block%181);
            if(!(block%17))for(uint32_t p=SN_DTIME;p<=SN_BTONE;p++){
                const param_desc_t *d=&TP[p];uint32_t r=draw();t->p[p]=(int16_t)(d->min+r%(d->max-d->min+1u));
            }
            uint32_t mask=(scene+block/32)&31;
            t->p[SN_TMIX]=mask&1?127:0;t->p[SN_BMIX]=mask&2?127:0;t->p[P_CHOR]=mask&4?127:0;
            t->p[P_DLY]=mask&8?127:0;t->p[P_REV]=mask&16?127:0;t->p[SN_FX_BYPASS]=block%71==0;
            uint32_t n=size[block%6];int32_t b[CTL];for(uint32_t i=0;i<n;i++)b[i]=block%37<12?0:(int32_t)(draw()&0x3ffffu)-131072;
#ifdef STATTIC_REFERENCE_SOURCE
            snow_fx_process(t,b,n);
#else
            snow_fx_core(&snow_fx[0],t->p,b,n);   /* (DR's rack is now the shared one: the same core) */
#endif
            h=hash(h,b,n*sizeof *b);
        }
        h=hash(h,&snow_fx[0],sizeof snow_fx[0]);printf("%u %u %016llx\n",scene,(unsigned)sizeof snow_fx[0],(unsigned long long)h);
    }return 0;
}
