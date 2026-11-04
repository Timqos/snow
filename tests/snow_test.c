/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SNOW 1
#include <assert.h>
#include "../web/emu/felucca_web.c"
static int32_t samples[44100];
static void advance(uint32_t ms) { for(uint32_t n=0;n<ms*441u/10u;n+=128)web_render(128); }
static void tap(uint32_t b) {web_buttons(1u<<b);advance(60);web_buttons(0);advance(60);}
static uint32_t sound(uint32_t track,uint32_t frames,uint32_t gate)
{
    track_t *t=&trk[track];voice_t *v=&t->v[0];int32_t block[CTL],last=0;
    memset(v,0,sizeof *v);v->note=60;v->vel=100;v->gate=v->active=1;v->stage=1;
    snow_note_on(t,v);uint32_t hash=0,peak=0;
    for(uint32_t pos=0;pos<frames;pos+=CTL) {
        if(pos>=gate)v->gate=0;
        snow_block(t);
        vmod_t m={.pitch16=60*16,.shape=64<<8,.amp0=last};m.amp1=snow_amp(t,v,0);last=m.amp1;
        memset(block,0,sizeof block);snow_render(t,v,block,CTL,&m);
        for(uint32_t i=0;i<CTL && pos+i<frames;i++) {
            int32_t x=block[i];if(pos+i<44100u)samples[pos+i]=x;
            uint32_t a=(uint32_t)(x<0?-x:x);if(a>peak)peak=a;
            hash=hash*31u+(uint32_t)x;
        }
    }
    assert(peak>50 && peak<60000);
    return hash;
}
static void synth_tests(void)
{
    for(uint32_t k=0;k<NTRK;k++) {
        assert(SNOW_ENG[k].presets==SNOW_PATCHES[k] && SNOW_ENG[k].npresets==10);
        assert(!strcmp(SNOW_ENG[k].name,SN_ROLES[k]));
        for(uint32_t other=0;other<NTRK;other++)assert(eng_allowed(k,ENGI_SNOW+other)==(k==other));
        uint32_t hashes[10];
        for(uint32_t p=0;p<10;p++) {
            apply_preset_to(&trk[k],p);hashes[p]=sound(k,16384,8192);
            for(uint32_t j=0;j<p;j++)assert(hashes[j]!=hashes[p]);
        }
    }
    apply_preset_to(&trk[0],0);(void)sound(0,22050,22050);
    uint32_t crossings=0;int64_t energy=0;
    for(uint32_t i=2206;i<8820;i++){crossings+=samples[i-1]<=0 && samples[i]>0;energy+=samples[i]<0?-samples[i]:samples[i];}
    double hz=(double)crossings*44100.0/(8820-2206);
    printf("Kick body after transient: %.1f Hz, %lld energy\n",hz,(long long)energy);
    assert(hz>=30 && hz<=90 && energy>10000);
    /* Every channel can use every algorithm and every oscillator waveform. */
    for(uint32_t a=0;a<6;a++) {
        apply_preset_to(&trk[4],0);trk[4].p[SN_ALG]=(int16_t)a;trk[4].p[SN_MIX]=55;
        uint32_t previous=0;
        for(uint32_t w=0;w<6;w++) {
            trk[4].p[SN_WAVE1]=(int16_t)w;trk[4].p[SN_PW]=w==4?40:64;
            uint32_t h=sound(4,8192,4096);assert(!w || h!=previous);previous=h;
        }
    }
    apply_preset_to(&trk[4],0);trk[4].p[SN_MODE]=1;trk[4].p[P_ATK]=0;
    uint32_t fast=sound(4,8192,4000);trk[4].p[P_ATK]=85;assert(sound(4,8192,4000)!=fast);
    trk[4].p[P_ATK]=0;trk[4].p[P_REL]=100;assert(sound(4,8192,4000)!=fast);
    trk[4].p[SN_L2_DEPTH]=40;trk[4].p[SN_L2_DEST]=1;
    uint32_t lfo1=trk[4].lfo_ph;int32_t before=snow_l2[4].value;
    for(uint32_t i=0;i<200;i++)snow_block(&trk[4]);
    assert(trk[4].lfo_ph==lfo1 && snow_l2[4].value!=before);
    uint32_t pages=0;sel_set(0);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_EDIT && page_visible(i))pages++;
    printf("Visible EDIT pages: %u\n",pages);assert(pages>=18);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].id[0]==P_ATK)assert(page_visible(i));
}
static void generation_tests(void)
{
    track_t *t=&trk[0];t->p[P_SLEN]=16;t->p[P_ROOT]=2;t->p[P_SCALE]=2;
    t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=50;t->p[SN_G_SEED]=7;t->p[SN_G_BASE]=60;
    motion_set_lock(t,0,P_E0,100);motion_store_t locks=motion;
    step_t other[NSTEP],first[NSTEP];memcpy(other,trk[1].step,sizeof other);
    assert(!snow_generate(0,0));memcpy(first,t->step,sizeof first);
    assert(!snow_generate(0,0));assert(!memcmp(first,t->step,sizeof first));
    assert(!memcmp(other,trk[1].step,sizeof other) && !memcmp(&locks,&motion,sizeof locks));
    uint32_t count=0;
    for(uint32_t i=0;i<16;i++)if(t->step[i].n){count++;assert(scale_snap(t,t->step[i].note[0])==t->step[i].note[0]);}
    assert(count==4);assert(!snow_generate(0,1));assert(memcmp(first,t->step,sizeof first));
    t->p[SN_G_DENS]=0;assert(!snow_generate(0,0));for(uint32_t i=0;i<16;i++)assert(t->step[i].time==ST_REST);
    t->p[SN_G_DENS]=100;assert(!snow_generate(0,0));for(uint32_t i=0;i<16;i++)assert(t->step[i].time==ST_NOTE);
    t->p[SN_G_LIVE]=1;song.playing=1;take_pass[0]=2;snow_gen_pass[0]=1;
    int16_t seed=t->p[SN_G_SEED];snow_sequence_poll();assert(t->p[SN_G_SEED]==((seed+1)&127));
    seed=t->p[SN_G_SEED];snow_sequence_poll();assert(t->p[SN_G_SEED]==seed);
    snow_gen_pass[0]=8;take_pass[0]=1;snow_sequence_poll();assert(t->p[SN_G_SEED]==seed);
    song.playing=0;t->p[SN_G_LIVE]=0;
}
static void patterns_and_snapshot(void)
{
    web_snow_demo();seq_stop();
    step_t a[NSTEP],other[NSTEP];memcpy(a,trk[0].step,sizeof a);memcpy(other,trk[1].step,sizeof other);
    assert(!snow_pattern_select(0,1));assert(lists.cur[0]==1);
    assert(snow_pattern_current(0)==1);
    trk[0].step[2]=(step_t){{70},1,ST_NOTE,0,100};
    assert(!snow_pattern_select(0,0));assert(lists.cur[0]==0);
    assert(!memcmp(a,trk[0].step,sizeof a) && !memcmp(other,trk[1].step,sizeof other));
    assert(!snow_pattern_select(0,1));assert(trk[0].step[2].note[0]==70);
    snow_temp_save();motion_store_t locks=motion;
    snow_mask=62;snow_control_all(0,10);assert(!snow_offset[0][0] && snow_offset[1][0]==10);
    assert(!memcmp(&locks,&motion,sizeof locks));
    tap(B_PLAY);advance(500);assert(song.playing);
    uint32_t pos=trk[0].seq_pos,index=trk[0].seq_idx,writes=web_flash_writes_count();
    trk[0].step[2].note[0]=80;assert(snow_temp_reload(63));
    assert(song.playing && trk[0].seq_pos==pos && trk[0].seq_idx==index);
    assert(trk[0].step[2].note[0]==70 && web_flash_writes_count()==writes);
    seq_stop();memset(snow_offset,0,sizeof snow_offset);snow_mask=63;
    trk[0].p[P_E0]=40;snow_control_all(0,10);project_t project;project_capture(&project);
    assert(project.t[0].p[P_E0]==50 && !project_restore_runtime(&project));
    assert(trk[0].p[P_E0]==50 && !snow_offset[0][0]);
}
static void style_tests(void)
{
    seq_stop();
    for(uint32_t style=0;style<NELEM(SN_GROOVES);style++) {
        for(uint32_t k=0;k<NTRK;k++) {
            track_t *t=&trk[k];t->p[P_SLEN]=16;t->p[SN_G_STYLE]=(int16_t)style;
            t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=50;t->p[SN_G_COMPLEX]=0;t->p[SN_G_VARIANT]=0;
            t->p[SN_G_ROTATE]=0;t->p[SN_G_SEED]=1;
            assert(!snow_generate(k,0));
            uint32_t mask=0;
            for(uint32_t i=0;i<16;i++)if(t->step[i].time==ST_NOTE) {
                mask|=1u<<i;
                assert(t->step[i].n==(k==5?3:1));
                for(uint32_t note=0;note<t->step[i].n;note++)assert(scale_snap(t,t->step[i].note[note])==t->step[i].note[note]);
            }
            assert(mask==SN_GROOVES[style].anchor[k]);
        }
    }
    /* At high variation the rhythm changes across seeds, while low settings keep genre anchors. */
    for(uint32_t k=0;k<NTRK;k++) {
        track_t *t=&trk[k];t->p[SN_G_STYLE]=3;t->p[SN_G_COMPLEX]=80;t->p[SN_G_VARIANT]=120;
        uint32_t first=0,different=0;
        for(uint32_t seed=0;seed<24;seed++) {
            t->p[SN_G_SEED]=(int16_t)seed;assert(!snow_generate(k,0));
            uint32_t mask=0;for(uint32_t i=0;i<16;i++)if(t->step[i].time==ST_NOTE)mask|=1u<<i;
            if(!seed)first=mask;else different+=mask!=first;
        }
        assert(different>2);
    }
    assert(!snow_generate_scene(3,1,1));assert(song.g[G_BPM]==172);
    for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[SN_G_STYLE]==3);
    puts("20 styles x 6 instrument roles: anchors, randomized variation, scales and ensemble passed");
}
static void fx_tests(void)
{
    seq_stop();track_t *t=&trk[0];apply_preset_to(t,0);
    int32_t b[CTL];
    /* A channel's impulse must never write another channel's state. */
    memset(snow_fx,0,sizeof snow_fx);t->p[P_DLY]=110;t->p[P_REV]=110;t->p[P_CHOR]=100;
    t->p[SN_DTIME]=5;t->p[SN_DFB]=85;
    int64_t tail=0;
    for(uint32_t block=0;block<1400;block++) {
        memset(b,0,sizeof b);if(!block)b[0]=60000;
        snow_fx_process(t,b,CTL);
        if(block>100)for(uint32_t i=0;i<CTL;i++){tail+=llabs(b[i]);assert(llabs(b[i])<500000);}
    }
    snow_fx_t *other=&snow_fx[1];const uint8_t *bytes=(const uint8_t *)other;
    for(uint32_t i=0;i<sizeof *other;i++)assert(bytes[i]==0);
    assert(tail>10000);assert(snow_fx[0].pos==1400*CTL);
    for(uint32_t block=0;block<4000;block++) {
        memset(b,0,sizeof b);snow_fx_process(t,b,CTL);
        if(block>=3980)for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[0].side[i]);
    }
    memset(b,0,sizeof b);snow_fx_process(&trk[1],b,CTL);
    for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[1].side[i]);
    /* Tape and bitcrush have audible, independently controlled responses. */
    uint32_t hash[3];
    for(uint32_t mode=0;mode<3;mode++) {
        memset(snow_fx,0,sizeof snow_fx);t->p[P_DLY]=t->p[P_REV]=t->p[P_CHOR]=0;
        t->p[SN_TMIX]=mode==1?100:0;t->p[SN_BMIX]=mode==2?100:0;
        t->p[SN_BBITS]=5;t->p[SN_BRATE]=9;uint32_t h=0;
        for(uint32_t block=0;block<100;block++) {
            for(uint32_t i=0;i<CTL;i++)b[i]=sine_i((block*CTL+i)*0x03124455u)*2;
            snow_fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++)h=h*31u+(uint32_t)b[i];
        }
        hash[mode]=h;
        for(uint32_t block=0;block<1000;block++) {
            memset(b,0,sizeof b);snow_fx_process(t,b,CTL);
            if(block>=990)for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[0].side[i]);
        }
    }
    assert(hash[0]!=hash[1]&&hash[0]!=hash[2]&&hash[1]!=hash[2]);
    /* Every FX value survives permanent project capture/restore, including appended stable IDs. */
    t->p[SN_DTIME]=73;t->p[SN_RPRE]=29;t->p[SN_TMIX]=83;t->p[SN_BBITS]=6;
    project_t project;project_capture(&project);t->p[SN_DTIME]=1;t->p[SN_TMIX]=0;
    assert(!project_restore_runtime(&project));
    assert(t->p[SN_DTIME]==73&&t->p[SN_RPRE]==29&&t->p[SN_TMIX]==83&&t->p[SN_BBITS]==6);
    uint8_t packed[SID_COUNT+1u];int16_t restored[P_COUNT]={0};
    assert(sid_put(packed,t->p)==sizeof packed);assert(sid_get(restored,packed,sizeof packed)==sizeof packed);
    assert(!memcmp(restored,t->p,sizeof restored));assert(sid_p(SID_E0)==P_E0&&sid_p(SID_E0+8u)==P_LPAN0);
    trk[5].p[SN_BBITS]=3;trk[5].p[SN_RTYPE]=2;
    assert(!project_save(0));t->p[SN_DTIME]=1;trk[5].p[SN_BBITS]=16;
    project_load(0);assert(trk[0].p[SN_DTIME]==73&&trk[5].p[SN_BBITS]==3&&trk[5].p[SN_RTYPE]==2);
    puts("Independent FX: isolated state/tails, Tape, Bitcrush and project restoration passed");
}
static void pattern_controls(void)
{
    web_snow_demo();seq_stop();snow_pattern_bank=0;
    web_buttons(1u<<B_SEQ);advance(700);assert(layer_open()==LAYER_SEQ);
    uint32_t before=snow_pattern_current(1),kick=snow_pattern_current(0),bass=snow_pattern_current(4);
    web_enc(EN_K1,1);advance(60);assert(snow_pattern_current(0)==kick+1&&snow_pattern_current(1)==before);
    web_buttons((1u<<B_SEQ)|(1u<<B_OCTUP));advance(60);
    web_buttons(1u<<B_SEQ);advance(60);assert(snow_pattern_bank==1);
    web_enc(EN_K1,1);advance(60);assert(snow_pattern_current(4)==bass+1&&snow_pattern_current(0)==kick+1);
    web_buttons(0);advance(90);
    tap(B_PLAY);assert(song.playing);
    uint32_t next=(snow_pattern_current(0)+1u)%NENT;
    assert(!snow_pattern_select(0,next));step_t outgoing[NSTEP];memcpy(outgoing,trk[0].step,sizeof outgoing);
    assert(snow_generate(0,1)==2&&!memcmp(outgoing,trk[0].step,sizeof outgoing));
    advance(2400);assert(snow_pattern_current(0)==next&&!snow_generate(0,1));seq_stop();
    puts("Held SEQ: channel pattern knobs, grid and channels 5-6 navigation passed");
}
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(1000);
    synth_tests();generation_tests();style_tests();patterns_and_snapshot();fx_tests();pattern_controls();
    puts("ALL SNOW 0.3 TESTS PASSED");return 0;
}
