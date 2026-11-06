/* SPDX-License-Identifier: GPL-3.0-only */
/* Physical panel routing, independent generator transactions and channel DSP. */
#define SNOW_AUDITION 1
#include "snow_test.c"
static void turn(uint32_t role,int32_t steps){web_enc(role,steps);advance(100);}
static void focus(uint32_t panel){turn(EN_SELECT,-10);turn(EN_SELECT,(int32_t)panel);assert(snow_home_panel==panel);}
static void controls(void)
{
    go_home();sel_set(0);int16_t bpm=song.g[G_BPM];
    for(int32_t v=-1;v<=1;v++){
        trk[0].p[SN_DJ_FILTER]=(int16_t)v;assert(snow_home_percent(&trk[0],SN_DJ_FILTER)==(uint32_t)(50+v));
        char text[8];const char *unit;param_format(&TP[SN_WIDTH],v,text,&unit);assert(atoi(text)==50+v);
    }
    trk[0].p[SN_DJ_FILTER]=0;
    for(uint32_t lock=0;lock<2;lock++){
        ui_prefs=lock?ui_prefs|PREF_BPM_LOCK:ui_prefs&~PREF_BPM_LOCK;
        for(uint32_t p=0;p<SN_HOME_COUNT;p++)focus(p);focus(SN_HOME_SYNTH);assert(song.g[G_BPM]==bpm);
    }
    /* GLO's existing unlocked-tempo behaviour is not a held-button combo. */
    ui_prefs&=~PREF_BPM_LOCK;tap(B_GLO);assert(!ui.home&&!fm1_in.buttons);
    turn(EN_SELECT,1);assert(song.g[G_BPM]==bpm+1);go_home();song.g[G_BPM]=bpm;
    for(uint32_t ch=0;ch<NTRK;ch++){
        sel_set(ch);focus(SN_HOME_SYNTH);int16_t old[P_COUNT];memcpy(old,trk[ch].p,sizeof old);
        turn(EN_K2,1);assert(trk[ch].p[P_E1]!=old[P_E1]&&snow_touch_state[ch].macro==1);
        for(uint32_t i=0;i<P_COUNT;i++)if(i!=P_E1)assert(trk[ch].p[i]==old[i]);
        focus(SN_HOME_MIX);static const uint8_t ids[]={P_LEVEL,P_PAN,SN_DJ_FILTER,SN_WIDTH};
        for(uint32_t k=0;k<4;k++){
            memcpy(old,trk[ch].p,sizeof old);turn(EN_K1+k,1);assert(trk[ch].p[ids[k]]>old[ids[k]]);
            for(uint32_t i=0;i<P_COUNT;i++)if(i!=ids[k])assert(trk[ch].p[i]==old[i]);
        }
        assert(snow_home_panel==SN_HOME_MIX);
        turn(EN_ALGO,ch<5?1:-1);assert(snow_home_panel==SN_HOME_MIX);sel_set(ch);
    }
    sel_set(5);focus(SN_HOME_GEN);uint8_t panel=snow_home_panel;
    int8_t octave=song.octave;
    web_buttons(1u<<B_HOME);advance(850);web_buttons((1u<<B_HOME)|(1u<<B_OCTUP));advance(400);assert(song.octave==octave);web_buttons(1u<<B_HOME);advance(60);web_buttons(0);advance(100);
    assert(trk[5].p[SN_PLAY_MODE]==1&&snow_home_panel==panel&&!ui.menu&&!ui.shift);
    web_buttons(1u<<B_HOME);advance(60);web_buttons((1u<<B_HOME)|(1u<<B_OCTUP));advance(60);web_buttons(1u<<B_HOME);advance(60);web_buttons(0);advance(100);
    assert(!trk[5].p[SN_PLAY_MODE]&&snow_home_panel==panel&&!ui.menu&&song.octave==octave);
    focus(SN_HOME_SYNTH);web_buttons(1u<<B_OCTUP);advance(60);assert(song.octave==octave+1);web_buttons(0);advance(100);assert(!trk[5].p[SN_PLAY_MODE]);tap(B_OCTDN);
    web_buttons(1u<<B_HOME);advance(850);web_buttons(0);advance(100);assert(ui.menu);ui.menu=0;go_home();
    for(uint32_t i=0;i<5;i++)assert(trk[i].p[SN_PLAY_MODE]==1);
    assert(!project_save(7));trk[5].p[SN_DJ_FILTER]=0;trk[5].p[SN_WIDTH]=0;project_load(7);
    assert(trk[5].p[SN_DJ_FILTER]&&trk[5].p[SN_WIDTH]);
    puts("HOME controls: five pages, no BPM change, unheld GLO tempo, four macros/mix knobs, channel focus, N/CHO shortcut and persisted mix passed");
}
static void generators(void)
{
    seq_stop();go_home();focus(SN_HOME_GEN);memset(&motion,0,sizeof motion);memset(snow_offset,0,sizeof snow_offset);
    for(uint32_t ch=0;ch<NTRK;ch++){
        sel_set(ch);track_t *t=&trk[ch];t->p[SN_G_DENS]=50;t->p[SN_G_LOCKS]=80;
        step_t before[NTRK][NSTEP];int16_t params[NTRK][P_COUNT];
        for(uint32_t k=0;k<NTRK;k++){memcpy(before[k],trk[k].step,sizeof before[k]);memcpy(params[k],trk[k].p,sizeof params[k]);}
        uint32_t count=snow_home_gen[ch][1];turn(EN_K2,3);assert(snow_home_gen[ch][1]==count+3);
        assert(memcmp(params[ch],t->p,sizeof params[ch]));for(uint32_t k=0;k<NTRK;k++){
            assert(!memcmp(before[k],trk[k].step,sizeof before[k]));if(k!=ch)assert(!memcmp(params[k],trk[k].p,sizeof params[k]));
        }
        assert(t->p[SN_DJ_FILTER]==params[ch][SN_DJ_FILTER]&&t->p[SN_WIDTH]==params[ch][SN_WIDTH]);
        if(ch==5)assert(t->p[P_REL]==SN_CH_RELEASE);
        motion_store_t locks=motion;int16_t settings[P_COUNT];memcpy(settings,t->p,sizeof settings);
        uint32_t seed=snow_seed(t);step_t steps[NSTEP];memcpy(steps,t->step,sizeof steps);
        count=snow_home_gen[ch][0];turn(EN_K1,-1);assert(snow_home_gen[ch][0]==count+1&&snow_seed(t)!=seed);
        uint32_t advanced=snow_seed(t);step_t rejected[NSTEP];memcpy(rejected,t->step,sizeof rejected);
        assert(memcmp(steps,t->step,sizeof steps));uint32_t generated=0;
        for(uint32_t e=0;e<motion.count;e++)generated+=motion.event[e].generated&&(motion.event[e].place>>6)==ch;assert(generated);
        /* One undo restores music; the original random stream keeps advancing. */
        web_buttons(1u<<B_SAVE);advance(850);web_buttons(0);advance(100);
        assert(!memcmp(steps,t->step,sizeof steps)&&snow_seed(t)==advanced);
        for(uint32_t id=0;id<P_COUNT;id++)if(id!=SN_G_SEED&&id!=SN_G_SEED_B&&id!=SN_G_SEED_C&&id!=SN_G_SEED_D)assert(settings[id]==t->p[id]);
        assert(motion.count==locks.count);for(uint32_t e=0;e<locks.count;e++)assert(!memcmp(&motion.event[e],&locks.event[e],sizeof locks.event[e]));ui.undo_w=0;
        turn(EN_K1,1);assert(memcmp(rejected,t->step,sizeof rejected)&&snow_seed(t)!=advanced);
        for(uint32_t knob=2;knob<4;knob++){
            memcpy(steps,t->step,sizeof steps);int16_t p[P_COUNT];memcpy(p,t->p,sizeof p);count=snow_home_gen[ch][knob];
            turn(EN_K1+knob,-2);assert(snow_home_gen[ch][knob]==count+2&&memcmp(p,t->p,sizeof p)&&!memcmp(steps,t->step,sizeof steps));
        }
        int8_t oct=song.octave;tap(B_OCTDN);assert(song.octave==oct&&!snow_hits(t->step,NSTEP));
        turn(EN_K1,1);tap(B_OCTUP);assert(song.octave==oct);for(uint32_t k=0;k<NTRK;k++)assert(!snow_hits(trk[k].step,NSTEP));
    }
    puts("HOME GEN: K1 phrase+locks, K2 sound, K3 LFO, K4 FX; detent counts, undo, selected/all clear and unchanged octave passed");
}
static void patterns_and_ms(void)
{
    seq_stop();go_home();focus(SN_HOME_PAT);int16_t params[NTRK][P_COUNT];
    for(uint32_t k=0;k<NTRK;k++)memcpy(params[k],trk[k].p,sizeof params[k]);
    for(uint32_t k=0;k<NTRK;k++){
        uint32_t enc=k<4?EN_K1+k:k==4?EN_ALGO:EN_PRESET;
        turn(enc,-10);assert(snow_pattern_current(k)==0);
        for(uint32_t p=0;p<ST_PATTERNS;p++){
            if(p)turn(enc,1);assert(snow_pattern_current(k)==p);
            track_t *t=&trk[k];memset(t->step,0,sizeof t->step);t->step[0].time=ST_NOTE;t->step[0].n=1;t->step[0].note[0]=(uint8_t)(40+k*3+p);t->step[0].vel=100;
        }
        turn(enc,20);assert(snow_pattern_current(k)==2);turn(enc,-20);assert(snow_pattern_current(k)==0);
        assert(trk[k].step[0].note[0]==40+k*3);
        assert(trk[k].p[SN_ALG]==params[k][SN_ALG]&&trk[k].preset<10);
    }
    for(uint32_t run=0;run<16;run++){
        uint32_t old[NTRK];for(uint32_t k=0;k<NTRK;k++)old[k]=snow_pattern_current(k);
        int8_t oct=song.octave;tap(run&1?B_OCTUP:B_OCTDN);assert(song.octave==oct);
        for(uint32_t k=0;k<NTRK;k++){uint32_t p=snow_pattern_current(k);assert(p<3&&p!=old[k]);assert(trk[k].step[0].note[0]==40+k*3+p);}
    }
    focus(SN_HOME_MS);sel_set(0);turn(EN_K1,4);assert(song.sel==4);
    int16_t level=trk[4].p[P_LEVEL];turn(EN_K2,-1);assert(trk[4].p[P_LEVEL]<level);
    int16_t p[P_COUNT];memcpy(p,trk[4].p,sizeof p);turn(EN_K3,4);turn(EN_K4,4);assert(!memcmp(p,trk[4].p,sizeof p));
    for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_MUTE]=0;
    web_keys(1u<<0);advance(60);web_keys(0);advance(80);assert(trk[0].p[P_MUTE]);
    web_keys(1u<<1);advance(60);web_keys(0);advance(80);assert(perf_solo==1);
    web_keys(1u<<1);advance(60);web_keys(0);advance(80);assert(!perf_solo);
    static const uint8_t white_keys[6]={0,2,4,6,7,9},black_keys[6]={1,3,5,8,10,13};
    for(uint32_t k=0;k<NTRK;k++){
        int16_t before=trk[k].p[P_MUTE];uint8_t solos=perf_solo;
        web_keys(1u<<white_keys[k]);advance(600);assert(trk[k].p[P_MUTE]==!before);web_keys(0);advance(60);assert(trk[k].p[P_MUTE]==!before);
        web_keys(1u<<white_keys[k]);advance(60);web_keys(0);advance(60);assert(trk[k].p[P_MUTE]==before);
        web_keys(1u<<black_keys[k]);advance(600);assert(perf_solo==(solos^(1u<<k)));web_keys(0);advance(60);assert(perf_solo==(solos^(1u<<k)));
        web_keys(1u<<black_keys[k]);advance(60);web_keys(0);advance(60);assert(perf_solo==solos);
    }
    for(uint32_t k=0;k<NTRK;k++)for(uint32_t v=0;v<NVOICE;v++)assert(!trk[k].v[v].gate);
    puts("HOME PAT/MS: six physical pattern encoders, exactly P1-P3, saved-music random switches, K1 channel/K2 level, keyboard mute/solo without notes passed");
}
static void snapshot(const char *dir,uint32_t page)
{
    if(!dir)return;char path[1024];snprintf(path,sizeof path,"%s/home-%u.ppm",dir,page+1);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240u*240u;i++){uint16_t c=swap16(web_fb[i]);fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
static void visual_controls(const char *dir)
{
    go_home();sel_set(0);ui.msg_t=ui.undo_w=0;
    for(uint32_t page=0;page<5;page++){
        focus(page);ui.msg_t=ui.undo_w=0;snow_home_active=0;advance(100);snapshot(dir,page);
        uint32_t draws=snow_home_state.card_blits;advance(500);assert(snow_home_state.card_blits==draws);
        if(page<3u){
            uint16_t art[84][208];int32_t top=page?99:92;uint32_t height=stattic_art_height[page];
            for(uint32_t y=0;y<height;y++)memcpy(art[y],web_fb+(top+y)*240+16,416);
            turn(EN_K4,1);
            for(uint32_t y=0;y<height;y++)assert(!memcmp(art[(y+6)%height],web_fb+(top+y)*240+16,416));
        }
    }
    puts("HOME visuals: all five native-resolution frames, no idle redraw, physical detent moves complete artwork six vertical rows with exact wrap passed");
}
static double filtered(uint32_t ch,int32_t setting,double hz)
{
    track_t *t=&trk[ch];memset(&snow_mix_fx[ch],0,sizeof snow_mix_fx[ch]);t->p[SN_DJ_FILTER]=setting;t->p[SN_WIDTH]=0;
    double energy=0;
    for(uint32_t i=0;i<44100;i++){
        if(!(i%CTL))snow_mix_begin(t);
        int32_t l=(int32_t)(12000*sin(6.283185307179586*hz*i/44100)),r=l;
        snow_mix_sample(t,&l,&r);assert(l==r&&l>-40000&&l<40000);if(i>22050)energy+=(double)l*l;
    }
    return energy;
}
static void channel_audio(void)
{
    track_t *t=&trk[0];memset(snow_mix_fx,0,sizeof snow_mix_fx);t->p[SN_DJ_FILTER]=t->p[SN_WIDTH]=0;
    assert(!snow_mix_begin(t));
    for(uint32_t i=0;i<1000;i++){int32_t l=(int32_t)i*19-9000,r=7000-(int32_t)i*17,a=l,b=r;snow_mix_sample(t,&a,&b);assert(a==l&&b==r);}
    for(uint32_t sign=0;sign<2;sign++){
        t->p[SN_WIDTH]=sign?63:-64;for(uint32_t i=0;i<200;i++)snow_mix_begin(t);
        int32_t l=10000,r=-2000;snow_mix_sample(t,&l,&r);
        assert(l+r==8000);if(!sign)assert(l==r&&l==4000);else assert(l-r==24000);
    }
    double low=filtered(0,-32,100),high=filtered(0,-32,5000);assert(low>high*50);
    low=filtered(0,32,100);high=filtered(0,32,5000);assert(high>low*50);
    for(uint32_t ch=0;ch<NTRK;ch++){
        memset(&snow_mix_fx[ch],0,sizeof snow_mix_fx[ch]);trk[ch].p[SN_DJ_FILTER]=trk[ch].p[SN_WIDTH]=0;
    }
    puts("Channel DSP: exact neutral bypass, true mono/normal/double stereo width, preserved mid, independent LP/HP spectrum and bounded samples passed");
}
static void mixed_audio(void)
{
    web_snow_demo();tap(B_PLAY);assert(song.playing);double peak=0;
    for(uint32_t change=0;change<12;change++){
        for(uint32_t ch=0;ch<NTRK;ch++)for(uint32_t i=0;i<web_snow_param_count();i++){
            uint32_t id=web_snow_param_id(i);
            if(id==SN_DJ_FILTER||id==SN_WIDTH)assert(!web_snow_set(ch,i,(change+ch)%3u==0?0:(change+ch)%3u==1?-64:63));
        }
        for(uint32_t block=0;block<70;block++){
            web_render(128);const float *l=web_out_l(),*r=web_out_r();
            for(uint32_t n=0;n<128;n++){assert(isfinite(l[n])&&isfinite(r[n])&&fabs(l[n])<1&&fabs(r[n])<1);if(fabs(l[n])>peak)peak=fabs(l[n]);}
        }
    }
    assert(peak>.001);seq_stop();puts("Six-track live DJ/width sweeps: real rack/mixer path, finite bounded stereo output passed");
}
int main(int argc,char **argv)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);controls();generators();patterns_and_ms();channel_audio();mixed_audio();visual_controls(argc>1?argv[1]:0);puts("ALL STATTIC HOME TESTS PASSED");return 0;
}
