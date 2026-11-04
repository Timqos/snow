<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow: хмарна збірка

Workflow **Snow firmware and simulator** збирає поточну прошивку Skiff для FM-1,
перевіряє її наявним набором тестів і збирає симулятор та конвертер у WebAssembly.
Звук прошивки на цьому етапі не змінено.

На Mac нічого встановлювати не потрібно. Відкрийте вкладку **Actions** у
`Timqos/snow`, виберіть успішний запуск і завантажте результат
`snow-firmware-and-simulator-…`. Він містить сайт, `.fwsc`, контрольну суму,
дані вихідного коміту й ліцензії. Логи доступні окремим artifact.

## Сторінка в браузері

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
та компілятор JieLi з `tools/get_toolchain.sh`.

```sh
sh tools/get_toolchain.sh /workspace/snow-deps/jieli
python3 tools/cloud_sdk.py /workspace/snow-deps/ac79
# Emscripten має бути у PATH.
JIELI_TOOLCHAIN=/workspace/snow-deps/jieli/toolchain \
AC79_SDK=/workspace/snow-deps/ac79 bash tools/cloud_build.sh
```

`cloud_sdk.py` завантажує тільки три необхідні файли з офіційного SDK за
фіксованим тегом і перевіряє SHA-256 з `tools/build.py` перед використанням.
Пакувальник переносить ліцензії й авторство вихідних компонентів.

Офіційні заводські пакети FM-1/SMK-37 і DaisySP не входять у цей checkout:
відповідні додаткові перевірки upstream пропускаються. Перевірки на фізичному
FM-1 виконуються окремо; браузерні тести не вимірюють його реальне навантаження.
