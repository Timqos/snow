/* SPDX-License-Identifier: GPL-3.0-only */
/* Exact DSP arithmetic, shared-bar generator contracts and real overlapping
 * CH sequence notes. These are software regressions, not target CPU timing. */
#include <stdint.h>
static uint32_t repair_clock,repair_clock_step;
static uint32_t repair_ticks(void){repair_clock+=repair_clock_step;return repair_clock;}
#define SNOW_MIX_TICKS() repair_ticks()
#define SNOW_MIX_HAND 40u
#include "c1_host.h"
#define STATTIC_TRANSPORT_LIBRARY 1
#include "stattic_transport_test.c"
#include <limits.h>
static void exact_math(void)
{
    for(int32_t x=-16777216;x<=16777216;x++)assert(snow_div127(x)==x/127);
    uint32_t r=1;
    for(uint32_t i=0;i<1000000;i++){
        r=r*1664525u+1013904223u;int32_t x=(int32_t)r;assert(snow_div127(x)==x/127);
    }
    assert(snow_div127(INT_MIN)==INT_MIN/127&&snow_div127(INT_MAX)==INT_MAX/127);
    for(uint32_t n=0;n<20000;n++){
        uint32_t all=0,lo,best=0;
        for(uint32_t k=0;k<NPART;k++){
            r=r*1664525u+1013904223u;part_cost[k]=(uint16_t)(n<64?((n>>k)&1u)*100u:r);all+=part_cost[k];
        }
        lo=all;
        for(uint32_t m=1;m<63;m++){
            uint32_t a=0;for(uint32_t k=0;k<NPART;k++)if((m>>k)&1u)a+=part_cost[k];
            uint32_t w=(a>all-a?a:all-a)+C1_HAND;if(w<lo){lo=w;best=m;}
        }
        assert(mix_split()==best);
    }
    /* Exercise the target-only timing feedback: the renderer's measured
     * cost, not its voice-count fallback, becomes the next split's weight. */
    part_cost[0]=400;repair_clock_step=1000;mix_part(&trk[0],CTL,&mx[0]);
    assert(part_cost[0]==1000);
    part_cost[0]=2000;mix_part(&trk[0],CTL,&mx[0]);assert(part_cost[0]==1875);repair_clock_step=0;
    felucca_dbg.late=3;assert(strstr(menu_vname(MI_CPU,0)," L3"));
    felucca_dbg.late=UINT32_MAX;song.cpu_q8=9999;
    const char *label=menu_vname(MI_CPU,0);assert(strstr(label,"L999")&&strlen(label)<20);
    snow_late_base=650;felucca_dbg.late=651;assert(snow_late_count()==1&&strstr(menu_vname(MI_CPU,0)," L1"));
    snow_late_base=felucca_dbg.late=song.cpu_q8=0;
    puts("DSP: exact /127 over the full sample-product range, one million int32 cases and boundaries; Gray split matches exhaustive masks including ties");
}
static void queued_generators(void)
{
    static int16_t before[NTRK][P_COUNT],expected[P_COUNT];
    static step_t old[NTRK][NSTEP],expected_steps[NSTEP];
    for(uint32_t bpm=90;bpm<=180;bpm+=30){
        song.g[G_BPM]=(int16_t)bpm;fixture();snow_home_panel=SN_HOME_GEN;
        memset(&motion,0,sizeof motion);undo_clear();
        for(uint32_t k=0;k<NTRK;k++){
            trk[k].p[P_SLEN]=64;trk[k].p[SN_G_LIVE]=0;trk[k].p[SN_G_LOCKS]=80;
            for(uint32_t id=0;id<P_COUNT;id++)before[k][id]=motion_base_value(&trk[k],id);
            memcpy(old[k],trk[k].step,sizeof old[k]);
        }
        /* Same random states and same generator calls as the existing STOP
         * path: timing must change, the generated music/sound must not. */
        sel_set(5);snow_sound_state[5]=0x12345678u;uint32_t state=snow_sound_state[5];
        song.playing=0;
        for(uint32_t knob=0;knob<4;knob++)assert(!snow_home_generate(knob));
        for(uint32_t id=0;id<P_COUNT;id++)expected[id]=motion_base_value(&trk[5],id);
        memcpy(expected_steps,trk[5].step,sizeof expected_steps);motion_store_t expected_motion=motion;
        uint32_t final_state=snow_sound_state[5];
        for(uint32_t k=0;k<NTRK;k++){memcpy(trk[k].p,before[k],sizeof before[k]);memcpy(trk[k].step,old[k],sizeof old[k]);}
        memset(&motion,0,sizeof motion);motion_base_valid=0;snow_sound_state[5]=state;undo_clear();song.playing=1;
        for(uint32_t knob=0;knob<4;knob++){
            uint32_t count=snow_home_gen[5][knob];assert(!snow_home_generate(knob));
            assert(snow_home_gen[5][knob]==count+1&&stattic_gen[5].at==16);
            for(uint32_t k=0;k<NTRK;k++){assert(!memcmp(trk[k].p,before[k],sizeof before[k]));assert(!memcmp(trk[k].step,old[k],sizeof old[k]));}
            assert(!motion.count);
        }
        assert(snow_sound_state[5]==final_state);
        assert(!memcmp(stattic_gen[5].step,expected_steps,sizeof expected_steps));
        for(uint32_t id=0;id<P_COUNT;id++)if((stattic_gen[5].mask[id/32]>>(id%32))&1u)assert(stattic_gen[5].p[id]==expected[id]);
        /* A live mixer edit while waiting must survive the sound's commit. */
        trk[5].p[P_LEVEL]=73;next_bar();assert(!stattic_gen[5].armed&&trk[5].p[P_LEVEL]==73&&trk[5].seq_idx==0);
        assert(!memcmp(trk[5].step,expected_steps,sizeof expected_steps));
        assert(motion.count==expected_motion.count);
        for(uint32_t id=0;id<P_COUNT;id++)if((stattic_gen[5].mask[id/32]>>(id%32))&1u)assert(motion_base_value(&trk[5],id)==expected[id]);
        assert(!memcmp(motion.event,expected_motion.event,motion.count*sizeof motion.event[0]));
        /* Completed batch: one normal undo/redo covers all four families. */
        undo_step(0);assert(!memcmp(trk[5].step,old[5],sizeof old[5]));undo_step(1);assert(!memcmp(trk[5].step,expected_steps,sizeof expected_steps));
    }
    fixture();snow_home_panel=SN_HOME_GEN;sel_set(0);undo_clear();
    step_t steps[NSTEP];memcpy(steps,trk[0].step,sizeof steps);
    assert(!snow_home_generate(0));memcpy(expected_steps,stattic_gen[0].step,sizeof expected_steps);
    undo_step(0);assert(!stattic_gen[0].armed&&!memcmp(trk[0].step,steps,sizeof steps));
    next_bar();assert(!memcmp(trk[0].step,steps,sizeof steps));undo_step(1);assert(!memcmp(trk[0].step,expected_steps,sizeof expected_steps));
    fixture();snow_home_panel=SN_HOME_GEN;memset(&motion,0,sizeof motion);
    for(uint32_t k=0;k<NTRK;k++){sel_set(k);trk[k].p[SN_G_LOCKS]=100;assert(!snow_home_generate(0));assert(stattic_gen[k].armed);}
    next_bar();assert(!stattic_gen_error&&motion.count<=MOTION_MAX);
    for(uint32_t k=0;k<NTRK;k++)assert(!stattic_gen[k].armed&&trk[k].seq_idx==0);
    /* A filled live motion pool after preparation rejects the whole future
     * batch instead of silently deleting another channel's manual locks. */
    fixture();snow_home_panel=SN_HOME_GEN;sel_set(0);memset(&motion,0,sizeof motion);
    trk[0].p[SN_G_LOCKS]=100;memcpy(steps,trk[0].step,sizeof steps);assert(!snow_home_generate(0));
    assert(stattic_gen[0].motion.count);
    for(uint32_t i=0;i<MOTION_MAX;i++)assert(!motion_put(&trk[1],i,P_E0,64,MOTION_LOCK));
    next_bar();assert(stattic_gen_error&&!stattic_gen[0].armed&&motion.count==MOTION_MAX&&!memcmp(steps,trk[0].step,sizeof steps));
    fixture();snow_home_panel=SN_HOME_GEN;sel_set(0);
    sel_set(0);assert(!snow_home_generate(1));assert(stattic_gen[0].armed);seq_stop();assert(!stattic_gen[0].armed);
    seq_start();events_block(1);assert(!snow_home_generate(2));launch_drop();assert(!stattic_gen[0].armed);
    puts("HOME 2: unchanged immediate-generator results/seeds, all four knobs enter together at BAR at 90..180 BPM, live mix preserved, batch undo/redo, queued cancellation, six-track locks, STOP and project drop passed");
}
static void bounded_chords(void)
{
    c1h_off=1;seq_stop();launch_drop();memset(&motion,0,sizeof motion);panic_req=63;events_block(CTL);
    for(uint32_t k=0;k<NTRK;k++){memset(trk[k].step,0,sizeof trk[k].step);trk[k].p[SN_G_LIVE]=0;}
    track_t *t=&trk[5];apply_preset_to(t,0);t->p[P_AMODE]=0;t->p[P_ESYNC]=0;t->p[P_LD_AMP]=0;
    t->p[P_SLEN]=64;t->p[P_SDIV]=2;t->p[P_DIR]=DIR_FWD;t->p[P_SSWING]=0;
    static const uint8_t chords[2][4]={{48,52,55,59},{50,53,57,60}};
    for(uint32_t j=0;j<2;j++){step_t *s=&t->step[j*8];s->time=ST_NOTE;s->n=4;s->vel=100;memcpy(s->note,chords[j],4);}
    events_block(CTL);song.g[G_BPM]=120;song.g[G_SWING]=0;song.g[G_CLOCK]=0;seq_start();
    int32_t out[CTL*2];uint64_t energy=0;
    for(uint32_t n=0;n<FS*11u/10u;n+=CTL){
        mix_block(out,CTL);uint32_t count=0;
        for(uint32_t i=0;i<NVOICE;i++)count+=t->v[i].active!=0;
        assert(count<=4u);
        for(uint32_t i=0;i<CTL*2;i++)energy+=(uint64_t)llabs(out[i]);
    }
    uint32_t active=0,old=0;
    for(uint32_t i=0;i<NVOICE;i++)if(t->v[i].active){
        active++;
        for(uint32_t j=0;j<4;j++)if(t->v[i].note==chords[0][j]){assert(!t->v[i].gate&&t->v[i].s[0]>4096);old++;}
    }
    assert(SNOW_ENG[5].poly==4u&&active==4u&&!old&&energy>1000000);
    assert(t->p[P_REL]==SN_CH_RELEASE&&NVOICE==8);
    puts("CH sequence: strict four active voices including release tails; the incoming four-note chord replaces the old tails; shared voice budget unchanged");
}
static void onset_prediction(void)
{
    seq_stop();panic_req=63;events_block(CTL);
    for(uint32_t k=0;k<NPART;k++){memset(trk[k].v,0,sizeof trk[k].v);snow_peak_voice[k]=0;snow_rendered_voices[k]=0;part_cost[k]=k<4?1000:k==5?6000:0;}
    uint32_t old=mix_split();trk[4].v[0].active=1;snow_peak_voice[4]=4000;
    snow_cost_onsets();assert(part_cost[4]==4000);
    uint32_t next=mix_split();assert(((next>>4)^(next>>5))&1u);
    uint32_t peak[2]={0,0},sum=0;
    for(uint32_t k=0;k<NPART;k++)sum+=part_cost[k];
    for(uint32_t k=0;k<NPART;k++){if((old>>k)&1u)peak[0]+=part_cost[k];if((next>>k)&1u)peak[1]+=part_cost[k];}
    peak[0]=peak[0]>sum-peak[0]?peak[0]:sum-peak[0];peak[1]=peak[1]>sum-peak[1]?peak[1]:sum-peak[1];assert(peak[1]<peak[0]);
    memset(trk[4].v,0,sizeof trk[4].v);
    puts("New low-note onset: predict work before worker dispatch; busy CH and new BS are split, peak estimated worker work falls before the first rendered block");
}
static void eight_undo_entries(void)
{
    seq_stop();launch_drop();undo_clear();upin.pin=upin.pin_next=0;track_t *t=&trk[0];t->p[P_E0]=0;
    for(uint32_t n=1;n<=20;n++){undo.keep=0;load_begin(t,UNDO_SOUND);t->p[P_E0]=(int16_t)n;load_end(t);}
    assert(undo_n==8);
    for(uint32_t n=20;n>12;n--){undo_step(0);assert(t->p[P_E0]==(int16_t)(n-1));}
    for(uint32_t n=13;n<=20;n++){undo_step(1);assert(t->p[P_E0]==(int16_t)n);}
    puts("Eight-entry undo ring preserved across wrap and the SRAM/pool boundary, with all eight undo/redo operations");
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);c1h_off=1;web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    exact_math();onset_prediction();queued_generators();bounded_chords();eight_undo_entries();puts("STATTIC REPAIR CONTRACTS PASSED; physical FM-1 deadline validation remains required");return 0;
}
