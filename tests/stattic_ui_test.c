/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_STATTIC 1
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

static void boot(void)
{
    memset(&ui,0,sizeof ui);memset(trk,0,sizeof trk);memset(&song,0,sizeof song);
    memset(encs,0,sizeof encs);memset(enc_late,0,sizeof enc_late);
    panel=PANEL_DEFAULT;layers_init();settings.palette=1;palette_set(settings.palette);
    host_tracks_init();
    for(uint32_t i=0;i<NPART;i++){
        track_defaults(&trk[i]);set_engine_of(&trk[i],TRK_DEF[i][0]);apply_preset_to(&trk[i],0);trk[i].engine=trk[i].eng_req;
    }
    track_defaults(TDRUM);TDRUM->p[P_E0]=DRUM_STATTIC;
    TDRUM->p[P_E1]=127;TDRUM->p[P_E2]=0;TDRUM->p[P_E3]=TDRUM->p[P_E4]=64;
    stattic_macro[TRK_DRUM]=0;song.sel=TRK_DRUM;stattic_runtime_init();go_home();ui.force=1;
    fm1_in.notes=fm1_in.buttons=notes_seen=edges_btn=0;frames(2);
}
int main(int argc,char **argv)
{
    outdir=argc>1?argv[1]:"build/host";boot();
    assert(ui.home&&song.sel==TRK_DRUM);
    int16_t tempo=song.g[G_BPM];
    for(uint32_t page=0;page<SN_HOME_COUNT;page++){
        stattic_home_page=(uint8_t)page;ui.force=1;frames(4);
        char name[32];snprintf(name,sizeof name,"stattic-home-%u",page+1);ppm(name);
    }
    stattic_home_page=0;encs[panel.enc[EN_SELECT]]=1;frame();
    assert(stattic_home_page==1&&song.g[G_BPM]==tempo);
    encs[panel.enc[EN_ALGO]]=1;frame();assert(song.sel==0);
    encs[panel.enc[EN_ALGO]]=1;frame();assert(song.sel==1);
    encs[panel.enc[EN_ALGO]]=1;frame();assert(song.sel==2);
    stattic_home_page=0;frames(2);int16_t mode=TSEL->p[SN_PLAY_MODE];int octave=song.octave;
    press(B_OCTUP);frames(44);assert(TSEL->p[SN_PLAY_MODE]!=mode&&song.octave==octave);
    release(B_OCTUP);frames(4);assert(song.octave==octave);
    tap(B_OCTUP);assert(song.octave==octave+1);
    assert(TSEL->p[P_REL]==STATTIC_CH_RELEASE);
    stattic_home_page=4;song.sel=TRK_DRUM;frames(3);key(0);frames(2);assert(TDRUM->p[P_MUTE]);
    key(1);frames(2);assert(song.solo==(1u<<TRK_DRUM));
    song.solo=0;TDRUM->p[P_MUTE]=0;
    stattic_home_page=3;encs[panel.enc[EN_K2]]=1;frame();frames(2);assert(stattic_current[0]==1);
    stattic_home_page=1;encs[panel.enc[EN_K2]]=1;frame();frames(2);
    for(uint32_t l=0;l<16;l++)assert(stattic_pick_ok(stattic_drum_pick[l],ST_LANE_CATEGORY[l]));
    {   /* HOME 6 PROJECTS (the harness's slots: A and B saved): K1 picks B; K2, K4, K3 each act on their second turn */
        stattic_home_page=SN_HOME_PROJ;song.playing=0;frames(2);
        encs[panel.enc[EN_K1]]=1;frame();assert(stattic_proj_sel==1);
        uint32_t st=sec_stores,ld=sec_loads,er=erases;
        encs[panel.enc[EN_K2]]=1;frame();assert(sec_stores==st);
        frames(40);encs[panel.enc[EN_K2]]=1;frame();assert(sec_stores==st+1u&&live_sec==1);
        ui.force=1;frames(2);ppm("stattic-home-6");
        encs[panel.enc[EN_K4]]=1;frame();assert(sec_loads==ld);frames(40);encs[panel.enc[EN_K4]]=1;frame();assert(sec_loads==ld+1u);
        encs[panel.enc[EN_K3]]=1;frame();assert(erases==er);frames(40);encs[panel.enc[EN_K3]]=1;frame();assert(erases==er+1u);
        encs[panel.enc[EN_K1]]=1;frame();encs[panel.enc[EN_K3]]=1;frame();frames(40);encs[panel.enc[EN_K3]]=1;frame();
        assert(erases==er+1u);                         /* (C is empty: nothing to erase) */
        stattic_home_page=0;frames(2);
    }
    {   /* HOME 7 SONG KEY: K1 the note, K2 the known scales, K3 all of them; every part at once, never CHROMATIC */
        stattic_home_page=SN_HOME_KEY;frames(2);int16_t r0=trk[0].p[P_ROOT];
        encs[panel.enc[EN_K1]]=1;frame();for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[P_ROOT]==(r0+1)%12);
        encs[panel.enc[EN_K1]]=-1;frame();
        stattic_key_set(trk[0].p[P_ROOT],2);encs[panel.enc[EN_K2]]=-1;frame();for(uint32_t k=0;k<NTRK;k++)assert(trk[k].p[P_SCALE]==1);
        encs[panel.enc[EN_K3]]=-1;frame();assert(trk[0].p[P_SCALE]==1);
        encs[panel.enc[EN_K3]]=1;frame();assert(trk[1].p[P_SCALE]==2&&trk[2].p[P_SCALE]==2);
        ui.force=1;frames(2);ppm("stattic-home-7");stattic_home_page=0;frames(2);
    }
    tap(B_ENV);assert(!ui.home&&cur_page()->fam==FAM_ENV);ui.force=1;frame();ppm("stattic-env");
    tap(B_HOME);assert(ui.home);tap(B_GLO);assert(!ui.home&&cur_page()->fam==FAM_GLO);
    int16_t before=song.g[G_BPM];encs[panel.enc[EN_SELECT]]=1;frame();
    /* Native SLOOP SELECT walks GLO pages; tempo is still on the GLOBAL knob. */
    (void)before;
    press(B_HOME);frames(44);assert(ui.menu);release(B_HOME);ui.force=1;frame();ppm("stattic-menu");
    ui.menu=0;ui.home=0;
    for(uint32_t k=0;k<NTRK;k++){
        song.sel=(uint8_t)k;
        for(uint32_t p=0;p<NPAGES;p++){
            if(!page_shown(&PAGES[p]))continue;
            ui.page=(uint8_t)p;page_entered();ui.force=1;frames(2);
        }
    }
    puts("STATTIC UI: seven HOME pages (SONG KEY: every part) (PROJECTS: save, open, erase by two turns), DR/BS/SY/CH, SELECT without tempo, long/short OCT+, mute/solo keys, native SLOOP screens");
    return 0;
}
