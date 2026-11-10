/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_STATTIC 1
#define main native_hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static int32_t output[CTL*2];
static void block(void) { mix_block(output,CTL); }
static void init(void)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);memset(&drums,0,sizeof drums);
    memset(snow_l2,0,sizeof snow_l2);memset(snow_lfo_clock,0,sizeof snow_lfo_clock);
    for(uint32_t g=0;g<G_COUNT;g++)song.g[g]=GP[g].def;
    song.g[G_BPM]=120;song.master_q12=2048;song.sel=TRK_DRUM;
    const uint8_t engine[3]={4,6,5};
    for(uint32_t k=0;k<NTRK;k++){
        for(uint32_t id=0;id<P_COUNT;id++)trk[k].p[id]=TP[id].def;
        trk[k].p[P_SLEN]=64;steps_clear(&trk[k]);
        if(k<NPART){host_preset(&trk[k],engine[k],0);trk[k].p[SN_PLAY_MODE]=k==2?0:1;}
        else trk[k].p[P_E0]=DRUM_STATTIC;
    }
    clk_beat=clk_pos=0;panic_req=transport_req=0;stattic_runtime_init();
}
static void next_bar(void)
{
    uint32_t bar=clk_beat/4u;
    while(clk_beat/4u==bar)block();
    block();
}
static void note(step_t *s,uint32_t n) { memset(s,0,sizeof *s);s->time=ST_NOTE;s->n=1;s->note[0]=n;s->vel=100; }
static void quantized(void)
{
    init();seq_start();
    note(&stattic_banks[0][1].step[0],60);
    note(&stattic_banks[1][1].step[0],72);
    assert(!stattic_pattern_select(0,1));assert(!stattic_pattern_select(1,1));
    stattic_ms_key(0,1); /* first white key: DR mute */
    stattic_ms_key(3,1); /* second black key: BS solo */
    assert(!trk[0].step[0].n&&!trk[1].step[0].n&&!trk[3].p[P_MUTE]&&!song.solo);
    for(uint32_t i=0;i<20;i++)block();
    assert(!trk[0].step[0].n&&!trk[1].step[0].n&&!trk[3].p[P_MUTE]&&!song.solo);
    next_bar();
    assert(trk[0].step[0].note[0]==60&&trk[1].step[0].note[0]==72);
    assert(trk[0].seq_idx==0&&trk[1].seq_idx==0);
    assert(stattic_current[0]==1&&stattic_current[1]==1&&trk[3].p[P_MUTE]&&song.solo==1);
    puts("patterns/mute/solo: one bar, no early change, pattern step zero");
}
static void generated(void)
{
    init();seq_start();
    int16_t before[P_COUNT];memcpy(before,trk[0].p,sizeof before);
    assert(!stattic_prepare(0,0));assert(!stattic_prepare(0,1));assert(!stattic_prepare(0,2));assert(!stattic_prepare(0,3));
    assert(!memcmp(before,trk[0].p,sizeof before)&&!trk[0].step[0].n);
    next_bar();assert(!stattic_change[0].armed);
    uint32_t notes=0;for(uint32_t i=0;i<64;i++)notes+=stattic_step_on(&trk[0].step[i]);assert(notes);
    assert(stattic_undo(0));next_bar();assert(!memcmp(before,trk[0].p,sizeof before));
    assert(stattic_undo(1));next_bar();
    notes=0;for(uint32_t i=0;i<64;i++)notes+=stattic_step_on(&trk[0].step[i]);assert(notes);
    puts("all four generators: prepared off the audio path, bar commit, undo/redo");
}
static void drums_catalog(void)
{
    init();uint8_t old[16];memcpy(old,stattic_drum_pick,sizeof old);
    assert(!stattic_prepare(TRK_DRUM,1));assert(!memcmp(old,stattic_drum_pick,sizeof old));
    stattic_runtime_tick();
    for(uint32_t i=0;i<16;i++)assert(stattic_pick_ok(stattic_drum_pick[i],ST_LANE_CATEGORY[i])&&!stattic_pick_user(stattic_drum_pick[i]));
    assert(memcmp(old,stattic_drum_pick,sizeof old));
    uint32_t r=123456;
    for(uint32_t attempt=0;attempt<500;attempt++){
        uint8_t was[16];memcpy(was,stattic_drum_pick,sizeof was);
        stattic_drum_random(stattic_drum_pick,&r);
        for(uint32_t i=0;i<16;i++){
            assert(stattic_pick_ok(stattic_drum_pick[i],ST_LANE_CATEGORY[i]));
            assert(stattic_drum_pick[i]!=was[i]);                 /* (every turn a new sound) */
        }
    }
    /* a seed makes the same sound every time, of its lane's category, in range */
    for(uint32_t lane=0;lane<16;lane++)for(uint32_t pick=0;pick<256;pick+=7){
        if(stattic_pick_user(pick))continue;
        dsnd_t a,b;uint32_t ca=stattic_micro_make(&a,lane,pick),cb=stattic_micro_make(&b,lane,pick);
        assert(ca==cb&&!memcmp(&a,&b,sizeof a));
        dsnd_t f=a;dsu_fix_sound(&f);assert(!memcmp(&a,&f,sizeof a));
    }
    stattic_drum_pick[0]=127;drum_on(36,100);              /* a missing user shot: the synthesised sound */
    assert(drums.v[0].active&&drums.synth[0]);
    puts("DR: micro drum seeds: every turn new, reproducible, in range; missing user shot falls back to synth");
}
static void phrases(void)
{
    init();
    for(uint32_t k=0;k<NTRK;k++)for(uint32_t seed=1;seed<=24;seed++){
        trk[k].p[SN_G_SEED]=(int16_t)seed;assert(!stattic_prepare(k,0));stattic_runtime_tick();
        uint64_t on=0;for(uint32_t i=0;i<64;i++)if(k==TRK_DRUM?dstep_mask(&trk[k].dstep[i]):stattic_step_on(&trk[k].step[i]))on|=UINT64_C(1)<<i;
        assert(on&&on>>32u);assert(memcmp(trk[k].step,trk[k].step+32,32u*sizeof(step_t)));
        if(k==1u)for(uint32_t i=0;i<64;i++)assert(trk[k].step[i].n<=1);
    }
    puts("64 steps: four varied bars, no duplicated 32-step half; SY plays notes");
}
#include "fixtures/stattic-scales.h"
static void scales(void)
{
    init();assert(NSCALES==35u&&NELEM(N_SCALE)==35u);
    for(uint32_t scale=0;scale<35;scale++){
        assert(!strcmp(N_SCALE[scale],expected_scale_names[scale]));assert(SCALE_MASK[scale]==expected_scale_masks[scale]);
        for(uint32_t root=0;root<12;root++){
            track_t *t=&trk[1];t->p[P_SCALE]=(int16_t)scale;t->p[P_ROOT]=(int16_t)root;t->p[P_QUANT]=1;t->p[P_CHORD]=0;
            for(uint32_t key=0;key<27;key++){
                uint32_t n=kb_pitch(t,key,0);assert(n<128&&((scale_mask(t)>>((n+120u-root)%12u))&1u));
            }
            for(uint32_t n=24;n<108;n++){
                uint32_t pitch=stattic_midi_pitch(t,n);assert((scale_mask(t)>>((pitch+120u-root)%12u))&1u);
            }
            t->p[SN_G_SEED]=(int16_t)(scale+root+1);assert(!stattic_prepare(1,0));stattic_runtime_tick();
            for(uint32_t i=0;i<64;i++)if(stattic_step_on(&t->step[i]))for(uint32_t j=0;j<t->step[i].n;j++)
                assert((scale_mask(t)>>((t->step[i].note[j]+120u-root)%12u))&1u);
            t=&trk[2];t->p[P_ROOT]=(int16_t)root;t->p[P_SCALE]=(int16_t)scale;t->p[SN_CHORD]=1;t->p[P_VOICE]=V_POLY;
            uint8_t chord[4];int32_t r;uint16_t mask;
            uint32_t count=chord_make(t,60+root,chord,&r,&mask);assert(count==3);
            for(uint32_t j=0;j<count;j++)assert((scale_mask(t)>>((chord[j]+120u-root)%12u))&1u);
        }
    }
    puts("all 35 STATTIC scales: frozen names/intervals, 12 roots, keys, MIDI, generator and diatonic chords");
}
static void shuffled_art(void)
{
    init();uint32_t prior=255,seen=0;
    for(uint32_t gesture=0;gesture<300;gesture++){
        stattic_home_page=(uint8_t)(gesture/4u%3u);uint32_t knob=gesture%4u;
        stattic_turn(knob,1);uint32_t image=stattic_art_pick[stattic_home_page];
        assert(image<ST_ART_COUNT&&image!=prior);prior=image;
        if(gesture%ST_ART_COUNT==0)seen=0;
        assert(!(seen&(1u<<image)));seen|=1u<<image;
        stattic_scroll_push[stattic_home_page]=0;uint32_t old=stattic_scroll[stattic_home_page];
        stattic_turn(knob,-2);assert(stattic_art_pick[stattic_home_page]==image);
        assert(stattic_scroll[stattic_home_page]==old&&stattic_scroll_push[stattic_home_page]==12u);   /* (a push, played out by HOME) */
        assert(gesture%ST_ART_COUNT!=ST_ART_COUNT-1u||seen==(1u<<ST_ART_COUNT)-1u);
    }
    puts("HOME 1-3 art: shuffled bag of all pictures, no repeats, stable during one knob gesture, upward seamless wrap");
}
#ifndef STATTIC_RUNTIME_HARNESS_ONLY
static int part_in_key(const track_t *t)
{
    uint32_t m=SCALE_MASK[t->p[P_SCALE]];
    for(uint32_t i=0;i<NSTEP;i++)for(uint32_t j=0;j<t->step[i].n;j++)
        if(!((m>>((t->step[i].note[j]-t->p[P_ROOT]+120)%12))&1u))return 0;
    return 1;
}
/* (2026-11-10) one key for the song: parts saved apart meet in the bass's key; a key change moves every note by its
 * degree; a pattern made before the change arrives in the new key */
static void song_key(void)
{
    init();
    for(uint32_t k=0;k<NPART;k++){trk[k].p[P_ROOT]=0;trk[k].p[P_SCALE]=k==1u?1:2;}
    trk[0].step[0]=(step_t){.note={63},.n=1,.time=ST_NOTE,.vel=100};
    trk[1].step[0]=(step_t){.note={64},.n=1,.time=ST_NOTE,.vel=100};
    stattic_banks[1][1].step[0]=(step_t){.note={69},.n=1,.time=ST_NOTE,.vel=100};
    stattic_key_unify();
    for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[P_SCALE]==2&&trk[k].p[P_ROOT]==0);
    assert(trk[1].step[0].note[0]==63&&stattic_banks[1][1].step[0].note[0]==68&&trk[0].step[0].note[0]==63);
    stattic_key_set(0,1);
    assert(trk[0].step[0].note[0]==64&&trk[1].step[0].note[0]==64&&stattic_banks[1][1].step[0].note[0]==69);
    stattic_key_set(2,1);                                   /* D major: up a tone */
    assert(trk[0].step[0].note[0]==66&&trk[2].p[P_ROOT]==2);
    stattic_key_set(11,1);                                  /* B major: the shorter way, down */
    assert(trk[0].step[0].note[0]==63);
    stattic_key_set(0,2);
    song.playing=1;
    for(uint32_t k=0;k<NPART;k++)assert(!stattic_prepare(k,0));
    stattic_key_set(0,1);                                   /* the patterns waiting were written in C minor */
    for(int b=0;b<3;b++)next_bar();
    for(uint32_t k=0;k<NPART;k++){assert(!stattic_change[k].armed);assert(trk[k].p[P_SCALE]==1);assert(part_in_key(&trk[k]));}
    for(uint32_t k=0;k<NPART;k++)assert(!stattic_prepare(k,2));      /* a new LFO set never brings a part's own key */
    for(int b=0;b<3;b++)next_bar();
    for(uint32_t k=0;k<NPART;k++)assert(trk[k].p[P_SCALE]==1&&trk[k].p[P_ROOT]==0);
    song.playing=0;
    puts("stattic song key: one key, notes moved by degree, waiting patterns follow");
}
static void sounding_pcs(const step_t *s,uint16_t *pcs)
{
    int last=-1;for(int i=0;i<64;i++){if(s[i].time==ST_NOTE&&s[i].n)last=i;else if(s[i].time!=ST_TIE)last=-1;
        pcs[i]=0;if(last>=0)for(int j=0;j<s[last].n;j++)pcs[i]|=(uint16_t)(1u<<(s[last].note[j]%12));}
}
static int semitone_rub(uint16_t a,uint16_t b)
{
    for(int x=0;x<12;x++)if((a>>x)&1)for(int y=0;y<12;y++)if((b>>y)&1){int d=(x-y+12)%12;if(d==1||d==11)return 1;}
    return 0;
}
/* (2026-11-10, the owner: "variety without end, but harmony") AMBIENT, every part at once, many times: the parts
 * share each bar's chord (bass and chords hardly ever a semitone apart), and no two phrases are the same */
static void ambient_harmony(void)
{
    init();stattic_key_set(0,1);trk[0].p[SN_G_SCOPE]=1;
    enum{G=60};static uint32_t h[3][G];long rub=0,sy=0,steps=0;
    for(int g=0;g<G;g++){
        stattic_prepare_scope(0,0);for(int i=0;i<4;i++)block();
        for(uint32_t k=0;k<NPART;k++){assert(!stattic_change[k].armed);assert(part_in_key(&trk[k]));}
        uint16_t b[64],c[64],y[64];sounding_pcs(trk[0].step,b);sounding_pcs(trk[2].step,c);sounding_pcs(trk[1].step,y);
        for(int i=0;i<64;i++){steps++;rub+=semitone_rub(b[i],c[i]);sy+=semitone_rub(y[i],b[i]|c[i]);}
        for(uint32_t k=0;k<NPART;k++){uint32_t x=2166136261u;for(int i=0;i<64;i++){x=(x^trk[k].step[i].time)*16777619u;for(int j=0;j<trk[k].step[i].n;j++)x=(x^trk[k].step[i].note[j])*16777619u;}h[k][g]=x;}
    }
    assert(rub*100<steps*5&&sy*100<steps*5);
    for(uint32_t k=0;k<NPART;k++){int same=0;for(int i=0;i<G;i++)for(int j=0;j<i;j++)same+=h[k][i]==h[k][j];assert(same<=2);}
    printf("stattic AMBIENT: parts on one chord a bar (semitone rubs %.1f%% / %.1f%%), %d phrases each, all different\n",100.0*rub/steps,100.0*sy/steps,G);
}
/* (2026-11-10, the owner) a K2 sound gliding in (MORPH): SY's short note is held on through the glide,
 * not cut before the next note, and ends when the glide is over */
static uint64_t morph_samples;
static void tick_ms(void){morph_samples+=CTL;fm1_ms=(uint32_t)(morph_samples*1000u/FS);}
static void morph_holds_notes(void)
{
    morph_samples=(uint64_t)fm1_ms*FS/1000u;
    init();stattic_key_set(0,1);song.g[G_BPM]=120;
    track_t *t=&trk[1];
    for(uint32_t i=0;i<NSTEP;i++){memset(&t->step[i],0,sizeof t->step[i]);t->step[i].time=ST_REST;}
    for(uint32_t i=0;i<NSTEP;i+=16u)t->step[i]=(step_t){.note={60},.n=1,.time=ST_NOTE,.vel=100};   /* (a short note a bar) */
    t->p[P_SLEN]=64;t->p[SN_MORPH_TIME]=4;t->p[SN_MORPH_START]=0;          /* (a 4 s glide on the 1/16 grid) */
    song.playing=1;
    uint32_t held_off=0,held_during=0,released_after=0;
    for(uint32_t n=0;n<(uint32_t)FS*2u;n+=CTL){block();tick_ms();}       /* (a bar: the note ends by itself) */
    held_off=t->seq_n==0;
    assert(!stattic_prepare(1,1));
    for(uint32_t n=0;n<(uint32_t)FS*3u;n+=CTL){block();tick_ms();if(stattic_morph_live[1]&&t->seq_n)held_during++;}
    for(uint32_t n=0;n<(uint32_t)FS*4u;n+=CTL){block();tick_ms();}
    released_after=!stattic_morph_live[1];
    song.playing=0;
    uint32_t gaps=0;for(uint32_t n=0;n<(uint32_t)FS*4u;n+=CTL){block();tick_ms();gaps+=t->seq_n==0;}
    assert(held_off);assert(held_during>200u);assert(released_after);assert(gaps>100u);
    printf("stattic MORPH: a short note held through the glide (%u blocks), released after\n",held_during);
}
/* (2026-11-10, the owner: a rasp on the kick) the kick never ratchets: its fastest figure is a 16th apart */
static void kick_no_ratchet(void)
{
    init();
    for(uint32_t g=0;g<300u;g++){
        trk[TRK_DRUM].p[SN_G_COMPLEX]=(int16_t)(g%101u);trk[TRK_DRUM].p[SN_G_SEED]=(int16_t)(g&127u);
        assert(!stattic_prepare(TRK_DRUM,g&1u?4u:0u));for(int i=0;i<3;i++)block();
        for(uint32_t s=0;s<NSTEP;s++)for(uint32_t l=0;l<16u;l++)
            if(ST_LANE_CATEGORY[l]==0u&&dstep_has(&trk[TRK_DRUM].dstep[s],l))assert(dstep_rat(&trk[TRK_DRUM].dstep[s],l)==0u);
            else if(dstep_has(&trk[TRK_DRUM].dstep[s],l))assert(dstep_rat(&trk[TRK_DRUM].dstep[s],l)<=1u);
    }
    puts("stattic DR: 300 kits at every COMPLEX, no kick ratchet (16ths and eighth triplets only), the others x2 at most");
}
int main(void)
{
    quantized();generated();drums_catalog();phrases();scales();shuffled_art();song_key();ambient_harmony();morph_holds_notes();kick_no_ratchet();return 0;
}

#endif
