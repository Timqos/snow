/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * The design (the device clock driven by the audio the worklet asks for, the exports below, the saved
 * flash handed back before boot) follows X0X's browser build, web/emu/x0x_web.c of charlesvestal/fm1-x0x
 * (GPL-3.0), a Felucca fork. */
/* Felucca in the browser: the firmware's sources, as the host tests build them (tests/hostsim.c: libc,
 * engines, voices, FX, sequencer; then the UI, main.c, storage and projects, in src/felucca.c's order),
 * compiled to WebAssembly and run in an AudioWorklet against a simulated FM-1:
 *   - the device clock: TIMER4 ticks (24 MHz) and fm1_ms advance with the audio the worklet asks for; the
 *     audio ISR (audio.c fm1_alnk0_irq, the real one) renders each I2S half (HALF_FRAMES) as the DMA would
 *     ask for it, and the main loop (main.c's, below in web_frame) runs every 15 ms, with ui_input on every
 *     millisecond in between as main.c's wait loop calls it;
 *   - the panel: buttons and encoders by their printed labels (panel.c's table maps them to the matrix),
 *     the keys as fm1_in.notes, the LEDs read back from fm1_led / fm1_led_dim through the key matrix;
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order);
 *   - the flash: a 1 MiB NOR image behind storage.c's hooks (settings, projects, user presets, the user
 *     sample slots read through it as XIP); the page keeps its storage sectors and gives them back at boot.
 * Not here: USB (MIDI, audio, the editor, updates), TRS MIDI, the serial console, the battery (full), the
 * crash / UBOOT paths. Nothing of the firmware changes for this file.
 *   web/emu/build.sh  ->  build/emu/{felucca.wasm, worklet.js, index.html} */
#include <stdint.h>

#define FELUCCA_WEB 1
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 0                    /* (Skiff 2.0) as a release (build.py --release): no console, no USB SERIAL row */
#endif
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "WEB"
#endif

static uint8_t nor[0x100000];            /* the SPI NOR (erased: 0xFF) */
#define SMP_USER_XIP(k) ((const uint8_t *)nor + SMP_USER_BASE + (k) * SMP_USER_SIZE)   /* eng_sample.c */
#define main hostsim_main
#include "../../tests/hostsim.c"
#undef main

/* ------------------------------------------------------------ the clock --- */
#define FM1_TICKS_PER_US 24u             /* TIMER4: 24 MHz */
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ------------------------------------------------------ audio (ALNK0) --- */
static uint32_t web_half;                /* the half the next render fills */
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
static void fm1_audio_stop(void) {}
void isr_alnk0(void) {}
#include "../../firmware/src/audio.c"

/* ------------------------------------------------------------ the panel --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h: key id at (column, row bit) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static uint32_t web_dim_lo;
static void fm1_led_dim_level(uint32_t lo) { web_dim_lo = lo; }
/* the power-on LED sweep (hal/fm1_input.h fm1_led_anim_start, the picture hal/fm1_led_anim.h): its frame from the
 * clock (a scan frame 1.1 ms), the page draws each LED's level (web_anim_levels); at its end, or on a key or a button
 * down, the last picture (the glow everywhere, or dark) and the UI's LEDs from the next frame, as the scan does */
#include "../../firmware/hal/fm1_led_anim.h"
static uint32_t web_anim_t0, web_anim_glow, web_anim_run;
static uint32_t web_anim_frame(void) { return (web_ticks - web_anim_t0) / (1100u * FM1_TICKS_PER_US); }
static void fm1_led_anim_start(uint32_t glow)
{
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
    memset(fm1_led_mid, 0, sizeof fm1_led_mid);
    web_anim_t0 = web_ticks;
    web_anim_glow = glow != 0u;
    web_anim_run = 1;
}
static int fm1_led_anim_on(void)
{
    if (web_anim_run && (web_anim_frame() >= FM1_ANIM_FRAMES || fm1_in.notes || fm1_in.buttons)) {
        uint32_t c, r;
        web_anim_run = 0;
        for (c = 0; c < FM1_NCOL; c++)
            for (r = 1; r < 5u; r++)
                if (web_anim_glow && FM1_KEYMAP[r][c] >= 0)
                    fm1_led_dim[c] |= (uint8_t)(1u << r);
    }
    return (int)web_anim_run;
}
static uint32_t web_pressed, web_released, web_notes_pressed;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released)
        *released = web_released;
    web_pressed = web_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = web_notes_pressed; web_notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = web_enc_steps[e % 7u]; web_enc_steps[e % 7u] = 0; return s; }
static void fm1_input_init(void) {}
static void fm1_input_tick(void) {}
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}

/* ----------------------------------------------------------- the screen --- */
static uint16_t web_fb[240 * 240];       /* RGB565 as the panel gets it (high byte first) */
static uint32_t web_draws;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = sw;
    web_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s)              /* MENU > SCREEN OFF (lcd.c): off, the page dark; on: redrawn before */
{
    if (!s) {
        memset(web_fb, 0, sizeof web_fb);
        web_draws++;
    }
}
static void lcd_wake_now(void) {}
static void lcd_init(void) {}

#include "../../firmware/src/gfx.c"
#include "../../firmware/src/panel.c"
#include "../../firmware/src/ui.c"
#include "../../firmware/src/menu_items.c"
#include "../../firmware/src/icons.c"
#include "../../firmware/src/ui_graph.c"
#include "../../firmware/src/ui_draw.c"
#include "../../firmware/src/ui_menu.c"
#include "../../firmware/src/ui_input.c"
#include "../../firmware/src/ui_layer.c"

/* ------------------------------------------------------------ the flash --- */
static uint32_t web_flash_writes;
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off >= sizeof nor || 4096u > sizeof nor - off)
        return -8;
    memset(nor + (off & ~4095u), 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)       /* NOR: a write only clears bits */
{
    const uint8_t *s = src;
    uint32_t i;
    if (off >= sizeof nor || n > sizeof nor - off)
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }             /* the FM-1's part (project.c) */
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../../firmware/src/storage.c"
#include "../../firmware/src/upreset.c"
#include "../../firmware/src/project.c"
#include "../../firmware/src/bank.c"           /* (Skiff: the patterns; phase 5: a project names them) */
#if FELUCCA_SNOW
#include "../../firmware/src/snow_sequence.c"
#endif

/* ------------------------------------------------- main.c and its HAL --- */
#if FELUCCA_CPU2
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets, core; } fm1_crash_t;
#else
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets; } fm1_crash_t;
#endif
#define FM1_CRASH_MAGIC 0x43525348u
static fm1_crash_t fm1_crash;                     /* (none: main.c boot_leds runs the sweep) */
static struct { uint8_t p3_rst, wdt_con; uint32_t rst_src; } fm1_boot;
uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1], _ramx_start[1], _ramx_end[1];
static uint32_t web_master_adc = 700;   /* MASTER, 0..1023 as the ADC reads it */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? (int32_t)web_master_adc : 620; }
static void fm1_adc_init(void) {}
static void fm1_timer5_ack(void) {}
static void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)isr; (void)prio; }
void isr_timer5(void) {}
static void fm1_reboot(void) {}
static void fm1_enter_uboot(void) {}
static void fm1_guard_lock_top(void) {}
static void fm1_irq_enable_all(void) {}
static void fm1_irq_init(void) {}
static void fm1_time_init(void) {}
static void fm1_reset_reason(void) {}
static void fm1_wdt_arm(uint32_t v) { (void)v; }
static void fm1_mailbox_clear(void) {}
static void fm1_guard_enable(uint32_t m) { (void)m; }
enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
#if FELUCCA_CPU2
/* (Skiff 1.3) the second core (hal/fm1_cpu1.h): none here, one core whatever MENU > CPU says (fx.c mix_block's C1_ON()
 * is 0 without felucca.c's macros) */
static uint8_t fm1_c1_on, fm1_c1_gave_up;
static void fm1_cpu1_halt(void) {}
static int fm1_cpu1_start(void) { return -1; }
static void fm1_guard_pc_open(void) {}
#endif
/* (Skiff) MENU > CPU CLOCK 320 MHZ (hal/fm1_clk.h): no PLL here; refused, as on a unit whose clock is not as expected */
enum { FM1_CLK320_EALREADY = 1, FM1_CLK320_EMODE };
static int fm1_clk320_go(uint32_t *t) { t[0] = t[1] = 0; return FM1_CLK320_EMODE; }
#if FELUCCA_CDC
static void cdc_task(void) {}
#endif
/* main.c's calls into the USB driver (registers): nothing here (usb.c's own functions stay as they are) */
#define usb_poll() ((void)0)
#define usb_start() ((void)0)
#define usb_retry(ms) ((void)(ms))
#define usb_detach() ((void)0)
#define ep1_tx() ((void)0)                       /* ((2.2) MIDI OUT's sender, nested in the render) */
#include "../../firmware/src/master.c"
#include "../../firmware/src/main.c"
#undef ep1_tx
#undef usb_poll
#undef usb_start
#undef usb_retry
#undef usb_detach

/* --------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame;

#ifdef FOLLOW_DEMO
/* (2.1, a demo build only: EMU_CFLAGS=-DFOLLOW_DEMO) a song for the chord follow, PLAY to hear it: T1 SOFT PAD holds C Am
 * F G a bar each; T2 SQR BASS's one-bar line and T3 SYNTH PLUCK's arp, both written in C, follow T1 (SCL > FOLLOW: KEY
 * and SCALE, by its ROOT); T6 a beat */
static void follow_demo(void)
{
    static const uint8_t CH[4][3] = {{60, 64, 67}, {57, 60, 64}, {53, 57, 60}, {55, 59, 62}};
    static const uint8_t BASS[16] = {36, 0, 48, 0, 36, 0, 43, 0, 36, 0, 48, 0, 46, 0, 43, 0};
    static const uint8_t ARP[16] = {60, 64, 67, 72, 67, 64, 60, 64, 60, 64, 67, 72, 76, 72, 67, 64};
    static const uint8_t BEAT[16] = {1, 0, 8, 0, 2, 0, 8, 0, 1, 0, 8, 1, 2, 0, 8, 8};   /* KICK 1, SNARE 2, CL HAT 8 */
    uint32_t k, i;
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_SLEN] = 32;
    }
    for (k = 0; k < 3u; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, 0);
        apply_preset_to(t, k == 0u ? 1u : k == 1u ? 2u : 8u);   /* SOFT PAD, SQR BASS, SYNTH PLUCK */
        t->p[P_SCALE] = 1;
        t->p[P_ROOT] = 0;
        t->p[P_SDIV] = 2;
    }
    set_engine_of(&trk[NTRK - 1u], ENGI_DRUM);
    apply_preset_to(&trk[NTRK - 1u], 0);
    trk[0].p[P_SLEN] = 64;
    for (i = 0; i < 64u; i++)
        trk[0].step[i] = i % 16u ? (step_t){{0}, 0, ST_TIE, 0, 0}
                                 : (step_t){{CH[i / 16u][0], CH[i / 16u][1], CH[i / 16u][2]}, 3, ST_NOTE, 0, 90};
    for (i = 0; i < 16u; i++) {
        if (BASS[i])
            trk[1].step[i] = (step_t){{BASS[i]}, 1, ST_NOTE, 0, 110};
        trk[2].step[i] = (step_t){{ARP[i]}, 1, ST_NOTE, 0, (uint8_t)(i % 4u ? 70 : 100)};
        if (BEAT[i])
            trk[NTRK - 1u].step[i] = (step_t){{0}, 0, ST_NOTE, 0, 100, BEAT[i]};
    }
    trk[1].p[P_SGATE] = 80;
    trk[2].p[P_SGATE] = 50;
    trk[1].p[P_FOLLOW] = trk[2].p[P_FOLLOW] = 1;        /* (T1) */
    trk[1].p[P_FMODE] = FM_KEY;
    trk[2].p[P_FMODE] = FM_SCALE;
    song.g[G_BPM] = 108;
}
#endif

/* main.c fm1_main's boot, up to the main loop (no USB, no UART, no panel setup) */
static void web_power_on(void)
{
    persist_boot();
    bank_boot();                          /* main.c: the patterns (bank.c; phase 5: every project's steps) */
    settings_init();
    usb_serial_apply();
    lcd_init();
    draw_splash();                        /* main.c: the splash (ui_draw.c) */
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    felucca_dbg.magic = DBG_MAGIC;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    felucca_init();
    master_boot();                        /* main.c (#137): the pot's level from the first block */
    kb_boot_hold = 1;                     /* main.c (#137): the keys silent under the splash */
    audio_init();
    boot_leds();                          /* main.c: the power-on LED sweep, from the scan's start */
#if FELUCCA_LEGACY
    proj_migrate(boot_clean);             /* main.c: projects of before phase 5 become SKF2 (bank.c) */
#endif
    unit_g_adopt = ui_set_rev < 2u;       /* main.c: (Skiff U3, R8) the unit's CLOCK MIDI IN TUNE */
    autosave_boot(boot_clean);            /* main.c: the last session's music (1.2, project.c) */
    unit_g_boot();
#ifdef FOLLOW_DEMO
    follow_demo();
#endif
    web_boot_ms = fm1_ms + 430u;         /* main.c: 30 + 400 ms before the first frame */
    web_booted = 1;
}

/* one pass of main.c fm1_main's loop (what the browser has: no USB, editor, UBOOT, console) */
static void web_frame(void)
{
    int32_t b = fm1_adc_read(FM1_ADC_BATT);
    if (b > 0)
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    master_poll();
    felucca_dbg.ui_frames++;
    ui_input();
    settings_poll();
    autosave_poll();
    ui_leds();
    ui_draw();
}

/* output ring, stereo float, filled by web_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

/* one millisecond of the device: the clock, the audio halves due by now, the main loop */
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += FS;
    while (web_samples_due >= 1000u * HALF_FRAMES) {      /* an I2S half: the audio ISR renders it */
        uint32_t k;
        const int32_t *o = &abuf[web_half * HALF_WORDS];
        web_samples_due -= 1000u * HALF_FRAMES;
        fm1_alnk0_irq();
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            ring[i] = (float)o[2u * k] / 8388608.0f;
            ring[i + 1] = (float)o[2u * k + 1u] / 8388608.0f;
        }
        web_half ^= 1u;
    }
    if (!web_booted || (int32_t)(fm1_ms - web_boot_ms) < 0)
        return;
    if (web_boot_ms) {                                    /* main.c: the splash goes, the loop starts */
        web_boot_ms = 0;
        kb_boot_hold = 0;
        lcd_fill(0, 0, 240, 240, T_BG);
        web_last_frame = fm1_ms - 15u;
    }
    if (fm1_ms - web_last_frame >= 15u) {                /* main.c: a frame, then ui_input until 15 ms are over */
        web_last_frame = fm1_ms;
        web_frame();
    } else {
        ui_input();                                       /* main.c's wait loop */
    }
}

#define EXPORT __attribute__((used, visibility("default")))
#undef __attribute__

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}

/* the flash: erased, then the page's saved sectors (web_nor), then boot */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
EXPORT void web_boot(void)
{
    if (!web_booted)
        web_power_on();
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 */
static uint32_t web_btn_labels, web_key_mask;
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    web_btn_labels = labels;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    web_pressed |= m & ~old;
    web_released |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    web_key_mask = mask;
    web_notes_pressed |= mask & ~old;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_enc_steps[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
/* a channel message from Web MIDI, as a USB-MIDI packet arrives (usb.c midi_enqueue): 0 = the ring was full */
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 1;
    return (uint32_t)midi_enqueue((status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}

/* the LEDs: bit b = the button labelled b lit (bit 14: PLAY's green), keys bit k; the dim glow and the breath alike */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}
EXPORT uint32_t web_lit_buttons(void)
{
    return led_buttons(fm1_led) | ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) << 14;
}
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT uint32_t web_dim_level(void) { return web_dim_lo; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_breath); }   /* #119: dark .. ~60 % of lit */
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_breath); }
EXPORT uint32_t web_mid_keys(void) { return led_keys(fm1_led_mid); }   /* the DRUM grid's beats: a steady mid level */
/* the power-on sweep running: each LED's level now (hal/fm1_led_anim.h: 0..64 /64 of lit, | 0x80 the glow under
 * it), 0..13 the buttons by label, 14..40 the keys F3..G5; 0 when it is not */
EXPORT uint8_t *web_anim_levels(void)
{
    static uint8_t lv[FM1_ANIM_NBTN + FM1_ANIM_NKEY];
    uint32_t f = web_anim_frame(), i;
    if (!fm1_led_anim_on())
        return 0;
    for (i = 0; i < FM1_ANIM_NBTN + FM1_ANIM_NKEY; i++)
        lv[i] = (uint8_t)fm1_anim_level(f, i < FM1_ANIM_NBTN ? (i < NB ? panel.btn[i] : i) : i, web_anim_glow);
    return lv;
}

/* for the test and the bench (web/emu/emu_test.mjs), a heavy song straight into the tracks (as hostsim's
 * renders set them): T1 FM6 PAD, T2 PHYS DRONE STRING (SYMP), T3 GRAIN CLOUD PAD, 4-note chords on all 16
 * steps, held; T4 DRUM, kick snare and hats; (Skiff phase 6) T5 a second kit, T6 SLICE. PLAY is the page's (or the
 * test's) to press */
EXPORT void web_test_heavy(void)
{
    static const uint8_t ENG[3][2] = {{12, 4}, {ENGI_PHYS, 7}, {8, 0}};
    static const uint8_t CH[4][4] = {{48, 55, 60, 64}, {45, 52, 57, 60}, {41, 48, 53, 57}, {43, 50, 55, 59}};
    uint32_t p, i;
    for (p = 0; p < 3u; p++) {
        track_t *t = &trk[p];
        host_preset(t, ENG[p][0], ENG[p][1]);
        t->p[P_VOICE] = V_POLY;
        t->p[P_SUS] = 127;
        for (i = 0; i < 16u; i++) {
            uint8_t n[4], k;
            for (k = 0; k < 4u; k++)
                n[k] = (uint8_t)(CH[i / 4u][k] + 12u * p);
            put_step(t, i, 4, n, ST_NOTE, 0);
        }
    }
    host_drums(&trk[3]);
    for (i = 0; i < 16u; i++) {
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        n[k++] = i % 2u ? 42 : 46;
        put_step(&trk[3], i, k, n, ST_NOTE, 0);
    }
    host_drums(&trk[4]);                              /* ((Skiff phase 6) the light tracks: T5 a second kit, kick on */
    for (i = 0; i < 16u; i++) {                       /* the beats, hats on the 16ths; T6 SLICE, a slice every 8th) */
        uint8_t n[2] = {36, (uint8_t)(i % 2u ? 42 : 46)};
        put_step(&trk[4], i, i % 4u ? 1u : 2u, i % 4u ? n + 1 : n, ST_NOTE, 0);
    }
    host_preset(&trk[5], FELUCCA_SLICE ? 13u : ENGI_SAMPLE, 0);
    for (i = 0; i < 16u; i += 2u) {
        uint8_t n = (uint8_t)(48u + i / 2u);
        put_step(&trk[5], i, 1, &n, ST_NOTE, 0);
    }
    ui.force = 1;
}

EXPORT uint32_t web_test_user_preset(uint32_t i) { return (uint32_t)up_used(i); }

EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return song.playing; }
#if FELUCCA_SNOW
/* Explicit demo command: never replaces a user's work during boot. */
EXPORT void web_snow_demo(void)
{
    uint32_t k, s;
    static const uint16_t hits[6] = {0x1111, 0x1010, 0xaaaa, 0x4080, 0x4545, 0x0101};
    seq_stop();
    for (k = 0; k < NTRK; k++) {
        motion_reset(&trk[k]);
        apply_preset_to(&trk[k], TRK_DEF[k][1]);
        trk[k].p[SN_G_LIVE] = 0;
        trk[k].p[P_SLEN] = 32;
        trk[k].p[P_SDIV] = 2;
        trk[k].p[P_SGATE] = k == 5u ? 127 : 75;
        for (s = 0; s < NSTEP; s++) {
            memset(&trk[k].step[s], 0, sizeof(step_t));
            trk[k].step[s].time = ST_REST;
            if (s < 32u && ((hits[k] >> (s%16u)) & 1u)) {
                trk[k].step[s].time = ST_NOTE;
                trk[k].step[s].n = 1;
                trk[k].step[s].note[0] = (uint8_t)(k == 4u ? (s >= 8u ? 55 : 60) : 60);
                trk[k].step[s].vel = (uint8_t)(k == 2u ? (s % 4u == 3u ? 85 : 60) : 105);
                if (k == 5u) {
                    trk[k].step[s].n = 3;
                    trk[k].step[s].note[1] = 64;
                    trk[k].step[s].note[2] = 67;
                }
            }
        }
    }
    song.g[G_BPM] = 120;
    memset(snow_offset, 0, sizeof snow_offset);
    motion_set_lock(&trk[0], 8, P_E0, 100);
    motion_set_lock(&trk[3], 7, P_E1, 100);
    motion.on = 63;
    snow_temp_save();
    ui.force = 1;
}
EXPORT void web_snow_save(void) { snow_temp_save(); }
EXPORT uint32_t web_snow_reload(uint32_t mask) { return (uint32_t)snow_temp_reload(mask & 63u); }
EXPORT void web_snow_select(uint32_t track) { if (track < NTRK) { sel_set(track); ui.force = 1; } }
EXPORT int32_t web_snow_value(uint32_t track, uint32_t knob) { return track < NTRK && knob < 4u ? snow_param(&trk[track], knob) : -1; }
EXPORT void web_snow_control(uint32_t knob, int32_t delta) { snow_control_all(knob, clamp(delta, -127, 127)); }
EXPORT void web_snow_mask(uint32_t mask) { snow_mask = (uint8_t)(mask & 63u); }
EXPORT uint32_t web_snow_mask_get(void) { return snow_mask; }
EXPORT uint32_t web_snow_selected(void) { return song.sel; }
/* The browser designer edits the same parameters as the hardware EDIT pages. */
static const struct { uint16_t id; uint8_t group; } web_snow_params[] = {
#define W(id,group) {id,group}
    W(P_E0,0),W(P_E1,0),W(P_E2,0),W(P_E3,0),W(P_E4,0),W(P_E5,0),W(P_E6,0),W(P_E7,0),
    W(SN_ALG,1),W(SN_WAVE1,1),W(SN_WAVE2,1),W(SN_WAVE3,1),W(SN_RATIO,1),W(SN_RATIO3,1),
    W(SN_PW,1),W(SN_MIX,1),W(SN_SUB,1),W(SN_NOISE,1),W(SN_FB_ROUTE,1),
    W(SN_TUNE,2),W(SN_FINE,2),W(SN_KEYTRACK,2),W(SN_PAMT,2),W(SN_PDEC,2),W(SN_FMDEC,2),
    W(SN_MODE,3),W(P_ATK,3),W(P_DEC,3),W(P_SUS,3),W(P_REL,3),
    W(SN_FILTER,4),W(SN_RESO,4),W(P_ED_FLT,4),W(P_ED_PIT,4),W(P_ED_SHP,4),
    W(P_EG2_ATK,5),W(P_EG2_DEC,5),W(P_EG2_SUS,5),W(P_EG2_REL,5),
    W(P_LRATE,6),W(P_LWAVE,6),W(P_LSYNC,6),W(P_LTRIG,6),W(P_LPHASE,6),W(P_LFADE,6),W(P_LPOL,6),
    W(P_LD_PIT,6),W(P_LD_FLT,6),W(P_LD_SHP,6),W(P_LD_AMP,6),
    W(SN_L2_RATE,7),W(SN_L2_WAVE,7),W(SN_L2_DEPTH,7),W(SN_L2_DEST,7),
    W(SN_L2_SYNC,7),W(SN_L2_PHASE,7),W(SN_L2_TRIG,7),W(SN_L2_FADE,7),
    W(P_M1SRC,8),W(P_M1DST,8),W(P_M1AMT,8),W(P_M2SRC,8),W(P_M2DST,8),W(P_M2AMT,8),
    W(P_M3SRC,8),W(P_M3DST,8),W(P_M3AMT,8),W(P_M4SRC,8),W(P_M4DST,8),W(P_M4AMT,8),
    W(P_VOICE,9),W(P_GLIDE,9),W(P_GLMODE,9),W(P_DETUNE,9),W(P_CHRD,9),W(P_VOIC,9),
    W(P_LEVEL,9),W(P_PAN,9),W(P_SPRD,9),W(P_MUTE,9),
    W(P_DIST,10),W(P_DTYPE,10),W(P_DTONE,10),W(P_ITYPE,10),W(P_IA,10),W(P_IB,10),W(P_IC,10),W(P_IMIX,10),
    W(P_DKSRC,10),W(P_DKAMT,10),W(P_DKREL,10),
    W(SN_G_SCOPE,11),W(SN_G_LOCKS,11),W(SN_G_COMPLEX,11),W(SN_G_HUMAN,11),W(SN_G_VARIANT,11),W(SN_G_ROTATE,11),
    W(SN_G_KIND,11),W(SN_G_DENS,11),W(SN_G_SYNC,11),W(SN_G_SEED,11),W(SN_G_BASE,11),W(SN_G_SPAN,11),
    W(SN_G_CONTOUR,11),W(SN_G_ACCENT,11),W(SN_G_LIVE,11),W(P_SCALE,11),W(P_ROOT,11),
    W(P_SLEN,12),W(P_SDIV,12),W(P_SSWING,12),W(P_SGATE,12),W(P_DIR,12),
    W(P_DLY,13),W(SN_DTIME,13),W(SN_DSYNC,13),W(SN_DFB,13),W(SN_DCOLOR,13),W(SN_DWIDTH,13),
    W(P_REV,14),W(SN_RTYPE,14),W(SN_RSIZE,14),W(SN_RDAMP,14),W(SN_RPRE,14),
    W(P_CHOR,15),W(SN_CRATE,15),W(SN_CDEPTH,15),W(SN_CFB,15),
    W(SN_TMIX,16),W(SN_TDRIVE,16),W(SN_TTONE,16),W(SN_TWOW,16),W(SN_TFLUT,16),
    W(SN_BMIX,17),W(SN_BBITS,17),W(SN_BRATE,17),W(SN_BTONE,17),W(SN_FX_BYPASS,18)
#undef W
};
static const char *const web_snow_groups[]={"MACROS","OSCILLATORS / ALGORITHM","TUNING / PITCH / FM ENVELOPE",
    "AMP ADSR","FILTER / ENV ROUTING","ENVELOPE 2","LFO 1","LFO 2","MOD MATRIX","VOICE / MIX",
    "DRIVE / INSERT / DUCK","GENERATIVE SEQUENCER","SEQUENCE","CHANNEL DELAY","CHANNEL REVERB","CHANNEL CHORUS","TAPE","BITCRUSH","FX RACK"};
EXPORT uint32_t web_snow_param_count(void) { return NELEM(web_snow_params); }
EXPORT uint32_t web_snow_param_id(uint32_t i) { return i<NELEM(web_snow_params)?web_snow_params[i].id:P_COUNT; }
EXPORT const char *web_snow_param_group(uint32_t i) { return i<NELEM(web_snow_params)?web_snow_groups[web_snow_params[i].group]:""; }
EXPORT const char *web_snow_param_name(uint32_t channel,uint32_t i) { return channel<NTRK && i<NELEM(web_snow_params)?track_desc(&trk[channel],web_snow_params[i].id)->label:""; }
EXPORT int32_t web_snow_param_min(uint32_t channel,uint32_t i) { return channel<NTRK && i<NELEM(web_snow_params)?track_desc(&trk[channel],web_snow_params[i].id)->min:0; }
EXPORT int32_t web_snow_param_max(uint32_t channel,uint32_t i) { return channel<NTRK && i<NELEM(web_snow_params)?track_desc(&trk[channel],web_snow_params[i].id)->max:0; }
EXPORT const char *web_snow_param_enum(uint32_t channel,uint32_t i,int32_t v)
{
    if(channel>=NTRK || i>=NELEM(web_snow_params))return 0;
    const param_desc_t *d=track_desc(&trk[channel],web_snow_params[i].id);
    return d->names && v>=d->min && v<=d->max?d->names[v-d->min]:0;
}
EXPORT int32_t web_snow_param_value(uint32_t channel,uint32_t i)
{ return channel<NTRK && i<NELEM(web_snow_params)?trk[channel].p[web_snow_params[i].id]:0; }
EXPORT const char *web_snow_param_text(uint32_t channel,uint32_t i)
{
    static char text[48];const char *unit=0;
    if(channel>=NTRK || i>=NELEM(web_snow_params))return "";
    uint32_t id=web_snow_params[i].id;
    param_format(track_desc(&trk[channel],id),trk[channel].p[id],text,&unit);
    /* Value and unit are separate in the hardware UI. */
    if(unit && str_len(text)+str_len(unit)+1u<sizeof text){
        uint32_t n=str_len(text);text[n++]=' ';str_cpy(text+n,unit,sizeof text-n);
    }
    return text;
}
EXPORT uint32_t web_snow_set(uint32_t channel,uint32_t i,int32_t value)
{
    if(channel>=NTRK || i>=NELEM(web_snow_params))return 1;
    track_t *t=&trk[channel];uint32_t id=web_snow_params[i].id;
    uint32_t guard=motion_guard();
    t->p[id]=(int16_t)param_fit(track_desc(t,id),value);
    motion_capture(t,id,t->p[id]);motion_unguard(guard);ui.force=1;
    return 0;
}
EXPORT uint32_t web_snow_generate(uint32_t channel,uint32_t vary) { return (uint32_t)snow_generate(channel,vary!=0); }
EXPORT uint32_t web_snow_pattern(uint32_t channel,uint32_t slot) { return (uint32_t)snow_pattern_select(channel,slot); }
EXPORT uint32_t web_snow_pattern_get(uint32_t channel) { return snow_pattern_current(channel); }
EXPORT uint32_t web_snow_preset(uint32_t channel,uint32_t preset)
{
    if(channel>=NTRK || preset>=10u)return 1;
    apply_preset_to(&trk[channel],preset);ui.force=1;return 0;
}
EXPORT const char *web_snow_preset_name(uint32_t channel,uint32_t preset) { return channel<NTRK && preset<10u?SNOW_PATCHES[channel][preset].name:""; }
EXPORT uint32_t web_snow_preset_count(uint32_t channel) { return channel<NTRK?10u:0u; }
EXPORT const char *web_snow_role(uint32_t channel) { return channel<NTRK?SN_ROLES[channel]:""; }
EXPORT uint32_t web_snow_pattern_used(uint32_t channel,uint32_t slot) { return snow_pattern_used(channel,slot); }
EXPORT uint32_t web_snow_pattern_active(uint32_t channel) { return channel<NTRK && lists.cur[channel]<NENT?lists.cur[channel]:0; }
EXPORT uint32_t web_snow_pattern_pending(uint32_t channel) { return channel<NTRK?bank_next(channel):255; }
EXPORT uint32_t web_snow_scene(uint32_t vary) { return snow_generate_scene(vary!=0); }
EXPORT uint32_t web_snow_genparm(uint32_t all) { return all?snow_parameter_scene():snow_generate_parameters(song.sel); }
EXPORT uint32_t web_snow_clear(uint32_t all) { return snow_clear_sequence(all!=0); }
static uint16_t web_snow_steps[64];static uint8_t web_snow_notes[64];
EXPORT const uint16_t *web_snow_grid(uint32_t ch) {
    ch%=NTRK;uint64_t locks=motion_lock_steps(ch);
    for(uint32_t i=0;i<NSTEP;i++){const step_t *s=&trk[ch].step[i];web_snow_steps[i]=(uint16_t)(s->vel|((s->time==ST_NOTE&&s->n)?256u:0u)|(((locks>>i)&1u)?512u:0u)|((step_ratchet(s)-1u)<<10)|(s->time==ST_TIE?4096u:0u));web_snow_notes[i]=s->n?s->note[0]:255;}
    return web_snow_steps;
}
EXPORT const uint8_t *web_snow_grid_notes(void){return web_snow_notes;}
EXPORT uint32_t web_snow_length(uint32_t ch){return ch<NTRK?(uint32_t)trk[ch].p[P_SLEN]:0;}
EXPORT uint32_t web_snow_position(uint32_t ch){return ch<NTRK&&song.playing?trk[ch].seq_idx:255;}
EXPORT uint32_t web_snow_color_count(void){return NPALETTES;}
EXPORT const char *web_snow_color_name(uint32_t c){return c<NPALETTES?UI_PALETTES[c].name:"";}
EXPORT uint32_t web_snow_color_get(void){return settings.palette;}
EXPORT void web_snow_color(uint32_t c){if(c<NPALETTES){settings.palette=(uint8_t)c;theme_apply();settings_save();ui.force=1;}}
EXPORT uint32_t web_snow_ui_cards(void){return snow_home_state.card_blits;}
EXPORT uint32_t web_snow_ui_scopes(void){return snow_home_state.scope_blits;}
EXPORT uint32_t web_snow_tempo_get(void) { return (uint32_t)song.g[G_BPM]; }
EXPORT void web_snow_tempo(uint32_t bpm) { song.g[G_BPM]=(int16_t)clamp((int32_t)bpm,40,240);ui.force=1; }
#endif
EXPORT uint32_t web_sample_rate(void) { return FS; }
