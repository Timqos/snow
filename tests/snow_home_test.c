/* SPDX-License-Identifier: GPL-3.0-only */
/* Physical panel routing, independent generator transactions and channel DSP. */
#define SNOW_AUDITION 1
#include "snow_test.c"
static void turn(uint32_t role,int32_t steps){web_enc(role,steps);advance(100);}
static void focus(uint32_t panel){turn(EN_SELECT,-10);turn(EN_SELECT,(int32_t)panel);assert(snow_home_panel==panel);}
static void controls(void)
{
    go_home();sel_set(0);int16_t bpm=song.g[G_BPM];
    for(uint32_t lock=0;lock<2;lock++){
        ui_prefs=lock?ui_prefs|PREF_BPM_LOCK:ui_prefs&~PREF_BPM_LOCK;
        focus(SN_HOME_MIX);focus(SN_HOME_GEN);focus(SN_HOME_SYNTH);assert(song.g[G_BPM]==bpm);
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
    web_buttons(1u<<B_OCTUP);advance(60);assert(song.octave==octave+1);web_buttons(0);advance(100);assert(!trk[5].p[SN_PLAY_MODE]);tap(B_OCTDN);
    web_buttons(1u<<B_HOME);advance(850);web_buttons(0);advance(100);assert(ui.menu);ui.menu=0;go_home();
    for(uint32_t i=0;i<5;i++)assert(trk[i].p[SN_PLAY_MODE]==1);
    assert(!project_save(7));trk[5].p[SN_DJ_FILTER]=0;trk[5].p[SN_WIDTH]=0;project_load(7);
    assert(trk[5].p[SN_DJ_FILTER]&&trk[5].p[SN_WIDTH]);
    puts("HOME controls: three focus positions, no BPM change, unheld GLO tempo, four macros/mix knobs, channel focus, N/CHO shortcut and persisted mix passed");
}
static void generators(void)
{
    seq_stop();go_home();focus(SN_HOME_GEN);memset(&motion,0,sizeof motion);memset(snow_offset,0,sizeof snow_offset);
    for(uint32_t ch=0;ch<NTRK;ch++){
        sel_set(ch);track_t *t=&trk[ch];t->p[SN_G_DENS]=50;t->p[SN_G_LOCKS]=80;
        step_t before[NTRK][NSTEP];int16_t params[NTRK][P_COUNT];
        for(uint32_t k=0;k<NTRK;k++){memcpy(before[k],trk[k].step,sizeof before[k]);memcpy(params[k],trk[k].p,sizeof params[k]);}
        uint32_t count=snow_home_gen[ch][0];turn(EN_K1,3);assert(snow_home_gen[ch][0]==count+3);
        assert(memcmp(params[ch],t->p,sizeof params[ch]));for(uint32_t k=0;k<NTRK;k++){
            assert(!memcmp(before[k],trk[k].step,sizeof before[k]));if(k!=ch)assert(!memcmp(params[k],trk[k].p,sizeof params[k]));
        }
        assert(t->p[SN_DJ_FILTER]==params[ch][SN_DJ_FILTER]&&t->p[SN_WIDTH]==params[ch][SN_WIDTH]);
        if(ch==5)assert(t->p[P_REL]==SN_CH_RELEASE);
        motion_store_t locks=motion;int16_t settings[P_COUNT];memcpy(settings,t->p,sizeof settings);
        uint32_t seed=snow_seed(t);step_t steps[NSTEP];memcpy(steps,t->step,sizeof steps);
        count=snow_home_gen[ch][1];turn(EN_K2,-1);assert(snow_home_gen[ch][1]==count+1&&snow_seed(t)!=seed);
        assert(memcmp(steps,t->step,sizeof steps));uint32_t generated=0;
        for(uint32_t e=0;e<motion.count;e++)generated+=motion.event[e].generated&&(motion.event[e].place>>6)==ch;assert(generated);
        /* One held-SAVE undo restores notes, locks and seed together. */
        web_buttons(1u<<B_SAVE);advance(850);web_buttons(0);advance(100);
        assert(!memcmp(steps,t->step,sizeof steps)&&!memcmp(settings,t->p,sizeof settings));
        assert(motion.count==locks.count);for(uint32_t e=0;e<locks.count;e++)assert(!memcmp(&motion.event[e],&locks.event[e],sizeof locks.event[e]));ui.undo_w=0;
        for(uint32_t knob=2;knob<4;knob++){
            memcpy(steps,t->step,sizeof steps);int16_t p[P_COUNT];memcpy(p,t->p,sizeof p);count=snow_home_gen[ch][knob];
            turn(EN_K1+knob,-2);assert(snow_home_gen[ch][knob]==count+2&&memcmp(p,t->p,sizeof p)&&!memcmp(steps,t->step,sizeof steps));
        }
        int16_t p[P_COUNT];memcpy(p,t->p,sizeof p);int8_t oct=song.octave;tap(B_OCTUP);assert(song.octave==oct+1&&!memcmp(p,t->p,sizeof p));tap(B_OCTDN);
    }
    puts("HOME GEN: exact detent counts, selected channel, sound/LFO/FX isolation, combined notes+locks with one undo, OCT remains octave passed");
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
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);controls();generators();channel_audio();mixed_audio();puts("ALL SNOW HOME TESTS PASSED");return 0;
}
