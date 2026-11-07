/* SPDX-License-Identifier: GPL-3.0-only */
/* Device report: CH clean alone, click when a low KK/BS onset joins it.
 * This checks digital headroom and voice continuity; DMA timing needs FM-1. */
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);seq_stop();
    int32_t out[2*CTL];
    for(uint32_t mode=0;mode<4;mode++){
    uint32_t peak=0,limited=0,cases=0,max_env=LIM_T;
    for(uint32_t side=0;side<=4;side+=4)for(uint32_t cp=0;cp<10;cp++)for(uint32_t sp=0;sp<10;sp++){
        panic_req=63;events_block(CTL);memset(&motion,0,sizeof motion);memset(snow_fx,0,sizeof snow_fx);
        for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_MUTE]=1;
        track_t *ch=&trk[5],*low=&trk[side];apply_preset_to(ch,cp);apply_preset_to(low,sp);events_block(CTL);
        ch->p[P_MUTE]=low->p[P_MUTE]=0;
        ch->p[P_LEVEL]=low->p[P_LEVEL]=mode==1?127:TP[P_LEVEL].def;
        song.master_q12=mode>=2?MASTER_FULL/5:MASTER_FULL;fx_usb_fixed=mode==3;
        fx_lowcut=0;lim_env=LIM_T;dc_l=dc_r=dce_l=dce_r=0;perf_solo=pf.mute=0;
        static const uint8_t notes[4]={48,52,55,60};for(uint32_t n=0;n<4;n++)trk_note_on(ch,notes[n],127);
        for(uint32_t b=0;b<700;b++){
            if(b==200)trk_note_on(low,side?36:60,127);
            mix_block(out,CTL);
            limited+=lim_env>LIM_T;
            if((uint32_t)lim_env>max_env)max_env=(uint32_t)lim_env;
            if(fx_usb_fixed)usb_fixed_dac(out,CTL);
            for(uint32_t i=0;i<CTL*2;i++){uint32_t a=(uint32_t)llabs(out[i]);if(a>peak)peak=a;assert(a<32768u);}
        }
        uint32_t held=0;for(uint32_t v=0;v<NVOICE;v++)if(ch->v[v].active&&ch->v[v].gate)held++;
        assert(held==4);cases++;
    }
    printf("CH + KK/BS mode %u: %u preset-pair attacks, CH four held voices retained; peak %u/32768, %u limited blocks, maximum envelope %u\n",mode,cases,peak,limited,max_env);
    /* Maximum-level stress may engage the protective limiter. Default
     * levels at 20% pre-master must stay below it in this digital test. */
    if(mode==2)assert(!limited);
    }
    puts("Digital pair/headroom contracts passed. Physical DMA deadlines still require FM-1 retest.");return 0;
}
