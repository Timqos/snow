/* SPDX-License-Identifier: GPL-3.0-only */
/* Full STATTIC device UI and DSP in one browser-owned instance. Only the
 * physical panel, display, flash transport and clock are host adapters. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define FELUCCA_STATTIC 1
#define FELUCCA_ARRANGER 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_CPU1 0
#define FELUCCA_CDC 1
static uint8_t sim_flash[0x100000];
#define user_flash ((uint8_t (*)[0xF000])(sim_flash+0xA0000))
#define SMP_USER_XIP(k) (sim_flash+0xA0000+((k)&3u)*0xF000u)
#define ST_XIP_PTR(off) (sim_flash+(off))
#define fm1_xip_ptr unused_target_xip_ptr
#include "../hal/fm1_xip.h"
#undef fm1_xip_ptr
static const uint8_t *fm1_xip_ptr(uint32_t off) { return sim_flash+off; }
static volatile uint32_t fm1_remote_btn;
#include "preview.c"
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; if(x+w>240||y+h>240)return; for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; if(x+w>240||y+h>240)return; for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t enc_late[7];                         /* a detent the timer ISR counts just after a read (as on the FM-1, where the
                                                     * scan runs while a UI pass does): it lands in the same pass */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = enc_late[e]; enc_late[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static uint32_t wdt_tick_ms;                       /* (stress_test: 1, the blocking PANEL setup's clock runs out) */
static void fm1_wdt_feed(void) { fm1_ms += wdt_tick_ms; }
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../src/panel.c"
#include "../src/ui.c"

static uint8_t flash_ok=1;
static int st_read(uint32_t off,void *p,uint32_t n) { if(off+n>sizeof sim_flash)return -1;memcpy(p,sim_flash+off,n);return 0; }
static int st_erase(uint32_t off) { if(off+4096>sizeof sim_flash)return -1;memset(sim_flash+off,255,4096);return 0; }
static int st_prog(uint32_t off,const void *p,uint32_t n) {
    if(off+n>sizeof sim_flash)return -1;for(uint32_t i=0;i<n;i++)sim_flash[off+i]&=((const uint8_t *)p)[i];return 0;
}
#include "../src/ui_song.c"
#include "../src/ui_studio.c"
#include "../src/icons.c"
#include "../src/ui_draw.c"
#include "../src/ui_vis.c"
#include "../src/ui_layers.c"
#include "../src/ui_menu.c"
#include "../src/ui_input.c"
#include "../src/storage.c"
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t x) { (void)x; }
static uint32_t fl_jedec_ram(void) { return 0xB36014; }
#define FL_JEDEC_OK(x) 1
#define FL_FAR(x) x
static void fl_plain_window_init(void) {}
static void up_boot(void);
#include "../src/project.c"
#include "../src/upreset.c"
static uint8_t sim_editor_response[1024],sim_editor_request[1024];
static uint32_t sim_editor_response_n;
static void ota_wire_send(const uint8_t *p,uint32_t n) { sim_editor_response_n=n;memcpy(sim_editor_response,p,n); }
static int ota_frame_get(const uint8_t **p,uint32_t *n) { (void)p;(void)n;return 0; }
static void ota_frame_done(void) {}
static int fl_write(uint32_t off,const void *data,uint32_t n) {
    const uint8_t *p=data;while(n){uint32_t k=256u-(off&255u);if(k>n)k=n;int rc=st_prog(off,p,k);if(rc)return rc;off+=k;p+=k;n-=k;}return 0;
}
static void fl_inval(uint32_t off,uint32_t n) { (void)off;(void)n; }
static int fl_erase4k_quiet(uint32_t off,uint32_t *took) { *took=0;return st_erase(off); }
#define SXQ 512u
static uint32_t so_w,so_r;
#include "../src/editor.c"
uint8_t *stattic_sim_request_buffer(void) { return sim_editor_request; }
uint8_t *stattic_sim_response_buffer(void) { return sim_editor_response; }
uint32_t stattic_sim_request(uint32_t n) { if(n>sizeof sim_editor_request)return 0;sim_editor_response_n=0;ed_handle(sim_editor_request,n);return sim_editor_response_n; }
#include "../src/splash.c"
static uint32_t sim_samples;
void stattic_sim_init(void)
{
    memset(sim_flash,255,sizeof sim_flash);memset(trk,0,sizeof trk);memset(&song,0,sizeof song);memset(&ui,0,sizeof ui);
    preview_init();arr_defaults(&arrangement);panel=PANEL_DEFAULT;panel_init();layers_init();palette_set(2);
    for(uint32_t k=0;k<NPART;k++){
        trk[k].engine=trk[k].eng_req=(uint8_t)trk_def_engine(k);apply_preset_to(&trk[k],0);
    }
    TDRUM->p[P_E0]=DRUM_STATTIC;TDRUM->p[P_E1]=127;TDRUM->p[P_E2]=0;TDRUM->p[P_E3]=TDRUM->p[P_E4]=64;
    for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_SLEN]=64;
    stattic_runtime_init();song.sel=TRK_DRUM;song.batt_raw=4200;song.master_q12=2048;
    stattic_macro[TRK_DRUM]=0;go_home();ui.force=1;sim_samples=0;fm1_ms=0;
    for(uint32_t i=0;i<NTRK;i++){stattic_prepare(i,0);stattic_runtime_tick();}
    ui_input();ui_draw();
}
uint16_t *stattic_sim_screen(void) { return screen; }
void stattic_sim_encoder(uint32_t i,int32_t n) { if(i<7)encs[panel.enc[i]]+=n; }
void stattic_sim_button(uint32_t i,uint32_t down) {
    if(i>=NB)return;uint32_t bit=1u<<panel.btn[i];
    if(down){fm1_in.buttons|=bit;edges_btn|=bit;}else fm1_in.buttons&=~bit;
}
void stattic_sim_key(uint32_t i,uint32_t down) { if(i>=27)return;if(down)fm1_in.notes|=1u<<i;else fm1_in.notes&=~(1u<<i); }
void stattic_sim_frame(void) { ui_input();ui_leds();ui_draw();sections_flush();autosave_tick(); }
float *stattic_sim_render(void) {
    float *p=preview_render();sim_samples+=CTL*1000u;
    while(sim_samples>=FS){sim_samples-=FS;fm1_ms++;}
    return p;
}
uint8_t *stattic_sim_flash(void) { return sim_flash; }
void stattic_sim_resume(void) { up_boot();autosave_resume();ui.force=1; }
uint32_t stattic_sim_state(uint32_t i) {
    switch(i){case 0:return song.sel;case 1:return stattic_home_page;case 2:return song.playing;
    case 3:return song.g[G_BPM];case 4:return ui.home;case 5:return ui.menu;case 6:return trk[2].p[SN_PLAY_MODE];case 7:return (uint32_t)(song.octave+3);
    case 8:return stattic_art_pick[stattic_home_page%3];case 9:return stattic_scroll[stattic_home_page%3];default:return 0;}
}

void stattic_sim_panic(void) { fm1_in.notes=fm1_in.buttons=0;preview_panic(); }
