<div align="center">

**English** · [Русский](README.ru.md)

<img src="docs/logo.png" width="96" alt="">

# Atoms

**Molecular dynamics and chemistry in real time**

by **felinebut** · Telegram channel [@felinebut67](https://t.me/felinebut67)

Gas, liquid, crystal, phase transitions and chemical reactions are not scripted here:<br>
they come out of interaction potentials and conservation laws.<br>
Next to them are worlds of other scales: nuclei and chain reactions, quarks, semiconductors and the electron wave.

[![Build](https://github.com/felixxxrrr8-afk/atoms-md/actions/workflows/build.yml/badge.svg)](https://github.com/felixxxrrr8-afk/atoms-md/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/felixxxrrr8-afk/atoms-md?label=download&color=222&style=flat-square)](https://github.com/felixxxrrr8-afk/atoms-md/releases/latest)
![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-444?style=flat-square)
![C++17](https://img.shields.io/badge/C%2B%2B-17-444?style=flat-square)
![OpenGL](https://img.shields.io/badge/OpenGL-1.x-444?style=flat-square)
[![License](https://img.shields.io/badge/license-GPL--3.0-444?style=flat-square)](LICENSE)

### [Download for Windows](https://github.com/felixxxrrr8-afk/atoms-md/releases/latest)

[In motion](#in-motion) · [Features](#features) · [Screenshots](#screenshots) · [Scenes](#scenes) · [Controls](#controls) · [Settings](#settings) · [How it works](#how-it-works) · [Building](#building-from-source)

<br>

<img src="docs/en/combustion.gif" width="640" alt="Hydrogen combustion">

<sub>2H₂ + O₂ → 2H₂O: a spark starts a chain reaction, and the heat of every new bond goes into motion</sub>

</div>

<br>

> [!NOTE]
> The interface is grayscale on purpose: color is kept for the atoms only, so the eye goes straight to the matter. The program speaks English and Russian — <kbd>Ctrl</kbd>+<kbd>L</kbd> switches them on the fly. Scenes are in the <kbd>Tab</kbd> menu, settings are under <kbd>F8</kbd>, and every button explains itself on hover.

## In motion

<table>
<tr>
<td width="50%"><img src="docs/en/nanowire.gif" alt="Gold nanowire"><br><sub><b>Nanowire tension.</b> A gold wire is pulled 1.5% per τ: slip, a neck, a chain of single atoms, rupture</sub></td>
<td width="50%"><img src="docs/en/shock-tube.gif" alt="Shock tube"><br><sub><b>Shock tube.</b> The membrane bursts, and dense hot gas drives a shock wave into the cold side</sub></td>
</tr>
<tr>
<td><img src="docs/en/melting.gif" alt="Crystal melting"><br><sub><b>Crystal melting.</b> Constant-power heating; color shows local order — green FCC turns into gray liquid</sub></td>
<td><img src="docs/en/gold.gif" alt="Gold nanoparticle melting"><br><sub><b>Gold nanoparticle.</b> Many-body metallic bonding; heating melts it from the surface inward</sub></td>
</tr>
<tr>
<td><img src="docs/en/sodium.gif" alt="Sodium in chlorine"><br><sub><b>Sodium in chlorine.</b> 2Na + Cl₂ → 2NaCl: the metal cluster burns, ion pairs gather into salt</sub></td>
<td><img src="docs/en/ncl3.gif" alt="NCl3 explosion"><br><sub><b>NCl₃ explosion.</b> A spark breaks the weak N–Cl bonds; the very strong N≡N bond releases the energy</sub></td>
</tr>
<tr>
<td><img src="docs/en/double-slit.gif" alt="Double slit"><br><sub><b>Double slit.</b> The electron wave passes both slits at once, and the interference fringes build up on the screen dot by dot</sub></td>
<td><img src="docs/en/tunneling.gif" alt="Tunneling"><br><sub><b>Tunneling.</b> A 2 eV packet meets a 3 eV barrier: part of the wave gets through — as much as theory says</sub></td>
</tr>
<tr>
<td><img src="docs/en/proton.gif" alt="Inside a proton"><br><sub><b>Inside a proton.</b> Three quarks on a Y-shaped gluon string; gluons repaint the quarks, but together they always stay “white”</sub></td>
<td><img src="docs/en/nuclear-explosion.gif" alt="Nuclear explosion"><br><sub><b>Nuclear explosion.</b> A supercritical uranium-235 sphere: the number of fissions doubles every ~10 ns until the assembly flies apart</sub></td>
</tr>
</table>

## Features

<table>
<tr>
<td width="50%" valign="top">

**Physics**
- Energies on the real scale: reference bond energies in eV, full Coulomb (DSF method), flexible water, Joung–Cheatham ions
- Lennard-Jones, Morse bonds, bond angles, hydrogen bonds, many-body metallic bonding (Gupta)
- Fast bond vibrations in inner steps (RESPA); Bussi, Langevin, Nosé–Hoover and Berendsen thermostats, C-rescale barostat
- Energy bookkeeping that counts every bit of external work, so integration drift is always visible

</td>
<td width="50%" valign="top">

**Chemistry**
- Bonds form, break and switch partners; reaction heat goes into motion, barriers follow Marcus
- Valence counted by electrons: hypervalent SF₆, PCl₅, XeF₄, H₂SO₄; lone pairs set the shape of a molecule
- Combustion, explosions, acids and bases, Grotthuss proton hopping, pH, catalysis on metal surfaces
- Light with a wavelength: a quantum hν breaks a bond only if the molecule absorbs it — frequency matters, not brightness
- All 118 elements and a library of 149 structures: molecules, ions, crystals, metal clusters

</td>
</tr>
<tr>
<td valign="top">

**Other scales** (the <kbd>Tab</kbd> menu)
- Nuclei: radioactive decay, the radon chain, critical mass, a reactor, a nuclear explosion — real half-lives and cross sections
- Quarks: inside a proton, string breaking, proton collision, a hadron builder, neutron decay
- Semiconductors: a diode, an LED and a solar cell, MOS and bipolar transistors
- Quantum waves: tunneling, the double slit, an oscillator, the quantum carpet, electron diffraction
- Atom structure (<kbd>F7</kbd>): electron clouds and the shapes of s, p, d, f orbitals instead of a planetary model

</td>
<td valign="top">

**Tools and analysis**
- Tweezers, heating and cooling brush, eraser, bond scissors, shock wave, field objects
- Scene editor: periodicity per axis, the kind of each wall (sticky, thermal, absorbing, mirror), a spherical or cylindrical vessel, grid placement, filling a region
- T, P, density and energies in real units; per-atom phase, g(r), Maxwell, MSD
- Reaction log with ΔH, k and K<sub>c</sub>; snapshots, frame recording, CSV, undo, saving

</td>
</tr>
</table>

## Screenshots

**Matter**

<table>
<tr>
<td width="50%"><img src="docs/en/nacl-water.png" alt="NaCl in water"><br><sub><b>NaCl in water.</b> Ions leave the crystal and collect hydration shells</sub></td>
<td width="50%"><img src="docs/en/crystal-melting.png" alt="Crystal melting"><br><sub><b>Crystal melting.</b> The Physics tab: the model's phase diagram and the current state</sub></td>
</tr>
<tr>
<td><img src="docs/en/quench-polycrystal.png" alt="Quench"><br><sub><b>Quench.</b> A polycrystal colored by local structure: FCC grains, HCP stacking faults</sub></td>
<td><img src="docs/en/glass.png" alt="Glass"><br><sub><b>Glass.</b> A fast-quenched Ar/Ne mixture has no time to order: an amorphous solid</sub></td>
</tr>
<tr>
<td><img src="docs/en/ice.png" alt="Ice"><br><sub><b>Ice.</b> Cubic ice held together by directional hydrogen bonds</sub></td>
<td><img src="docs/en/cross-section.png" alt="Cross-section"><br><sub><b>Cross-section.</b> The plane (<kbd>X</kbd>) hides everything in front of it — the inside of a crystal</sub></td>
</tr>
<tr>
<td><img src="docs/en/gold-nanoparticle.png" alt="Gold nanoparticle"><br><sub><b>Gold nanoparticle.</b> FCC inside, melting starts at the surface</sub></td>
<td><img src="docs/en/sintering.png" alt="Sintering"><br><sub><b>Sintering.</b> Au and Ag nanoparticles grow a neck below the melting point</sub></td>
</tr>
<tr>
<td><img src="docs/en/liquid-vapor.png" alt="Liquid and vapor"><br><sub><b>Liquid and vapor.</b> Both densities are compared with the phase diagram</sub></td>
<td><img src="docs/en/cavitation.png" alt="Cavitation"><br><sub><b>Cavitation.</b> A stretched liquid tears apart into vapor bubbles</sub></td>
</tr>
<tr>
<td><img src="docs/en/adsorption.png" alt="Adsorption"><br><sub><b>Adsorption.</b> Gas settles on attracting walls; coverage θ and pressure reach equilibrium</sub></td>
<td><img src="docs/en/wetting.png" alt="Wetting"><br><sub><b>Wetting.</b> The contact angle of a droplet on an attracting wall</sub></td>
</tr>
<tr>
<td><img src="docs/en/poiseuille-flow.png" alt="Poiseuille flow"><br><sub><b>Poiseuille flow.</b> A parabolic velocity profile between two walls</sub></td>
<td><img src="docs/en/heat-conduction.png" alt="Heat conduction"><br><sub><b>Heat conduction.</b> A linear T(x) profile and κ in W/(m·K)</sub></td>
</tr>
<tr>
<td><img src="docs/en/shock-tube.png" alt="Shock tube"><br><sub><b>Shock tube.</b> The measured front speed against theory</sub></td>
<td><img src="docs/en/brownian-motion.png" alt="Brownian motion"><br><sub><b>Brownian motion.</b> A heavy particle among light atoms, MSD ∝ t</sub></td>
</tr>
<tr>
<td><img src="docs/en/nanowire.png" alt="Nanowire"><br><sub><b>Nanowire tension.</b> A gold wire thinned down to a chain of atoms</sub></td>
<td><img src="docs/en/condensation-graphs.png" alt="Condensation"><br><sub><b>Condensation.</b> The Plots tab: f(v), g(r), T, P, E, MSD, molecules, phase diagram</sub></td>
</tr>
</table>

**Chemistry**

<table>
<tr>
<td width="50%"><img src="docs/en/hydrogen-combustion.png" alt="Hydrogen combustion"><br><sub><b>Hydrogen combustion.</b> The event log and the mixture composition on the right</sub></td>
<td width="50%"><img src="docs/en/propane-combustion.png" alt="Propane combustion"><br><sub><b>Propane combustion.</b> C₃H₈ + 5O₂ from a spark, with reaction rates</sub></td>
</tr>
<tr>
<td><img src="docs/en/methane-chlorination.png" alt="Methane chlorination"><br><sub><b>Methane chlorination.</b> UV flashes split Cl₂, and a radical chain makes CH₃Cl and HCl</sub></td>
<td><img src="docs/en/ncl3-explosion.png" alt="NCl3 explosion"><br><sub><b>NCl₃ explosion.</b> 2NCl₃ → N₂ + 3Cl₂</sub></td>
</tr>
<tr>
<td><img src="docs/en/nickel-hydrogenation.png" alt="Hydrogenation on nickel"><br><sub><b>Hydrogenation on nickel.</b> H₂ dissociates on a Ni cluster and adds to ethylene</sub></td>
<td><img src="docs/en/platinum-catalysis.png" alt="Platinum catalysis"><br><sub><b>Platinum catalysis.</b> H₂ and O₂ react only on the surface of a Pt nanoparticle</sub></td>
</tr>
<tr>
<td><img src="docs/en/acid-ph.png" alt="Acid in water"><br><sub><b>Acid in water.</b> HCl + H₂O → H₃O⁺ + Cl⁻, the pH scale and proton hops</sub></td>
<td><img src="docs/en/ozone.png" alt="Ozone decomposition"><br><sub><b>Ozone decomposition.</b> O₃ → O₂ + O on heating</sub></td>
</tr>
<tr>
<td><img src="docs/en/electrophoresis.png" alt="Electrophoresis"><br><sub><b>Electrophoresis.</b> Na⁺ and Cl⁻ drift in opposite directions, water dipoles turn</sub></td>
<td><img src="docs/en/molecule-gallery.png" alt="Structure library"><br><sub><b>Structure library.</b> C₆₀, graphene, diamond, ice, NaCl, silica, metal clusters and organic molecules</sub></td>
</tr>
<tr>
<td><img src="docs/en/valence-gallery.png" alt="Hypervalent molecules"><br><sub><b>Hypervalent molecules.</b> H₂SO₄, SF₆, PCl₅, XeF₄, ClF₃, SF₄, BrF₅, XeF₂, XeO₃, HNO₃, SO₄²⁻, PO₄³⁻: lone pairs set the shape</sub></td>
<td><img src="docs/en/library-panel.png" alt="Chemistry tab"><br><sub><b>Chemistry tab.</b> Reaction switches, the wavelength of light, the event log and the structure library</sub></td>
</tr>
</table>

**The quantum and subatomic world**

<table>
<tr>
<td width="50%"><img src="docs/en/orbital-shapes.png" alt="Orbital shapes"><br><sub><b>Atom structure (F7).</b> The shapes of s, p, d and f orbitals; lobe color is the sign of the wave function</sub></td>
<td width="50%"><img src="docs/en/orbital-cloud.png" alt="Orbital cloud"><br><sub><b>A 3d electron of iron.</b> Points fall with density |ψ|²; on the right, the subshells and where their electrons are</sub></td>
</tr>
<tr>
<td><img src="docs/en/double-slit.png" alt="Double slit"><br><sub><b>Double slit.</b> The Schrödinger equation on a 256×256 grid; color is the phase of the wave, hits on the screen on the right</sub></td>
<td><img src="docs/en/tunneling.png" alt="Tunneling"><br><sub><b>Tunneling.</b> 16.6% got through the barrier; plane-wave theory says 15.3%</sub></td>
</tr>
<tr>
<td><img src="docs/en/reactor.png" alt="Nuclear reactor"><br><sub><b>Nuclear reactor.</b> Water slows the neutrons, boron rods keep k ≈ 1; Monte Carlo neutron transport</sub></td>
<td><img src="docs/en/proton.png" alt="Inside a proton"><br><sub><b>Inside a proton.</b> The Cornell potential and the gluon string; the proton mass is mostly field energy</sub></td>
</tr>
<tr>
<td><img src="docs/en/led.png" alt="LED"><br><sub><b>LED.</b> Electrons and holes meet in the junction and emit photons of the band-gap color</sub></td>
<td><img src="docs/en/mosfet.png" alt="MOSFET"><br><sub><b>MOSFET.</b> The gate voltage gathers an electron channel at the surface; the band diagram on the right</sub></td>
</tr>
</table>

**Interface**

<table>
<tr>
<td width="50%"><img src="docs/en/scenes-menu.png" alt="Scene menu"><br><sub><b>Scene menu.</b> 65 experiments in six sections, many with variants</sub></td>
<td width="50%"><img src="docs/en/periodic-table.png" alt="Periodic table"><br><sub><b>Periodic table.</b> 118 elements; the card shows the electron configuration, physical properties and a thumbnail of the electron cloud</sub></td>
</tr>
<tr>
<td><img src="docs/en/scene-editor.png" alt="Scene editor"><br><sub><b>The Scene tab.</b> Box faces of different kinds: sticky, thermal at 300 K, absorbing, mirror</sub></td>
<td><img src="docs/en/field-objects.png" alt="Field objects"><br><sub><b>Field objects and measurements.</b> A heater, a barrier, a selection and an angle</sub></td>
</tr>
<tr>
<td><img src="docs/en/settings.png" alt="Settings"><br><sub><b>Settings (F8).</b> Interface, molecule model and font, graphics, camera, simulation, files and window</sub></td>
<td><img src="docs/en/help.png" alt="Help"><br><sub><b>Cheat sheet (H).</b> Every key and mouse action on one page</sub></td>
</tr>
</table>

## Quick start

1. From [Releases](https://github.com/felixxxrrr8-afk/atoms-md/releases/latest), download **`AtomsSetup.exe`** — a one-file installer: pick a folder, and it creates shortcuts on the desktop and in the Start menu and registers the program in the list of installed apps (uninstall it from there). No administrator rights needed.
2. Or download `atoms-windows-x64.zip`, unzip it anywhere and run `atoms.exe` — nothing to install.
3. <kbd>Tab</kbd> opens the scene menu, <kbd>E</kbd> the periodic table, <kbd>F7</kbd> the atom structure, <kbd>H</kbd> a cheat sheet, <kbd>F8</kbd> the settings, <kbd>F1</kbd> the full manual.

Windows 10 or 11 (x64) and any OpenGL-capable GPU. SmartScreen may warn that the program is unsigned: "More info" → "Run anyway".

The installer takes command-line switches: `/S` for no window, `/D=folder`, `/nodesktop`, `/nostartmenu`, `/norun`; `uninstall.exe /S` removes the program without questions.

## Scenes

<details>
<summary><b>Matter</b> — 22 scenes</summary>

| Key | Scene | What to watch |
|---|---|---|
| <kbd>1</kbd> | Ideal gas | PV = NkT, Maxwell distribution, pressure on the walls |
| <kbd>2</kbd> | Crystal melting | constant-power heating, the T(t) plateau; variants: HCP, BCC, SC, NaCl, ice |
| <kbd>3</kbd> | Boiling | liquid under gravity, a hot bottom, evaporation |
| <kbd>4</kbd> | Condensation | supersaturated vapor → nuclei → droplets |
| <kbd>5</kbd> | Diffusion | two gases mixing, MSD(t) and the coefficient D |
| <kbd>8</kbd> | Brownian motion | a heavy particle among atoms, MSD ∝ t |
| <kbd>9</kbd> | Quench | polycrystal and grain boundaries; variant: glass |
| <kbd>Shift</kbd>+<kbd>1</kbd> | Gold nanoparticle | metallic bonding, surface melting |
| menu | Heat conduction | hot and cold walls, a linear T(x) profile, Fourier's law and κ |
| menu | Shock tube | shock and rarefaction waves, front speed against theory; variant: Joule expansion |
| menu | Barometric formula | ρ ∝ exp(−mgh/kT): heavy Ar stays below light Ne |
| menu | Effusion | Graham's law: He leaks through slits ≈ 3.2 times faster than Ar |
| menu | Nanoparticle sintering | Au and Ag fuse in the solid state |
| menu | Adiabatic compression | a piston slowly compresses helium: T·V<sup>γ−1</sup> ≈ const |
| menu | Wetting | contact angle of a droplet; variant: non-wetting |
| menu | Seeded crystallization | a supercooled liquid grows on a pinned crystallite |
| menu | Poiseuille flow | a parabolic velocity profile in a channel |
| menu | Liquid and vapor | a liquid film evaporates to saturation; densities against the phase diagram |
| menu | Adsorption | gas settles on attracting walls: coverage θ and pressure |
| menu | Temperature equalization | hot neon and cold krypton: equal energies, different speeds |
| menu | Cavitation | a stretched liquid tears apart into vapor bubbles |
| menu | Nanowire tension | slip, necking, an atomic chain and rupture of a gold wire |

</details>

<details>
<summary><b>Chemistry</b> — 24 scenes</summary>

| Key | Scene | What to watch |
|---|---|---|
| <kbd>6</kbd> | NaCl in hot water | ions from corners and edges go into solution, hydration shells |
| <kbd>7</kbd> | Hydrogen combustion | 2H₂ + O₂ → 2H₂O from a spark, the gas heats up to thousands of kelvins; variant: H₂ + Cl₂ |
| <kbd>0</kbd> | Chemical equilibrium | Cl₂ ⇌ 2Cl at 5500 K under a piston, Le Chatelier's principle |
| <kbd>Shift</kbd>+<kbd>2</kbd> | Iron oxidation | O₂ splits on hot iron, an oxide layer grows |
| <kbd>Shift</kbd>+<kbd>3</kbd> | Sodium in chlorine | 2Na + Cl₂ → 2NaCl without a barrier, the metal glows |
| <kbd>Shift</kbd>+<kbd>4</kbd> | Methane combustion | CH₄ + 2O₂ → CO₂ + 2H₂O |
| <kbd>Shift</kbd>+<kbd>5</kbd> | Electrophoresis | ions in water in a 0.5 V/nm field, current I |
| menu | Acid in water | HCl + H₂O → H₃O⁺ + Cl⁻, Grotthuss mechanism, pH |
| menu | Neutralization | H₃O⁺ + OH⁻ → 2H₂O; variant: titration |
| menu | Ethanol combustion | C₂H₅OH + 3O₂ from a spark |
| menu | Oxyhydrogen | 2H₂ + O₂ in a closed vessel: the gas reaches ~4000 K, the pressure grows tens of times |
| menu | Platinum catalysis | H₂ and O₂ split on the Pt surface, and water assembles from the atoms |
| menu | Methane chlorination | a radical chain started by UV light at 700 K |
| menu | Equilibrium H₂ + I₂ ⇌ 2HI | Bodenstein's experiment at 3500 K, K<sub>c</sub> |
| menu | Hydrogenation on nickel | C₂H₄ + H₂ → C₂H₆ |
| menu | Peroxide decomposition | 2H₂O₂ → 2H₂O + O₂ on platinum |
| menu | Acetylene combustion | 2C₂H₂ + 5O₂ → 4CO₂ + 2H₂O |
| menu | H₂ + Br₂ in light | a slower chain: Br + H₂ is endothermic |
| menu | Chlorine displaces bromine | Cl + HBr → HCl + Br |
| menu | Hydrogen and fluorine | H₂ + F₂ → 2HF without a spark: stray light is enough |
| menu | Ozone decomposition | O₃ → O₂ + O on heating |
| menu | Propane combustion | C₃H₈ + 5O₂ → 3CO₂ + 4H₂O |
| menu | NCl₃ explosion | weak N–Cl bonds give way to the very strong N≡N |
| menu | Autoignition | heating without a spark up to the ignition point; variant: H₂ + Cl₂ |

</details>

<details>
<summary><b>Nuclei</b> — 5 scenes</summary>

| Scene | What to watch |
|---|---|
| Half-life | nuclei decay at random, yet their number falls exactly as N₀·2<sup>−t/T½</sup>; isotopes from F-18 to uranium-238 |
| Radon decay chain | Rn-222 → … → Pb-206: α and β decays, periods from microseconds to years |
| Critical mass | does the number of neutrons grow in a uranium-235 sphere: in the model the sphere becomes critical at a radius of ≈ 9 cm (the Godiva assembly: 8.7 cm) |
| Nuclear reactor | water slows the neutrons, boron rods keep k ≈ 1; without water the reactor dies out |
| Nuclear explosion | a supercritical assembly: an avalanche of fissions, energy in TNT equivalent |

</details>

<details>
<summary><b>Quarks</b> — 5 scenes</summary>

| Scene | What to watch |
|---|---|
| Inside a proton | three colored quarks on a Y-string; variant: a neutron |
| String breaking | the antiquark is pulled harder than 0.9 GeV/fm — the string breaks and creates a new pair; the V(r) plot |
| Proton collision | strings break into chains of mesons — hadron jets |
| Hadron builder | assemble a particle from quarks: baryons and mesons with masses from the PDG tables |
| Neutron decay from the inside | d → u + W⁻ → electron + antineutrino |

</details>

<details>
<summary><b>Semiconductors</b> — 4 scenes</summary>

| Scene | What to watch |
|---|---|
| Diode | a p–n junction, the depletion layer, the current–voltage curve against the Shockley formula |
| LED and solar cell | recombination gives photons of the band-gap color; light creates pairs and current |
| MOSFET | the gate gathers an electron channel, drain current, saturation |
| Bipolar transistor | the base current controls a collector current tens of times larger, β |

</details>

<details>
<summary><b>Quantum waves</b> — 5 scenes</summary>

| Scene | What to watch |
|---|---|
| Tunneling | the transmitted share of the wave against theory; a click on the wave is a position measurement |
| Double slit | interference one electron at a time; a detector at the slits erases it |
| Quantum oscillator | coherent and squeezed states, ⟨x⟩(t) against the classical ball |
| Quantum carpet | a packet in a box spreads and revives after T = 4mL²/(πħ) |
| Electron diffraction | the Davisson–Germer experiment: beams at the angles d·sin θ = nλ |

</details>

## Controls

| | |
|---|---|
| <kbd>Tab</kbd> · <kbd>E</kbd> · <kbd>F7</kbd> · <kbd>F8</kbd> | scene menu · periodic table · atom structure · settings |
| <kbd>H</kbd> · <kbd>F1</kbd> · <kbd>Ctrl</kbd>+<kbd>L</kbd> | cheat sheet · manual · interface language |
| <kbd>Space</kbd> · <kbd>S</kbd> · <kbd>R</kbd> | pause · single step · reset scene |
| <kbd>Alt</kbd>+<kbd>1</kbd>…<kbd>0</kbd>, <kbd>Alt</kbd>+<kbd>F</kbd> | tools and field objects |
| LMB · RMB · <kbd>Shift</kbd>+RMB | tool · heat · cool |
| <kbd>Ctrl</kbd>+LMB, wheel | camera: rotate and zoom; <kbd>V</kbd> fly mode, <kbd>X</kbd> cross-section |
| <kbd>C</kbd> · <kbd>B</kbd> · <kbd>T</kbd> | atom coloring · bonds · trails |
| <kbd>L</kbd> · <kbd>K</kbd> · <kbd>U</kbd> | flash of light (wavelength in the Chemistry tab) · catalyst zone · reverse time |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd> | undo |
| <kbd>F5</kbd> / <kbd>F9</kbd>, <kbd>Ctrl</kbd>+<kbd>S</kbd> / <kbd>Ctrl</kbd>+<kbd>O</kbd> | quick and regular save and load of `.atoms` files |
| <kbd>F12</kbd> · <kbd>Ctrl</kbd>+<kbd>F12</kbd> · <kbd>Ctrl</kbd>+<kbd>E</kbd> | PNG snapshot · frame recording · export plots to CSV |

In the Scene tab, turn on "exactly on grid": a click with the add tool puts one molecule at a grid node on the working plane, and <kbd>Shift</kbd>+drag lays down a row.

Every key, tool and the physics behind them: [MANUAL.html](docs/MANUAL.html) in English and [ИНСТРУКЦИЯ.html](docs/ИНСТРУКЦИЯ.html) in Russian. Both are installed with the program and included in the release archive.

## Settings

<kbd>F8</kbd> or the gear in the top bar. Changes apply at once and are saved to `atoms.ini` next to the program.

| Section | What you can change |
|---|---|
| Interface | language, interface scale (auto or 100–300%), font, tooltips, the atom card under the cursor, the clock in the top bar, how long notifications stay |
| Graphics | molecule model (ball-and-stick, van der Waals, sticks or auto), vertical sync, scene background, depth dimming, highlights on atoms, glow of red-hot atoms, box outline, bond thickness |
| Camera and mouse | rotation sensitivity, inverted vertical axis, wheel step, camera smoothing, auto-rotation speed |
| Simulation | number of worker threads, undo depth, pause while the window is inactive, what to open on startup — the ideal gas, the last scene or the whole previous session |
| Files and window | where snapshots, frames and CSV go (next to the program or in Pictures), which frames to record, remembering the window position, starting in full screen |

## How it works

One simulation step (`mdStep` in [`src/physics.inl`](src/physics.inl)):

```mermaid
flowchart LR
    A["Half kick and drift<br>velocity Verlet"] --> B["Slow forces<br>LJ · DSF Coulomb · H-bonds<br>metal · walls"]
    B --> R["Fast forces<br>bonds and angles<br>in inner steps"]
    R --> C["Half kick"]
    C --> D["Thermostat<br>Bussi · Langevin ·<br>Nosé–Hoover · Berendsen"]
    D --> E["Chemistry<br>bonds break, form<br>and hop; ΔE → motion"]
    E --> F["C-rescale barostat<br>field objects"]
    F --> G["Adaptive<br>time step"]
    G --> A
```

**Interactions.** Energies are on the real scale: 1 eV = 83.0ε. Van der Waals forces use the Lennard-Jones potential with Lorentz–Berthelot mixing. Electrostatics is full Coulomb by the damped shifted force method (DSF, α = 0.23 Å⁻¹, cutoff 9.5 Å) from a table; charged fragments also get their self-energy. Ions use Joung–Cheatham parameters, and water is a flexible SPC/Fw-type model (at 300 K its density is 0.96 g/cm³ and diffusion 2.5·10⁻⁹ m²/s; real water has 0.997 and 2.3·10⁻⁹). Covalent bonds use the Morse potential with reference energies, lengths and stiffnesses from vibrational spectra; angles come from VSEPR domains in which lone pairs push the bonds apart; a double bond resists twisting (π torsion). Metals use the many-body Gupta (Cleri–Rosato) potential. Fast bond vibrations run in RESPA inner steps.

**Chemistry.** Reactions are events inside the same molecular dynamics. Valence is counted by electrons: sulfur, phosphorus, chlorine and xenon with electronegative neighbors become hypervalent (SF₆, PCl₅, H₂SO₄, XeF₄), with up to six bonds per atom. A bond forms when two approaching atoms both have a free valence, breaks when it is overstretched, and an atom switches partners when it overcomes a barrier. The barrier follows Marcus: E = E₀(1 + ΔH/4E₀)² from the reaction's intrinsic barrier E₀ and its heat ΔH, so the barrier multiplier changes rates but not equilibria. The energy of every event is accounted for exactly, so combustion heats the mixture by itself. Proton transfer includes the rearrangement of water around the ions, and metals catalyze the splitting of molecules on their surface (Langmuir–Hinshelwood). Light of wavelength λ breaks a bond with a quantum hν = hc/λ only if the quantum is enough both to break it and to be absorbed: Cl₂ splits in blue light, while methane is transparent down to 144 nm.

**Worlds of other scales.** Nuclei: real half-lives (NUBASE 2020) and Monte Carlo neutron transport with cross sections of uranium, hydrogen and boron. Quarks: the Cornell potential (σ = 0.9 GeV/fm, αs = 0.3), a Y-string through the Fermat point, relativistic dynamics, string breaking with pair creation, hadron masses from the PDG tables. Semiconductors: a two-dimensional drift–diffusion model (the Poisson and continuity equations, the Scharfetter–Gummel scheme) with silicon parameters: the diode current matches the Shockley formula and the MOSFET current matches theory. The electron wave: the Schrödinger equation on a 256×256 grid by the split-step FFT method.

**Units.** Lengths and temperatures are calibrated to argon: σ = 0.3405 nm, ε/k = 139.8 K — the model's critical point matches argon's (150.7 K), and the triple point comes out at ≈ 87 K against the measured 83.8 K.

**Robustness.** A stability guard keeps state snapshots and, if the simulation blows up, rolls back and continues with a smaller step. Everything the user does is counted as external work W, so E − W is conserved and its drift shows how honest the integration is.

### Headless checks

```bash
atoms.exe --selftest                 # every scene in the menu: stability, energy drift, NaN
atoms.exe --gradcheck K N            # forces against −∇U by numerical differentiation
atoms.exe --evcheck K N              # exact energy balance of every reaction
atoms.exe --kin K N T V              # long run of a scene: phase, reactions, live measurement
atoms.exe --phystest MODE K N        # coex, melt, triple, npt, vir, fo, guard
atoms.exe --water T P N              # the water model: density, energy, diffusion
atoms.exe --chemtest lib             # insert every library structure and check it stays intact
atoms.exe --nuck                     # criticality of a uranium sphere and of the reactor
atoms.exe --semi                     # current–voltage curves of the diode and the transistors
atoms.exe --walltest                 # Scene tab walls: mirror, thermal, absorbing, vessel
atoms.exe --lighttest                # photodissociation threshold for light of different wavelengths
atoms.exe --uitest [--langcheck]     # click through the whole interface; list untranslated strings
atoms.exe --lang en --shot K N png   # render scene K and save a window snapshot
```

Reports go to `.log` files next to the program. The pictures on this page are made by [`tools/media.ps1`](tools/media.ps1): it runs `--shot` for every screenshot and records frames for the animations, which [`tools/gif.py`](tools/gif.py) turns into GIFs.

## Building from source

No external libraries: MSVC from Visual Studio or Build Tools is enough.

```bash
build.bat
```

`build.bat` finds Visual Studio by itself; `build.bat asan` builds a debug version with AddressSanitizer, and `build.bat setup` also builds the installer `AtomsSetup.exe` (the program, manuals, license and the OpenMP runtime inside). By hand, from an x64 Native Tools Command Prompt:

```bash
rc /nologo /fo res\atoms.res res\atoms.rc
cl /O2 /openmp /utf-8 /EHsc /std:c++17 /fp:fast main.cpp res\atoms.res /Fe:atoms.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib opengl32.lib comdlg32.lib shell32.lib
```

Every push to `main` is built by [GitHub Actions](https://github.com/felixxxrrr8-afk/atoms-md/actions); a new `ProductVersion` in `res/atoms.rc` publishes a new release with the archive and the installer.

### Layout

One translation unit: `main.cpp` includes the modules from `src/` in order. Comments in the code are in Russian.

| Path | Contents |
|---|---|
| [`main.cpp`](main.cpp) | entry point and the list of headless modes |
| [`src/core.inl`](src/core.inl) | constants and units, elements and their properties, system state |
| [`src/settings.inl`](src/settings.inl) | program settings and `atoms.ini` |
| [`src/physics.inl`](src/physics.inl) | forces, DSF Coulomb, neighbor lists, RESPA, walls and vessels, thermostats, barostat, field objects, stability guard |
| [`src/chemistry.inl`](src/chemistry.inl) | reactions and barriers, valence, proton transfer, catalysis, light, pH, bond table, the structure library |
| [`src/analysis.inl`](src/analysis.inl) | measurements for plots, local structure, per-atom phase |
| [`src/presets.inl`](src/presets.inl) | scenes, their live measurements, undo |
| [`src/render.inl`](src/render.inl) | OpenGL rendering: the ball-and-stick model, glow, box faces and vessels, the camera |
| [`src/ui.inl`](src/ui.inl) | panels, tools, plots, periodic table, scene menu |
| [`src/panel_phys.inl`](src/panel_phys.inl), [`src/panel_chem.inl`](src/panel_chem.inl), [`src/panel_scene.inl`](src/panel_scene.inl), [`src/panel_settings.inl`](src/panel_settings.inl) | the Physics, Chemistry and Scene tabs, the settings window |
| [`src/orbitals.inl`](src/orbitals.inl) | atom structure: electron clouds and orbital shapes |
| [`src/world_nuclear.inl`](src/world_nuclear.inl), [`src/world_quark.inl`](src/world_quark.inl), [`src/world_semi.inl`](src/world_semi.inl), [`src/world_wave.inl`](src/world_wave.inl), [`src/worlds.inl`](src/worlds.inl) | the worlds of nuclei, quarks, semiconductors and the electron wave; their panels |
| [`src/app.inl`](src/app.inl) | window and main loop, input, saving, PNG, self-tests |
| [`src/lang.inl`](src/lang.inl), [`src/lang_table.inl`](src/lang_table.inl) | language switching and the Russian → English string table |
| [`installer/`](installer) | the one-file installer and the uninstaller |
| [`res/`](res) | icon and version resource |
| [`docs/`](docs) | manuals in two languages, screenshots and animations (`docs/en`, `docs/ru`) |
| [`tools/`](tools) | the translation table checker and the scripts that make pictures for this page |

See [CHANGELOG.md](CHANGELOG.md) for what changed between versions.

## Simplifications

Atoms move by classical mechanics; quantum effects are shown where they cannot be avoided: electron clouds, the electron wave in its own scenes, the absorption threshold of light. Each atom pair has a single bond curve, and electron transfer is not modeled explicitly: oxidation happens through bonds. The simulation covers picoseconds, so reactions that are slow at room temperature run in the scenes at high temperatures or from a spark or light. Water autoionization is left out: in a box of ~10⁻²³ L its equilibrium is zero ions. Quarks are a string model rather than quantum chromodynamics; nuclei are point-like with Monte Carlo neutron transport; the semiconductor is two-dimensional.

## License

[GPL-3.0](LICENSE). You may use, study and modify the code; derived programs must be distributed with source code under the same license.
