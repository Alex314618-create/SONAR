# ADR-0001: Technology Stack Selection

> **Status**: Accepted
> **Date**: 2026-03-05
> **Deciders**: Architect (Human), Claude Opus 4.6 (Advisory)

## Context

Project SONAR is being rebuilt from a Win32 raycasting prototype (pure C, software
rendered, no audio) into a production-quality game. The prototype demonstrated the
core gameplay concept (first-person sonar exploration in darkness) but has
fundamental limitations:

1. **Platform lock-in**: Win32 API, Windows only
2. **No audio**: Critical flaw for a game about *sound*
3. **2.5D rendering**: Raycasting limits environmental complexity
4. **No build system**: Single compilation unit, manual builds
5. **No dependency management**: Everything hand-rolled

We need a tech stack that:
- Remains in **pure C** (project constraint)
- Enables **3D mesh-based rendering** with shader effects
- Provides **3D spatial audio** (directional sonar echoes)
- Is **cross-platform** (Windows primary, Linux/macOS stretch)
- Has **manageable complexity** for a small team + AI agents

## Decision

| Layer          | Choice          | Alternatives Considered          |
|----------------|-----------------|----------------------------------|
| Language       | C11             | — (project constraint)           |
| Build          | CMake >= 3.20   | Meson, Premake                   |
| Deps           | vcpkg           | Conan, git submodules, system pkg|
| Window/Input   | SDL2            | GLFW, raw Win32                  |
| Rendering      | OpenGL 3.3 Core | Vulkan, software rendering       |
| Math           | cglm            | HandmadeMath, linmath.h, custom  |
| Models         | glTF 2.0 (cgltf)| OBJ, custom binary               |
| Audio          | OpenAL Soft     | SDL_mixer, miniaudio, FMOD       |
| Audio decode   | stb_vorbis, dr_wav | SDL_mixer built-in             |

## Rationale

### SDL2 over GLFW
- SDL2 provides audio subsystem (backup), gamepad support, and broader platform
  coverage. GLFW is lighter but window-only.

### OpenGL 3.3 over Vulkan
- Vulkan adds massive complexity for minimal benefit at our scale.
  OpenGL 3.3 Core is universally supported, well-documented, and sufficient
  for our visual style (dark environments, particle effects, post-processing).

### cglm over HandmadeMath
- cglm is a direct port of GLM (the OpenGL standard math library), has better
  documentation, more functions, and an active community. HandmadeMath is simpler
  but lacks features we'll need (quaternions, frustum culling).

### glTF 2.0 over OBJ
- glTF supports materials, animations, and skeletal data natively.
  OBJ is simpler but would require a separate material system.
  cgltf is a single-header C parser — minimal integration effort.

### OpenAL Soft over SDL_mixer
- SDL_mixer: no 3D spatial positioning. For a sonar game, hearing direction
  and distance is gameplay-critical.
- miniaudio: capable but smaller community, less documentation.
- FMOD: proprietary license, overkill for our needs.
- OpenAL Soft: open source, 3D spatial audio, well-established API, HRTF support.

### vcpkg over Conan/submodules
- vcpkg integrates seamlessly with CMake via toolchain file.
- Conan has more complex configuration.
- Git submodules bloat the repo and require manual build integration.

## Consequences

### Positive
- Cross-platform from day one (SDL2 + OpenGL)
- Rich audio capabilities (3D spatial, HRTF) matching the game's identity
- Modern 3D rendering with shader pipeline
- Well-supported, documented libraries with large communities
- Reproducible builds via vcpkg manifest mode

### Negative
- OpenGL 3.3 lacks some modern features (compute shaders require 4.3)
- OpenAL API is somewhat dated (but OpenAL Soft extensions help)
- vcpkg requires initial setup on each developer machine
- glTF parsing is more complex than OBJ (mitigated by cgltf)

### Risks
- cglm is header-only; aggressive inlining may slow compilation on large files
- OpenAL Soft on macOS is community-maintained (Apple deprecated OpenAL)
- glTF 2.0 spec is large; we'll use a subset (static meshes first, animation later)
