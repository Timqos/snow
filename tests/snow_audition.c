/* SPDX-License-Identifier: GPL-3.0-only
 * Real engine, dry fixed-velocity hits: factory order, 1 second + 300 ms gap.
 * Reuses the native render helper; never substitutes a JavaScript synth. */
#define SNOW_AUDITION 1
#include "snow_test.c"
static void word(FILE *f,uint32_t v,uint32_t n){for(uint32_t i=0;i<n;i++)fputc((int)(v>>(i*8u))&255,f);}
int main(int argc,char **argv)
{
    if(argc!=2)return 1;web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200u);
    static const char *const names[]={"kick","snare","hat","perc"};
    for(uint32_t k=0;k<4;k++) {
        char path[1024];snprintf(path,sizeof path,"%s/%s-presets.wav",argv[1],names[k]);
        FILE *f=fopen(path,"wb");if(!f)return 2;
        uint32_t bytes=10u*(44100u+13230u)*2u;
        fwrite("RIFF",1,4,f);word(f,bytes+36u,4);fwrite("WAVEfmt ",1,8,f);word(f,16,4);word(f,1,2);word(f,1,2);word(f,44100,4);word(f,88200,4);word(f,2,2);word(f,16,2);fwrite("data",1,4,f);word(f,bytes,4);
        for(uint32_t p=0;p<10;p++) {
            apply_preset_to(&trk[k],p);sound(k,44100,44100);
            for(uint32_t i=0;i<44100;i++)word(f,(uint16_t)(int16_t)clamp(samples[i]/2,-32767,32767),2);
            for(uint32_t i=0;i<13230;i++)word(f,0,2);
        }
        fclose(f);
    }
    return 0;
}
