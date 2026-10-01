# Changelog

[Русская версия](CHANGELOG.ru.md)

## 5.0

- Energies on the real scale: 1 eV = 83ε (was 4ε). Reference bond energies, full Coulomb by the DSF method, flexible SPC/Fw-type water, Joung–Cheatham ions, bond stiffnesses from vibrational spectra; fast bond vibrations run in RESPA inner steps. The chemistry scenes were retuned to real temperatures: water at 300 K, combustion from a spark up to thousands of kelvins.
- Valence counted by electrons: hypervalent molecules (SF₆, PCl₅, XeF₄, H₂SO₄, HClO₄ and others), lone pairs as VSEPR domains, π torsion of double bonds, up to six bonds per atom. The library has 149 structures.
- Reaction barriers follow Marcus, from the intrinsic barrier and the reaction heat; proton transfer with water rearrangement; Langmuir–Hinshelwood catalysis on metal surfaces.
- Light with a wavelength (the Chemistry tab, key <kbd>L</kbd>): a quantum breaks a bond only if the molecule absorbs it — Cl₂ splits in blue light, methane only in the far ultraviolet.
- Ball-and-stick molecules: glossy balls in element colors and two-colored cylinder bonds, multiple bonds as parallel tubes; also van der Waals spheres and sticks. Red-hot atoms glow by Planck's law. A new interface font.
- Orbitals in the simulation: every atom in a molecule has clouds for its bonds, lone pairs and unpaired electrons, arranged by VSEPR. A bond forms only if the atoms meet along a free orbital (the steric factor): a radical takes an atom along the axis of its p orbital, an atom is pulled off from behind its bond, a radical adds to a double bond from the side, through the π cloud, and a proton lands on a lone pair. The switch is in the Chemistry tab; the orbitals layer (<kbd>Shift</kbd>+<kbd>O</kbd>) shows the clouds right in the scene.
- Atom structure (<kbd>F7</kbd>): the cloud is ray-traced as a glowing gas — the whole atom with a quarter cut away, showing the shell layers and the nodes between them; a single orbital is colored by the sign of ψ, with a glossy surface enclosing 85% of the probability; the s, p, d, f shapes are smooth glossy figures. A cloud thumbnail in the atom card. The mouse wheel (and <kbd>+</kbd> <kbd>−</kbd>, the atom · 1s · nucleus · quarks buttons) zooms in up to 140,000 times: the cloud turns into a cross-section through the nucleus with labeled shell rings, inside the 1s shell the cloud is almost uniform, then comes the nucleus of protons and neutrons, and inside a proton three quarks and the gluon field string; a scale bar (Å, pm, fm) at the bottom. Shell sizes follow Slater's rules with the effective quantum number n* (uranium was drawn with an 18 Å radius, now 5 Å), a filled f subshell is a sphere, the radial plot uses a logarithmic r. Fixed: dark "scratches" on orbital lobes, labels overflowing the buttons for heavy atoms, the axes jittering against the cloud, the colors of the angular part in the corner — now the sign of ψ, as in the cloud.
- The average shape of molecules on screen: bonds to hydrogen vibrate in 10–30 fs, faster than a frame, and H atoms jittered. At room temperature these vibrations are "frozen", so the shape averaged over about 60 fs is drawn; the simulation does not change, and it can be turned off in the settings. The number of steps per frame is chosen in advance, so time runs evenly, without jerks.
- Molecules are placed already in thermal equilibrium: vibrations get their share of energy at once. Previously hydrogen in a gas stayed a hundred kelvins colder than its neighbors.
- Worlds of other scales, 19 scenes: nuclei (decay, the radon chain, critical mass, a reactor, a nuclear explosion), quarks (inside a proton, string breaking, proton collision, a hadron builder, neutron decay), semiconductors (a diode, an LED and a solar cell, MOS and bipolar transistors), quantum waves (tunneling, the double slit, an oscillator, the quantum carpet, electron diffraction).
- The Scene tab is an editor: periodicity per axis, the kind of each wall (normal, sticky, thermal with its own temperature, absorbing, mirror), a spherical or cylindrical vessel, exact grid placement of molecules and filling a region.
- A one-file installer `AtomsSetup.exe`: a folder of your choice, shortcuts, uninstall from the list of apps; the portable archive stays. The OpenMP and Visual C++ runtime DLLs lie next to the program, so it starts on a clean Windows without the Visual C++ Redistributable; the build checks this itself (`tools/checkdeps.ps1`).
- Energy drift: a reaction event did not recompute the pairs of neighbors whose distance along bonds changed (1-3 and 1-4) — iron with oxygen drifted by 5–8% in 13 ps, now 0.1%; the first step of a hot scene ran with the base time step (a 0.2% jump, and an "explosion" without RESPA); after shrinking, the time step grows more carefully, and combustion keeps its energy 3–5 times better; a spark no longer pushes the whole gas.
- Fixes: after a cold scene the stability guard rolled a hot one back on every step; right after a spark the first steps ran with a "cold" time step; switching periodic boundaries to walls tore molecules apart at the box edge; the analysis called a hot salt solution "crystallization" (ice is now judged by the tetrahedral order of a molecule averaged over a picosecond); ice in the melting scene melted within the first picosecond — all protons in the lattice were ordered, every molecule's dipole pointed the same way, and the strain of the ideal lattice heated the crystal by a hundred kelvins. Now the protons are disordered as in real ice, the lattice is relaxed along the forces before the start, the crystal is larger and is heated from 100 K: first the surface premelts, then the core melts at about 270–300 K; minimizing the window recomputed the interface scale for a 200×200 window and rebuilt the fonts twice.
- New headless checks: `--semi`, `--nuck`, `--water`, `--walltest`, `--lighttest`, `--jitter` (smoothness of the picture and the temperature of each element).

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
