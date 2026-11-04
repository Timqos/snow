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
        assert(SNOW_ENG[k].presets==SNOW_ENG[0].presets && SNOW_ENG[k].npresets==18);
        for(uint32_t p=0;p<18;p++){apply_preset_to(&trk[k],p);(void)sound(k,16384,8192);}
    }
    apply_preset_to(&trk[0],0);(void)sound(0,22050,22050);
    uint32_t crossings=0;int64_t energy=0;
    for(uint32_t i=2206;i<8820;i++){crossings+=samples[i-1]<=0 && samples[i]>0;energy+=samples[i]<0?-samples[i]:samples[i];}
    double hz=(double)crossings*44100.0/(8820-2206);
    printf("Kick body after transient: %.1f Hz, %lld energy\n",hz,(long long)energy);
    assert(hz>=30 && hz<=90 && energy>10000);
    /* Every channel can use every algorithm and every oscillator waveform. */
    for(uint32_t a=0;a<6;a++) {
        apply_preset_to(&trk[0],12);trk[0].p[SN_ALG]=(int16_t)a;trk[0].p[SN_MIX]=55;
        uint32_t previous=0;
        for(uint32_t w=0;w<6;w++) {
            trk[0].p[SN_WAVE1]=(int16_t)w;trk[0].p[SN_PW]=w==4?40:64;
            uint32_t h=sound(0,8192,4096);assert(!w || h!=previous);previous=h;
        }
    }
    apply_preset_to(&trk[0],12);trk[0].p[SN_MODE]=1;trk[0].p[P_ATK]=0;
    uint32_t fast=sound(0,8192,4000);trk[0].p[P_ATK]=85;assert(sound(0,8192,4000)!=fast);
    trk[0].p[P_ATK]=0;trk[0].p[P_REL]=100;assert(sound(0,8192,4000)!=fast);
    trk[0].p[SN_L2_DEPTH]=40;trk[0].p[SN_L2_DEST]=1;
    uint32_t lfo1=trk[0].lfo_ph;int32_t before=snow_l2[0].value;
    for(uint32_t i=0;i<200;i++)snow_block(&trk[0]);
    assert(trk[0].lfo_ph==lfo1 && snow_l2[0].value!=before);
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
    assert(count==8);assert(!snow_generate(0,1));assert(memcmp(first,t->step,sizeof first));
    t->p[SN_G_DENS]=0;assert(!snow_generate(0,0));for(uint32_t i=0;i<16;i++)assert(t->step[i].time==ST_REST);
    t->p[SN_G_DENS]=100;assert(!snow_generate(0,0));for(uint32_t i=0;i<16;i++)assert(t->step[i].time==ST_NOTE);
    t->p[SN_G_LIVE]=1;song.playing=1;take_pass[0]=2;snow_gen_pass[0]=1;
    int16_t seed=t->p[SN_G_SEED];snow_sequence_poll();assert(t->p[SN_G_SEED]==((seed+1)&127));
    seed=t->p[SN_G_SEED];snow_sequence_poll();assert(t->p[SN_G_SEED]==seed);
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
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(1000);
    synth_tests();generation_tests();patterns_and_snapshot();
    puts("ALL SNOW 0.2 TESTS PASSED");return 0;
}
