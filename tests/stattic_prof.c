/* SPDX-License-Identifier: GPL-3.0-only */
/* Host CPU profile of a playing four-channel STATTIC scene (not an FM-1 timing).
 * Prints per-stage cost per DMA half (256 frames) in host ticks: mean, p99, max,
 * and an output hash so DSP changes can be checked for bit-exactness. */
#include <x86intrin.h>
#define STATTIC_PROFILE_TICKS() ((uint32_t)__rdtsc())
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
#include <stdlib.h>
static int cmpu(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return x<y?-1:x>y;}
#define NH (FS*60u/256u)
static uint64_t h_part[NTRK+3][NH];
int main(int argc,char **argv)
{
    uint32_t seconds=argc>1?(uint32_t)atoi(argv[1]):30u;
    init();
    for(uint32_t k=0;k<NTRK;k++){stattic_prepare(k,0);stattic_runtime_tick();}
    if(getenv("FXON"))for(uint32_t k=0;k<NTRK;k++){trk[k].p[P_CHOR]=40;trk[k].p[P_DLY]=40;trk[k].p[P_REV]=40;}
    song.master_q12=getenv("MASTER")?atoi(getenv("MASTER")):4096;seq_start();
    uint64_t hash=UINT64_C(1469598103934665603);
    FILE *dump=getenv("DUMP")?fopen(getenv("DUMP"),"wb"):0;
    if(getenv("DIST"))for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_DIST]=(int16_t)atoi(getenv("DIST"));
    if(getenv("TFLT"))for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_TFLT]=(int16_t)atoi(getenv("TFLT"));
    if(getenv("ONLY"))for(uint32_t k=0;k<NTRK;k++)if(k!=(uint32_t)atoi(getenv("ONLY")))trk[k].p[P_LEVEL]=0;
    uint32_t stop_at=getenv("STOPAT")?(uint32_t)atoi(getenv("STOPAT")):0;
    uint32_t halves=FS*seconds/256u;if(halves>NH)halves=NH;
    for(uint32_t h=0;h<halves;h++){
        stattic_profile_begin();
        uint64_t t0=__rdtsc();
        for(uint32_t b=0;b<256u/CTL;b++){
            block();
            if(dump)fwrite(output,sizeof output,1,dump);
            for(uint32_t i=0;i<CTL*2;i++)hash=(hash^(uint32_t)output[i])*UINT64_C(1099511628211);
        }
        uint64_t t1=__rdtsc();
        stattic_profile_end();
        if(!(h%((FS/256u)*2u)))stattic_runtime_tick();
        if(stop_at&&h==stop_at*(FS/256u))seq_stop();
        if(getenv("FREEZE")){if(h==3u*(FS/256u))punch.req=(int8_t)PX_FREEZE;if(h==6u*(FS/256u))punch.req=-1;}
        for(uint32_t k=0;k<NTRK;k++)h_part[k][h]=stattic_profile.last[k][2];
        h_part[NTRK][h]=stattic_profile.events_last;h_part[NTRK+1][h]=stattic_profile.buses_last;h_part[NTRK+2][h]=t1-t0;
    }
    if(getenv("PARTS")){FILE *f=fopen(getenv("PARTS"),"w");for(uint32_t h=0;h<halves;h++)fprintf(f,"%llu %llu %llu %llu\n",(unsigned long long)h_part[0][h],(unsigned long long)h_part[1][h],(unsigned long long)h_part[2][h],(unsigned long long)h_part[3][h]);fclose(f);}
    static const char *names[]={"BS","SY","CH","DR","events","buses","WALL"};
    for(uint32_t k=0;k<NTRK+3;k++){
        static uint64_t s[NH];memcpy(s,h_part[k],halves*sizeof *s);qsort(s,halves,sizeof *s,cmpu);
        uint64_t sum=0;for(uint32_t h=0;h<halves;h++)sum+=s[h];
        printf("%-7s mean %8llu p50 %8llu p99 %8llu max %8llu\n",names[k],(unsigned long long)(sum/halves),
               (unsigned long long)s[halves/2],(unsigned long long)s[halves*99/100],(unsigned long long)s[halves-1]);
    }
    if(dump)fclose(dump);
    printf("hash %016llx\n",(unsigned long long)hash);
    return 0;
}
