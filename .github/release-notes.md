### English

Download **atoms-windows-x64.zip**, unzip it anywhere and run `atoms.exe`. Nothing to install.

**New in 4.1**
- Settings window: <kbd>F8</kbd> or the gear in the top bar. Interface scale, tooltips, notifications, vertical sync, scene background, depth dimming, bond thickness, mouse sensitivity, camera smoothing, worker threads, undo depth, pause in the background, what to open on startup (including the whole previous session), where snapshots go, window position. Everything is saved in `atoms.ini`.
- Snapshots, frame recordings and CSV can go to Pictures\Atoms; quick save always lives next to `atoms.exe`
- English translation reviewed
- Fixes: quick save and CSV export no longer depend on the working folder; a failed font atlas allocation no longer crashes the program; the brush circle no longer shows through open windows

**Inside the archive**
- `atoms.exe` — the program
- `MANUAL.html` — the full manual in English, opens with <kbd>F1</kbd> when the interface is in English
- `ИНСТРУКЦИЯ.html` — the same manual in Russian
- `vcomp140.dll` — Microsoft's OpenMP runtime; needed if the Visual C++ Redistributable is not installed
- `LICENSE` — GPL-3.0

**Requirements:** Windows 10 or 11 (x64) and any OpenGL-capable GPU, integrated graphics included.

Windows SmartScreen may warn that the program has no paid code signature: "More info" → "Run anyway".

The full list of changes is in [CHANGELOG.md](https://github.com/felixxxrrr8-afk/atoms-md/blob/main/CHANGELOG.md).

---

### Русский

Скачайте **atoms-windows-x64.zip**, распакуйте в любую папку и запустите `atoms.exe`. Устанавливать ничего не нужно.

**Новое в 4.1**
- Окно настроек: <kbd>F8</kbd> или шестерёнка в верхней панели. Масштаб интерфейса, подсказки, уведомления, вертикальная синхронизация, фон сцены, затемнение дальних атомов, толщина связей, чувствительность мыши, плавность камеры, число потоков, глубина отмены, пауза в фоне, что открывать при запуске (в том числе весь прошлый сеанс), куда сохранять снимки, положение окна. Всё хранится в `atoms.ini`.
- Снимки, запись кадров и CSV можно сохранять в «Изображения\Атомы»; быстрое сохранение всегда рядом с `atoms.exe`
- Вычитан английский перевод
- Исправления: быстрое сохранение и экспорт CSV больше не зависят от рабочей папки; при нехватке памяти под атлас шрифтов программа не падает; кисть не просвечивает сквозь открытые окна

**В архиве**
- `atoms.exe` — программа
- `ИНСТРУКЦИЯ.html` — подробная инструкция, открывается по <kbd>F1</kbd>
- `MANUAL.html` — та же инструкция на английском
- `vcomp140.dll` — библиотека OpenMP от Microsoft; нужна, если нет Visual C++ Redistributable
- `LICENSE` — лицензия GPL-3.0

**Требования:** Windows 10 или 11 (x64), видеокарта с OpenGL — подойдёт любая встроенная.

SmartScreen может предупредить, что у программы нет платной цифровой подписи: «Подробнее» → «Выполнить в любом случае».

Полный список изменений — в [CHANGELOG.ru.md](https://github.com/felixxxrrr8-afk/atoms-md/blob/main/CHANGELOG.ru.md).
