/* SPDX-License-Identifier: GPL-3.0-only
 * (2026-11-09, the owner) HOME 1-3: one 1-bit story scene instead of the nine pictures, the same on all
 * three pages. A little man walks the garden a step a beat (standing when stopped) and waters the flowers;
 * four of them are the channels, DR BS SY CH, flashing their colour on their notes (grey when muted);
 * stars twinkle, and sparkle on the hats. The knobs call guests into the sky: K1 a UFO, K2 a planet, K3 an
 * astronaut, K4 a comet. One guest at a time plays to its end; turns meanwhile wait in a queue of two (a new
 * one takes the last place; a guest already there or waiting is not asked again), so turning a lot never
 * makes the sky flicker. White, the theme's accent for small things, the channels' colours for the flowers.
 * Main loop only: ~10 frames a second, the scene's own area drawn, nothing on the audio path but two bytes
 * the note paths set (drums.c stattic_scene_notes, stattic_scene_hats). */
#include "stattic_scene.h"
#define SC_FRAME_MS 100u
#define SC_W 104                                       /* the scene in its own pixels, each 2 x 2 on the screen */
#define SC_STARS 28u
enum { SG_NONE, SG_UFO, SG_PLANET, SG_ASTRO, SG_COMET };
static const uint8_t SG_LEN[5]={0,64,100,90,28};       /* frames each guest plays (6.4 s, 10 s, 9 s, 2.8 s) */
static const uint8_t SC_FLOWER_X[4]={10,28,46,64},SC_FLOWER_TRK[4]={3,0,1,2};
#define SC_HOUSE_X 84                                  /* (2026-11-10, the owner) the house on the right, its dish sending */
#define SC_WALK_END 70
static struct {
    uint32_t ms,frame,beat,rnd;
    uint8_t guest,queue[2],nq,pause,gt,inited;
    int16_t mx;uint8_t right,leg,water,target,flash[4],grow[4],signal;
    uint8_t sx[SC_STARS],sy[SC_STARS],star[SC_STARS],stimer[SC_STARS];
} sc;
static uint32_t sc_rand(void){uint32_t x=sc.rnd?sc.rnd:0x2545f491u;x^=x<<13;x^=x>>17;x^=x<<5;return sc.rnd=x;}
static void stattic_scene_knob(uint32_t knob)
{
    uint8_t g=(uint8_t)(SG_UFO+(knob&3u));
    sc.signal=16;                                       /* (every turn: the dish sends, in the channel's colour) */
    if(sc.guest==g)return;
    for(uint32_t i=0;i<sc.nq;i++)if(sc.queue[i]==g)return;
    if(sc.nq<2u)sc.queue[sc.nq++]=g;else sc.queue[1]=g;
}
static void sc_init(void)
{
    sc.rnd=0x9e3779b9u;
    for(uint32_t k=0;k<SC_STARS;k++){
        uint32_t x,y,tries=0;
        do{x=sc_rand()%100u+2u;y=sc_rand()%100u;}while(++tries<20u&&x>76u&&y<26u);   /* (not on the moon) */
        sc.sx[k]=(uint8_t)x;sc.sy[k]=(uint8_t)y;
    }
    for(uint32_t f=0;f<4u;f++)sc.grow[f]=(uint8_t)(3u+f%3u);
    sc.mx=2;sc.right=1;sc.inited=1;
}
static void sc_step(void)                               /* a beat: a step, or a watering beat */
{
    if(sc.water){
        if(!--sc.water)sc.grow[sc.target]=(uint8_t)(sc.grow[sc.target]>=8u?3u:sc.grow[sc.target]+1u);
        return;
    }
    sc.leg^=1u;sc.mx=(int16_t)(sc.mx+(sc.right?3:-3));
    if(sc.mx>=SC_WALK_END){sc.mx=SC_WALK_END;sc.right=0;}
    if(sc.mx<=0){sc.mx=0;sc.right=1;}
    if(sc.right)for(uint32_t f=0;f<4u;f++){
        int32_t d=(int32_t)SC_FLOWER_X[f]-(sc.mx+11);
        if(d>=-1&&d<=1){sc.water=2;sc.target=(uint8_t)f;}
    }
}
/* one frame when it is due; 1 when the scene changed */
static int stattic_scene_tick(void)
{
    if(fm1_ms-sc.ms<SC_FRAME_MS)return 0;
    sc.ms=fm1_ms;sc.frame++;
    if(!sc.inited)sc_init();
    uint8_t notes=stattic_scene_notes,hats=stattic_scene_hats;
    stattic_scene_notes=0;stattic_scene_hats=0;
    for(uint32_t f=0;f<4u;f++){if((notes>>SC_FLOWER_TRK[f])&1u)sc.flash[f]=3;else if(sc.flash[f])sc.flash[f]--;}
    for(uint32_t k=0;k<SC_STARS;k++)if(sc.stimer[k]&&!--sc.stimer[k])sc.star[k]=0;
    if(sc.signal)sc.signal--;
    for(uint32_t i=0;i<hats&&i<2u;i++){uint32_t k=sc_rand()%SC_STARS;sc.star[k]=2;sc.stimer[k]=3;}
    if(!(sc.frame&3u)){uint32_t k=sc_rand()%SC_STARS;if(!sc.star[k]){sc.star[k]=1;sc.stimer[k]=5;}}
    if(song.playing){if(clk_beat!=sc.beat){sc.beat=clk_beat;sc_step();}}
    else{sc.water=0;sc.leg=0;sc.beat=clk_beat;}
    if(sc.guest){if(++sc.gt>=SG_LEN[sc.guest]){sc.guest=SG_NONE;sc.gt=0;sc.pause=6;}}
    else if(sc.pause)sc.pause--;
    else if(sc.nq){sc.guest=sc.queue[0];sc.queue[0]=sc.queue[1];sc.nq--;sc.gt=0;}
    else if(song.playing&&sc_rand()%700u==0u)stattic_scene_knob(3);   /* (now and then a comet of its own) */
    return 1;
}
