<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow: хмарна збірка

Поточний Snow: [симулятор 0.6.2](https://timqos.github.io/snow/snow/?v=062).
Наприкінці хмарної збірки обовʼязково виконується
[аудит встановлення й відкату](SNOW_HARDWARE_AUDIT.md): Snow ↔ Skiff,
захищені області флеш, перерваний запис, заводські V15/V16 та сумісність
завантаження після зміни прошивки. Програмні тести не замінюють фізичний FM-1.
Нижче збережено нотатки початкового етапу з базовим Skiff.

Workflow **Snow firmware and simulator** збирає поточну прошивку Skiff для FM-1,
перевіряє її наявним набором тестів і збирає симулятор та конвертер у WebAssembly.
Звук прошивки на цьому етапі не змінено.

**Робочий браузерний симулятор:**
[Snow FM-1 на GitHub Pages](https://timqos.github.io/snow/try/).
Натисніть **Switch on**. Сторінка та її WebAssembly-модуль доступні через HTTPS.
Це базовий Skiff, а не готові нові інструменти Snow.

Резервне приватне прев'ю власника:
[Sites](https://snow-fm1-lab.superfalcon1.chatgpt.site/try/).

На Mac нічого встановлювати не потрібно. Відкрийте вкладку **Actions** у
`Timqos/snow`, виберіть успішний запуск і завантажте результат
`snow-firmware-and-simulator-…`. Він містить сайт, `.fwsc`, контрольну суму,
дані вихідного коміту й ліцензії. Логи доступні окремим artifact.

## Сторінка в браузері

Готова базова сторінка записана в гілку `gh-pages`. Після ввімкнення Pages
власником репозиторію GitHub успішно виконав `pages build and deployment` для
коміту `0667ba4201a7fe611ef3b3dad52ef22bda96d312` і опублікував сайт.
Перевірено HTTP 200 для головної сторінки, `try/`, `felucca.wasm`, worklet
та panel.js; WebAssembly віддається з типом `application/wasm`.
Нижче — маршрут автоматичної публікації майбутніх збірок вихідного коду.

Після потрапляння workflow у `main` у **Settings → Pages → Build and deployment**
потрібно вибрати **GitHub Actions**. Далі в **Actions → Snow firmware and simulator →
Run workflow** вибрати `main` і ввімкнути `publish_pages`.
Workflow публікує лише збірку, що пройшла перевірки.

Для автоматичної публікації наступних змін `main` задайте змінну репозиторію
**SNOW_PUBLISH_PAGES=true**. Без цієї змінної звичайні pushes тільки збираються
й перевіряються. Pull requests та гілки `codex/…` не публікуються.

На сайті головна сторінка описує стан проєкту, **Запустити симулятор** відкриває
`try/`. Це справжній C-код Skiff, зібраний для WebAssembly, а не емуляція
процесора, що завантажує готовий `.fwsc`.

## Повторення у хмарному Linux

Залежності встановлюються у хмарному середовищі, не на комп'ютері користувача:
Python 3.12 з Pillow 13.0.0 і fonttools 4.61.1, Node.js 22, Emscripten 3.1.64
та компілятор JieLi `20260730.1` з `tools/cloud_toolchain.sh`, з перевіркою SHA-256.

```sh
bash tools/cloud_toolchain.sh /workspace/snow-deps/jieli
python3 tools/cloud_sdk.py /workspace/snow-deps/ac79
# Emscripten має бути у PATH.
JIELI_TOOLCHAIN=/workspace/snow-deps/jieli/toolchain \
AC79_SDK=/workspace/snow-deps/ac79 bash tools/cloud_build.sh
```

`cloud_sdk.py` завантажує тільки три необхідні файли з офіційного SDK за
фіксованим тегом і перевіряє SHA-256 з `tools/build.py` перед використанням.
Пакувальник переносить ліцензії й авторство вихідних компонентів.

Виміри базової збірки та початкові обмеження DSP: [BASELINE_AUDIT.md](BASELINE_AUDIT.md).

Офіційні заводські пакети FM-1/SMK-37 і DaisySP не входять у цей checkout:
відповідні додаткові перевірки upstream пропускаються. Перевірки на фізичному
FM-1 виконуються окремо; браузерні тести не вимірюють його реальне навантаження.
