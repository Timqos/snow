/* SPDX-License-Identifier: GPL-3.0-only
 * User's STATTIC mark. Fixed geometry, a short fade, then a readable hold.
 * Existing strip canvas only; no audio-thread work or image decoding. */
#include "stattic_logo.h"
#define SNOW_BOOT_MS 1500u
#define SNOW_BOOT_FRAME_MS 50u
static uint8_t snow_boot_frame=255;
static void snow_splash_frame(uint32_t elapsed)
{
    uint32_t frame=(elapsed<900u?elapsed:900u)/SNOW_BOOT_FRAME_MS;
    if(frame==snow_boot_frame)return;snow_boot_frame=(uint8_t)frame;
    uint32_t appear=(uint32_t)clamp((int32_t)elapsed*100/650,0,100);
    cv_begin(112,112,T_BG);uint16_t ink=ux_mix(T_BG,T_TEXT,appear);
    for(uint32_t y=0;y<98u;y++)for(uint32_t x=0;x<112u;x++)
        if((ST_LOGO[y][x/8u]>>(7u-x%8u))&1u)cv_rect((int32_t)x,7+(int32_t)y,1,1,ink);
    cv_blit(64,38);
    appear=(uint32_t)clamp(((int32_t)elapsed-420)*100/300,0,100);
    draw_text_box(0,166,240,&AF_L,"STATTIC",ux_mix(T_BG,T_THEME,appear),1);
    draw_text_box(0,205,240,&AF_S,"FM-1 FIRMWARE",ux_mix(T_BG,T_MID,appear),1);
    lcd_sync();
}
static void draw_splash(void)
{
    lcd_fill(0,0,240,240,T_BG);snow_boot_frame=255;snow_splash_frame(0);
}
