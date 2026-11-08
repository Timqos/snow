/* SPDX-License-Identifier: GPL-3.0-only */
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
int main(void)
{
    init();seq_start();stattic_prepare(0,1);int16_t sound[P_COUNT];memcpy(sound,stattic_change[0].value.p,sizeof sound);
    assert(!stattic_pattern_select(0,1));assert(stattic_change[0].what&ST_CHANGE_SOUND);assert(!memcmp(sound,stattic_change[0].value.p,sizeof sound));
    song.sel=0;stattic_clear(0);assert(stattic_change[0].what&ST_CHANGE_SOUND);next_bar();
    for(uint32_t id=0;id<P_COUNT;id++)if(stattic_change[0].mask[id/32u]>>(id%32u)&1u)assert(trk[0].p[id]==sound[id]);
    init();seq_start();stattic_staging=15u;stattic_prepare(0,0);clk_beat=4;stattic_runtime_tick();assert(stattic_change[0].armed&&!trk[0].step[0].n);
    for(uint32_t k=1;k<NTRK;k++)stattic_prepare(k,0);stattic_group_end(15u);
    for(uint32_t k=0;k<NTRK;k++)assert(stattic_change[k].armed&&stattic_change[k].bar==2);
    clk_beat=8;stattic_runtime_tick();for(uint32_t k=0;k<NTRK;k++)assert(!stattic_change[k].armed);
    init();for(uint32_t i=0;i<11;i++){stattic_undo_mark(&trk[0],100+i);trk[0].p[P_PAN]=(int16_t)(i+1);}
    assert(stattic_history_n==8);
    for(int i=10;i>=3;i--){assert(stattic_undo(0));stattic_runtime_tick();assert(trk[0].p[P_PAN]==i);}assert(!stattic_undo(0));
    for(int i=4;i<=11;i++){assert(stattic_undo(1));stattic_runtime_tick();assert(trk[0].p[P_PAN]==i);}
    init();track_t *t=&trk[1];t->p[P_M1SRC]=MS_RAND;t->p[P_M1DST]=MD_TIME;t->p[P_M1AMT]=63;
    uint32_t delay=micro_units(t,0,BEAT_U/4u);for(uint32_t i=0;i<20;i++)assert((uint32_t)micro_units(t,0,BEAT_U/4u)==delay);
    t->p[P_DIR]=1;t->micro[63]=-12;assert(micro_units(t,0,BEAT_U/4u)<(int32_t)(BEAT_U/4u*7/16u));
    init();trk[2].p[P_VOICE]=V_POLY;trk[2].p[P_CHORD]=1;stattic_midi_on(&trk[2],2,60,100);assert(stattic_midi_held[23].n>=3);
    trk[2].p[P_SCALE]=34;trk[2].p[SN_CHORD]=8;stattic_midi_off(&trk[2],2,60);
    for(uint32_t i=0;i<NVOICE;i++)assert(!trk[2].v[i].gate);
    puts("state: pending sound survives pattern/clear; ALL stages across a bar; 8-entry undo/redo; stable TIME modulation; MIDI chord ownership");return 0;
}
