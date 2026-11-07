/* SPDX-License-Identifier: GPL-3.0-only */
/* User contract: CH must never have six active voices. Its four-voice cap
 * includes release/fading voices; all other channel caps and shared budget
 * remain intact. Test actual allocation/rendering on one/two host cores. */
#include "c1_host.h"
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"

static uint32_t ch_count(void)
{
    uint32_t count=0;
    for(uint32_t v=0;v<NVOICE;v++)count+=trk[5].v[v].active!=0;
    assert(count<=4u);return count;
}
static void render(uint32_t blocks)
{
    int32_t out[CTL*2];
    while(blocks--){mix_block(out,CTL);ch_count();assert(snow_rendered_voices[5]<=4u);}
}
static void fixture(uint32_t dual)
{
    c1h_off=!dual;seq_stop();launch_drop();panic_req=63;events_block(CTL);
    memset(&motion,0,sizeof motion);perf_solo=0;
    for(uint32_t k=0;k<NTRK;k++){
        apply_preset_to(&trk[k],0);trk[k].p[P_MUTE]=0;trk[k].p[P_AMODE]=0;
        /* Each test scene begins with empty voice histories. */
        memset(trk[k].v,0,sizeof trk[k].v);trk[k].nmono=trk[k].mono_note=0;
    }
    events_block(CTL);render(2);assert(!ch_count());
    assert(SNOW_ENG[5].poly==4u&&NVOICE==8&&VOICES_2CORE==12);
    for(uint32_t k=0;k<5;k++)assert(SNOW_ENG[k].poly==4u);
    assert(trk[5].p[P_REL]==SN_CH_RELEASE);
}
static void chords(uint32_t dual)
{
    fixture(dual);track_t *ch=&trk[5];uint8_t previous[CHORD_MAX]={0};uint32_t prev_n=0;
    for(uint32_t size=3;size<=4;size++){
        ch->p[SN_PLAY_MODE]=0;ch->p[P_CHRD]=size==3?CH_MAJ:CH_MAJ7;ch->p[P_VOIC]=VC_CLOSE;
        for(uint32_t n=0;n<30;n++){
            for(uint32_t i=0;i<prev_n;i++)trk_note_off(ch,previous[i]);
            trk_note_off(&trk[4],32u+(n+11u)%12u);
            trk_note_on(&trk[4],32u+n%12u,100);trk_note_on(&trk[0],60,100);
            uint8_t notes[CHORD_MAX];uint32_t count=chord_build(ch,48u+n%12u,notes);assert(count==size);
            for(uint32_t i=0;i<count;i++){trk_note_on(ch,notes[i],100);ch_count();}
            assert(ch_count()==4u||(n==0&&size==3));
            memcpy(previous,notes,count);prev_n=count;render(90);
        }
    }
    puts(dual?"CH two cores: triads/sevenths remain complete; held + released voices never exceed four":"CH one core: triads/sevenths remain complete; held + released voices never exceed four");
}
static void notes_and_mix(uint32_t dual)
{
    fixture(dual);track_t *ch=&trk[5];ch->p[SN_PLAY_MODE]=1;
    for(uint32_t k=0;k<5;k++)trk_note_on(&trk[k],k==4?36:60,100);
    for(uint32_t n=0;n<60;n++){
        uint32_t note=48u+n%18u;trk_note_on(ch,note,100);ch_count();render(8);
        trk_note_off(ch,note);ch_count();render(8);
    }
    /* A fading CH slot is still counted toward the hard cap. */
    for(uint32_t i=0;i<4;i++)if(ch->v[i].active)voice_kill(&ch->v[i]);
    ch_count();
    for(uint32_t n=60;n<64;n++){trk_note_on(ch,n,100);ch_count();}
    render(20);assert(ch_count()==4u&&ch->p[P_REL]==SN_CH_RELEASE);
    puts(dual?"CH two cores: NOTES, repeated starts, five other roles and fading-slot reuse stay within four":"CH one core: NOTES, repeated starts, five other roles and fading-slot reuse stay within four");
}
static void unison(uint32_t dual)
{
    fixture(dual);track_t *ch=&trk[5];ch->p[P_VOICE]=V_UNISON;
    for(uint32_t n=48;n<60;n++){
        trk_note_on(ch,n,100);assert(ch_count()==4u);render(4);
        trk_note_off(ch,n);ch_count();render(4);
    }
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);setenv("C1_MASK","auto",1);c1h_off=1;
    web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    for(uint32_t dual=0;dual<2;dual++){chords(dual);notes_and_mix(dual);unison(dual);}
    puts("STRICT CH FOUR-VOICE CONTRACT PASSED; physical FM-1 onset retest remains necessary");return 0;
}
