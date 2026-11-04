/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SNOW 1
#include <assert.h>
#include "../web/emu/felucca_web.c"

static void advance(uint32_t ms)
{
    for (uint32_t n = 0; n < ms * 44u; n += 128u) web_render(128);
}
static void tap(uint32_t b)
{
    web_buttons(1u << b); advance(60); web_buttons(0); advance(60);
}
static void machine_health(void)
{
    int32_t buffer[CTL];
    vmod_t mod = {.amp0=20000,.amp1=20000,.pitch16=60*16,.shape=64<<8};
    uint32_t k, pr, frame, sig[6] = {0};
    for (k = 0; k < NTRK; k++) {
        assert(trk[k].eng_req == ENGI_SNOW+k);
        for (uint32_t e = 0; e < NENGINES; e++) assert(eng_allowed(k,e) == (e == ENGI_SNOW+k));
        for (pr = 0; pr < 3; pr++) {
            voice_t v = {.note=60,.vel=100,.gate=1};
            uint32_t nonzero=0; int32_t peak=0;
            apply_preset_to(&trk[k],pr);
            snow_note_on(&trk[k],&v);
            for (frame=0;frame<44100u*2u;frame+=CTL) {
                if (frame>=22050u) v.gate=0;
                mod.amp0=mod.amp1=snow_amp(&trk[k],&v,0);
                memset(buffer,0,sizeof buffer);
                snow_render(&trk[k],&v,buffer,CTL,&mod);
                for (uint32_t i=0;i<CTL;i++) {
                    int32_t a=buffer[i]<0?-buffer[i]:buffer[i];
                    if(a>peak)peak=a;
                    nonzero += a>4;
                    if(!pr) sig[k]=sig[k]*31u+(uint32_t)buffer[i];
                }
            }
            assert(nonzero>100 && peak>100 && peak<30000);
            printf("Snow %s preset %u: peak %d, audible and bounded\n",SNOW_ENG[k].name,pr,peak);
        }
        apply_preset_to(&trk[k],0);
    }
    for(k=0;k<NTRK;k++)for(pr=k+1;pr<NTRK;pr++)assert(sig[k]!=sig[pr]);
}
static uint32_t macro_signature(uint32_t role, uint32_t knob, int16_t value)
{
    track_t *t=&trk[role]; voice_t v={.note=60,.vel=100,.gate=1};
    vmod_t mod={.pitch16=60*16,.shape=64<<8}; int32_t b[CTL]; uint32_t hash=0;
    apply_preset_to(t,0); t->p[P_E0+knob]=value; snow_note_on(t,&v);
    for(uint32_t n=0;n<8192;n+=CTL) {
        mod.amp0=mod.amp1=snow_amp(t,&v,0); memset(b,0,sizeof b); snow_render(t,&v,b,CTL,&mod);
        for(uint32_t i=0;i<CTL;i++)hash=hash*31u+(uint32_t)b[i];
    }
    return hash;
}
int main(void)
{
    web_nor_erase(); web_boot(); advance(1000);
    printf("Snapshot budget: %zu bytes\n",sizeof snow_saved);
    machine_health();
    for(uint32_t t=0;t<NTRK;t++) for(uint32_t k=0;k<8;k++) {
        uint32_t a=macro_signature(t,k,12), b=macro_signature(t,k,110);
        if(a==b)printf("Inactive parameter: track %u knob %u\n",t,k);
        assert(a!=b);
    }
    web_snow_demo();
    motion_set_lock(&trk[0],0,P_E0,110);
    motion.on=63;
    motion_store_t before=motion;
    snow_temp_save();
    snow_mask=62; snow_control_all(0,12);
    assert(snow_offset[0][0]==0 && snow_offset[1][0]==12);
    assert(!memcmp(&before,&motion,sizeof motion));
    snow_mask=63;
    tap(B_PLAY); advance(800);
    assert(song.playing);
    uint32_t position=trk[0].seq_pos, index=trk[0].seq_idx, writes=web_flash_writes_count();
    trk[0].step[0].note[0]=80; trk[1].p[P_E1]=127;
    assert(snow_temp_reload(63));
    assert(song.playing && trk[0].seq_pos==position && trk[0].seq_idx==index);
    assert(trk[0].step[0].note[0]==60 && !memcmp(&before,&motion,sizeof motion));
    assert(web_flash_writes_count()==writes);
    assert(!snow_offset[1][0]);
    trk[0].p[P_E0]=22; trk[1].p[P_E0]=33;
    assert(snow_temp_reload(1));
    assert(trk[0].p[P_E0]==snow_saved.p[0][P_E0] && trk[1].p[P_E0]==33);
    /* Actual physical button route, not just the snapshot helper. */
    web_buttons(1u<<B_GLO); advance(80);
    web_buttons((1u<<B_GLO)|(1u<<B_SAVE)); advance(60);
    web_buttons(1u<<B_GLO); advance(60);
    assert(snow_saved.valid);
    trk[1].p[P_E0]=100;
    web_buttons((1u<<B_GLO)|(1u<<B_HOME)); advance(60);
    web_buttons(0); advance(100);
    assert(trk[1].p[P_E0]==33);
    web_buttons(0); advance(100);
    /* Musical snapshots never allocate beyond the shared 64-event pool. */
    before=motion;
    motion.count=MOTION_MAX;
    for(uint32_t i=0;i<MOTION_MAX;i++) {
        motion.event[i]=snow_saved.locks.event[0]; motion.event[i].place=(uint16_t)(64u+i);
    }
    int16_t was=trk[0].p[P_E0];
    assert(!snow_temp_reload(1) && trk[0].p[P_E0]==was);
    motion=before;
    /* A permanent project captures the effective macro once, not twice on load. */
    seq_stop(); memset(snow_offset,0,sizeof snow_offset); trk[0].p[P_E0]=40;
    snow_control_all(0,10);
    project_t project; project_capture(&project);
    assert(project.t[0].p[P_E0]==50);
    assert(!project_restore_runtime(&project));
    assert(!snow_offset[0][0] && trk[0].p[P_E0]==50);
    puts("ALL SNOW TESTS PASSED");
    return 0;
}
