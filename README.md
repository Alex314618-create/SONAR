<div align="center">

# S O N A R

<br>

![C11](https://img.shields.io/badge/C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL_3.3-5586A4?style=for-the-badge&logo=opengl&logoColor=white)
![SDL2](https://img.shields.io/badge/SDL2-1D3557?style=for-the-badge&logo=SDL&logoColor=white)
![OpenAL](https://img.shields.io/badge/OpenAL_Soft-0D1B2A?style=for-the-badge)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Blender](https://img.shields.io/badge/Blender-E87D0D?style=for-the-badge&logo=blender&logoColor=white)

<br>

*You cannot see. You can only listen.*

**A first-person atmospheric exploration game where darkness is absolute and sound is your only light.**




## What is SONAR?

SONAR is a single-player, first-person exploration game in the tradition of atmospheric horror — but without jump scares or combat. The tension comes from **darkness and resource scarcity**. Every sonar pulse costs energy. Fire too often and you go blind; conserve too much and you get lost.

The world reveals itself as a **pointillistic point cloud**. Over time, the accumulated echoes build a ghostly, impressionistic portrait of the space you inhabit.



---

<br>

## The Story is Unfinished

> *...and that's by design.*

The engine and core systems are complete. The narrative is intentionally left **open**. SONAR is built as a story-driven framework: the architecture supports branching levels, environmental storytelling through trigger galleries, creature encounters, locked-door puzzles, and password mechanisms — all configurable through Blender without touching C code.

What's missing is the story itself:

- *Who is the player?*
- *What happened to this place?*
- *Why is it dark?*
- *What waits at the end?*

**These questions are yours to answer.**

<br>

---

<br>

## Looking for Contributors

<div align="center">

*SONAR needs people, not just code.*

</div>

<br>

The codebase is stable, documented, and modular — designed for people to join at any layer.

| | Role | What you'd work on |
|:---:|------|-------------------|
| <img src="https://img.shields.io/badge/-E87D0D?style=flat-square" height="14"/> | **3D Artist** | Design levels and creatures in Blender. Export as `.glb`. No code required. See the [Blender Guide](docs/guides/blender-level-authoring.md). |
| <img src="https://img.shields.io/badge/-9B59B6?style=flat-square" height="14"/> | **Narrative Designer** | Build the story. Design level progression, environmental clues, the narrative arc. The engine supports it — it just needs a voice. |
| <img src="https://img.shields.io/badge/-2ECC71?style=flat-square" height="14"/> | **Sound Designer** | Ambient soundscapes, creature vocalizations, interaction cues. The player hears the world before they see it. |
| <img src="https://img.shields.io/badge/-E74C3C?style=flat-square" height="14"/> | **Composer** | Atmospheric music, generative ambient layers, tension-reactive audio. |
| <img src="https://img.shields.io/badge/-3498DB?style=flat-square" height="14"/> | **Level Designer** | Combine triggers, galleries, stalkers, doors, and creatures into compelling spaces. All data-driven through Blender. |
| <img src="https://img.shields.io/badge/-00599C?style=flat-square" height="14"/> | **Programmer (C11)** | New entity types, AI behaviors, rendering effects, UI systems. Pure C11, no C++. |

### Getting Started

```
1.  Read the Game Design Document          →  docs/gdd.md
2.  Read the Technical Design Document     →  docs/tdd.md
3.  Artists: read the Blender Guide        →  docs/guides/blender-level-authoring.md
4.  Build the project, fire some sonar     →  see "Building" below
```

<br>

---

<br>

## Features

<table>
<tr>
<td width="50%" valign="top">

### Core Mechanics
- **Complete darkness** — nothing renders without sonar
- **Dual sonar modes** — wide scatter vs. narrow focused beam
- **Energy economy** — every action has a cost
- **65,536-point ring buffer** — echoes accumulate into maps
- **Spatial audio** — hear geometry before you see it

### Creatures & Threats
- **Stalker AI** — appears behind you when you use sonar; closes distance with each pulse; retreats when you go silent
- **Passive creatures** — orange point clouds, revealed only by their own sound

</td>
<td width="50%" valign="top">

### World Systems
- **Blender → glTF pipeline** — levels as `.glb`, no custom editor
- **Data-driven entities** — defined via glTF custom properties
- **Trigger galleries** — sequential reveals with starfield sparkle
- **Dynamic doors** — per-triangle collision toggle
- **Password dials** — red clues + blue mechanisms

### Visual Effects
- **Additive-blended point clouds** — all sonar rendering
- **Breathing minimap** — diagonal wave animation
- **Particle VFX** — shockwave rings, collapse bursts
- **Starfield sparkle** on gallery reveals

</td>
</tr>
</table>

<br>

---

<br>

## Tech Stack

<div align="center">

| Layer | Technology | Note |
|:------|:-----------|:-----|
| ![C](https://img.shields.io/badge/-00599C?style=flat-square&logo=c&logoColor=white) Language | **C11** | No C++. Pure and portable. |
| ![GL](https://img.shields.io/badge/-5586A4?style=flat-square&logo=opengl&logoColor=white) Graphics | **OpenGL 3.3 Core** | GLAD loader |
| ![SDL](https://img.shields.io/badge/-1D3557?style=flat-square) Window | **SDL2** | Cross-platform window + input |
| ![Math](https://img.shields.io/badge/-2ECC71?style=flat-square) Math | **cglm** | Header-only |
| ![Model](https://img.shields.io/badge/-E87D0D?style=flat-square) Models | **cgltf** | glTF 2.0 / .glb, header-only |
| ![Audio](https://img.shields.io/badge/-9B59B6?style=flat-square) Audio | **OpenAL Soft** | Built from source |
| ![Decode](https://img.shields.io/badge/-E74C3C?style=flat-square) Decoding | **dr_wav, stb_vorbis** | Header-only |
| ![Build](https://img.shields.io/badge/-064F8C?style=flat-square&logo=cmake&logoColor=white) Build | **CMake 3.20+** | Single CMakeLists.txt |
| ![Blend](https://img.shields.io/badge/-E87D0D?style=flat-square&logo=blender&logoColor=white) Levels | **Blender** | glTF 2.0 export pipeline |

</div>

All dependencies live in `third_party/` and are built from source. No vcpkg or package manager required.

<br>

---

<br>

## Building

### Prerequisites

- CMake 3.20+
- C11 compiler (MSVC 2022, GCC 12+, Clang 15+)
- Git

### Clone & Build

```bash
# Clone with dependencies
git clone --recurse-submodules https://github.com/yourname/sonar.git
cd sonar

# Windows (MSVC)
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Debug

# Linux / macOS
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run
./build/Debug/sonar        # Windows
./build/sonar              # Linux / macOS
```

> **Windows note**: Debug builds produce `SDL2d.dll`. The CMakeLists copies it automatically. If the game still won't start: `cp build/Debug/SDL2d.dll build/Debug/SDL2.dll`

<br>

---

<br>

## Project Structure

```
sonar/
├── src/
│   ├── main.c                  # Game loop, input, window
│   ├── core/                   # Window, input, timer
│   ├── sonar/                  # Sonar system, raycast, energy
│   ├── render/                 # OpenGL renderer, point clouds, HUD, VFX, camera, shaders
│   ├── world/                  # Map loader, entities, triggers, stalker AI, physics
│   └── audio/                  # OpenAL context, spatial positioning, sound loading
├── shaders/                    # GLSL shaders
├── assets/
│   ├── maps/                   # .glb level files
│   └── sounds/                 # .wav / .ogg audio
├── docs/
│   ├── gdd.md                  # Game Design Document
│   ├── tdd.md                  # Technical Design Document
│   ├── adr/                    # Architecture Decision Records (0001–0009)
│   ├── api/                    # Module API reference
│   └── guides/                 # Blender Level Authoring Guide
├── third_party/                # SDL2, cglm, cgltf, OpenAL Soft, dr_libs, stb
└── CMakeLists.txt
```

<br>

---

<br>

## Documentation

<div align="center">

| | Document | What's inside |
|:---:|----------|-------------|
| <img src="https://img.shields.io/badge/-FF2626?style=flat-square" height="12"/> | [Game Design Document](docs/gdd.md) | Vision, mechanics, world design, audio, narrative |
| <img src="https://img.shields.io/badge/-2673FF?style=flat-square" height="12"/> | [Technical Design Document](docs/tdd.md) | Architecture, module contracts, data flow |
| <img src="https://img.shields.io/badge/-E87D0D?style=flat-square" height="12"/> | [Blender Level Authoring Guide](docs/guides/blender-level-authoring.md) | Complete reference for level artists |
| <img src="https://img.shields.io/badge/-00D4BF?style=flat-square" height="12"/> | [API Reference](docs/api/) | Per-module technical documentation |

</div>

<details>
<summary><strong>Architecture Decision Records (ADR 0001–0009)</strong></summary>
<br>

| ADR | Decision |
|-----|----------|
| [0001](docs/adr/0001-tech-stack.md) | Technology Stack Selection |
| [0002](docs/adr/0002-project-structure.md) | Project Directory and Module Structure |
| [0003](docs/adr/0003-deps-management.md) | Dependency Management via third_party/ |
| [0004](docs/adr/0004-m3-gap-log.md) | Sonar Firing Mechanism Gaps vs Prototype |
| [0005](docs/adr/0005-sonar-visual-design.md) | Sonar Visual Design: Primitives and Blending |
| [0006](docs/adr/0006-world-authoring-blender-gltf.md) | World Authoring: Blender + glTF 2.0 |
| [0007](docs/adr/0007-entity-system.md) | Lightweight Entity System |
| [0008](docs/adr/0008-trigger-stalker-entities.md) | Trigger and Stalker Entities |
| [0009](docs/adr/0009-door-collision-toggle.md) | Dynamic Door Collision Toggle |

</details>

<br>

---

<br>

## Controls

<div align="center">

| Key | Action |
|:---:|--------|
| `W` `A` `S` `D` | Move |
| `Mouse` | Look |
| `LMB` | Fire wide sonar pulse |
| `RMB` | Fire focused sonar beam |
| `F` | Open / close door |
| `E` | Interact with password dial |
| `M` | Toggle minimap (costs energy) |
| `Esc` | Pause / quit |

</div>

<br>

---

<br>

## Extensibility

SONAR is a **content-driven framework**. The engine handles rendering, physics, audio, and entity logic — what the player *experiences* is defined by level data.

<table>
<tr>
<td width="50%" valign="top">

### No code required
- New levels with unique layouts
- Creature encounters (custom mesh + sound + timing)
- Gallery sequences (sequential art reveals)
- Door puzzles with password dials
- Stalker encounters with tuned aggression
- Ambient soundscapes

</td>
<td width="50%" valign="top">

### Engine work needed
- New entity types (platforms, NPCs)
- New rendering effects (fog, water)
- New AI behaviors
- Save/load system
- Menu and UI overhaul
- Multiplayer

</td>
</tr>
</table>

<br>

---

<br>

## Status

<div align="center">

*The engine is ready. The world is waiting to be built.*

</div>

<br>

| Milestone | Description | Status |
|:---------:|-------------|:------:|
| M1 | Engine skeleton — window, GL, input, timer | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M2 | Physics and collision | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M3 | Sonar raycast and point cloud | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M4 | HUD, energy bar, minimap | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M5 | Visual FX — shockwave, additive blending | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M6 | World system + entity system | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M7 | Trigger zones, Stalker AI, particle VFX | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M8 | Player interaction, doors, minimap overlay | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M9 | Creature & gallery content enablement | ![done](https://img.shields.io/badge/-done-00D4BF?style=flat-square) |
| M10+ | Levels, narrative, audio content | ![open](https://img.shields.io/badge/-open-1A5C54?style=flat-square) |

<br>

---

<div align="center">

<br>

*Built with care. Waiting for a story.*

<br>

**License**: TBD. All rights reserved until a license is chosen.

</div>
