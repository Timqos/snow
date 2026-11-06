# STATTIC 0.8.0-rc5 — simulator and manual

The firmware previously named Snow is now STATTIC. The repository and
historical URLs retain `snow` so existing links and backups keep working.

- [Simulator and current English control manual](https://timqos.github.io/snow/snow/)
- [STATTIC installer](https://timqos.github.io/snow/snow/install/)
- [Original V15/V16 factory installer](https://timqos.github.io/snow/snow/install/factory.html)
- [Agreed five-page interface and exact physical controls](STATTIC_WORK_PLAN.md)
- [Current implementation and verification limits](STATTIC_RELEASE.md)
- [Installation and rollback audit](SNOW_HARDWARE_AUDIT.md)

HOME SELECT now browses five pages. Page 2 uses K1 sequence, K2 sound,
K3 LFO, K4 FX. Page 4 uses six separate physical encoders for KK/SD/HH/PR/BS/CH
patterns P1–P3. On page 5, K1 selects channel and K2 changes volume; the first
six WHITE keys toggle MUTE and first six BLACK keys toggle SOLO.
The complete lower captions share one enlarged 8×14 pixel face. Upper HOME
content remains unchanged. Other menus/editors use 14–28-pixel capitals and
settings show three tall rows with unchanged controls.
The enlarged lower captions are page-specific; the empty HOME visualizer strip
is removed. HOME 3 shows the active parameter’s percentage. GLO SELECT retains unheld tempo control.
All six channels boot at 64 steps. Generation covers four varied bars, with
optional bar 1-to-3 repetition. During PLAY, pattern, HOME mute/solo and all four HOME 2 generator changes
wait for the next shared transport bar. On STOP, they apply immediately.

This is experimental custom firmware. Back up before installation and use
it at your own risk and responsibility, subject to applicable law. Match the
retained startup head when returning to factory V15/V16. Software tests and
browser output do not establish physical-device audio/CPU qualification.
