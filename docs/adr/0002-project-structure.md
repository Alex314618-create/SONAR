# ADR-0002: Project Directory and Module Structure

> **Status**: Accepted
> **Date**: 2026-03-05
> **Deciders**: Architect (Human), Claude Opus 4.6 (Advisory)

## Context

The SONAR prototype was a flat directory with 9 `.c` files and 1 `.h` file.
All types and globals were declared in a single `game.h`. This worked for a
prototype but doesn't scale:

- No separation between engine subsystems
- Global state scattered across files
- No clear ownership or dependency direction
- No room for tests, tools, or assets

We need a structure that:
- Groups code by **subsystem** (render, audio, sonar, world, UI)
- Makes **dependencies explicit** and preferably one-directional
- Separates **engine code** from **game-specific logic**
- Accommodates **assets, shaders, docs, and build configuration**

## Decision

### Top-level layout
```
sonar/
├── src/           # All C source code
├── shaders/       # GLSL shaders (loaded at runtime)
├── assets/        # Runtime assets (models, sounds, maps, textures)
├── docs/          # All documentation
├── agents/        # AI agent role definitions
├── cmake/         # Custom CMake modules
├── tests/         # Test programs
└── tools/         # Asset pipeline and utility scripts
```

### Source module structure (`src/`)
```
src/
├── main.c         # Entry point only — delegates to modules
├── core/          # Foundation: window, input, timer, game state machine
├── render/        # OpenGL rendering pipeline
├── audio/         # OpenAL audio subsystem
├── world/         # Game world: maps, entities, physics
├── sonar/         # Sonar-specific mechanics
└── ui/            # HUD, fonts, menus
```

### Dependency direction (strict)
```
main.c
  └── core/     (no deps on other src/ modules)
       └── render/  (depends on core/)
       └── audio/   (depends on core/)
       └── world/   (depends on core/)
            └── sonar/  (depends on core/, render/, audio/, world/)
            └── ui/     (depends on core/, render/)
```

**Rule**: Lower layers MUST NOT depend on higher layers.
`core/` never includes from `render/`. `render/` never includes from `sonar/`.

### Module conventions
- Each module directory gets its own `.h/.c` pairs
- Each module has `module_init()` / `module_shutdown()` lifecycle
- Init order defined in `main.c`; shutdown in reverse order
- Cross-module communication via function calls, not shared globals

## Consequences

### Positive
- Clear boundaries make modules independently testable
- New team members (or AI agents) can understand one module without reading all
- Dependency direction prevents circular includes
- Shader/asset separation enables hot-reloading in the future

### Negative
- More files than the prototype (9 → ~25-30 initially)
- Requires discipline to maintain dependency direction
- Some code may feel over-structured for a small project initially

### Migration
- Prototype code in `doomer/` is reference material only
- No direct code migration — the new project is a clean rewrite
- Game logic concepts (sonar raycasting, energy system) will be reimplemented
  against the new module interfaces
