/* SPDX-License-Identifier: GPL-3.0-only
 * User-supplied seven-cell snowflake, also web/snow/logo.svg. Main loop only;
 * the existing canvas is reused, with no additional framebuffer or audio work. */
#define SNOW_BOOT_MS 1500u
#define SNOW_BOOT_FRAME_MS 50u
static const uint8_t SNOW_LOGO_ROWS[7]={0x49,0x2a,0x1c,0x77,0x1c,0x2a,0x49};
static uint8_t snow_boot_frame=255;
static void snow_splash_frame(uint32_t elapsed)
{
    uint32_t frame=(elapsed<900u?elapsed:900u)/SNOW_BOOT_FRAME_MS;
    if(frame==snow_boot_frame)return;
    snow_boot_frame=(uint8_t)frame;
    cv_begin(112,112,T_BG);
    for(int32_t y=0;y<7;y++)for(int32_t x=0;x<7;x++)if((SNOW_LOGO_ROWS[y]>>x)&1u){
        int32_t ax=x>3?x-3:3-x,ay=y>3?y-3:3-y;
        int32_t delay=(ax>ay?ax:ay)*70;
        int32_t size=clamp(((int32_t)elapsed-delay)*14/350,0,14);
        if(size)cv_rect(7+x*14+(14-size)/2,7+y*14+(14-size)/2,size,size,T_THEME);
    }
    cv_blit(64,38);
    uint32_t appear=(uint32_t)clamp(((int32_t)elapsed-420)*100/300,0,100);
    draw_text_box(0,166,240,&AF_L,"Snow",ux_mix(T_BG,T_THEME,appear),1);
    draw_text_box(0,205,240,&AF_S,"FM-1 FIRMWARE",ux_mix(T_BG,T_MID,appear),1);
    lcd_sync();
}
static void draw_splash(void)
{
    lcd_fill(0,0,240,240,T_BG);
    snow_boot_frame=255;
    snow_splash_frame(0);
}
