/* SPDX-License-Identifier: GPL-3.0-only */
/* Manual sound-design surface, captured before the rc8 execution changes.
 * Exact PCM and final oscillator/envelope/filter state are required. This
 * cannot measure physical FM-1 interrupt or flash-cache timing. */
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"

static uint32_t capture(uint32_t role)
{
    uint32_t h=2166136261u;
    track_t *t=&trk[role];
    for(uint32_t alg=0;alg<6;alg++)for(uint32_t wave=0;wave<6;wave++)
    for(uint32_t route=0;route<3;route++)for(uint32_t filt=0;filt<4;filt++){
        apply_preset_to(t,(alg+wave)%10u);
        memset(snow_l2,0,sizeof snow_l2);
        voice_t v={0};v.active=v.gate=1;v.stage=1;v.note=36u+wave*7u;v.vel=80u+alg*9u;
        t->p[SN_ALG]=(int16_t)alg;t->p[SN_WAVE1]=(int16_t)wave;
        t->p[SN_WAVE2]=(int16_t)((wave+alg)%6u);t->p[SN_WAVE3]=(int16_t)((wave+route+1u)%6u);
        t->p[SN_FB_ROUTE]=(int16_t)route;t->p[SN_FILTER]=(int16_t)filt;
        t->p[SN_SUB]=(int16_t)((wave+route)%2u?0:71);t->p[SN_NOISE]=(int16_t)(filt*29u);
        t->p[P_E5]=(int16_t)(route*49u);t->p[P_E7]=(int16_t)(wave*23u);
        t->p[SN_L2_DEST]=(int16_t)(alg*2u);t->p[SN_L2_DEPTH]=79;
        t->p[P_ESYNC]=(int16_t)(alg&1u);song.g[G_BPM]=(int16_t)(90u+wave*17u);
        snow_note_on(t,&v);int32_t last=0;
        for(uint32_t block=0;block<48;block++){
            if(block==24u)v.gate=0;
            snow_block(t);esync_coef(t);
            vmod_t m={.pitch16=(int32_t)v.note*16,.shape=(int32_t)(wave*17u)<<8,
                      .cutoff=(int32_t)(block%9u)*384-1536,.fine=(int32_t)route*11-11,.amp0=last};
            m.amp1=snow_amp(t,&v,0);last=m.amp1;
            int32_t out[CTL]={0};snow_render(t,&v,out,CTL,&m);
            for(uint32_t i=0;i<CTL;i++)h=pcm_hash(h,out[i]);
        }
        for(uint32_t i=0;i<3;i++)h=pcm_hash(h,(int32_t)v.ph[i]);
        for(uint32_t i=0;i<10;i++)h=pcm_hash(h,v.s[i]);
    }
    return h;
}
int main(void)
{
    /* Captured from clean rc7 source 2497b53, before changing execution.
     * Never regenerate these to excuse a different synth/FX sound. */
    static const uint32_t expected[6]={0x695d640du,0x7edfcde1u,0x22251c2cu,0x593011adu,0xc4c5a0ddu,0x9a73da68u};
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    for(uint32_t role=0;role<NTRK;role++){
        uint32_t h=capture(role);printf("Role %u operator surface: %08x\n",role,h);
        assert(h==expected[role]);
    }
    puts("MANUAL OPERATOR SURFACE PASSED: 2592 sounds, exact PCM and final state");
    return 0;
}
