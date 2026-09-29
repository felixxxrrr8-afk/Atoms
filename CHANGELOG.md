# Changelog

[Русская версия](CHANGELOG.ru.md)

## 4.1

- Settings window (<kbd>F8</kbd> or the gear in the top bar): interface scale, tooltips, notifications, vertical sync, scene background, depth dimming, highlights, bond thickness, mouse sensitivity and inverted axis, camera smoothing, worker threads, undo depth, pause in the background, startup scene or the whole previous session, where snapshots go, window position. Everything is kept in `atoms.ini`.
- Snapshots, frame recordings and CSV exports can go to Pictures\Atoms instead of the program folder; quick save always lives next to `atoms.exe`, no matter where the program was started from.
- The manuals moved to `docs/`; <kbd>F1</kbd> finds them both next to the program and in `docs/`.
- English translation reviewed: consistent terms, US spelling, fixed a few wrong messages.
- Fixes: quick save and CSV export used the current directory, so a shortcut with another working folder put files in unexpected places; a failed font atlas allocation crashed the program; the brush circle and the molecule highlight showed through open windows.
- The README has animations and 34 screenshots in each language, made by `tools/media.ps1`.

## 4.0

- The model is three-dimensional only; the flat mode and the <kbd>D</kbd> key are gone.
- 8 new scenes, 46 in all: liquid and vapor, adsorption, temperature equalization, cavitation, gold nanowire tension, propane combustion, NCl₃ explosion, autoignition.
- The structure library grew from 65 to 127 entries: NO, NO₂, NF₃, PCl₃, BF₃, SiF₄, CS₂, cyclohexane, naphthalene, pyridine, styrene, aniline, glycerol, amino acids, new ions, fullerene C₆₀, Rh and Pb clusters.
- Reference bond data for N–F, P–F, S–F, S–Cl, Si–F, B–F, B–Cl, B–O, C–B, interhalogens, H–Se, H–Ge, H–As; tetrahedral NH₄⁺ and BH₄⁻, pyramidal H₃O⁺.
- The piston can be dragged with the mouse.

## 3.1

- Eight more scenes: adiabatic compression, wetting, seeded crystallization, Poiseuille flow, H₂ + Br₂ in light, chlorine displacing bromine, H₂ + F₂, ozone decomposition.

## 3.0

- The interface speaks English and Russian and switches on the fly (<kbd>Ctrl</kbd>+<kbd>L</kbd>).
- Grayscale interface: only the atoms keep their colors.
- The scene menu is split into Matter and Chemistry; new scenes: heat conduction, shock tube, barometric formula, effusion, sintering, methane chlorination, H₂ + I₂ ⇌ 2HI, hydrogenation on nickel, peroxide decomposition, acetylene combustion.
- A library of 65 structures and a property card for every element.
- English manual and bilingual releases.

## 2.1

- First public version: molecular dynamics and chemistry of 118 elements, thermostats and barostat, field objects, plots, periodic table, saving and loading.
