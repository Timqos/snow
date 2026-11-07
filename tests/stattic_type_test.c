/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
static void type_hook(int32_t,int32_t,int32_t,int32_t,const char *,uint32_t);
#define GFX_HOOKS 1
#define GFX_HOOK_TEXT(a,b,c,d,s,f) type_hook(a,b,c,d,s,f)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_BLIT(a,b,c) ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define SNOW_AUDITION 1
#include "snow_test.c"
static const char *context;
static uint32_t clipped,cut;
static void type_hook(int32_t x0,int32_t y0,int32_t x1,int32_t y1,const char *s,uint32_t flags)
{
    if((flags&32u)||!context)return;
    if(flags&2u){if(clipped<90)printf("CLIP %s: %s [%d,%d..%d,%d]\n",context,s,x0,y0,x1,y1);clipped++;}
    if((flags&1u)&&!(flags&8u)){if(cut<40)printf("CUT %s: %s\n",context,s);cut++;}
}
static void capture(const char *dir,const char *name)
{
    if(!dir)return;char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240*240;i++){uint16_t p=swap16(web_fb[i]);fputc((p>>11)*255/31,f);fputc(((p>>5)&63)*255/63,f);fputc((p&31)*255/31,f);}fclose(f);
}
int main(int argc,char **argv)
{
    const char *dir=argc>1?argv[1]:0;setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    printf("Font capitals S=%u M=%u V=%u L=%u\n",AF_S_CAP_H,AF_M_CAP_H,AF_V_CAP_H,AF_L_CAP_H);
    for(uint32_t ch=0;ch<NTRK;ch++){
        sel_set(ch);
        for(uint32_t p=0;p<NPAGES;p++)if(page_visible(p)){
            ui.home=ui.menu=ui.confirm=ui.layer=ui.shift=ui.ly=0;ui.page=p;ui.force=1;ui.msg_t=0;
            char name[64];snprintf(name,sizeof name,"ch%u-page%u-%s",ch,p,PAGES[p].title);context=name;advance(80);
            if(ch==5){char file[32];snprintf(file,sizeof file,"page-%u",p);capture(dir,file);}
            int16_t *value[4]={0},saved[4];const param_desc_t *desc[4];
            for(uint32_t k=0;k<4;k++){desc[k]=page_desc(cur_page(),k,&value[k]);if(value[k])saved[k]=*value[k];}
            for(uint32_t edge=0;edge<2;edge++){
                for(uint32_t k=0;k<4;k++)if(value[k]&&desc[k])*value[k]=edge?desc[k]->max:desc[k]->min;
                ui.force=1;advance(80);
            }
            for(uint32_t k=0;k<4;k++)if(value[k])*value[k]=saved[k];
        }
    }
    context="menu";ui.home=1;ui.menu=1;ui.layer=ui.ly=0;
    snow_profile_max_ticks=UINT32_MAX;
    snow_profile_event=(snow_profile_t){UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    for(uint32_t row=0;row<MI_COUNT;row++){ui.menu_sel=row;ui.force=1;advance(70);if(row==MI_CPU||row==MI_COLOR||row==MI_APEAK||row==MI_CHSYNTH||row==MI_C1WAIT){char name[24];snprintf(name,sizeof name,"menu-%u",row);capture(dir,name);}}
    ui.menu=2;ui.menu_scroll=0;ui.force=1;advance(100);capture(dir,"about");
    ui.menu=0;go_home();name_open(NK_USER_SAVE,0);ui.force=1;context="name";advance(100);capture(dir,"name");name_close();
    for(uint32_t k=1;k<CF_COUNT;k++){ui.confirm=k;ui.force=1;context="confirm";advance(60);if(k==CF_CLEAR_SEQ)capture(dir,"confirm");}
    ui.confirm=0;context=0;printf("Type sweep: %u clipped draws, %u shortened fixed fields\n",clipped,cut);assert(!clipped&&!cut);return 0;
}
