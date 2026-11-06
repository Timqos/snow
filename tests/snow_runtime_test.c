/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression for the first physical-device report: dormant FX work,
 * bypass leakage, GLO navigation, and unchanged active effect/synth output.
 * Host timings compare revisions; they are NOT an FM-1 deadline test. */
#define SNOW_AUDITION 1
#include "snow_test.c"
#include <time.h>

static uint32_t pcm_hash(uint32_t h, int32_t sample)
{
    return (h ^ (uint32_t)sample) * 16777619u;
}

static void fx_patch(track_t *t, uint32_t mode)
{
    apply_preset_to(t, 0);
    t->p[SN_FX_BYPASS] = 0;
    t->p[P_CHOR] = mode == 1 || mode == 8 ? 71 : 0;
    t->p[P_DLY] = mode == 2 || mode == 8 ? 67 : 0;
    t->p[P_REV] = mode >= 3 && mode <= 5 || mode == 8 ? 73 : 0;
    t->p[SN_TMIX] = mode == 6 || mode == 8 ? 47 : 0;
    t->p[SN_BMIX] = mode == 7 || mode == 8 ? 29 : 0;
    t->p[SN_RTYPE] = mode >= 3 && mode <= 5 ? (int16_t)(mode - 3) : 1;
    t->p[SN_DTIME] = 37; t->p[SN_DSYNC] = 0; t->p[SN_DFB] = 63;
    t->p[SN_DCOLOR] = 87; t->p[SN_DWIDTH] = 91;
    t->p[SN_RSIZE] = 79; t->p[SN_RDAMP] = 71; t->p[SN_RPRE] = 23;
    t->p[SN_CRATE] = 39; t->p[SN_CDEPTH] = 51; t->p[SN_CFB] = 19;
    t->p[SN_TDRIVE] = 31; t->p[SN_TTONE] = 97;
    t->p[SN_TWOW] = 13; t->p[SN_TFLUT] = 7;
    t->p[SN_BBITS] = 7; t->p[SN_BRATE] = 5; t->p[SN_BTONE] = 99;
}

static uint32_t fx_capture(uint32_t mode)
{
    track_t *t = &trk[0];
    int32_t block[CTL];
    uint32_t h = 2166136261u;
    memset(snow_fx, 0, sizeof snow_fx);
    fx_patch(t, mode);
    for (uint32_t b = 0; b < 1800; b++) {
        for (uint32_t i = 0; i < CTL; i++)
            block[i] = b < 900 ? sine_i((b * CTL + i) * 0x0712345u) * 2 : 0;
        snow_fx_process(t, block, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            h = pcm_hash(h, block[i]);
            h = pcm_hash(h, snow_fx[0].side[i]);
        }
    }
    return h;
}

static uint32_t synth_capture(uint32_t channel)
{
    uint32_t h = 2166136261u;
    for (uint32_t preset = 0; preset < 10; preset++) {
        apply_preset_to(&trk[channel], preset);
        (void)sound(channel, 8192, 4096);
        for (uint32_t i = 0; i < 8192; i++) h = pcm_hash(h, samples[i]);
    }
    return h;
}

static void unchanged_audio(void)
{
    /* Captured from unmodified Snow 0.7.3, commit 25007e2, before the runtime
     * fix: mono + stereo side, 1.3 s each; ten factory sounds per channel.
     * These constants must not be regenerated to excuse an optimisation. */
    static const uint32_t fx[] = {0x43cf67fb,0x051f16a7,0x1bda95ab,
        0xb2ffbf3f,0xab0a132b,0xef3b8b4f,0x929186c5,0xc0c51419,0xe499bb81};
    static const uint32_t synth[] = {0x289a1769,0x708a8a58,0xc6515e20,
        0x4c085da8,0xc240851f,0x04764298};
    for (uint32_t m = 0; m < NELEM(fx); m++) assert(fx_capture(m) == fx[m]);
    /* Kick factory patches are intentionally redesigned in STATTIC.
     * All other factories retain the original immutable audio hashes. */
    for (uint32_t k = 1; k < NTRK; k++) assert(synth_capture(k) == synth[k]);
    puts("Audio optimisation: 50 unchanged factory sounds and nine active/dry FX paths match 0.7.3 exactly");
}

static void glo_routes(void)
{
    go_home(); sel_set(0); ui.fam_last[FAM_GLO] = 0;
    for (uint32_t n = 0; n < 8; n++) {
        /* Inspect the route before draw_page's fallback can hide a bad page. */
        (void)page_tap(B_GLO);
        assert(page_visible(ui.page));
        assert(cur_page()->graph == (n % 2 ? GR_GLOBAL : GR_TRK));
    }
    for (uint32_t lock = 0; lock < 2; lock++) {
        ui_prefs = lock ? ui_prefs | PREF_BPM_LOCK : ui_prefs & ~PREF_BPM_LOCK;
        go_home(); ui.fam_last[FAM_GLO] = 0; tap(B_GLO);
        uint32_t page = ui.page;
        int16_t tempo = song.g[G_BPM];
        web_enc(EN_SELECT, 1); advance(80);
        assert(song.g[G_BPM] == tempo + 1 && ui.page == page);
        tap(B_GLO); page = ui.page; tempo = song.g[G_BPM];
        web_enc(EN_SELECT, -1); advance(80);
        assert(song.g[G_BPM] == tempo - 1 && ui.page == page);
        go_home(); tempo = song.g[G_BPM];
        web_enc(EN_SELECT, 1); advance(80);
        assert(song.g[G_BPM] == tempo);
    }
    puts("GLO: visible MIXER/GLOBAL cycle, unheld SELECT tempo with either BPM-lock preference; HOME stays panel selection");
}

static void unchanged_gap_audio(void)
{
    /* Original 0.7.3: note, 20 s silence, another note. Change delay time and
     * crush rate during the gap; sleeping must preserve clocks/hold phase. */
    static const uint32_t expected[]={0xca549f21,0x70cf4c91,0x5cfdb485,0x33c9ce51,
        0x09bd8ad1,0x8d4af0fd,0x4187138d,0x21e76565};
    for(uint32_t mode=1;mode<=8;mode++){
        memset(snow_fx,0,sizeof snow_fx);fx_patch(&trk[0],mode);
        uint32_t h=2166136261u;int32_t b[CTL];
        for(uint32_t block=0;block<30000;block++){
            if(block==13000)trk[0].p[SN_DTIME]=9;
            if(block==15000)trk[0].p[SN_BRATE]=11;
            if(block==29000)assert(!snow_fx[0].running);
            for(uint32_t i=0;i<CTL;i++)b[i]=block<400||block>=29200?sine_i((block*CTL+i)*0x0712345u)*2:0;
            snow_fx_process(&trk[0],b,CTL);
            for(uint32_t i=0;i<CTL;i++)h=pcm_hash(pcm_hash(h,b[i]),snow_fx[0].side[i]);
        }
        assert(h==expected[mode-1]);
    }
    puts("FX sleep: eight long-gap renders match 0.7.3 exactly, with full tails, changed delay time and sample/hold phase");
}

static void bypass_and_dormancy(void)
{
    track_t *t = &trk[0];
    int32_t b[CTL], dry[CTL];
    memset(snow_fx, 0, sizeof snow_fx); fx_patch(t, 8);
    for (uint32_t n = 0; n < 600; n++) {
        for (uint32_t i = 0; i < CTL; i++) b[i] = sine_i((n * CTL + i) * 0x0712345u) * 2;
        snow_fx_process(t, b, CTL);
    }
    t->p[SN_FX_BYPASS] = 1;
    for (uint32_t i = 0; i < CTL; i++) b[i] = dry[i] = (int32_t)i * 501 - 8000;
    snow_fx_process(t, b, CTL);
    assert(!memcmp(b, dry, sizeof b));
    for (uint32_t i = 0; i < CTL; i++) assert(!snow_fx[0].side[i]);
    uint32_t wp = snow_fx[0].dw;
    for (uint32_t n = 0; n < 200; n++) {
        memcpy(b, dry, sizeof b); snow_fx_process(t, b, CTL);
        assert(!memcmp(b, dry, sizeof b) && snow_fx[0].dw == wp);
    }
    /* Re-enable a previously full rack on silence: stale samples must not return.
     * Every effect is also toggled independently while the others remain enabled. */
    t->p[SN_FX_BYPASS] = 0;
    for (uint32_t n = 0; n < 1800; n++) {
        memset(b, 0, sizeof b); snow_fx_process(t, b, CTL);
        for (uint32_t i = 0; i < CTL; i++) assert(!b[i] && !snow_fx[0].side[i]);
    }
    static const uint16_t level[] = {P_CHOR, P_DLY, P_REV, SN_TMIX, SN_BMIX};
    for (uint32_t e = 0; e < NELEM(level); e++) {
        for (uint32_t n = 0; n < 1200; n++) {
            for (uint32_t i = 0; i < CTL; i++) b[i] = sine_i((n * CTL + i) * 0x0712345u);
            snow_fx_process(t, b, CTL);
        }
        int16_t amount = t->p[level[e]]; t->p[level[e]] = 0;
        for (uint32_t n = 0; n < 20000; n++) { memset(b, 0, sizeof b); snow_fx_process(t, b, CTL); }
        t->p[level[e]] = amount;
        for (uint32_t n = 0; n < 1800; n++) {
            memset(b, 0, sizeof b); snow_fx_process(t, b, CTL);
            for (uint32_t i = 0; i < CTL; i++) assert(!b[i] && !snow_fx[0].side[i]);
        }
    }
    puts("FX: immediate dry bypass, dormant delay work suspended, no stale sound after rack/individual-effect re-enable");
}

static void cpu_defaults(void)
{
    assert(CPU2_SET && (CPU2_BYTE & SN_CPU_DEFAULT_APPLIED));
    uint8_t saved = CPU2_BYTE;
    CPU2_BYTE = 6u; /* earlier 1-core setting; preserve preview and 320 MHz */
    snow_cpu_boot_defaults(); assert(CPU2_SET && CPU2_BYTE == (6u|129u));
    snow_cpu_save_defaults();
    menu_put(MI_CPU, 0); assert(!CPU2_SET);
    sks_t packed = persist_saved; settings_export(&packed);
    CPU2_BYTE = 0;
    assert(settings_import(&packed, sizeof packed));
    snow_cpu_boot_defaults();
    assert(!CPU2_SET && (CPU2_BYTE & SN_CPU_DEFAULT_APPLIED));
    CPU2_BYTE = saved;
    puts("Snow CPU: new/old settings migrate to two cores once; saved one-core/fault fallback is respected");
}

static void chord_release_audio(void)
{
    /* Exercise note-on/off -> allocator -> mixer -> engine amplitude, not
     * only snow_amp() in isolation. A label alone cannot prove a release. */
    track_t *t = &trk[5];
    int32_t out[2 * CTL];
    for (uint32_t bpm = 90; bpm <= 180; bpm += 30) {
        song.g[G_BPM] = (int16_t)bpm; song.g[G_CLOCK] = 0;
        panic_req = 63; events_block(CTL);
        apply_preset_to(t, 0); t->p[P_AMODE] = 0; t->p[P_ESYNC] = 0;
        t->p[P_LD_AMP] = 0;
        events_block(CTL); /* consume the preset-load panic before the new note */
        trk_note_on(t, 60, 100);
        for (uint32_t b = 0; b < FS / CTL / 4; b++) mix_block(out, CTL);
        voice_t *v = 0;
        for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].note == 60) v = &t->v[i];
        if (!v || !v->gate || v->s[0] <= 1 << 22) {
            fprintf(stderr,"CH probe bpm=%u mode=%d voice=%d mute=%d engine=%u/%u env=%d\n",
                    bpm,t->p[SN_MODE],t->p[P_VOICE],t->p[P_MUTE],t->engine,t->eng_req,v?v->s[0]:-1);
            for (uint32_t i=0;i<NVOICE;i++) fprintf(stderr,"v%u active=%u gate=%u note=%u stage=%u own=%d/%d\n",
                    i,t->v[i].active,t->v[i].gate,t->v[i].note,t->v[i].stage,t->v[i].s[0],t->v[i].s[1]);
        }
        assert(v && v->gate && v->s[0] > 1 << 22);
        trk_note_off(t, 60); assert(!v->gate);
        uint32_t initial = (uint32_t)v->s[0], duration = beat_samples() * 8u;
        uint32_t halfway = duration / CTL / 2, total = duration / CTL;
        uint64_t late_energy = 0;
        for (uint32_t b = 0; b < total; b++) {
            mix_block(out, CTL);
            assert(v->active);
            if (b == halfway) assert((uint32_t)v->s[0] > initial / 15 && (uint32_t)v->s[0] < initial / 7);
            if (b > total * 3u / 4u)
                for (uint32_t i = 0; i < 2u * CTL; i++) late_energy += (uint64_t)llabs(out[i]);
        }
        assert((uint32_t)v->s[0] > initial / 150 && (uint32_t)v->s[0] < initial / 65);
        assert(late_energy > 1000);
    }
    /* The real overload mechanism can shorten that release: reproducing it
     * explains why host envelope timing is not an on-device CPU guarantee.
     * Keep Skiff's overload protection; remove wasted work instead. */
    shed_req = shed_over = 0;
    (void)shed_check(SHED_TICKS + 1u); (void)shed_check(SHED_TICKS + 1u);
    assert(shed_req);
    voice_t *released = 0;
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && !t->v[i].gate) released = &t->v[i];
    assert(released); shed_voice(); mix_block(out, CTL); assert(!released->active);
    shed_req = shed_over = 0; song.g[G_BPM] = 120;
    puts("CH: actual mixed release remains audible for two bars at 90/120/150/180 BPM; overload eviction reproduced separately");
}

static void bench(void)
{
    int32_t out[2 * CTL];
    for (uint32_t active = 0; active < 2; active++) {
        for (uint32_t k = 0; k < NTRK; k++) fx_patch(&trk[k], active ? 8 : 0);
        clock_t start = clock();
        for (uint32_t b = 0; b < 30000; b++) mix_block(out, CTL);
        printf("Host-only %s six racks: %.3f s for %.2f audio seconds\n", active ? "active" : "dry",
               (double)(clock() - start) / CLOCKS_PER_SEC, 30000.0 * CTL / FS);
    }
}

#ifndef SNOW_RUNTIME_LIBRARY
int main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IOLBF, 0);
    web_nor_erase(); web_boot(); advance(SNOW_BOOT_MS + 200);
    seq_stop(); memset(&pf, 0, sizeof pf);
    if (argc > 1 && !strcmp(argv[1], "--capture")) {
        for (uint32_t mode = 0; mode <= 8; mode++) printf("FX %u %08x\n", mode, fx_capture(mode));
        for (uint32_t k = 0; k < NTRK; k++) printf("SYNTH %u %08x\n", k, synth_capture(k));
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--bench")) { bench(); return 0; }
    if (argc > 1 && !strcmp(argv[1], "--bypass")) { bypass_and_dormancy(); return 0; }
    cpu_defaults(); unchanged_audio(); unchanged_gap_audio(); glo_routes(); bypass_and_dormancy(); chord_release_audio();
    puts("SNOW RUNTIME REGRESSIONS PASSED (physical-device qualification still required)");
    return 0;
}
#endif
