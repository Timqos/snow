/* SPDX-License-Identifier: GPL-3.0-only */
/* Snow 0.2 reuses retired DIGITAL/kit parameters; stored IDs and P_COUNT stay stable. */
/* The retired FX envelope amount is now SEL input mode. Zero preserves the
 * one-key chords of existing Snow projects; one plays individual notes. */
#define SN_PLAY_MODE P_ED_FX
#define SN_CH_RELEASE 113 /* ESYNC index 22: two bars */
#define SN_CH_VOICES 4u   /* CH total, including held notes and release tails. */
#define ST_PATTERNS 3u    /* STATTIC: three visible/performance patterns per channel */
/* Retired lane DECAY slots were zero in Snow: old projects stay neutral. */
#define SN_DJ_FILTER P_LDECAY5
#define SN_WIDTH P_LDECAY6
/* Append after the original sixteen scales: their stored numbers never move.
 * One definition supplies both the selector names and the pitch-class masks. */
#define SN_EXTRA_SCALES(X) \
    X("8-TONE SPANISH",0x57b) \
    X("BHAIRAV",0x9b3) \
    X("DORIAN #4",0x6cd) \
    X("HIRAJOSHI",0x18d) \
    X("INSEN",0x4a3) \
    X("IWATO",0x463) \
    X("KUMOI",0x28d) \
    X("LOCRIAN SUPER",0x55b) \
    X("LYDIAN AUGMENTED",0xb55) \
    X("LYDIAN DOMINANT",0x6d5) \
    X("MAJOR HARMONIC",0x9b5) \
    X("MESSIAEN 3",0xddd) \
    X("MESSIAEN 4",0x9e7) \
    X("MESSIAEN 5",0x8e3) \
    X("MESSIAEN 6",0xd75) \
    X("MESSIAEN 7",0xbef) \
    X("MINOR HUNGARIAN",0x9cd) \
    X("MINOR MELODIC DOWN",0x5ad) \
    X("PHRYGIAN DOMINANT",0x5b3)
#define SN_ALG P_FM1_ATK
#define SN_WAVE1 P_FM1_DEC
#define SN_WAVE2 P_FM1_SUS
#define SN_RATIO P_FM1_REL
#define SN_NOISE P_FM1_LEVEL
#define SN_TUNE P_FM2_ATK
#define SN_FINE P_FM2_DEC
#define SN_MODE P_FM2_SUS
#define SN_PDEC P_FM2_REL
#define SN_PAMT P_FM2_LEVEL
#define SN_FILTER P_FM3_ATK
#define SN_RESO P_FM3_DEC
#define SN_PW P_FM3_SUS
#define SN_SUB P_FM3_REL
#define SN_MIX P_FM3_LEVEL
#define SN_WAVE3 P_FM4_ATK
#define SN_RATIO3 P_FM4_DEC
#define SN_FMDEC P_FM4_SUS
#define SN_FB_ROUTE P_FM4_REL
#define SN_KEYTRACK P_FM4_LEVEL
#define SN_G_DENS P_LN0
#define SN_G_SYNC P_LN1
#define SN_G_SEED P_LN2
#define SN_G_SPAN P_LN3
#define SN_G_CONTOUR P_LN4
#define SN_G_ACCENT P_LN5
#define SN_G_KIND P_LN6
#define SN_G_LIVE P_LN7
#define SN_G_BASE P_LTUNE0
/* Former genre selector is now the reference groovebox's parameter-lock amount. */
#define SN_G_LOCKS SN_G_STYLE
#define SN_G_SEED_B P_LTUNE1
#define SN_G_SEED_C P_LTUNE2
#define SN_G_SEED_D P_LTUNE3
#define SN_G_SCOPE P_LTUNE4
#define SN_L2_RATE P_LPAN0
#define SN_L2_WAVE P_LPAN1
#define SN_L2_DEPTH P_LPAN2
#define SN_L2_DEST P_LPAN3
#define SN_L2_SYNC P_LPAN4
#define SN_L2_PHASE P_LPAN5
#define SN_L2_TRIG P_LPAN6
#define SN_L2_FADE P_LPAN7
