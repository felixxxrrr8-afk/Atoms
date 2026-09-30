# Changelog

[Русская версия](CHANGELOG.ru.md)

## 5.0

- Energies on the real scale: 1 eV = 83ε (was 4ε). Reference bond energies, full Coulomb by the DSF method, flexible SPC/Fw-type water, Joung–Cheatham ions, bond stiffnesses from vibrational spectra; fast bond vibrations run in RESPA inner steps. The chemistry scenes were retuned to real temperatures: water at 300 K, combustion from a spark up to thousands of kelvins.
- Valence counted by electrons: hypervalent molecules (SF₆, PCl₅, XeF₄, H₂SO₄, HClO₄ and others), lone pairs as VSEPR domains, π torsion of double bonds, up to six bonds per atom. The library has 149 structures.
- Reaction barriers follow Marcus, from the intrinsic barrier and the reaction heat; proton transfer with water rearrangement; Langmuir–Hinshelwood catalysis on metal surfaces.
- Light with a wavelength (the Chemistry tab, key <kbd>L</kbd>): a quantum breaks a bond only if the molecule absorbs it — Cl₂ splits in blue light, methane only in the far ultraviolet.
- Ball-and-stick molecules: glossy balls in element colors and two-colored cylinder bonds, multiple bonds as parallel tubes; also van der Waals spheres and sticks. Red-hot atoms glow by Planck's law. A new interface font.
- Atom structure (<kbd>F7</kbd>): electron clouds sampled from |ψ|², single orbitals and the shapes of s, p, d, f; a cloud thumbnail in the atom card.
- Worlds of other scales, 19 scenes: nuclei (decay, the radon chain, critical mass, a reactor, a nuclear explosion), quarks (inside a proton, string breaking, proton collision, a hadron builder, neutron decay), semiconductors (a diode, an LED and a solar cell, MOS and bipolar transistors), quantum waves (tunneling, the double slit, an oscillator, the quantum carpet, electron diffraction).
- The Scene tab is an editor: periodicity per axis, the kind of each wall (normal, sticky, thermal with its own temperature, absorbing, mirror), a spherical or cylindrical vessel, exact grid placement of molecules and filling a region.
- A one-file installer `AtomsSetup.exe`: a folder of your choice, shortcuts, uninstall from the list of apps; the portable archive stays.
- Fixes: after a cold scene the stability guard rolled a hot one back on every step; right after a spark the first steps ran with a "cold" time step (energy drift in combustion scenes is 10–20 times lower now); switching periodic boundaries to walls tore molecules apart at the box edge.
- New headless checks: `--semi`, `--nuck`, `--water`, `--walltest`, `--lighttest`.

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
