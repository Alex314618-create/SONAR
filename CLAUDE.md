# SONAR - Project Constitution

> **Version**: 0.1.0-pre
> **Status**: Pre-production
> **Last Updated**: 2026-03-05

## 1. Project Identity

**SONAR** is a first-person exploration game where the player navigates pitch-black environments using only acoustic echolocation. The world is invisible until revealed by sonar pulses — sound is both weapon and sight.

- **Genre**: First-person exploration / horror
- **Platform**: Windows (primary), Linux/macOS (stretch)
- **Language**: C11 (strictly no C++)
- **License**: Proprietary

## 2. Core Principle

> **Documentation >= Code**
>
> Every architectural decision, API surface, and design rationale MUST be documented
> BEFORE or AT THE TIME OF implementation. Code without documentation is incomplete work.
> We follow big-tech documentation standards: clear, versioned, reviewable, and traceable.

## 3. Tech Stack

| Layer          | Technology      | Version   |
|----------------|-----------------|-----------|
| Language       | C11             | -         |
| Build          | CMake           | >= 3.20   |
| Deps           | vcpkg           | latest    |
| Window/Input   | SDL2            | >= 2.28   |
| Rendering      | OpenGL          | 3.3 Core  |
| Math           | cglm            | >= 0.9    |
| Models         | glTF 2.0 (cgltf)| -         |
| Audio          | OpenAL Soft     | >= 1.23   |
| Audio Decode   | stb_vorbis, dr_wav | -      |
| Font           | stb_truetype (planned) | -   |

## 4. Directory Structure

```
sonar/
├── CLAUDE.md              # THIS FILE — project constitution
├── CMakeLists.txt         # Root build configuration
├── vcpkg.json             # Dependency manifest
├── cmake/                 # Custom CMake modules
├── src/
│   ├── main.c             # Entry point + game loop
│   ├── core/              # Window, input, timer, game state
│   ├── render/            # OpenGL renderer, shaders, models, camera
│   ├── audio/             # OpenAL init, sound loading, 3D spatial
│   ├── world/             # Map, entities, physics/collision
│   ├── sonar/             # Sonar mechanics, raycasting, energy
│   └── ui/                # HUD, font, menus
├── shaders/               # GLSL vertex/fragment shaders
├── assets/
│   ├── models/            # glTF 2.0 model files
│   ├── sounds/            # .ogg/.wav audio files
│   ├── maps/              # Level data (.json)
│   └── textures/          # Texture files
├── docs/
│   ├── gdd.md             # Game Design Document
│   ├── tdd.md             # Technical Design Document
│   ├── adr/               # Architecture Decision Records
│   ├── api/               # Module API documentation
│   └── guides/            # Developer guides
├── agents/                # AI agent role documents
├── tests/                 # Test programs
└── tools/                 # Build/asset pipeline tools
```

## 5. Coding Standards

### 5.1 Naming Conventions
- **Functions**: `module_verb_noun()` — e.g., `renderer_draw_mesh()`, `audio_play_sound()`
- **Types**: `PascalCase` — e.g., `SonarPoint`, `MeshData`
- **Constants/Macros**: `UPPER_SNAKE_CASE` — e.g., `MAX_SONAR_POINTS`
- **Local variables**: `camelCase` — e.g., `rayDir`, `hitDist`
- **Global variables**: `g_camelCase` — e.g., `g_running` (minimize globals)
- **File-static variables**: `s_camelCase` — e.g., `s_shaderProgram`

### 5.2 File Organization
- Every `.c` file has a corresponding `.h` file (except `main.c`)
- Headers use `#pragma once` (supported by all target compilers)
- Headers contain ONLY: type definitions, function declarations, macros, extern declarations
- No function implementations in headers (except `static inline` for trivial helpers)

### 5.3 Code Documentation
- Every public function: doxygen-style comment block in the `.h` file
- Every file: top comment with module name, purpose, and author/date
- Complex algorithms: inline comments explaining the "why", not the "what"
- No commented-out code in committed files

### 5.4 Error Handling
- All resource allocation (malloc, fopen, GL object creation) MUST be checked
- Use return codes (0 = success, negative = error) for functions that can fail
- Log errors via a centralized logging module (not bare printf)
- Never silently swallow errors

### 5.5 Memory Management
- Every `malloc`/`calloc` must have a corresponding `free` path
- Module init/shutdown pattern: `module_init()` / `module_shutdown()`
- No memory leaks tolerated — every module cleans up on shutdown

## 6. Documentation Standards

### 6.1 Document Types and Ownership
| Document | Owner | Format | Location |
|----------|-------|--------|----------|
| Game Design Document (GDD) | CIO Agent | Markdown | `docs/gdd.md` |
| Technical Design Document (TDD) | CIO Agent | Markdown | `docs/tdd.md` |
| Architecture Decision Records | CIO Agent | Markdown | `docs/adr/` |
| Module API Docs | Engineer Agent | Markdown | `docs/api/` |
| Developer Guides | Engineer Agent | Markdown | `docs/guides/` |
| Agent Role Definitions | Architect | Markdown | `agents/` |

### 6.2 ADR (Architecture Decision Record) Format
Every significant technical decision gets an ADR:
- File: `docs/adr/NNNN-title.md`
- Sections: Status, Context, Decision, Consequences
- ADRs are immutable once accepted; superseded by new ADRs if reversed

### 6.3 Documentation Quality Gates
- No PR/change is complete without updated documentation
- API changes require updated `docs/api/` entries
- New modules require a brief design doc or ADR justifying their existence

## 7. Git Workflow

- `main` branch: stable, always builds
- Feature branches: `feature/description`
- Commit messages: imperative mood, concise (`Add sonar raycast module`)
- No force pushes to main

## 8. Build & Run

```bash
# First time setup
vcpkg install sdl2 openal-soft cglm
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Run
./build/sonar
```

## 9. Agent Organization

This project uses a multi-agent AI workflow with **direct sub-agent dispatch**.

### 9.1 Active Agents
| Agent | Model | 职责 | 调用方式 |
|-------|-------|------|----------|
| **Architect** | Opus 4.6 | 系统设计、调度、审查 | 主会话（与 Alex 对话） |
| **Engineer** | Sonnet 4.6 | C 代码实现、构建 | Architect 通过 Agent tool 直接调用 |
| **CIO** | Sonnet 4.6 | GDD、TDD、ADR、API 文档 | Architect 通过 Agent tool 直接调用 |
| **Alex** | 人类 | 游戏创意、最终决策、测试 | 对话 |

### 9.2 调度模式
```
         Alex (创意 + 决策)
            ↕ 对话
      ┌─────────────┐
      │  Architect   │ ← Claude Opus 4.6, 主会话
      │  (总设计师)   │
      └──┬───────┬───┘
         │       │     Agent tool 直接调用
    ┌────▼────┐ ┌▼────────┐
    │Engineer │ │  CIO    │
    │(Sonnet) │ │(Sonnet) │
    └─────────┘ └─────────┘
```

- Architect **直接通过 Agent tool** 调用 Engineer 和 CIO
- 无依赖的任务**并行调用**；有依赖的**顺序调用**
- 每个 agent 任务小而精确，一次一件事
- Agent prompt 模板见 `agents/engineer-subagent.md` 和 `agents/cio-subagent.md`

### 9.3 Legacy Agents (备用)
- **Prompt Engineer** (`prompt-engineer.md`): AI prompt 设计师。低频使用。
- 手动多实例模式 (`engineer.md`, `cio.md`): Alex 手动复制粘贴启动语到独立实例。仅在 Agent tool 不可用时使用。

Each agent operates with its own role document and follows this constitution.
