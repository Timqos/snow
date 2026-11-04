/* SPDX-License-Identifier: GPL-3.0-only
 * Snow: original fixed-point FM machines, 2026. No recorded sound sources.
 * State stays in voice_t: no additional per-part sample or delay buffers. */
static uint32_t snow_role(const track_t *t) { return (uint32_t)(t - trk) % NTRK; }
static int16_t snow_offset[NTRK][4];
static uint8_t snow_mask = (1u << NTRK) - 1u;
static int32_t snow_param(const track_t *t, uint32_t k)
{
    return clamp(t->p[P_E0 + k] + (k < 4u ? snow_offset[snow_role(t)][k] : 0), 0, 127);
}

static void snow_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = 1 << 24;                 /* amplitude / pitch-sweep envelope */
    v->s[1] = 0;                       /* control ticks since strike */
    v->s[2] = v->s[3] = 0;
    v->s[4] = (int32_t)((v->note + 1u) * 0x9e3779b9u);
    v->s[5] = v->s[6] = v->s[7] = 0;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
}

static int snow_done(track_t *t, voice_t *v)
{
    return (snow_role(t) < 4u || !v->gate) && v->s[1] > 1 && v->s[0] < 4096;
}

static int32_t snow_amp(track_t *t, voice_t *v, int32_t adsr)
{
    uint32_t d = (uint32_t)snow_param(t, 3), role = snow_role(t);
    uint32_t ms = (role == 2u ? 8u : 20u) + d * d / (role == 2u ? 8u : 2u);
    ms = ms * (64u + (uint32_t)t->p[P_DEC]) / 128u;
    uint32_t k = (uint32_t)(((uint64_t)CTL * 1000u * 65536u) / (44100u * ms));
    int32_t sustain = role >= 4u && v->gate ? t->p[P_SUS] << 17 : 0;
    (void)adsr;
    if (!v->gate && role >= 4u) k = ENV_EXP[t->p[P_REL] & 127];
    v->s[0] += mulq16(sustain - v->s[0], k > 60000u ? 60000u : k);
    v->s[1]++;
    {
        uint64_t attack = (uint64_t)ENV_LIN[t->p[P_ATK] & 127] * (uint32_t)v->s[1];
        int32_t gain = attack >= (1u << 24) ? 32767 : (int32_t)(attack >> 9);
        return mulq15(mulq15(clamp(v->s[0] >> 9, 0, 32767), gain), v->vel * 258);
    }
}

static void snow_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t role = snow_role(t), i, a = v->ph[0], b = v->ph[1], c = v->ph[2];
    static const int8_t octave[6] = {-24, -12, 24, 0, -12, 0};
    int32_t env = clamp(v->s[0] >> 9, 0, 32767), prev = v->s[6], low = v->s[5], rng = v->s[4];
    int32_t tone = clamp(snow_param(t, 0) + (m->shape - (64 << 8)) / 256, 0, 127);
    int32_t sweep = (snow_param(t, 2) * env) >> 15;
    int32_t pitch = clamp(m->pitch16 + octave[role] * 16 + sweep * (role == 0u ? 6 : 3), 0, 2047);
    uint32_t inc = pitch_inc((uint32_t)pitch), ratio = 128u + (uint32_t)snow_param(t, 1) * 16u;
    uint32_t incm = (uint32_t)(((uint64_t)inc * ratio) >> 8);
    uint32_t inc2 = role == 2u ? (uint32_t)(((uint64_t)inc * (2048u + (uint32_t)p[P_E4] * 12u)) >> 10) :
                    role == 5u ? inc + (uint32_t)(((uint64_t)inc * (uint32_t)p[P_E4]) >> 15) : inc >> 1;
    int32_t index = (tone * (role < 4u ? env : 32767)) >> 8;
    int32_t index0 = v->s[7];
    int32_t cut = clamp(p[P_E6] + m->cutoff / 256, 0, 127);
    int32_t fk = 256 + cut * cut * 3 / 2; /* one-pole, stable and bounded */
    for (i = 0; i < n; i++) {
        int32_t mod = sine_i(b + (uint32_t)(prev * p[P_E5] * 128));
        int32_t smooth_index = index0 + (((index - index0) * (int32_t)(i + 1u)) >> CTL_LOG2);
        int32_t carrier = sine_i(a + (uint32_t)(mod * smooth_index));
        int32_t x = carrier;
        if (role == 0u) {                 /* rounded body plus a short FM strike */
            x = (carrier * (127 - p[P_E4] / 2) + sine_i(c) * (p[P_E4] / 2)) / 127;
        } else if (role == 1u) {          /* tuned FM body and generated snappy noise */
            int32_t noise = (int32_t)(noise32(&rng) >> 16) - 32768;
            x = (carrier * (127 - p[P_E4]) + noise * p[P_E4]) / 127;
        } else if (role == 2u) {          /* inharmonic FM/ring pair, no hat samples */
            x = mulq15(carrier, sine_i(c));
        } else if (role == 3u) {          /* bell/wood body with independent second partial */
            x = (carrier * 3 + sine_i(c + (uint32_t)(mod * p[P_E4] * 256))) / 4;
        } else if (role == 4u) {          /* FM bass and a controllable sine sub */
            x = (carrier * (127 - p[P_E4] / 2) + sine_i(c) * (p[P_E4] / 2)) / 127;
        } else if (role == 5u) {
            x = (carrier + sine_i(c + (uint32_t)(mod * smooth_index))) / 2;
        }
        low += mulq15(x - low, fk);
        if (role == 2u) x -= low; else x = low;
        x = soft_knee(x * (128 + p[P_E7] * 3) / 128, 22000);
        out[i] += voice_amp(x, m, i);
        prev = mod;
        a += inc; b += incm; c += inc2;
    }
    v->ph[0] = a; v->ph[1] = b; v->ph[2] = c;
    v->s[6] = prev; v->s[5] = low; v->s[4] = rng; v->s[7] = index;
}

#define SNOW_PATCH(n,t,s,w,d,mx,fb,cu,dr,su,tag) \
    {n,{t,s,w,d,mx,fb,cu,dr},{0,d,su,40},0,0,FX(0,0,0,0),TAG(tag)}
static const preset_t SNOW_PATCHES[6][3] = {
    {SNOW_PATCH("ROUND KICK",20,0,88,58,0,0,115,14,0,PT_DRUM),
     SNOW_PATCH("HARD KICK",80,8,110,45,0,38,122,60,0,PT_DRUM),
     SNOW_PATCH("FM BOOM",96,33,70,90,0,52,108,35,0,PT_DRUM)},
    {SNOW_PATCH("SNAPPY",70,24,40,40,90,20,105,18,0,PT_DRUM),
     SNOW_PATCH("METAL SNARE",115,65,25,60,48,60,115,35,0,PT_DRUM),
     SNOW_PATCH("SOFT SNARE",40,8,15,35,115,8,90,5,0,PT_DRUM)},
    {SNOW_PATCH("CLOSED HAT",105,81,8,30,50,40,35,8,0,PT_DRUM),
     SNOW_PATCH("OPEN HAT",115,87,0,90,80,55,22,18,0,PT_DRUM),
     SNOW_PATCH("TINY METAL",65,44,25,12,60,65,50,30,0,PT_DRUM)},
    {SNOW_PATCH("FM BELL",95,48,30,80,80,30,115,8,0,PT_DRUM),
     SNOW_PATCH("WOOD HIT",55,20,85,35,40,65,90,20,0,PT_DRUM),
     SNOW_PATCH("ALIEN PERC",120,110,110,65,120,100,120,40,0,PT_FX)},
    {SNOW_PATCH("DEEP BASS",45,8,15,75,90,20,85,18,85,PT_BASS),
     SNOW_PATCH("RUBBER BASS",95,24,65,45,65,65,105,40,65,PT_BASS),
     SNOW_PATCH("HOLLOW BASS",80,40,0,85,100,35,115,8,100,PT_BASS)},
    {SNOW_PATCH("GLASS CHORD",75,24,4,90,30,15,110,5,100,PT_PAD),
     SNOW_PATCH("SOFT CHORD",35,8,8,100,60,5,90,0,110,PT_PAD),
     SNOW_PATCH("METAL CHORD",110,48,30,70,80,65,118,25,80,PT_KEYS)}
};
#undef SNOW_PATCH

#define SNOW_MACHINE(k,n,polyphony,hit,extra) { \
    .name=n, .page_title={"MACROS","FM DETAIL"}, \
    .edit={{"TONE",0,0,127,64,F_PCT,0},{"SHAPE",0,0,127,32,F_PCT,0}, \
           {"SWEEP",0,0,127,32,F_PCT,0},{"DECAY",0,0,127,64,F_PCT,0}, \
           {extra,0,0,127,64,F_PCT,0},{"FEEDBACK",0,0,127,0,F_PCT,0}, \
           {"CUTOFF",0,0,127,110,F_CUTOFF,0},{"DRIVE",0,0,127,0,F_PCT,0}}, \
    .presets=SNOW_PATCHES[k], .npresets=3, .knob={P_E0,P_E1,P_E2,P_E3}, \
    .poly=polyphony, .oneshot=hit, .ownenv=1, .keep=0xe0, .note_on=snow_note_on, \
    .render=snow_render, .amp=snow_amp, .done=snow_done }
static const engine_t SNOW_ENG[6] = {
    SNOW_MACHINE(0,"KICK",1,1,"BODY"), SNOW_MACHINE(1,"SNARE",1,1,"SNAPPY"),
    SNOW_MACHINE(2,"HAT",1,1,"METAL"), SNOW_MACHINE(3,"PERC",1,1,"PARTIAL"),
    SNOW_MACHINE(4,"BASS",1,0,"SUB"), SNOW_MACHINE(5,"CHORD",3,0,"SPREAD")
};
#undef SNOW_MACHINE
