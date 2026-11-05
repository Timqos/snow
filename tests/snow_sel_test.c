/* SPDX-License-Identifier: GPL-3.0-only */
/* Real keyboard/MIDI/recording/persistence and tempo envelope paths. */
#define SNOW_AUDITION 1
#include "snow_test.c"
static uint32_t gated(const track_t *t)
{
    uint32_t n=0;for(uint32_t i=0;i<NVOICE;i++)n+=t->v[i].active&&t->v[i].gate;return n;
}
static void keys(uint32_t mask){web_keys(mask);keyboard_block();}
static void midi_note(uint32_t note,uint32_t velocity)
{
    midi_enqueue(0x9u|(0x95u<<8)|(note<<16)|(velocity<<24),1);events_block(CTL);
}
static void scales(void)
{
    /* Independently written interval sets, not the production table macro. */
    static const char *const intervals[]={
        "0123456789AB","024579B","023578A","023579A","024579A","02479","0357A","023578B",
        "013578A","024679B","013568A","023579B","03567A","02468A","0134679A","0235689B",
        "0134568A","014578B","023679A","02378","0157A","0156A","02379","013468A",
        "024689B","024679A","024578B","0234678AB","0125678B","01567B","024568AB",
        "012356789B","023678B","023578A","014578A"};
    assert(NELEM(intervals)==NELEM(SCALE_MASK)&&TP[P_SCALE].max==34);
    track_t *t=&trk[5];
    for(uint32_t s=0;s<NELEM(intervals);s++){
        uint32_t expected=0;for(const char *p=intervals[s];*p;p++)expected|=1u<<(*p<='9'?*p-'0':*p-'A'+10);
        t->p[P_SCALE]=s;assert(scale_mask(t)==expected);
        for(uint32_t root=0;root<12;root++){
            t->p[P_ROOT]=root;t->p[P_QUANT]=1;
            for(uint32_t key=0;key<27;key++){
                uint32_t n=kb_map(t,key);assert(n<=127&&((expected>>((n+120-root)%12))&1u));
            }
            t->p[P_QUANT]=2;
            for(uint32_t key=0;key<27;key++)if(!key_black(key)){
                uint32_t n=kb_map(t,key);assert(n<=127&&((expected>>((n+120-root)%12))&1u));
            }
        }
        t->p[P_ROOT]=0;t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=100;
        for(uint32_t mode=0;mode<2;mode++){
            t->p[SN_PLAY_MODE]=mode;assert(!snow_generate(5,1));
            for(uint32_t i=0;i<(uint32_t)t->p[P_SLEN];i++){
                assert(t->step[i].n==(mode?1u:3u));
                for(uint32_t j=0;j<t->step[i].n;j++)assert((expected>>(t->step[i].note[j]%12))&1u);
            }
        }
    }
    puts("SEL: 35 pitch sets, all screenshot scales, every root, SNAP/WHITE and mode-aware generation passed");
}
static void input_modes(void)
{
    track_t *t=&trk[5];sel_set(5);seq_stop();panic_req=63;events_block(CTL);
    t->p[P_QUANT]=0;t->p[P_ROOT]=0;t->p[P_SCALE]=1;t->p[P_TRANS]=0;t->p[P_AMODE]=0;
    t->p[P_VOICE]=V_POLY;t->p[P_CHRD]=CH_MAJ;t->p[P_VOIC]=VC_CLOSE;t->p[SN_PLAY_MODE]=0;
    keys(1u<<7);assert(gated(t)==3&&kb_chn[7]==3);
    t->p[SN_PLAY_MODE]=1;keys(0);assert(!gated(t)); /* release the old chord after toggling */
    keys((1u<<7)|(1u<<11)|(1u<<14));assert(gated(t)==3&&kb_chn[7]==1&&kb_chn[11]==1&&kb_chn[14]==1);
    keys(0);assert(!gated(t));
    /* Manual step entry and live input both store exactly C E G. */
    ui.cursor=0;ui.entry_open=0;seq_entry((1u<<7)|(1u<<11)|(1u<<14));
    assert(t->step[0].n==3&&t->step[0].note[0]==60&&t->step[0].note[1]==64&&t->step[0].note[2]==67);
    ui.entry_open=0;song.rec=1u<<5;song.playing=1;t->seq_idx=4;t->seq_pos=0;
    t->step[4]=(step_t){.time=ST_REST};keys((1u<<7)|(1u<<11)|(1u<<14));
    assert(t->step[4].n==3&&t->step[4].note[0]==60&&t->step[4].note[1]==64&&t->step[4].note[2]==67);
    keys(0);song.rec=song.playing=0;
    song.rec=1u<<5;song.playing=1;t->seq_idx=6;t->seq_pos=0;t->step[6]=(step_t){.time=ST_REST};
    midi_note(60,100);midi_note(64,100);midi_note(67,100);assert(gated(t)==3);
    assert(t->step[6].n==3&&t->step[6].note[0]==60&&t->step[6].note[1]==64&&t->step[6].note[2]==67);
    t->p[SN_PLAY_MODE]=0;midi_note(60,0);midi_note(64,0);midi_note(67,0);assert(!gated(t));
    song.rec=song.playing=0;
    midi_note(60,100);assert(gated(t)==3);t->p[SN_PLAY_MODE]=1;midi_note(60,0);assert(!gated(t));
    t->p[P_AMODE]=AM_UP;keys((1u<<7)|(1u<<11)|(1u<<14));assert(t->nheld==3);
    t->p[SN_PLAY_MODE]=0;keys(0);assert(!t->nheld);keys(1u<<7);assert(t->nheld==3);keys(0);t->p[P_AMODE]=0;
    /* Sound browsing/randomisation never changes the chosen input mode. */
    t->p[SN_PLAY_MODE]=1;for(uint32_t p=0;p<10;p++){apply_preset_to(t,p);assert(t->p[SN_PLAY_MODE]==1);}
    step_t old[NSTEP];memcpy(old,t->step,sizeof old);
    ui.home=0;ui.page=page_first(FAM_SCL);ui.force=1;advance(100);
    assert(cur_page()->id[0]==SN_PLAY_MODE&&cur_page()->id[2]==P_SCALE);
    web_enc(EN_K1,-1);advance(80);assert(!t->p[SN_PLAY_MODE]);
    web_enc(EN_K1,1);advance(80);assert(t->p[SN_PLAY_MODE]==1&&!memcmp(old,t->step,sizeof old));
    t->p[P_SCALE]=31;assert(!project_save(6));t->p[P_SCALE]=0;t->p[SN_PLAY_MODE]=0;
    project_load(6);assert(t->p[P_SCALE]==31&&t->p[SN_PLAY_MODE]==1);
    for(uint32_t f=0;f<3;f++)assert(!snow_random_section(5,f==0?FAM_EDIT:f==1?FAM_LFO:FAM_FX)&&t->p[SN_PLAY_MODE]==1);
    assert(!motion_param(SN_PLAY_MODE));
    puts("NOTES/CHORDS: keys, MIDI, held-note release, step/live custom chords, arp, physical SEL knob and project round-trip passed");
}
static void release_time(void)
{
    track_t *t=&trk[5];sel_set(5);
    for(uint32_t preset=0;preset<10;preset++){
        apply_preset_to(t,preset);assert(t->p[P_REL]==SN_CH_RELEASE);
        for(uint32_t n=0;n<50;n++){
            assert(!snow_random_sound(5)&&t->p[P_REL]==SN_CH_RELEASE);
            assert(!snow_random_section(5,FAM_LFO)&&t->p[P_REL]==SN_CH_RELEASE);
        }
    }
    assert(esync_idx(t->p[P_REL])==22);
    char val[16];const char *unit;param_format(track_desc(t,P_REL),t->p[P_REL],val,&unit);assert(!strcmp(val,"2BAR"));
    for(uint32_t bpm=90;bpm<=180;bpm+=30)for(uint32_t sync=0;sync<2;sync++){
        song.g[G_BPM]=bpm;song.g[G_CLOCK]=0;t->p[P_ESYNC]=sync;esync_coef(t);
        uint32_t duration=beat_samples()*8u;voice_t v={.active=1,.gate=0,.vel=100};
        v.s[0]=1<<24;v.s[1]=1;t->p[SN_MODE]=1;
        for(uint32_t n=0;n<duration;n+=CTL)snow_amp(t,&v,0);
        assert(v.s[0]>(1<<24)/150&&v.s[0]<(1<<24)/65); /* ~99% released after two bars */
    }
    t->p[P_REL]=103;for(uint32_t i=0;i<20;i++)assert(!snow_random_sound(5)&&t->p[P_REL]==103);
    t->p[P_REL]=SN_CH_RELEASE;song.g[G_CLOCK]=1;midi_beat_samples=22050;esync_coef(t);
    assert(t->esync_k[2]==esync_one(22050,SN_CH_RELEASE,2));song.g[G_CLOCK]=0;midi_beat_samples=0;
    song.g[G_BPM]=120;
    puts("CH ENV: all ten presets default to actual tempo-synced 2BAR, EDIT/LFO keep release, 90..180 BPM timing passed");
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    assert(!trk[5].p[SN_PLAY_MODE]&&trk[5].p[P_REL]==SN_CH_RELEASE);
    for(uint32_t i=0;i<5;i++)assert(trk[i].p[SN_PLAY_MODE]==1);
    scales();input_modes();release_time();puts("ALL SNOW SEL / CH RELEASE TESTS PASSED");return 0;
}
