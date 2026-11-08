/* SPDX-License-Identifier: GPL-3.0-only */
/* Timings are accumulated per DMA half. Each part has exactly one writer:
 * its native SLOOP render core. The audio ISR publishes only after joining. */
#ifndef STATTIC_PROFILE_TICKS
#define STATTIC_PROFILE_TICKS() 0u
#endif
static struct {
    uint32_t seq,halves,current[NTRK][3],last[NTRK][3],max[NTRK][3];
    uint32_t bad[NTRK][3],bad_count,bad_wall,bad_nested,bad_events,bad_buses;
    uint32_t events_now,events_last,events_max,buses_now,buses_last,buses_max;
    /* (rasp fix 2) where a late half went: core 0's own units, core 1's job, the wait,
     * the split used (OR of the blocks' core-1 masks) and the event stages */
    uint32_t c0_now,split_now,ev_in_now,ev_rt_now,ev_seq_now,ev_in_last,ev_rt_last,ev_seq_last;
    uint32_t bad_c0,bad_c1,bad_wait,bad_split,bad_ev_in,bad_ev_rt,bad_ev_seq,c0_last,split_last;
} stattic_profile;
static void stattic_profile_begin(void)
{
    memset(stattic_profile.current,0,sizeof stattic_profile.current);
    stattic_profile.events_now=stattic_profile.buses_now=0;
    stattic_profile.c0_now=stattic_profile.split_now=0;
    stattic_profile.ev_in_now=stattic_profile.ev_rt_now=stattic_profile.ev_seq_now=0;
}
static void stattic_profile_end(void)
{
    stattic_profile.seq++;RING_PUBLISH();
    for(uint32_t k=0;k<NTRK;k++)for(uint32_t j=0;j<3;j++){
        uint32_t n=stattic_profile.current[k][j];stattic_profile.last[k][j]=n;
        if(n>stattic_profile.max[k][j])stattic_profile.max[k][j]=n;
    }
    stattic_profile.events_last=stattic_profile.events_now;stattic_profile.buses_last=stattic_profile.buses_now;
    stattic_profile.c0_last=stattic_profile.c0_now;stattic_profile.split_last=stattic_profile.split_now;
    stattic_profile.ev_in_last=stattic_profile.ev_in_now;stattic_profile.ev_rt_last=stattic_profile.ev_rt_now;
    stattic_profile.ev_seq_last=stattic_profile.ev_seq_now;
    if(stattic_profile.events_now>stattic_profile.events_max)stattic_profile.events_max=stattic_profile.events_now;
    if(stattic_profile.buses_now>stattic_profile.buses_max)stattic_profile.buses_max=stattic_profile.buses_now;
    stattic_profile.halves++;RING_PUBLISH();stattic_profile.seq++;
}
