### English

Download **AtomsSetup.exe** — a one-file installer: pick a folder, get shortcuts on the desktop and in the Start menu, uninstall from the list of apps. No administrator rights needed. Or download **atoms-windows-x64.zip**, unzip it anywhere and run `atoms.exe` — nothing to install.

**New in 5.0**
- Energies on the real scale: reference bond energies, full Coulomb, flexible water, bond stiffnesses from vibrational spectra; the chemistry scenes run at real temperatures
- Valence counted by electrons: hypervalent SF₆, PCl₅, XeF₄, H₂SO₄; lone pairs set the shape of a molecule; Marcus barriers
- Ball-and-stick molecules in element colors, glow of red-hot atoms, a new font
- Orbitals in the simulation: a bond forms only if the atoms meet along a free orbital; the electron clouds are shown right in the scene (<kbd>Shift</kbd>+<kbd>O</kbd>)
- Atom structure (<kbd>F7</kbd>): a glowing electron cloud with a quarter cut away, a single orbital colored by the sign of ψ with its 85% surface, smooth shapes of s, p, d, f orbitals; the mouse wheel zooms into the atom — through the shells to the nucleus of protons and neutrons and the quarks inside a proton
- The average shape of molecules on screen: hydrogen atoms no longer jitter; molecules are placed already in thermal equilibrium
- Light with a wavelength: a quantum breaks a bond only if the molecule absorbs it
- 19 scenes in worlds of other scales: nuclei and chain reactions, quarks, semiconductors, quantum waves
- The Scene tab: periodicity per axis, the kind of each wall, a spherical or cylindrical vessel, grid placement, filling a region
- Fixes in the stability guard, the time step and the energy of reaction events: much lower energy drift in combustion and oxidation scenes

**Inside the archive** (the installer puts the same files into the chosen folder)
- `atoms.exe` — the program
- `MANUAL.html` — the full manual in English, opens with <kbd>F1</kbd> when the interface is in English
- `ИНСТРУКЦИЯ.html` — the same manual in Russian
- `vcomp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` — Microsoft's OpenMP and Visual C++ runtime: they lie next to the program, so nothing else has to be installed, not even the Visual C++ Redistributable
- `LICENSE` — GPL-3.0

**Requirements:** Windows 10 or 11 (x64) and any OpenGL-capable GPU, integrated graphics included.

Windows SmartScreen may warn that the program has no paid code signature: "More info" → "Run anyway".

The full list of changes is in [CHANGELOG.md](https://github.com/felixxxrrr8-afk/atoms-md/blob/main/CHANGELOG.md).

---

### Русский

Скачайте **AtomsSetup.exe** — установщик одним файлом: выберите папку, он сделает ярлыки на рабочем столе и в меню «Пуск», удалить программу можно из списка приложений. Права администратора не нужны. Или скачайте **atoms-windows-x64.zip**, распакуйте в любую папку и запустите `atoms.exe` — без установки.

**Новое в 5.0**
- Энергии в настоящей шкале: справочные энергии связей, полный кулон, гибкая вода, жёсткости связей по колебательным спектрам; химические сцены идут при настоящих температурах
- Валентность по электронам: гипервалентные SF₆, PCl₅, XeF₄, H₂SO₄; неподелённые пары задают форму молекулы; барьеры по Маркусу
- Шаростержневые молекулы цвета элементов, свечение раскалённых атомов, новый шрифт
- Орбитали в симуляции: связь возникает, только если атомы сошлись вдоль свободной орбитали; облака электронов видны прямо в сцене (<kbd>Shift</kbd>+<kbd>O</kbd>)
- Строение атома (<kbd>F7</kbd>): светящееся облако электронов с вырезанной четвертью, орбиталь с цветом по знаку ψ и поверхностью 85%, гладкие формы орбиталей s, p, d, f; колесо мыши приближает внутрь атома — через оболочки к ядру из протонов и нейтронов и к кваркам внутри протона
- Средняя форма молекул на экране: водород больше не дрожит; молекулы ставятся сразу в тепловом равновесии
- Свет с длиной волны: квант рвёт связь, только если молекула его поглощает
- 19 сцен в мирах других масштабов: ядра и цепная реакция, кварки, полупроводники, квантовые волны
- Вкладка «Сцена»: периодичность по осям, вид каждой стенки, сосуд-шар или цилиндр, расстановка по сетке, заливка области
- Исправления стража устойчивости, шага по времени и энергии событий реакций: дрейф энергии в сценах горения и окисления намного меньше

**В архиве** (установщик кладёт те же файлы в выбранную папку)
- `atoms.exe` — программа
- `ИНСТРУКЦИЯ.html` — подробная инструкция, открывается по <kbd>F1</kbd>
- `MANUAL.html` — та же инструкция на английском
- `vcomp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` — библиотеки OpenMP и среды Visual C++ от Microsoft: они лежат рядом с программой, поэтому ничего доустанавливать не нужно, даже Visual C++ Redistributable
- `LICENSE` — лицензия GPL-3.0

**Требования:** Windows 10 или 11 (x64), видеокарта с OpenGL — подойдёт любая встроенная.

SmartScreen может предупредить, что у программы нет платной цифровой подписи: «Подробнее» → «Выполнить в любом случае».

Полный список изменений — в [CHANGELOG.ru.md](https://github.com/felixxxrrr8-afk/atoms-md/blob/main/CHANGELOG.ru.md).
