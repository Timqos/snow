/* SPDX-License-Identifier: GPL-3.0-only */
/* KEYS demo: each SY/CH factory preset playing a short progression on CH, written as WAV
 * (host render of the firmware DSP, no device timing). Usage: stattic_keys_demo OUTDIR */
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
#include <stdlib.h>
static void wav(const char *path,const int16_t *s,uint32_t frames)
{
    FILE *f=fopen(path,"wb");uint32_t data=frames*4u,v;uint16_t h;
    fwrite("RIFF",1,4,f);v=36u+data;fwrite(&v,4,1,f);fwrite("WAVEfmt ",1,8,f);v=16;fwrite(&v,4,1,f);
    h=1;fwrite(&h,2,1,f);h=2;fwrite(&h,2,1,f);v=FS;fwrite(&v,4,1,f);v=FS*4u;fwrite(&v,4,1,f);h=4;fwrite(&h,2,1,f);h=16;fwrite(&h,2,1,f);
    fwrite("data",1,4,f);fwrite(&data,4,1,f);fwrite(s,4,frames,f);fclose(f);
}
int main(int argc,char **argv)
{
    static int16_t buf[FS*8*2];
    static const uint8_t prog[4][3]={{48,52,59},{45,48,55},{41,45,52},{43,47,53}};   /* Cmaj7-ish, Am7, Fmaj7, G7 (3 voices) */
    for(uint32_t pr=0;pr<10;pr++){
        init();
        for(uint32_t k=0;k<NTRK;k++)if(k!=2)trk[k].p[P_LEVEL]=0;
        host_preset(&trk[2],STATTIC_CH_ENGINE,pr);trk[2].p[SN_PLAY_MODE]=1;trk[2].p[P_REL]=40;song.master_q12=2048;
        uint32_t w=0;
        for(uint32_t c=0;c<4;c++){
            uint32_t vel=c&1?70:110;
            for(uint32_t i=0;i<3;i++)trk_note_on(&trk[2],prog[c][i]+12,vel);
            for(uint32_t b=0;b<FS*3u/2u/CTL;b++){block();for(uint32_t i=0;i<CTL*2;i++)buf[w++]=(int16_t)output[i];}
            for(uint32_t i=0;i<3;i++)trk_note_off(&trk[2],prog[c][i]+12);
            for(uint32_t b=0;b<FS/2u/CTL;b++){block();for(uint32_t i=0;i<CTL*2;i++)buf[w++]=(int16_t)output[i];}
        }
        char path[512];snprintf(path,sizeof path,"%s/%02u-%s.wav",argc>1?argv[1]:".",pr+1,SNOW_PATCHES[5][pr].name);
        for(char *c=path+strlen(argc>1?argv[1]:".")+1;*c;c++)if(*c==' ')*c='-';
        wav(path,buf,w/2u);printf("%s\n",path);
    }
    return 0;
}
