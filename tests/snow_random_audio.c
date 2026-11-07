/* SPDX-License-Identifier: GPL-3.0-only
 * Deterministic random-patch auditions through the actual C oscillator,
 * filter and envelope. Dry renders expose the instrument beneath the FX. */
#define SNOW_AUDITION 1
#include "snow_test.c"
int main(int argc,char **argv)
{
    if(argc!=2)return 1;web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200u);
    uint32_t mask[(P_COUNT+31u)/32u];sel_set(0);snow_edit_mask(mask);
    for(uint32_t k=0;k<NTRK;k++){
        char path[1024];snprintf(path,sizeof path,"%s/random-%u.pcm",argv[1],k);
        FILE *f=fopen(path,"wb");if(!f)return 2;
        for(uint32_t n=0;n<64;n++){
            int16_t next[P_COUNT];snow_design_sound(&trk[k],k,(n+1u)*0x9e3779b9u,mask,next);
            memcpy(trk[k].p,next,sizeof next);sound(k,32768,k>=4?16384:32768);
            for(uint32_t i=0;i<32768;i++){
                int16_t x=(int16_t)clamp(samples[i]/2,-32767,32767);
                fputc(x&255,f);fputc((uint16_t)x>>8,f);
            }
        }
        fclose(f);
    }
    return 0;
}
