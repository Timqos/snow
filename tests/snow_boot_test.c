/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SNOW 1
#include <assert.h>
#include "../web/emu/felucca_web.c"
static void until(uint32_t start,uint32_t ms){while(fm1_ms-start<ms)web_render(128);}
static void capture(const char *dir,uint32_t ms)
{
    if(!dir)return;
    char path[1024];snprintf(path,sizeof path,"%s/boot-%04u.ppm",dir,ms);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240u*240u;i++){
        uint16_t c=swap16(web_fb[i]);fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);
    }
    fclose(f);
}
int main(int argc,char **argv)
{
    web_nor_erase();web_boot();uint32_t start=fm1_ms;
    uint32_t last=0,states=0,draws=web_draws;uint16_t hold[240*240];
    for(uint32_t ms=50;ms<=900;ms+=50){
        until(start,ms);uint32_t ink=0;
        for(uint32_t y=38;y<150;y++)for(uint32_t x=64;x<176;x++)ink+=web_fb[y*240+x]!=swap16(T_BG);
        assert(ink>=last);states+=ink!=last;last=ink;
        assert(kb_boot_hold&&web_boot_ms);
        if(ms==150||ms==400||ms==900)capture(argc>1?argv[1]:0,ms);
    }
    assert(states>=8&&last==24u*14u*14u);assert(web_draws-draws<=54u);
    /* Full reference silhouette, including the central negative-space cell. */
    static const uint8_t reference[7]={0x49,0x2a,0x1c,0x77,0x1c,0x2a,0x49};
    for(uint32_t y=0;y<7;y++)for(uint32_t x=0;x<7;x++)
        assert(web_fb[(38+7+y*14+7)*240+64+7+x*14+7]==swap16((reference[y]>>x)&1u?T_THEME:T_BG));
    memcpy(hold,web_fb,sizeof hold);draws=web_draws;until(start,1450);
    assert(kb_boot_hold&&web_boot_ms);assert(web_draws==draws&&!memcmp(hold,web_fb,sizeof hold));
    until(start,1650);assert(!kb_boot_hold&&!web_boot_ms);assert(memcmp(hold,web_fb,sizeof hold));
    capture(argc>1?argv[1]:0,1650);
    puts("SNOW BOOT PASSED: reference logo, >=8 growth states, 20 fps cap, readable hold, 1.5 s startup and clean UI handoff");
    return 0;
}
