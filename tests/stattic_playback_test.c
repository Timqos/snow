/* SPDX-License-Identifier: GPL-3.0-only
 * Regression for the second device report: distortion appears only while
 * the sequencer runs (2 cores, up to 80% displayed load), then STOP is clean.
 * This exercises real playback/locks and tails; it cannot time the FM-1. */
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"
static void phrases_and_locks(void)
{
    seq_stop();memset(&motion,0,sizeof motion);
    for(uint32_t ch=4;ch<6;ch++){
        track_t *t=&trk[ch];apply_preset_to(t,0);t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=50;t->p[SN_G_LOCKS]=100;t->p[P_SLEN]=32;
        uint32_t notes=0,ties=0,sevenths=0;
        for(uint32_t n=0;n<512;n++){
            assert(!snow_generate(ch,1));
            for(uint32_t i=0;i<32;i++){ties+=t->step[i].time==ST_TIE;if(t->step[i].time==ST_NOTE&&t->step[i].n){notes++;sevenths+=t->step[i].n==4;}}
            assert(!snow_generate_parameters(ch));
            for(uint32_t i=0;i<motion.count;i++)if((motion.event[i].place>>6)==ch&&motion.event[i].generated){
                uint32_t id=motion.event[i].id;
                assert(id!=P_REL&&id!=SN_MODE&&id!=SN_ALG&&id!=SN_RATIO&&id!=P_E1&&id!=P_DEC);
                if(ch==5)assert(id!=P_E0); /* no sudden dense-FM sound replacement */
            }
        }
        assert(ties>notes/2&&notes>600);if(ch==5)assert(sevenths>10);
        printf("Role %u: %u note events, %u held steps, %u seventh chords; generated locks preserve articulation and harmonic core\n",ch,notes,ties,sevenths);
    }
}
static void long_playback(void)
{
    panic_req=63;events_block(CTL);web_snow_demo();memset(&motion,0,sizeof motion);
    for(uint32_t k=0;k<NTRK;k++){
        trk[k].p[SN_G_LIVE]=0;trk[k].p[P_MUTE]=0;trk[k].p[P_LEVEL]=90;
        assert(!snow_generate(k,1));assert(!snow_generate_parameters(k));assert(!snow_random_section(k,FAM_FX));
    }
    int16_t chord[P_COUNT];for(uint32_t id=0;id<P_COUNT;id++)chord[id]=motion_base_value(&trk[5],id);
    tap(B_PLAY);assert(song.playing);double peak=0;uint64_t energy=0;
    for(uint32_t b=0;b<FS*60u/128u;b++){
        web_render(128);const float *l=web_out_l(),*r=web_out_r();
        for(uint32_t i=0;i<128;i++){
            assert(isfinite(l[i])&&isfinite(r[i])&&fabs(l[i])<1&&fabs(r[i])<1);
            if(fabs(l[i])>peak)peak=fabs(l[i]);energy+=(uint64_t)(fabs(l[i])*32768);
        }
    }
    seq_stop();
    assert(trk[5].p[P_REL]==chord[P_REL]&&trk[5].p[SN_MODE]==chord[SN_MODE]&&trk[5].p[SN_ALG]==chord[SN_ALG]);
    assert(trk[5].p[P_E0]==chord[P_E0]&&trk[5].p[P_DEC]==chord[P_DEC]);
    assert(energy>1000000&&peak>.01);
    /* The actual envelope/mixer test is repeated after a minute of six-track
     * playback and effects, rather than assuming a clean startup is enough. */
    chord_release_audio();
    printf("60-second six-channel playback with generated locks and strong FX: bounded stereo peak %.4f, CH core unchanged after STOP, real two-bar release still passes\n",peak);
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    phrases_and_locks();long_playback();puts("STATTIC PLAYBACK REGRESSIONS PASSED; physical peak-load retest remains required");return 0;
}
