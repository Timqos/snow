/* SPDX-License-Identifier: GPL-3.0-only
 * (2026-11-09) HOME 1-3's scene, drawn (stattic_scene.c keeps its state; the UI only) */
/* ---- drawing, into the HOME canvas tile (cv_oy) */
static void sc_plot(int32_t top,int32_t x,int32_t y,uint16_t ink)
{
    if(x<0||x>=SC_W||y<0)return;
    int32_t sx=16+2*x,py=top+2*y+cv_oy;uint16_t c=swap16(ink);
    for(int32_t dy=0;dy<2;dy++){int32_t yy=py+dy;if((uint32_t)yy<cv_h){cv_px[(uint32_t)yy*cv_w+(uint32_t)sx]=c;cv_px[(uint32_t)yy*cv_w+(uint32_t)sx+1u]=c;}}
}
static void sc_sprite(int32_t top,uint32_t id,int32_t x,int32_t y,int flip,uint16_t ink,uint16_t acc)
{
    const st_sprite_t *s=&SC_SPRITES[id];
    for(int32_t r=0;r<s->h;r++)for(int32_t c=0;c<s->w;c++){
        uint32_t bit=1u<<(s->w-1-c);int32_t xx=x+(flip?s->w-1-c:c);
        if(s->white[r]&bit)sc_plot(top,xx,y+r,ink);
        else if(s->accent[r]&bit)sc_plot(top,xx,y+r,acc);
    }
}
static int32_t sc_ease(int32_t a,int32_t b,uint32_t t,uint32_t n)   /* smoothstep from a to b */
{
    int32_t q=(int32_t)(t*1024u/(n?n:1u));q=q*q/1024*(3072-2*q)/1024;
    return a+(b-a)*q/1024;
}
static void stattic_scene_draw(int32_t top,int32_t bottom,uint16_t white,uint16_t accent,uint16_t grey)
{
    int32_t H=(bottom-top)/2,gy=H-11,sky=gy-12;
    if(!sc.inited)sc_init();
    for(uint32_t k=0;k<SC_STARS;k++){                  /* stars: dots; a sparkle a plus */
        int32_t x=sc.sx[k],y=1+(int32_t)sc.sy[k]*(sky-2)/100;
        if(sc.star[k]==2u){sc_plot(top,x,y,white);sc_plot(top,x-1,y,white);sc_plot(top,x+1,y,white);sc_plot(top,x,y-1,white);sc_plot(top,x,y+1,white);sc_plot(top,x-2,y,grey);sc_plot(top,x+2,y,grey);}
        else if(sc.star[k]==1u){sc_plot(top,x,y,white);sc_plot(top,x,y-1,grey);sc_plot(top,x,y+1,grey);}
        else sc_plot(top,x,y,(k&1u)?grey:white);
    }
    sc_sprite(top,SC_MOON,88,3,0,white,accent);
    uint32_t t=sc.gt,n=SG_LEN[sc.guest];
    switch(sc.guest){
    case SG_UFO:{                                     /* glides in, hovers with its beam, leaves up and away */
        int32_t x,y=6+((sc.frame>>2)&1u);
        if(t<22u)x=sc_ease(-16,44,t,22);
        else if(t<44u){x=44;
            for(int32_t i=0;i<12&&7+y+i<sky+8;i++){int32_t w=2+i/2;for(int32_t xx=-w;xx<=w;xx++)if(((xx+i+(int32_t)sc.frame)&1)==0)sc_plot(top,x+7+xx,y+6+i,accent);}
        }else{x=sc_ease(44,112,t-44u,n-44u);y-=sc_ease(0,6,t-44u,n-44u);}
        sc_sprite(top,SC_UFO,x,y,0,white,accent);break;}
    case SG_PLANET:                                   /* drifts slowly across */
        sc_sprite(top,SC_PLANET,108-(int32_t)(t*130u/n),8,0,white,accent);break;
    case SG_ASTRO:{                                   /* floats down across, turning */
        int32_t x=sc_ease(106,-12,t,n),y=2+(int32_t)(t*(uint32_t)(sky>22?sky-20:2)/n);
        sc_sprite(top,(t/10u)&1u?SC_ASTRO_B:SC_ASTRO_A,x,y,(t/20u)&1u,white,accent);break;}
    case SG_COMET:{                                   /* streaks down to the left, its tail behind */
        int32_t x=108-(int32_t)(t*104u/n),y=2+(int32_t)(t*(uint32_t)(sky>12?sky-8:2)/n);
        for(int32_t i=1;i<18;i++){if(i>6&&(i&1))continue;sc_plot(top,x+3+i,y+1-i*(sky>12?sky-8:2)/104,i<7?white:grey);}
        sc_sprite(top,SC_COMET,x,y,0,white,accent);break;}
    default:break;
    }
    for(int32_t x=0;x<SC_W;x++)sc_plot(top,x,gy,white);      /* the ground, and its stripes */
    for(int32_t x=0;x<SC_W;x++){uint32_t h=(uint32_t)x*2654435761u;
        if(((h>>7)%9u)<3u)sc_plot(top,x,gy+3,grey);
        if(((h>>11)%11u)<2u)sc_plot(top,x,gy+6,grey);
        if(((h>>15)%13u)<2u)sc_plot(top,x,gy+9,grey);}
    static const uint8_t sprout[4]={28,52,76,100};
    for(uint32_t i=0;i<4u;i++)sc_sprite(top,SC_SPROUT,sprout[i]-1,gy-3,0,white,accent);
    for(uint32_t f=0;f<4u;f++){                        /* the four channels' flowers */
        int32_t x=SC_FLOWER_X[f],h=sc.grow[f];uint32_t k=SC_FLOWER_TRK[f];
        for(int32_t i=1;i<=h;i++)sc_plot(top,x,gy-i,white);
        sc_plot(top,x+1,gy-h/2,white);
        uint16_t ink=trk[k].p[P_MUTE]||trk_silent(&trk[k])?grey:sc.flash[f]?TE_COL[k&3u]:white;
        sc_sprite(top,SC_FLOWER,x-2,gy-h-5,0,ink,accent);
    }
    if(sc.water){                                      /* watering: the can held out, drops falling */
        sc_sprite(top,SC_MAN_W,sc.mx,gy-10,0,white,accent);
        for(int32_t i=0;i<3;i++)sc_plot(top,sc.mx+11+(i&1),gy-5+(int32_t)((sc.frame+(uint32_t)i*2u)%4u),accent);
    }else sc_sprite(top,sc.leg?SC_MAN_B:SC_MAN_A,sc.mx,gy-10,!sc.right,white,accent);
}
