/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME launches and mixer changes share the transport's bar, even when the
 * six tracks have different lengths, divisions and swing. */
#define SNOW_AUDITION 1
#include "snow_test.c"
static void move_samples(uint32_t n)
{
    while(n){uint32_t b=n>CTL?CTL:n;events_block(b);bank_poll();n-=b;}
}
static uint32_t before_bar(void)
{return (16u-(launch_steps&15u))*div_samples(2)-launch_pos-1u;}
static uint32_t note_of(uint32_t ch,uint32_t slot){return 36u+ch*5u+slot*12u;}
static void fixture(void)
{
    seq_stop();launch_drop();memset(&motion,0,sizeof motion);go_home();snow_home_panel=SN_HOME_MS;
    song.g[G_CLOCK]=0;song.g[G_SWING]=19;ui_launch=LAUNCH_NOW;
    static const uint8_t lengths[]={5,23,64,11,31,48},divisions[]={0,1,2,3,6,8};
    for(uint32_t ch=0;ch<NTRK;ch++){
        track_t *t=&trk[ch];t->p[SN_G_LIVE]=0;t->p[P_MUTE]=0;
        for(uint32_t slot=0;slot<3;slot++){
            assert(!snow_pattern_select(ch,slot));
            t->p[P_SLEN]=lengths[(ch+slot)%6];t->p[P_SDIV]=divisions[(ch+slot)%6];
            t->p[P_SSWING]=(int16_t)(ch*7u);t->p[P_DIR]=DIR_FWD;
            memset(t->step,0,sizeof t->step);
            for(uint32_t i=0;i<(uint32_t)t->p[P_SLEN];i++){
                t->step[i].time=ST_NOTE;t->step[i].n=1;t->step[i].vel=100;t->step[i].note[0]=(uint8_t)note_of(ch,slot);
            }
            assert(!bank_put(ch,slot,0));
        }
        assert(!ent_go(ch,0));
    }
    seq_start();events_block(1);move_samples(div_samples(2)*3u+71u);
    assert(launch_steps==3u);
}
static void next_bar(void){move_samples(before_bar());assert((launch_steps&15u)==15u);move_samples(1);assert(!(launch_steps&15u));}
static void launch_and_mix(void)
{
    for(uint32_t bpm=90;bpm<=180;bpm+=30){
        song.g[G_BPM]=(int16_t)bpm;fixture();
        for(uint32_t ch=0;ch<NTRK;ch++){
            assert(!snow_pattern_select(ch,1));assert(launch_q[ch].mode==ST_LAUNCH_BAR&&launch_q[ch].at==16);
        }
        layer_keys(1u<<2);layer_keys(1u<<13); /* White G3: mute SD; black F#4: solo CH. */
        assert(stattic_ms.mute==2&&stattic_ms.solo==32&&!perf_solo&&!trk[1].p[P_MUTE]);
        move_samples(before_bar());
        for(uint32_t ch=0;ch<NTRK;ch++)assert(bank_cur(ch)==0&&trk[ch].step[0].note[0]==note_of(ch,0));
        assert(!perf_solo&&!trk[1].p[P_MUTE]);
        move_samples(1);
        for(uint32_t ch=0;ch<NTRK;ch++)assert(bank_cur(ch)==1&&trk[ch].seq_idx==0&&trk[ch].step[0].note[0]==note_of(ch,1)&&bank_next(ch)==BK_NONE);
        assert(perf_solo==32&&trk[1].p[P_MUTE]&&!stattic_ms.mute&&!stattic_ms.solo&&ui_launch==LAUNCH_NOW);
        move_samples(div_samples(2)*3u+33u);layer_keys(1u<<2);layer_keys(1u<<13);
        snow_home_panel=SN_HOME_SYNTH;layer_keys(0); /* Leaving HOME 5 must not apply or discard the request. */
        move_samples(before_bar());assert(perf_solo==32&&trk[1].p[P_MUTE]);move_samples(1);
        assert(!perf_solo&&!trk[1].p[P_MUTE]);
    }
    puts("Shared BAR: six unequal LEN/DIV/SWING patterns start on their first step in the same audio block; mute/unmute and solo/unsolo obey that bar at 90..180 BPM");
}
static void cancellation_and_batches(void)
{
    fixture();assert(!snow_pattern_select(0,1));assert(!snow_pattern_select(0,2));assert(bank_next(0)==2);
    assert(!snow_pattern_select(0,0));assert(bank_next(0)==BK_NONE);
    layer_keys(1u<<0);layer_keys(1u<<0);layer_keys(1u<<1);layer_keys(1u<<1);
    assert(!stattic_ms.mute&&!stattic_ms.solo&&!perf_solo);next_bar();assert(bank_cur(0)==0&&!trk[0].p[P_MUTE]&&!perf_solo);
    assert(!snow_pattern_select(0,1));assert(!snow_pattern_select(0,2));next_bar();assert(bank_cur(0)==2&&trk[0].seq_idx==0);
    fixture();assert(!stattic_random_patterns());uint32_t picked[NTRK];
    for(uint32_t ch=0;ch<NTRK;ch++){picked[ch]=bank_next(ch);assert(picked[ch]>0&&picked[ch]<3&&launch_q[ch].at==16);}
    next_bar();for(uint32_t ch=0;ch<NTRK;ch++)assert(bank_cur(ch)==picked[ch]&&trk[ch].seq_idx==0);
    /* Main-loop decoding can straddle a bar; publish the ready group once. */
    fixture();uint32_t ready=0;assert(!stattic_pattern_prepare(0,1,&ready));next_bar();assert(bank_cur(0)==0);
    assert(!stattic_pattern_prepare(5,2,&ready));stattic_patterns_arm(ready);assert(launch_q[0].at==32&&launch_q[5].at==32);
    next_bar();assert(bank_cur(0)==1&&bank_cur(5)==2&&trk[0].seq_idx==0&&trk[5].seq_idx==0);
    fixture();static const uint8_t white[6]={0,2,4,6,7,9},black[6]={1,3,5,8,10,13};
    for(uint32_t k=0;k<NTRK;k++)layer_keys((1u<<white[k])|(1u<<black[k]));
    assert(stattic_ms.mute==63&&stattic_ms.solo==63&&!perf_solo);next_bar();assert(perf_solo==63);
    for(uint32_t k=0;k<NTRK;k++){assert(trk[k].p[P_MUTE]);layer_keys((1u<<white[k])|(1u<<black[k]));}
    next_bar();assert(!perf_solo);for(uint32_t k=0;k<NTRK;k++)assert(!trk[k].p[P_MUTE]);
    puts("Queued controls: replace/cancel, no page-exit side effects, RANDOM PATT atomic group and staging across a bar passed");
}
static void stop_and_project(void)
{
    fixture();assert(!snow_pattern_select(0,1));layer_keys(1u<<0);layer_keys(1u<<1);seq_stop();bank_poll();
    assert(bank_cur(0)==1&&trk[0].p[P_MUTE]&&perf_solo==1&&!stattic_ms.mute&&!stattic_ms.solo);
    layer_keys(1u<<0);layer_keys(1u<<1);assert(!trk[0].p[P_MUTE]&&!perf_solo);assert(!snow_pattern_select(0,2)&&bank_cur(0)==2);
    static project_t saved;project_capture(&saved);
    seq_start();events_block(1);layer_keys(1u<<0);layer_keys(1u<<1);assert(!snow_pattern_select(0,1));
    assert(!project_restore_runtime(&saved));assert(!stattic_ms.mute&&!stattic_ms.solo&&!stattic_solo&&!perf_solo&&bank_next(0)==BK_NONE&&!trk[0].p[P_MUTE]);
    seq_start();events_block(1);next_bar();assert(!trk[0].p[P_MUTE]&&!perf_solo&&bank_cur(0)==2);seq_stop();
    puts("STOP applies requests immediately; stopped controls are immediate; project restore drops stale requests and transient solo");
}
static void external_clock(void)
{
    fixture();seq_stop();bank_poll();song.g[G_BPM]=120;song.g[G_CLOCK]=1;events_block(1);
    midi_enqueue(0xFu|(0xFAu<<8),1u);uint32_t pulses=0,armed=0,landed=0;
    for(uint32_t b=0;b<FS*3u/128u;b++){
        while((uint64_t)b*128u*24u>=(uint64_t)pulses*22050u){midi_enqueue(0xFu|(0xF8u<<8),1u);pulses++;}
        uint32_t before=launch_steps;web_render(128);
        if(!armed&&launch_steps>=3){
            for(uint32_t ch=0;ch<NTRK;ch++)assert(!snow_pattern_select(ch,1));
            layer_keys(1u<<2);layer_keys(1u<<13);armed=1;
        }
        if(armed&&launch_steps<16){
            for(uint32_t ch=0;ch<NTRK;ch++)assert(web_snow_pattern_active(ch)==0);
            assert(!perf_solo&&!trk[1].p[P_MUTE]);
        }
        if(before<16&&launch_steps>=16){
            for(uint32_t ch=0;ch<NTRK;ch++)assert(web_snow_pattern_active(ch)==1&&trk[ch].seq_idx==0);
            assert(perf_solo==32&&trk[1].p[P_MUTE]);landed=1;
        }
    }
    assert(armed&&landed);seq_stop();song.g[G_CLOCK]=0;events_block(1);
    puts("External USB MIDI clock: six staged patterns and HOME mute/solo commit together on the master's next bar");
}
static void sixty_four(void)
{
    assert(TP[P_SLEN].def==64);for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[P_SLEN]==64);
    for(uint32_t ch=0;ch<NTRK;ch++){
        track_t *t=&trk[ch];t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=50;t->p[SN_G_LOCKS]=0;
        uint32_t tail_notes=0,repeats=0;
        for(uint32_t run=0;run<128;run++){
            static const uint8_t density[]={5,20,50,80,95};t->p[SN_G_DENS]=density[run%5];
            assert(!snow_generate(ch,1)&&t->p[P_SLEN]==64);
            uint32_t bar[4];for(uint32_t b=0;b<4;b++)bar[b]=(uint32_t)snow_hits(t->step+b*16u,16);
            assert(bar[0]&&bar[1]&&bar[2]&&bar[3]);
            assert(bar[1]!=bar[0]&&bar[1]!=bar[2]&&bar[3]!=bar[0]&&bar[3]!=bar[1]&&bar[3]!=bar[2]);
            assert(memcmp(t->step,t->step+32,32u*sizeof t->step[0]));
            repeats+=!memcmp(t->step,t->step+32,16u*sizeof t->step[0]);
            for(uint32_t i=32;i<64;i++)tail_notes+=t->step[i].time==ST_NOTE&&t->step[i].n;
        }
        assert(tail_notes>100&&repeats>5&&repeats<110);
        t->p[SN_G_DENS]=0;assert(!snow_generate(ch,1)&&!snow_hits(t->step,64));
        t->p[SN_G_DENS]=100;assert(!snow_generate(ch,1)&&snow_hits(t->step,64)==UINT64_MAX);
        t->p[P_SLEN]=16;
        printf("64-step role %u: %u notes in steps 33..64; varied bars 2/4, %u optional bar 1->3 repeats, no cloned halves; 0/100%% density preserved\n",ch,tail_notes,repeats);
    }
    unit_g_boot();for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[P_SLEN]==64);
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    sixty_four();launch_and_mix();cancellation_and_batches();external_clock();stop_and_project();
    puts("ALL STATTIC TRANSPORT / 64-STEP TESTS PASSED");return 0;
}
