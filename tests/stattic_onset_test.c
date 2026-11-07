/* SPDX-License-Identifier: GPL-3.0-only */
/* Retriggered low notes under a held CH, including arbitrary sound design,
 * synchronized clocks, effects and quiet MASTER. Exact old PCM is required;
 * this test cannot reproduce physical DMA stalls. */
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"

static void waveform_equivalence(void)
{
    uint32_t r=1;
    for(uint32_t n=0;n<300000;n++){
        r=r*1664525u+1013904223u;uint32_t phase=r;
        r=r*1664525u+1013904223u;uint32_t inc=r;
        uint32_t wave=n%9u,pw=(n%127u)<<25;
#ifndef STATTIC_OLD_REFERENCE
        assert(snow_wave_fast(phase,inc,wave,pw)==snow_wave(phase,inc,wave,pw));
#endif
    }
#ifndef STATTIC_OLD_REFERENCE
    for(uint32_t clock=0;clock<2;clock++)for(uint32_t channel=0;channel<NTRK;channel++){
        uint32_t previous=0;
        for(uint32_t n=1;n<=2000;n++){
            uint32_t period=n%3u?previous:n*97u+1u;
            if(!period)period=1;
            uint32_t q=0xffffffffu/period,r=0xffffffffu%period;
            uint32_t ref=q*CTL+(r*CTL+CTL)/period;
            assert(snow_sync_inc(&trk[channel],clock,period)==ref);
            assert(snow_sync_inc(&trk[channel],clock,period)==ref);
            previous=period;
        }
    }
#endif
}

static uint32_t retriggers(uint32_t role,uint32_t kind)
{
    seq_stop();launch_drop();panic_req=63;events_block(CTL);
    memset(&motion,0,sizeof motion);memset(&pf,0,sizeof pf);
    memset(snow_fx,0,sizeof snow_fx);memset(snow_mix_fx,0,sizeof snow_mix_fx);
    for(uint32_t k=0;k<NTRK;k++){trk[k].p[P_MUTE]=1;pf.mg[k]=32768;}
    track_t *low=&trk[role],*ch=&trk[5];
    apply_preset_to(low,kind%10u);apply_preset_to(ch,kind%10u);events_block(CTL);
    if(kind>=2u){
        uint32_t mask[(P_COUNT+31u)/32u];snow_edit_mask(mask);
        snow_design_sound(low,role,0x9276541u+kind*0x9e3779b9u,mask,low->p);
        snow_design_sound(ch,5,0x1276541u+kind*0x9e3779b9u,mask,ch->p);
    }
    low->p[P_MUTE]=ch->p[P_MUTE]=0;low->p[P_LEVEL]=ch->p[P_LEVEL]=104;
    if(kind&1u){
        low->p[P_LSYNC]=7;ch->p[P_LSYNC]=8;
        low->p[SN_L2_SYNC]=ch->p[SN_L2_SYNC]=9;
    }
    fx_usb_fixed=kind==3u;fx_lowcut=0;song.master_q12=kind&1u?409:MASTER_FULL;
    lim_env=LIM_T;dc_l=dc_r=dce_l=dce_r=0;perf_solo=0;
    static const uint8_t chord[4]={48,52,55,59};
    for(uint32_t i=0;i<4;i++)trk_note_on(ch,chord[i],100);
    uint32_t hash=2166136261u,note=0,starts=0;int32_t out[2*CTL];
    for(uint32_t b=0;b<3000;b++){
        if(!(b%47u)){
            if(note)trk_note_off(low,note);
            note=role==4u?32u+(starts*7u)%17u:60u;trk_note_on(low,note,80u+starts%48u);starts++;
        }
        if(b%47u==33u)trk_note_off(low,note);
        if(b==900u)song.g[G_BPM]=91;
        if(b==1500u){low->p[P_SDIV]=3;song.g[G_BPM]=177;}
        if(b==2100u){low->p[P_LSYNC]=9;ch->p[SN_L2_SYNC]=3;}
        mix_block(out,CTL);if(fx_usb_fixed)usb_fixed_dac(out,CTL);
        for(uint32_t i=0;i<CTL*2u;i++){
            assert(llabs(out[i])<32768);hash=pcm_hash(hash,out[i]);
        }
    }
    printf("Low role %u scene %u: %u retriggers under held CH, PCM %08x\n",role,kind,starts,hash);
    return hash;
}

int main(void)
{
    /* Captured from unmodified rc5 source 27d4554. Keep these immutable:
     * changing an oscillator, envelope, preset, seed or FX must fail here. */
    static const uint32_t expected[2][4]={
        {0x2d147f29u,0xe772eb79u,0xe359209du,0xf77c17a7u},
        {0x0361aa6du,0x3dcf521du,0xfcf5802au,0x61c7df5eu}
    };
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    waveform_equivalence();
    for(uint32_t role=0;role<=4;role+=4)for(uint32_t kind=0;kind<4;kind++)assert(retriggers(role,kind)==expected[role/4u][kind]);
    puts("ONSET SOFTWARE CHECKS PASSED; physical DMA margin requires FM-1");return 0;
}
