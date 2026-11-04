<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow 0.2: відкритий синтез і генеративний секвенсор

Симулятор: **https://timqos.github.io/snow/snow/**.
Базовий Skiff: https://timqos.github.io/snow/try/.
Мета проєкту: [SNOW_SPEC.md](SNOW_SPEC.md).

## Що змінилося після 0.1

- **Шість універсальних каналів.** Усі 18 стартових звуків доступні на
  будь-якому каналі. Назви ударних/басу/акордів не обмежують синтез.
- Три осцилятори; алгоритми FM PAIR, FM CHAIN, FM PAR, RING, ADDITIVE,
  FOLD. Хвилі кожного: sine, triangle, saw, square, pulse, organ.
  Ratio, pulse width, noise, mix, oscillator 3 level, feedback/routing,
  tuning, fine tune й keytracking редагуються окремо.
- Повна **AMP ADSR**, режими HIT/GATED, незалежні pitch amount/decay та
  FM decay. Ударний pitch envelope більше не тягнеться разом із довгим
  amplitude decay. Тест основи стартового кіка після transient: **53.3 Гц**.
- Filter OFF/LP/HP/BP, resonance, cutoff, drive; envelope 2, matrix
  modulation. Два справді незалежні LFO. Колишня сторінка LFO SYNC
  не рахується другим LFO: LFO 2 має окремі phase/state/rate та destination.
- **19 видимих EDIT-сторінок** на кожному каналі, включно з ADSR,
  осциляторами, pitch, filter, LFO і voice. Веблабораторія відкриває
  **110 параметрів** із реальних descriptors C-коду прошивки, з units та
  enum-значеннями. Повзунки не реалізують окремий браузерний синтезатор.
- Власний детермінований генератор: weighted-grid rhythm, scale-constrained
  melody walk/rise/fall/arch/motif, seed, density, syncopation, accents,
  span, root/scale. RHYTHM, MELODY або BOTH; variation та EVOLVE раз на
  1/2/4 цикли патерна. Згенеровані кроки можна редагувати. Locks та інші
  канали зберігаються. Алгоритми Tessera не копіювалися.
- **Патерни A–P незалежно на кожному каналі**, на наявному банку Skiff.
  Нові слоти порожні; перехід зберігає поточні кроки. Під час гри —
  штатне квантування запуску; інші канали не перезапускаються.
  Патерни зберігають sequence/locks; звук — окрема властивість каналу.
- Збережені Control All з маскою, locks, RAM Temp Save/Reload під час гри,
  проєкти, probability, ratchets, microtiming, swing та довжини до 64 кроків.

## Як перевірити

1. **Увімкнути звук → Завантажити демо → PLAY / STOP** вгорі сторінки.
2. Обрати CH 1–6. У «Стартовий звук» кожен канал має ті самі 18 пресетів.
3. Відкрити OSCILLATORS / ALGORITHM та AMP ADSR. Міняти алгоритм, хвилі,
   noise, envelope mode, attack/decay/sustain/release; для sustain/release
   обрати GATED. Для чистої перевірки кіка — стартовий звук ROUND KICK.
4. GENERATIVE SEQUENCER: встановити density, generate kind, seed, scale,
   contour. «Генерувати з seed» повторює результат; «Нова варіація» змінює
   seed. EVOLVE автоматично змінює вибраний канал раз на задані цикли.
5. Перейти на B потрібного каналу, створити інший рисунок і повернутися
   на A. Решта каналів продовжує свої патерни.
6. Temp Save → змінити звук/кроки → Reload all або Reload channel.

Демо замінює поточні кроки всіх каналів і вимикає EVOLVE. Ключ збереження
браузера — `snow-fm1-prototype-02`: дані 0.1 зі зміненими значеннями retired
параметрів не імпортуються автоматично. Temp Save існує до перезавантаження;
постійні проєкти зберігаються звичайним SAVE в цьому браузері.

На FM-1: EDIT відкриває параметри, SEQ → GEN… → **OCT+** генерує варіацію.
SAVE → **PATTERNS 1–4 / PATTERNS 5–6**: кожна K-ручка вибирає A–P свого каналу.
GLO + K1–K4 — Control All; GLO + SAVE — Temp Save; GLO + HOME — Reload;
GLO + OCT− — маска; GLO + OCT+ — Reload одного каналу.
У браузері Shift-клік утримує GLO, ручки панелі тягнуться вертикально.

## Збірка та перевірки

```sh
FELUCCA_SNOW=1 sh build.sh
python3 tools/snow_preview.py
```

Результат `build/snow-preview/`: WASM, веблабораторія й `snow-0.2.fwsc`.
Хмарні залежності: [CLOUD_BUILD.md](CLOUD_BUILD.md).

`tests/snow_test.c`: 18 пресетів на всіх шести каналах, kick body frequency,
алгоритми/хвилі, ADSR, незалежність LFO, доступність EDIT, seed/density/scale,
збереження locks, EVOLVE, перехід між патернами, RAM Reload без зупинки,
відновлення проєкту. `tests/snow_wasm.mjs`: реальний WASM-аудіомікс,
детермінованість, Control All/Reload, 110 параметрів, генерація/патерни,
пакети та metadata AudioWorklet. Базовий набір Skiff перевіряється окремо.

Апаратна збірка 0.2: image 563740 B; RAM data+bss 84504/98304 B;
pool 310280/356352 B; RAM code+ramx 24240/24576 B. Залишаються ввімкненими
перевірки бюджету RAM/Flash, ROM, упаковки й HAL. Числа остаточної збірки
можуть трохи відрізнятися; її лог є джерелом точних значень.

## Що ще не завершено

Незалежні chorus/delay/reverb для кожного каналу: наразі є власні
filter/drive/insert, а send-ефекти спільні. Фінальне піксельне оформлення
й подальше розширення синтезу теж попереду. Якість кінцевого набору звуків
потребує прослуховування. На фізичному FM-1 ще не перевірено CPU, затримку
та стабільність; браузерні/host-тести цього не підтверджують. Код нічого
не встановлює на пристрій.
