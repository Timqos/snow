/* SPDX-License-Identifier: GPL-3.0-only */
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
int main(void)
{
    init();for(uint32_t k=0;k<NTRK;k++){stattic_prepare(k,0);stattic_runtime_tick();}
    for(uint32_t k=0;k<NTRK;k++){
        trk[k].p[P_LRATE]=127;trk[k].p[P_LWAVE]=4;
        trk[k].p[P_M1SRC]=MS_LFO;trk[k].p[P_M1DST]=MD_RATE;trk[k].p[P_M1AMT]=63;
    }
    song.master_q12=4096;seq_start();uint64_t hash=UINT64_C(1469598103934665603);uint32_t audible=0,peak=0;
    for(uint32_t b=0;b<FS*16u/CTL;b++){
        if(!(b%500u)){
            uint32_t k=b/500u%NTRK,knob=b/500u%4u;stattic_prepare(k,knob);
            if(k<NPART){trk[k].p[SN_ALG]=(int16_t)(b/500u%6);trk[k].p[SN_WAVE1]=(int16_t)(b/500u%6);trk[k].p[SN_FILTER]=(int16_t)(b/500u%4);}
        }
        if(b%700u==100u){trk[0].p[SN_G_SCOPE]=1;stattic_prepare_scope(0,2);trk[0].p[SN_G_SCOPE]=0;}
        if(b%900u==250u){trk[1].p[P_M1SRC]=MS_LFO;trk[1].p[P_M1DST]=MD_E1+2;trk[1].p[P_M1AMT]=40;}
        if(b%1100u==1000u)stattic_pattern_select(2,(stattic_current[2]+1u)%3u);
        if(b%1200u==300u){for(uint32_t i=0;i<4;i++)trk_note_on(&trk[2],48+i*4,100);}
        if(b%1200u==400u){for(uint32_t i=0;i<4;i++)trk_note_off(&trk[2],48+i*4);}
        block();assert(voices_busy()<=NVOICE);uint32_t ch=0;for(uint32_t i=0;i<NVOICE;i++)ch+=!!trk[2].v[i].active;assert(ch<=4);
        for(uint32_t i=0;i<CTL*2;i++){
            int32_t x=output[i];assert(x>=-32768&&x<=32767);audible+=x!=0;uint32_t a=(uint32_t)(x<0?-x:x);if(a>peak)peak=a;
            hash=(hash^(uint32_t)x)*UINT64_C(1099511628211);
        }
        assert(drum_kit()==DRUM_STATTIC);
        for(uint32_t i=0;i<NDRUM;i++)assert(!drums.synth[i]);
    }
    assert(audible>FS&&peak>100);printf("STATTIC four-track mix %016llx peak %u: 16 s, all generators, live chords, CH <=4, sampled DR only\n",(unsigned long long)hash,peak);
    return 0;
}
