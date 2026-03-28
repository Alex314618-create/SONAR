# SONAR — Technical Design Document

> **Version**: 0.9.3
> **Status**: Draft
> **Last Updated**: 2026-03-24
> **Owner**: CIO Agent
> **Related ADRs**: [0001-tech-stack](adr/0001-tech-stack.md), [0002-project-structure](adr/0002-project-structure.md), [0003-deps-management](adr/0003-deps-management.md), [0004-m3-gap-log](adr/0004-m3-gap-log.md), [0005-sonar-visual-design](adr/0005-sonar-visual-design.md), [0006-world-authoring-blender-gltf](adr/0006-world-authoring-blender-gltf.md), [0007-entity-system](adr/0007-entity-system.md), [0009-door-collision-toggle](adr/0009-door-collision-toggle.md)

---

## Table of Contents
1. [Architecture Overview](#1-architecture-overview)
2. [Module Specifications](#2-module-specifications)
3. [Data Flow](#3-data-flow)
4. [Rendering Pipeline](#4-rendering-pipeline)
5. [Audio Pipeline](#5-audio-pipeline)
6. [Sonar System Architecture](#6-sonar-system-architecture)
7. [Asset Formats](#7-asset-formats)
8. [Build System](#8-build-system)
9. [Performance Budget](#9-performance-budget)
10. [Open Technical Questions](#10-open-technical-questions)
11. [Changelog](#11-changelog)
12. [M3 — Sonar System + Audio System](#12-m3--sonar-system--audio-system)
13. [M4 — Sonar Mechanism Fidelity + HUD + Visual FX](#13-m4--sonar-mechanism-fidelity--hud--visual-fx)
14. [M5 — Sonar Visual Refactor](#14-m5--sonar-visual-refactor)
15. [M6 — World System + Entity System](#15-m6--world-system--entity-system)
16. [M6c — Trigger, Stalker & Particle VFX](#16-m6c--trigger-stalker--particle-vfx)
17. [M8 — Player Interaction, Door System & Minimap Toggle](#17-m8--player-interaction-door-system--minimap-toggle)
18. [M9 — Creature TTL Propagation, Door Audio & Blender Authoring Guide](#18-m9--creature-ttl-propagation-door-audio--blender-authoring-guide)

---

## 1. Architecture Overview

### 1.1 System Diagram

```
┌─────────────────────────────────────────────────┐
│                    main.c                        │
│              (game loop orchestrator)            │
└─────────┬────────┬────────┬────────┬────────────┘
          │        │        │        │
    ┌─────▼──┐ ┌───▼───┐ ┌─▼────┐ ┌─▼───┐
    │ core/  │ │render/ │ │audio/│ │ ui/ │
    │        │ │        │ │      │ │     │
    │window  │ │renderer│ │audio │ │ hud │
    │input   │ │shader  │ │sound │ │font │
    │timer   │ │mesh    │ │spatial│ │     │
    │game    │ │model   │ │      │ │     │
    │        │ │camera  │ │      │ │     │
    │        │ │sonar_fx│ │      │ │     │
    │        │ │postfx  │ │      │ │     │
    └────────┘ └───┬────┘ └──┬───┘ └─────┘
                   │         │
              ┌────▼─────────▼────┐
              │      world/       │
              │  map  entity      │
              │  physics          │
              └────────┬──────────┘
                       │
                 ┌─────▼──────┐
                 │   sonar/   │
                 │ sonar      │
                 │ raycast    │
                 │ energy     │
                 └────────────┘
```

### 1.2 Dependency Rules

| Module   | May Depend On                | Must NOT Depend On          |
|----------|------------------------------|-----------------------------|
| core/    | (none — foundation layer)    | render/, audio/, world/, sonar/, ui/ |
| render/  | core/                        | audio/, sonar/, ui/         |
| audio/   | core/                        | render/, sonar/, ui/        |
| world/   | core/                        | render/, audio/, sonar/, ui/|
| sonar/   | core/, world/                | render/, audio/, ui/        |
| ui/      | core/, render/               | audio/, world/, sonar/      |
| main.c   | ALL modules                  | —                           |

**Note**: `sonar/` does NOT depend on `render/` or `audio/`. The sonar module
computes ray hits and manages point data. Rendering of sonar points is in
`render/sonar_fx.c`. Sonar audio is triggered by `main.c` calling into `audio/`.
This separation keeps the sonar logic testable without a GPU or audio device.

### 1.3 Initialization Order
```c
// In main.c
window_init();      // SDL2 + GL context
renderer_init();    // GL state, default shaders
audio_init();       // OpenAL context + device
input_init();       // SDL event state
timer_init();       // Performance counter
world_init();       // Load level data
sonar_init();       // Sonar state
hud_init();         // HUD resources
// ... game loop ...
hud_shutdown();
sonar_shutdown();
world_shutdown();
timer_shutdown();
input_shutdown();
audio_shutdown();
renderer_shutdown();
window_shutdown();
```

---

## 2. Module Specifications

### 2.1 core/window
**Purpose**: Create and manage the application window and OpenGL context.

| Function | Description |
|----------|-------------|
| `window_init(int w, int h, const char *title)` | Create SDL2 window + GL 3.3 Core context |
| `window_shutdown()` | Destroy window and SDL context |
| `window_swap()` | Swap front/back buffers |
| `window_should_close()` | Returns non-zero if close requested |
| `window_set_should_close(int close)` | Set the should-close flag |
| `window_get_size(int *w, int *h)` | Current window dimensions |

### 2.2 core/input
**Purpose**: Abstract SDL2 input events into a queryable state.

| Function | Description |
|----------|-------------|
| `input_init()` | Initialize input state |
| `input_update()` | Poll SDL events, update key/mouse state |
| `input_key_down(int key)` | Is key currently pressed? |
| `input_key_pressed(int key)` | Was key just pressed this frame? |
| `input_mouse_delta(int *dx, int *dy)` | Mouse movement since last frame |
| `input_set_mouse_captured(int cap)` | Lock/unlock mouse to window |

### 2.3 core/timer
**Purpose**: Frame timing and delta time.

| Function | Description |
|----------|-------------|
| `timer_init()` | Initialize timing state |
| `timer_tick()` | Call once per frame; updates dt |
| `timer_dt()` | Returns delta time (seconds, clamped to 0.05) |
| `timer_fps()` | Returns smoothed FPS estimate |

### 2.4 render/shader
**Purpose**: Load, compile, and manage GLSL shader programs.

| Function | Description |
|----------|-------------|
| `shader_load(const char *vertPath, const char *fragPath)` | Load and compile shader program, returns handle |
| `shader_use(uint32_t program)` | Bind shader program |
| `shader_set_mat4(uint32_t prog, const char *name, mat4 m)` | Set mat4 uniform |
| `shader_set_vec3(uint32_t prog, const char *name, vec3 v)` | Set vec3 uniform |
| `shader_set_float(uint32_t prog, const char *name, float f)` | Set float uniform |
| `shader_set_int(uint32_t prog, const char *name, int i)` | Set int uniform |
| `shader_set_mat3(uint32_t prog, const char *name, const float *mat)` | Set mat3 uniform (normal matrix) |
| `shader_destroy(uint32_t program)` | Delete shader program |

### 2.5 render/mesh
**Purpose**: VAO/VBO abstraction for renderable geometry.

```c
typedef struct {
    uint32_t vao;
    uint32_t vbo;
    uint32_t ebo;
    int      indexCount;
    int      vertexCount;
} Mesh;
```

| Function | Description |
|----------|-------------|
| `mesh_create(const float *verts, int nv, const uint32_t *indices, int ni)` | Upload geometry to GPU |
| `mesh_draw(const Mesh *m)` | Bind VAO and draw |
| `mesh_destroy(Mesh *m)` | Free GPU resources |

### 2.6 render/model
**Purpose**: Load glTF 2.0 files via cgltf and convert to Mesh objects.

| Function | Description |
|----------|-------------|
| `model_load(const char *path)` | Parse glTF, create Mesh(es) |
| `model_draw(const Model *m, uint32_t shader)` | Render all meshes |
| `model_destroy(Model *m)` | Free all GPU + CPU resources |

### 2.7 render/camera
**Purpose**: First-person camera with view/projection matrix computation.

```c
typedef struct {
    vec3  position;
    vec3  front;
    vec3  up;
    vec3  right;
    float yaw;
    float pitch;
    float fov;
    float near;
    float far;
} Camera;
```

| Function | Description |
|----------|-------------|
| `camera_init(Camera *cam, float posX, float posY, float posZ, float yaw, float pitch)` | Set initial state |
| `camera_update(Camera *cam, float dx, float dy)` | Apply mouse delta |
| `camera_view_matrix(const Camera *cam, mat4 dest)` | Compute view matrix |
| `camera_proj_matrix(const Camera *cam, float aspect, mat4 dest)` | Compute projection matrix |

### 2.8 render/sonar_fx
**Purpose**: Render all persistent sonar echo points using `GL_POINTS`.

Points are sized in the vertex shader by distance (1–3 px), clipped to hard
circles in the fragment shader, and composited with additive blending. See
[ADR 0005](adr/0005-sonar-visual-design.md) for design rationale.

**GL state contract**:
- Primitive: `GL_POINTS` with `GL_PROGRAM_POINT_SIZE` enabled
- Blend: `glBlendFunc(GL_ONE, GL_ONE)` — additive
- Depth: `glDepthFunc(GL_LEQUAL)` while rendering; restored to `GL_LESS` after

| Function | Description |
|----------|-------------|
| `sonar_fx_init()` | Create point VAO/VBO, load `shaders/sonar_point.vert/frag` |
| `sonar_fx_render(const SonarPoint *points, int count, float *view, float *proj, vec3 camPos)` | Upload point data and draw |
| `sonar_fx_shutdown()` | Free VAO/VBO GPU resources |

### 2.9 audio/audio
**Purpose**: Initialize and manage OpenAL context.

| Function | Description |
|----------|-------------|
| `audio_init()` | Open device, create context, configure HRTF |
| `audio_shutdown()` | Destroy context and close device |
| `audio_set_listener(vec3 pos, vec3 front, vec3 up)` | Update listener position/orientation |

### 2.10 audio/sound
**Purpose**: Load and play sound effects.

| Function | Description |
|----------|-------------|
| `sound_load(const char *path)` | Load .wav/.ogg into OpenAL buffer |
| `sound_play(uint32_t buffer, vec3 pos, float gain)` | Play sound at 3D position |
| `sound_play_2d(uint32_t buffer, float gain)` | Play non-spatial sound |
| `sound_destroy(uint32_t buffer)` | Free audio buffer |

### 2.11 sonar/sonar
**Purpose**: Core sonar mechanics — point generation, firing modes, explored grid.

| Function | Description |
|----------|-------------|
| `sonar_init()` | Initialize point buffer, explored grid, and LCG RNG |
| `sonar_frame_begin()` | Record current write-head as this frame's start index. Call once per frame BEFORE any firing. |
| `sonar_get_frame_start()` | Return write-head index captured by last `sonar_frame_begin()` |
| `sonar_get_write_head()` | Return current write-head index (= total points written mod MAX_SONAR_POINTS) |
| `sonar_set_player(pos, forward, up, right)` | Cache player pose for passive ping. Call once per frame before `sonar_update()`. |
| `sonar_fire_pulse(origin, forward, up, right)` | Fire wide or focused pulse; returns new point count |
| `sonar_fire_continuous(origin, forward, up, right, dt)` | Per-frame streaming fire using uniform disk sampling; returns new point count |
| `sonar_toggle_mode()` | Switch between SONAR_MODE_WIDE and SONAR_MODE_FOCUSED |
| `sonar_get_mode()` | Return current `SonarMode` |
| `sonar_update(float dt)` | Tick passive ping timer, age points |
| `sonar_get_points()` | Return `const SonarPoint *` pointer to circular buffer |
| `sonar_get_point_count()` | Return active point count (up to MAX_SONAR_POINTS) |
| `sonar_get_explored_grid()` | Return `const int *` to flat explored grid `[EXPLORE_GRID_H * EXPLORE_GRID_W]` |
| `sonar_clear()` | Erase all points and reset explored grid |
| `sonar_shutdown()` | Free resources |

**Frame-start API** (`sonar_frame_begin` / `sonar_get_frame_start` / `sonar_get_write_head`):

`vfx_render_laser_lines()` needs to know which points were added **this frame** so
it can draw lines only to new hits. Because the buffer is circular and can wrap,
two indices are exposed:

```
frameStart = sonar_get_frame_start()   // snapshot from sonar_frame_begin()
writeHead  = sonar_get_write_head()    // current head after firing

// Points added this frame occupy [frameStart, writeHead) mod MAX_SONAR_POINTS
```

`main.c` calls `sonar_frame_begin()` before input processing, then reads both
indices after firing to pass them to `vfx_render_laser_lines()`.

**Continuous fire ray distribution**: `sonar_fire_continuous()` uses **uniform disk
sampling** (polar: `r = sqrt(randf())`, `θ = randf() · 2π`) rather than independent
rectangular random offsets. This eliminates center-clustering and produces even
coverage across the cone's cross-section.

### 2.12 sonar/raycast
**Purpose**: 3D ray-world intersection tests.

| Function | Description |
|----------|-------------|
| `raycast_init(const float *tris, int triCount)` | Initialize raycast with collision triangle data |
| `raycast_cast(const float *origin, const float *dir, float maxDist, float *outHit, float *outNormal, int *outTriIndex)` | Cast ray against world geometry; returns distance or -1.0f |
| `raycast_set_tris_enabled(int start, int count, int enabled)` | Enable/disable a range of triangles for raycasting (door system); see ADR 0009 |
| `raycast_shutdown()` | Release raycast resources |

### 2.13 world/map
**Purpose**: Level loading via cgltf (glTF 2.0 / `.glb`). Extracts visual meshes,
collision meshes (`col_*` nodes), and entity definitions from glTF extras. Replaces
the former hardcoded procedural geometry. See [ADR 0006](adr/0006-world-authoring-blender-gltf.md).

Pass `NULL` to `map_load()` to fall back to the procedural test level (development use only).

| Function | Description |
|----------|-------------|
| `map_load(const char *path)` | Parse `.glb` via cgltf; extract visual meshes, `col_*` collision meshes, entity definitions from extras; `NULL` → procedural test level |
| `map_get_render_model()` | Get renderable `Model` for the current level |
| `map_get_collision_verts()` | Get flat triangle array (9 floats/tri) for physics |
| `map_get_collision_tri_count()` | Get number of collision triangles |
| `map_get_player_spawn()` | Get player spawn position (vec3) from `player_spawn` Empty node |
| `map_get_player_yaw()` | Get player initial yaw in radians from `player_spawn` node Y rotation |
| `map_get_entity_count()` | Return number of entities parsed from glTF extras |
| `map_get_entities()` | Return mutable `Entity *` pointer to internal entity array |
| `map_get_clue_color(int tri_index, float *out_rgb)` | Check if a collision triangle belongs to a clue surface; returns 1 if clue found |
| `map_get_collision_range(const char *name, int *out_start, int *out_count)` | Look up triangle range for a named `col_*` node (door system); returns 0 on success |
| `map_get_mesh_range_tris(int index, const float **out_verts, int *out_count)` | Get triangle data for a `vis_*` MeshRange by index |
| `map_shutdown()` | Free all level resources |

### 2.14 world/physics
**Purpose**: AABB collide-and-slide movement against triangle soup collision geometry.

```c
typedef struct {
    float min[3];   /* AABB minimum corner, relative to entity position */
    float max[3];   /* AABB maximum corner, relative to entity position */
} AABB;
```

| Function | Description |
|----------|-------------|
| `physics_init(const float *tris, int triCount)` | Initialize with collision triangle data |
| `physics_move(const float *pos, const float *vel, const AABB *bounds, float *outPos)` | Move with collision response |
| `physics_set_tris_enabled(int start, int count, int enabled)` | Enable/disable a range of collision triangles (door system); see ADR 0009 |
| `physics_shutdown()` | Release physics resources |

### 2.15 world/entity
**Purpose**: Lightweight typed entity system for dynamic and interactive world objects.
See [ADR 0007](adr/0007-entity-system.md) for design rationale and full color table.

```c
typedef enum {
    ENTITY_CREATURE,
    ENTITY_DIAL,
    ENTITY_DOOR,
    ENTITY_SOUND,
    ENTITY_TRIGGER,
    ENTITY_STALKER,
} EntityType;

typedef struct {
    EntityType  type;
    float       pos[3];       // world-space position
    float       yaw;          // initial facing, radians
    char        id[32];       // e.g. "creature_01"
    char        sound[64];    // asset path relative to assets/sounds/, may be empty
    char        code[16];     // dial code, may be empty
    char        target[32];   // activation signal target entity id, may be empty
    float       interval;     // sound repeat interval, seconds (0 = no repeat)
    char        mesh_ref[32]; // vis_* mesh name for reveal (trigger/stalker/creature)
    float       ttl;          // sonar point lifetime override (0 = permanent)
    float       radius;       // trigger/interaction radius (0 = use default)
    /* runtime state */
    float       sound_timer;  // countdown to next ambient sound
    int         mesh_index;   // index into MeshRange array (-1 = no mesh)
    int         active;       // runtime toggle: door open/closed, etc.
} Entity;
```

| Function | Description |
|----------|-------------|
| `entity_init(Entity *entities, int count)` | Initialize runtime state for all entities (reset timers, load sound buffers) |
| `entity_update(float dt, Entity *entities, int count)` | Tick sound timers; trigger passive sonar reveal on creature sound events; send activation signals |
| `entity_find_nearest_interactable(const Entity *entities, int count, const float pos[3], float maxDist)` | Find nearest `ENTITY_DIAL` or `ENTITY_DOOR` within `maxDist`; returns index or -1 |
| `entity_activate(Entity *e)` | Process player F-key interaction: dial code log, door collision toggle |
| `entity_shutdown()` | Free entity resources (destroy cached sound buffers) |

**Passive sonar reveal** (creature sound event):
- On `sound_timer` expiry: play sound at `entity.pos`; iterate `mesh_index` collision verts
- Project each vertex to world space; insert `SonarPoint {pos, {1.0, 0.55, 0.1}, age=0.001f, ttl=e->ttl}`
- `ttl = 0` (default): `sonar_update()` applies the 0.8 s default threshold; `ttl > 0`: custom lifetime
- Max 200 verts per creature (decimated at load time if mesh exceeds limit)

### 2.16 world/trigger
**Purpose**: Static trigger zone management — zone/step activation, delay timers, and
mesh reveal injection. Source files: `src/world/trigger.h`, `src/world/trigger.c`.

```c
typedef enum { TRIGGER_MODE_ZONE, TRIGGER_MODE_STEP } TriggerMode;

typedef struct {
    int         entity_index;      // index into map's entity array
    int         fired;             // 0 = unfired, 1 = fired (one-shot)
    float       delay;             // seconds after zone activation before reveal fires
    float       delay_timer;       // countdown: set to delay on zone activation
    float       radius;            // activation radius in metres
    char        zone_id[32];       // zone identifier for grouped activation; "" = standalone
    TriggerMode mode;              // TRIGGER_MODE_ZONE or TRIGGER_MODE_STEP
    int         zone_activated;    // set to 1 when zone activation is latched (delay ticking)
} TriggerState;
```

| Function | Description |
|----------|-------------|
| `trigger_init()` | Scan entity list for `ENTITY_TRIGGER`; allocate `TriggerState` table; pre-load sound buffers |
| `trigger_update(float dt, vec3 player_pos)` | Evaluate zone/step logic; tick `delay_timer`; call `trigger_reveal_mesh()` when timer expires |
| `trigger_reveal_mesh(int trigger_idx)` | Extract triangles from `vis_*` mesh via `map_get_mesh_range_tris()`; generate 10 pts/triangle (3 verts + 3 edge midpoints + 1 centroid + 3 jitter ±0.06 m); 70 % permanent (`ttl = 0`), 30 % transient (`ttl = 0.3s`); cap at 500 pts; insert via `sonar_add_point()` |
| `trigger_shutdown()` | Free all trigger state |

**Color**: default `(0, 0.85, 0.75)` cyan; overrideable per entity.

**Dependencies**: `sonar/sonar`, `world/map`, `audio/spatial`

### 2.17 world/stalker
**Purpose**: Stalker entity state machine — sonar-driven approach, behind-position
computation, mesh reveal, and phase transitions. Source files: `src/world/stalker.h`,
`src/world/stalker.c`.

```c
typedef enum {
    STALKER_DORMANT,
    STALKER_APPROACHING,
    STALKER_VISIBLE,
    STALKER_DEPARTING
} StalkerPhase;

typedef struct {
    int         entity_index;       // index into map's entity array
    StalkerPhase phase;
    float       current_dist;       // current distance from player, metres
    float       start_dist;         // initial / retreat distance (default 12.0 m)
    float       step_dist;          // distance closed per sonar fire (default 1.5 m)
    float       retreat_time;       // sonar-inactivity threshold before retreat (default 15.0 s)
    float       reveal_timer;       // countdown for VISIBLE phase (2.0 s)
    float       idle_timer;         // time since last sonar fire (used for retreat)
    int         last_fire_count;    // sonar fire count snapshot for change detection
    float       appear_pos[3];      // world position where mesh was last revealed
    float       depart_timer;       // countdown for DEPARTING animation completion
    char        sound_appear[64];   // asset path relative to assets/sounds/
    char        sound_depart[64];   // asset path relative to assets/sounds/
} StalkerState;
```

| Function | Description |
|----------|-------------|
| `stalker_init()` | Scan entity list for `ENTITY_STALKER`; allocate `StalkerState`; pre-load sound buffers |
| `stalker_update(float dt, vec3 player_pos, vec3 player_fwd)` | Drive state machine; poll `sonar_get_fire_count()` for new fires; call `compute_behind_pos()`; phase transitions; schedule VFX |
| `compute_behind_pos(vec3 player_pos, vec3 player_fwd, float dist, vec3 out)` | Negate XZ forward, walk `dist` metres; raycast collision check with 0.5 m wall safety margin; clamp Y to floor |
| `stalker_reveal_mesh(StalkerState *s)` | Extract `vis_*` triangles; generate outline at `appear_pos`; color `(1.0, 0.15, 0.15)`; TTL 2.0 s on all points; insert via `sonar_add_point()` |
| `stalker_shutdown()` | Free all stalker state |

**Phase transition table**:

| From | To | Trigger |
|------|----|---------|
| DORMANT | APPROACHING | `sonar_get_fire_count()` increments |
| APPROACHING | VISIBLE | `current_dist ≤ step_dist` (minimum 1.5 m reached) |
| VISIBLE | DEPARTING | `reveal_timer` reaches 0.0 s |
| DEPARTING | DORMANT | `current_dist` restored to `start_dist` |

**Dependencies**: `sonar/sonar`, `sonar/raycast`, `world/map`, `render/vfx_particles`

### 2.18 render/vfx_particles
**Purpose**: CPU-simulated particle system for short-lived VFX bursts (shockwave ring,
sandfall collapse). Rendered as `GL_POINTS` with additive alpha blending. Source files:
`src/render/vfx_particles.h`, `src/render/vfx_particles.c`.

```c
#define MAX_VFX_PARTICLES 2048

typedef struct {
    float pos[3];       // world position
    float vel[3];       // velocity in m/s
    float color[3];     // RGB [0, 1]
    float life;         // remaining lifetime, seconds
    float max_life;     // total lifetime at spawn, seconds
} VfxParticle;
```

**GPU vertex layout** (7 floats per particle, 28 bytes):

| Attribute | Offset | Components |
|-----------|--------|------------|
| `a_pos` | 0 | `float[3]` world position |
| `a_color` | 12 | `float[3]` RGB |
| `a_alpha` | 24 | `float` derived as `life / max_life` |

| Function | Description |
|----------|-------------|
| `vfx_particles_init()` | Create VAO/VBO (`MAX_VFX_PARTICLES × 28` bytes); load `shaders/particle.vert/frag` |
| `vfx_particles_update(float dt)` | Integrate velocity; apply gravity where applicable; decrement `life`; free dead particles |
| `vfx_particles_render(float *view, float *proj)` | Pack live particles into GPU buffer via `glBufferSubData`; draw with `GL_POINTS`, `GL_SRC_ALPHA / GL_ONE` blend |
| `vfx_emit_shockwave(vec3 origin, vec3 color, float speed, float lifetime, int count)` | Spawn `count` particles in a horizontal ring; uniform angular distribution; all velocities outward at `speed` |
| `vfx_emit_collapse(vec3 origin, vec3 color, int count)` | Spawn `count` particles with downward gravity (9.8 m/s²) and random horizontal drift ±0.3 m/s; lifetime randomised in `[0.8, 1.4]` s |
| `vfx_particles_shutdown()` | Free VAO/VBO GPU resources |

**Shockwave parameters** (Stalker appear):
- `count = 64`, `color = (1.0, 0.4, 0.1)`, `speed = 4.0 m/s`, `lifetime = 0.6 s`

**Collapse parameters** (Stalker depart):
- `count = 48`, `color = (0.8, 0.5, 0.1)`, gravity downward at 9.8 m/s², lifetime `[0.8, 1.4]` s

**Shaders**: `shaders/particle.vert` (size: `gl_PointSize = clamp(8.0 / dist, 2.0, 8.0)`),
`shaders/particle.frag` (hard-circle discard + alpha from `a_alpha`).

**Blend**: `glBlendFunc(GL_SRC_ALPHA, GL_ONE)` — additive alpha, particles brighten their background.

### 2.19 world/map — MeshRange Extension
**Purpose**: Extend `map.c` to parse `vis_*` Blender nodes into a separate indexed
mesh buffer, independent of the collision triangle buffer. Consumed by `trigger_reveal_mesh()`
and `stalker_reveal_mesh()`.

```c
#define MAX_MESH_RANGES     32
#define MAX_MESH_RANGE_TRIS 8192

typedef struct {
    char  name[64];       // Blender node name, e.g. "vis_fossil_01"
    int   tri_offset;     // index of first triangle in s_meshRangeBuf
    int   tri_count;      // number of triangles belonging to this mesh
} MeshRange;
```

File-static storage in `src/world/map.c`:
```c
static float     s_meshRangeBuf[MAX_MESH_RANGE_TRIS * 9]; // 9 floats per triangle
static MeshRange s_meshRanges[MAX_MESH_RANGES];
static int       s_meshRangeCount = 0;
```

| Function | Description |
|----------|-------------|
| `map_get_mesh_range_count()` | Return number of parsed `vis_*` mesh ranges |
| `map_get_mesh_range_by_name(const char *name)` | Return `const MeshRange *` matching `name`; `NULL` if not found |
| `map_get_mesh_range_tris(int index, const float **out_verts, int *out_count)` | Set `*out_verts` to first triangle's float data and `*out_count` to triangle count for range `index` |

The `s_meshRangeBuf` is populated during `map_load()` traversal alongside the existing
`col_` collision extraction pass. `vis_*` nodes are not uploaded to the GPU render model
(they are reveal-only data).

### 2.20 sonar/sonar — TTL Extension & Fire Counter
**Purpose**: Extend `SonarPoint` and `sonar.c` to support per-point custom TTL values
and expose a monotonic fire counter for Stalker proximity detection.

#### SonarPoint TTL Field

The existing `age` sentinel scheme (`0.0` = permanent, `> 0` = transient, `-1.0` = dead)
is extended with a per-point `ttl` field:

```c
typedef struct {
    float pos[3];     // world position (x, y, z)
    float color[3];   // RGB normalized floats [0, 1]
    float age;        // seconds elapsed since spawn (sentinel: -1.0 = dead)
    float ttl;        // custom TTL: 0.0 = use default 0.8 s; > 0.0 = expire at this threshold
} SonarPoint;         // 32 bytes per point
```

`sonar_update()` per-point TTL logic:
```c
if (p->age > 0.0f) {
    p->age += dt;
    float threshold = (p->ttl > 0.0f) ? p->ttl : 0.8f;
    if (p->age >= threshold)
        p->age = -1.0f;  /* dead */
}
```

Callers set `ttl = 0.3f` for the 30% flash-tier trigger points; permanent points
(`ttl = 0.0f`) never age past their initial `age = 0.0f` and are therefore never culled.

#### Fire Counter

```c
int sonar_get_fire_count(void);
```

A file-static `int s_fireCount` incremented by 1 inside `sonar_fire_pulse()` and
once per streaming burst in `sonar_fire_continuous()`. The counter is monotonically
increasing and never reset. `stalker_update()` snapshots it each frame and detects
new fires by comparing to `last_fire_count`.

---

## 3. Data Flow

### 3.1 Frame Data Flow
```
input_update()
    → key/mouse state
        → player_update(dt)
            → camera position, direction
        → sonar_update(dt)
            → sonar_continuous_fire(dt)
                → raycast_cast_batch()
                    → sonar_add_point() × N

renderer_begin_frame()
    → clear buffers
    → set view/proj from camera
        → model_draw(level)
        → sonar_fx_update(points)
        → sonar_fx_draw()
        → hud_draw()
        → postfx_apply()
renderer_end_frame()
    → window_swap()

audio_set_listener(camera.pos, camera.front, camera.up)
    → sound_play(sonar_ping, ...) if fired this frame
```

### 3.2 View Bob (`main.c`)

Walking view bob is applied **only at view-matrix computation time** and never
written back to `cam.position`:

```c
// Pseudocode — src/main.c render block
if (playerMoving) bobPhase += 7.5f * 2.0f * M_PI * dt;

vec3 bobgedPos = cam.position;
bobgedPos[1] += 0.028f * sinf(bobPhase);       // Y oscillation
bobgedPos[0] += 0.009f * sinf(bobPhase * 0.5f); // X sway

camera_view_matrix_from_pos(&cam, bobgedPos, viewMat); // temporary position
// cam.position unchanged — physics / audio / HUD depth use original
```

| Parameter | Value |
|-----------|-------|
| Y amplitude | 0.028 units |
| X amplitude | 0.009 units |
| Frequency | 7.5 Hz |
| Phase advance | `bobPhase += 7.5 · 2π · dt` per frame while moving |
| Scope | View matrix only — collision, audio listener, depth HUD use real position |

---

## 4. Rendering Pipeline

### 4.1 Frame Stages (M5)

All steps execute between `renderer_begin_frame()` (GL clear) and
`renderer_end_frame()` (buffer swap). Source: `src/main.c` render block.

| Step | Name | Module | Notes |
|------|------|--------|-------|
| 1 | **Depth-only world pass** | `render/model` | `glColorMask(GL_FALSE, …)` — writes depth only; color output suppressed |
| 2 | **Sonar points** | `render/sonar_fx` | `GL_POINTS`, `GL_LEQUAL`, additive blend; all persistent points |
| 3 | **Laser lines** | `render/vfx` | 2D screen-space `GL_LINES`; only when LMB held; only this-frame points |
| 4 | **Fullscreen VFX** | `render/vfx` | Scanlines, pulse ripple — two sequential fullscreen quad draws (vignette removed in M8) |
| 5 | **Gun sprite** | `render/vfx` | Procedural 2D sprite, bottom-right; brightened when LMB held |
| 6 | **HUD** | `render/hud` | Energy bar, minimap, mode indicator, title, depth, FPS — orthographic, no depth test |

```
renderer_begin_frame()         // glClear color+depth; clear to (0.008, 0.008, 0.02)
  ├─ 1. depth-only world pass  // depth buffer filled; no color output
  ├─ 2. sonar_fx_render()      // GL_POINTS, LEQUAL, additive blend
  ├─ 3. vfx_render_laser_lines() (if LMB)
  ├─ 4. vfx_render_scanlines()
  │    vfx_render_pulse_ripple()
  ├─ 5. vfx_render_gun()
  └─ 6. hud_render()
renderer_end_frame()           // window_swap()
```

**Why depth-only pre-pass (Step 1)?** Sonar points lie exactly on wall surfaces.
Without a prior depth write, they cannot be occluded by walls. The pre-pass
populates the depth buffer so Step 2 (`GL_LEQUAL`) correctly hides points behind
walls while showing co-planar points. See ADR 0005.

### 4.2 Shader List
| Shader | Purpose | Inputs |
|--------|---------|--------|
| `basic.vert/frag` | Level geometry (depth pass + lit color) | MVP matrices, minimal lighting |
| `sonar_point.vert/frag` | Sonar echo points (`GL_POINTS`) | View/proj, per-point pos+color; outputs `gl_PointSize` |
| `vignette.vert/frag` | Fullscreen VFX (scanlines, pulse ripple; vignette mode retained in shader but no longer invoked) | Screen-space quad, effect uniforms |
| `hud.vert/frag` | HUD solid elements (energy bar, minimap, crosshair) | Orthographic MVP, per-vertex color |
| `text.vert/frag` | HUD text glyphs — samples `GL_R8` font atlas as alpha mask | Orthographic MVP, pos+uv+color per vertex |
| `laser.vert/frag` | Laser lines (2D screen-space) | Orthographic MVP, per-vertex color |

### 4.3 render/sonar_fx Module Specification

- **Primitive**: `GL_POINTS` (single vertex per sonar point)
- **Vertex data**: `float pos[3], float color[3]` (28 bytes per point; `age` field not uploaded)
- **Point sizing**: `gl_PointSize = clamp(3.0 / dist, 1.0, 3.0)` — 3 px near, 1 px far
- **Fragment clip**: discard if `length(gl_PointCoord - 0.5) > 0.5` → hard circle
- **Blend**: `glBlendFunc(GL_ONE, GL_ONE)` — additive; bright clusters emerge from density
- **Depth**: `glDepthFunc(GL_LEQUAL)` scoped to this draw; restored to `GL_LESS` after
- **Upload**: full buffer uploaded via `glBufferSubData` each frame (1 MB at 65 536 points)

### 4.4 render/vfx Module Specification

**Laser lines** (`vfx_render_laser_lines`):
- Projects each this-frame sonar point through view/proj matrices to screen coords
- Draws `GL_LINES` from gun muzzle pixel to projected pixel in orthographic space
- No depth test (`glDisable(GL_DEPTH_TEST)` for this pass)
- Frame range: points in `[frameStart, writeHead)` mod `MAX_SONAR_POINTS`
- Capped at 500 lines per frame (`MAX_LASER_LINES` in `vfx.c`)

**Gun sprite** (`vfx_render_gun`):
- Procedural geometry: batched colored quads (`GL_TRIANGLES`), up to 512 vertices
- Position: fixed bottom-right, 2D orthographic
- State: base color vs brightened when `lmbHeld == 1`
- No external texture asset — geometry is defined in code

**Fullscreen effects** (scanlines / pulse ripple; vignette removed in M8):
- Single fullscreen quad VAO (NDC `[-1, 1]²`)
- Single shared shader (`vignette.vert/frag`) with mode uniform (vignette mode `u_mode == 1` retained in shader code but no longer invoked from `main.c`)
- `vfx_update(dt)` ticks pulse ripple timer; effect inactive when `s_rippleTimer <= 0`

### 4.5 render/hud Module Specification

- **Projection**: orthographic, `(0, 0)` = top-left, `(winW, winH)` = bottom-right

#### Batch System

The HUD uses two independent vertex batches per frame:

| Batch | Shader | Vertex layout | Content |
|-------|--------|---------------|---------|
| Solid-color quad batch | `hud.vert/frag` | `pos.xy + color.rgba` (6 floats/vert) | Energy bar, minimap cells, crosshair |
| Text glyph batch | `text.vert/frag` | `pos.xy + uv.xy + color.rgba` (8 floats/vert) | All text strings |

Each batch is flushed with a single `glDrawArrays` call (up to 16 384 vertices per batch).

#### Font Atlas

- **Library**: `stb_truetype` (`stbtt_BakeFontBitmap`)
- **Font**: DM Mono Regular (`assets/fonts/DMMono-Regular.ttf`)
- **Texture**: single `512 × 512` `GL_R8` texture, red channel = coverage
- **Texture filtering**: `GL_NEAREST` for both min and mag filters — produces pixel-sharp glyph edges without bilinear blur
- **Sizes baked**: 15 px (labels/values) in the **upper half** (rows 0–255); 22 px (title) in the **lower half** (rows 256–511)
- **Coordinate fixup**: `stbtt_BakeFontBitmap` always fills from row 0. The large-font bake is written to a separate 256-row region; its `y0`/`y1` quad coordinates must be offset by `+256` to address the correct atlas rows at sample time.

#### Data Sources

- **Minimap**: iterates `sonar_get_explored_grid()` — draws dynamically-sized squares per explored cell (scaled to fit within the centered panel)
- **Energy bar**: proportional fill from `energy_get_fraction()`; border always visible

| HUD element | Position | Data source |
|-------------|----------|-------------|
| Minimap | Centered overlay (when toggled by M key) | `sonar_get_explored_grid()` |
| Title ("SONAR") | Top-left | Static |
| Depth ("DEPTH: -XX M") | Below title | `cam.position[1]` |
| FPS counter | Top-right | `timer_fps()` |
| Mode indicator | Above energy bar | `sonar_get_mode()` |
| Energy bar | Bottom-right | `energy_get_fraction()` |
| Crosshair | Screen center | Static |

---

## 5. Audio Pipeline

### 5.1 Initialization
```
OpenAL Soft device → context → enable HRTF
    → pre-load sound buffers
    → create source pool (N reusable sources)
```

### 5.2 Per-Frame Audio Update
```
audio_set_listener(camera.pos, camera.front, camera.up)
for each triggered sound:
    → acquire source from pool
    → attach buffer, set position, gain, pitch
    → play
for each active source:
    → if finished playing, return to pool
```

### 5.3 Sound Source Pool
- Pre-allocate N OpenAL sources (e.g., 32)
- Sonar echoes may need many simultaneous sources
- If pool exhausted: skip lowest-priority sound (furthest from player)

---

## 6. Sonar System Architecture

### 6.1 Overview
```
sonar_fire_pulse()
    │
    ├── Generate ray directions (spread pattern)
    │
    ├── For each ray:
    │   └── raycast_cast(origin, dir, maxDist, &hit)
    │       └── If hit:
    │           ├── sonar_add_point(hit.pos + scatter, color)
    │           ├── [audio] queue echo sound at hit.pos
    │           └── [optional] floor/ceiling points near hit
    │
    └── Deduct energy, set cooldown, trigger gun animation
```

### 6.2 Raycast Implementation (3D)
The prototype uses 2D DDA raycasting against a grid. The new version uses
**ray-triangle intersection** against the level's collision mesh.

Options:
- **Brute force**: Test every triangle (OK for small levels)
- **BVH**: Bounding Volume Hierarchy for O(log n) ray tests (needed for complex levels)
- **Recommendation**: Start with brute force, add BVH when profiling shows need

### 6.3 Point Storage
```c
#define MAX_SONAR_POINTS   65536
#define SONAR_MAX_RANGE    25.0f

typedef struct {
    float pos[3];     // world position (x, y, z)
    float color[3];   // RGB normalized floats [0, 1]
    float age;        // seconds since spawn (reserved; not yet used for fade)
} SonarPoint;        // 28 bytes per point
```

Storage is a file-static circular buffer in `src/sonar/sonar.c`:

```c
static SonarPoint s_points[MAX_SONAR_POINTS];
static int        s_pointCount;   // active point count, capped at MAX_SONAR_POINTS
static int        s_writeHead;    // next write index (mod MAX_SONAR_POINTS)
static int        s_frameStart;   // write-head snapshot from sonar_frame_begin()
```

**Note**: color was changed from `uint32_t` (packed RGBA) to `float[3]` in M5 to
avoid precision loss when sampling fractional color ranges (GDD §6.2) and to
upload directly to the GPU without unpacking.

---

## 7. Asset Formats

### 7.1 Level Format

Levels are authored in Blender and exported as `.glb` (glTF 2.0 binary). See
[ADR 0006](adr/0006-world-authoring-blender-gltf.md). The companion JSON metadata
format is **superseded**; all level data is embedded in the glTF file.

```
assets/models/levelname.glb    ← single file: geometry + collision + entity defs
```

#### Blender Node Naming Conventions

| Prefix / Name | Type | Behaviour in loader |
|---------------|------|---------------------|
| *(no prefix)* | Visual mesh | Uploaded to GPU via `model_load()`; not used for physics |
| `col_` | Collision mesh | Extracted as triangle soup for `physics_init()` and raycast; never rendered |
| `player_spawn` | Empty object | Initial player position (translation) and yaw (Y rotation) |
| `entity_creature_<id>` | Empty object + extras | Parsed as `ENTITY_CREATURE` |
| `entity_dial_<id>` | Empty object + extras | Parsed as `ENTITY_DIAL` |
| `entity_door_<id>` | Empty object + extras | Parsed as `ENTITY_DOOR` |
| `entity_sound_<id>` | Empty object + extras | Parsed as `ENTITY_SOUND` |

#### Material Naming Conventions

| Material name | Effect |
|---------------|--------|
| `mat_clue_red` | Sonar hits generate red points `[1.0, 0.15, 0.15]` |
| `mat_clue_blue` | Sonar hits generate blue points `[0.15, 0.45, 1.0]` |
| *(any other)* | Standard per-surface color with random range |

### 7.2 Level JSON Schema
```json
{
    "version": 1,
    "player": { "x": 2.5, "y": 2.5, "z": 0.0, "yaw": 90.0 },
    "storyZones": [
        { "bounds": { "min": [0,0,0], "max": [5,3,5] }, "text": "..." }
    ],
    "winZone": { "min": [12,0,27], "max": [18,3,32] },
    "teleports": [
        { "bounds": {...}, "target": [10,0,15], "facing": [0,0,1] }
    ],
    "colors": {
        "wall": { "r": [0,0], "g": [200,255], "b": [180,220] },
        "floor": { "r": [0,0], "g": [180,240], "b": [140,180] },
        "ceiling": { "r": [40,70], "g": [140,180], "b": [200,255] }
    },
    "audio": {
        "ambient": "drone_01.ogg",
        "reverb": "large_hall"
    }
}
```

### 7.3 Sound Files
- Format: `.ogg` (Vorbis) for music/ambient, `.wav` for short effects
- Sample rate: 44100 Hz
- Channels: Mono for 3D-positioned sounds, stereo for ambient/music

---

## 8. Build System

> **Note**: vcpkg was replaced by `third_party/` git clone in M1. See
> [ADR 0003](adr/0003-deps-management.md) for rationale.

### 8.1 CMake Structure
```
CMakeLists.txt (root)
├── cmake/
│   └── (custom find modules if needed)
├── src/
│   ├── third_party/glad/   ← committed (generated OpenGL loader)
│   └── (all .c files listed in root CMakeLists.txt)
├── third_party/            ← git-ignored, populated by setup script
│   ├── SDL/
│   ├── openal-soft/
│   ├── cglm/
│   └── cgltf/
├── shaders/ (copied to build dir by CMake)
└── assets/  (copied to build dir by CMake)
```

### 8.2 Dependencies

| Library | Source | Integration |
|---------|--------|-------------|
| SDL2 | `third_party/SDL` (git clone) | `add_subdirectory` |
| OpenAL Soft | `third_party/openal-soft` (git clone) | `add_subdirectory` |
| cglm | `third_party/cglm` (git clone) | `add_subdirectory` |
| cgltf | `third_party/cgltf` (git clone) | header-only, `include_directories` |
| glad | `src/third_party/glad/` (committed) | compiled directly into target |
| stb_vorbis | `src/third_party/` (committed) | single-header |
| dr_wav | `src/third_party/` (committed) | single-header |

### 8.3 First-Time Setup
```bash
# Clone all external dependencies
bash tools/setup_deps.sh

# Configure and build
cmake -B build -S .
cmake --build build

# Run
./build/sonar
```

### 8.4 Compiler Flags
- `-std=c11 -Wall -Wextra -Wpedantic` (GCC/Clang)
- `/std:c11 /W4` (MSVC)
- Debug: `-g -O0 -DSONAR_DEBUG`
- Release: `-O2 -DNDEBUG`

---

## 9. Performance Budget

| Metric | Budget | Notes |
|--------|--------|-------|
| Frame time | < 16.67ms (60 FPS) | Target on mid-range GPU |
| Sonar point upload | < 2ms | 65K points × 28 bytes = ~1.8 MB |
| Raycast (burst) | < 5ms | 250 rays against collision mesh |
| Audio update | < 1ms | Source pool management |
| Draw calls | < 50 per frame | Sonar = 1 draw call |
| Max entities per level | 32 | Hard cap enforced by loader |
| Creature passive reveal | ≤ 200 SonarPoints per event | Mesh decimated at load time if > 200 verts |

---

## 10. Open Technical Questions

| # | Question | Status | Blocking |
|---|----------|--------|----------|
| 1 | BVH for raycasting: when to implement? | Deferred to M3 | No |
| 2 | Hot-reload shaders in debug mode? | Nice-to-have | No |
| 3 | Separate collision mesh from render mesh? | Resolved in M2: both derived from same procedural geometry; separate when file-based maps land | No |
| 4 | Thread audio update on separate thread? | Deferred | No |
| 5 | Window resizing support? | Deferred | No |
| 6 | Explored grid origin/size constants — need re-evaluation once first Blender level defines real map bounds | Open — blocked on first level asset | Yes (minimap accuracy) |
| 7 | Creature movement — static position in M6; pathfinding algorithm TBD | Deferred post-M6 | No |

---

## 11. Changelog

| Date       | Version | Changes                  |
|------------|---------|--------------------------|
| 2026-03-05 | 0.1.0   | Initial draft from prototype analysis and tech stack decisions |
| 2026-03-06 | 0.2.0   | M2 update: actual API signatures for all implemented modules; add glad dependency; replace vcpkg with third_party/ build system (ADR 0003); add world/physics module spec; resolve open question #3 |
| 2026-03-06 | 0.3.0   | M3 update: add M3 implementation record (sonar system + audio system); link ADR 0004 gap log |
| 2026-03-06 | 0.4.0   | M4 update: add M4 implementation record (sonar fidelity, HUD, visual FX); update ADR 0004 gap statuses; carry gap 6 to M5 |
| 2026-03-06 | 0.5.0   | M5 update: add M5 milestone record; update render pipeline (6-step order, depth pre-pass); update sonar_fx/vfx/hud module specs; update SonarPoint struct (float color, age field); add sonar frame-start API; link ADR 0005 |
| 2026-03-06 | 0.6.0   | M6 controls/polish: update §4.5 HUD to dual-batch + TTF (DM Mono, stb_truetype, 512×512 GL_R8 atlas, y0/y1 fixup); add text.vert/frag to shader list; add §3.2 view bob (main.c); add disk sampling note to sonar/sonar continuous fire |
| 2026-03-07 | 0.7.0   | M6 world/entity: update §2.13 world/map (cgltf loader, entity API); add §2.15 world/entity (Entity struct, API, passive reveal spec); update §7.1 glTF node/material naming conventions; update §9 perf budget (entity limits); add §10 Q6/Q7; link ADR 0006/0007; add §15 M6 milestone record |
| 2026-03-11 | 0.7.1   | M6a entity system + sonar TTL (§15.4); M6b main.c wiring + map entity API (§15.5); add docs/api/entity.md |
| 2026-03-18 | 0.8.0   | M6c trigger/stalker/particles spec: add §2.16 world/trigger (TriggerState, zone/step logic, starfield reveal), §2.17 world/stalker (StalkerState, 4-phase FSM, compute_behind_pos), §2.18 render/vfx_particles (VfxParticle, MAX 2048, shockwave/collapse emitters, particle.vert/frag), §2.19 map MeshRange extension (MAX_MESH_RANGES=32, MAX_MESH_RANGE_TRIS=8192, map_get_mesh_range_tris API), §2.20 sonar TTL per-point field + sonar_get_fire_count(); add §16 M6c milestone record; update TDD header version |
| 2026-03-18 | 0.9.0   | M8: add §17 — F-key interaction (entity_find_nearest_interactable), door collision toggle (CollisionRange, physics/raycast tri disable mask), minimap with M key (energy drain 20/s, direction indicator), vignette removal; link ADR 0009 |
| 2026-03-18 | 0.9.1   | M8 doc audit: correct §17.3 minimap from fullscreen overlay to centered square panel (70 % of shorter axis, deep blue theme, diagonal wave animation, 5-quad direction line, below-panel energy bar); remove vignette from §4.1 pipeline diagram; add GL_NEAREST font filtering to §4.5; update §2.12 raycast API to match implementation; update §2.13 map API (add collision_range, clue_color, entities); update §2.14 physics API (add set_tris_enabled); update §2.15 Entity struct (add TRIGGER/STALKER types, active field, mesh_ref, ttl, radius) and entity API (add find_nearest_interactable) |
| 2026-03-24 | 0.9.3   | M9: §2.15 passive reveal spec updated — `passive_reveal()` now propagates `e->ttl` to `SonarPoint.ttl` (0 = default 0.8 s, >0 = custom); §17.2 door activate spec updated with spatial audio on toggle; add §18 M9 milestone record |

---

## 12. M3 — Sonar System + Audio System

> **Milestone**: M3 (complete)
> **Date**: 2026-03-06

This section records what was implemented in M3 and the integration state at milestone close.
Known gaps vs. the prototype are tracked in [ADR 0004](adr/0004-m3-gap-log.md).

---

### 12.1 M3a — Sonar System

**New modules**: `sonar/raycast`, `sonar/energy`, `sonar/sonar`, `render/sonar_fx`

#### Raycast (`src/sonar/raycast.c`)

- Algorithm: **Möller–Trumbore** ray-triangle intersection
- Geometry: brute-force test against collision triangle soup from `world/map`
- No BVH (deferred; see open question #1)

#### Energy (`src/sonar/energy.c`)

| Parameter | Value |
|-----------|-------|
| Max energy | 100 |
| Recharge rate | 12 / sec |
| Wide pulse cost | 20 |
| Focused pulse cost | 15 |
| Continuous fire cost | 20 / sec |

#### Sonar Point Buffer (`src/sonar/sonar.c`)

| Parameter | Value |
|-----------|-------|
| Buffer size | 65 536 points (`MAX_SONAR_POINTS`) |
| Layout | Circular buffer (oldest points overwritten) |
| Persistence | No fade — points are permanent until overwritten |
| Point data | `{float x, y, z; uint32_t color}` — 16 bytes/point |

#### Ray Distribution

- **Wide pulse**: Fibonacci spiral covering a cone (wide spread angle)
- **Focused pulse**: Fibonacci spiral in a narrow cone
- **Continuous fire**: Single forward ray per frame

#### Sonar FX Render (`src/render/sonar_fx.c`)

| Parameter | Value |
|-----------|-------|
| Primitive | `GL_POINTS` |
| Blending | Additive — `glBlendFunc(GL_ONE, GL_ONE)` |
| Point size | Distance-scaled via `gl_PointSize` in vertex shader |
| Upload | `glBufferSubData` per frame (only dirty range) |

---

### 12.2 M3b — Audio System

**New modules**: `audio/audio`, `audio/sound`, `audio/spatial`

#### Device & Context

- OpenAL Soft device opened with default output
- HRTF enabled (binaural spatialization)
- Source pool: **32 pre-allocated sources** (LRU eviction when exhausted)

#### Audio Decoding

| Format | Decoder |
|--------|---------|
| `.wav` | dr_wav (single-header, `src/third_party/`) |
| `.ogg` | stb_vorbis (single-header, `src/third_party/`) |

#### Listener Update

- `audio_set_listener(pos, front, up)` called every frame, synced to camera
- Ensures all 3D-positioned sources are spatialized correctly

---

### 12.3 M3 Integration State

| Input | Action | Status |
|-------|--------|--------|
| SPACE | Fire wide sonar pulse + play `sonar_ping.wav` | Implemented |
| LMB (hold) | Continuous fire | Implemented |
| RMB | Toggle wide ↔ focused mode | Implemented |

**Known gaps** (to be closed in M4): see [ADR 0004](adr/0004-m3-gap-log.md) for full details.

| Gap | Summary |
|-----|---------|
| G1 | Single hit point only — no scatter / secondary points |
| G2 | No floor points sampled along ray trajectory |
| G3 | No automatic passive 360° ping |
| G4 | Fixed colors — no per-surface random range |
| G5 | No visual FX (device sprite, laser lines, pulse ripple, ambient dust) |
| G6 | `sndEcho` loaded but no playback path to sonar hit points |

---

## 13. M4 — Sonar Mechanism Fidelity + HUD + Visual FX

> **Milestone**: M4 (complete)
> **Date**: 2026-03-06

M4 closed five of the six gaps from [ADR 0004](adr/0004-m3-gap-log.md). Gap 6 (echo audio
playback) is carried forward to M5.

---

### 13.1 M4a — Sonar Firing Mechanism (`src/sonar/sonar.c`)

#### Multi-Point Scatter Per Hit

On each wall hit, the sonar system now generates multiple points per ray using
probabilistic density weights:

| Point type | Probability | Offset |
|------------|-------------|--------|
| Primary wall point | 100% | ±0.04 XZ jitter |
| Second wall point | 50% × density | ±0.04 XZ jitter |
| Floor-at-base point | 60% × density | at hit XZ, Y = 0 |
| Ceiling point | 40% × density | at hit XZ, Y = ceiling height |

Density is mode-dependent: wide pulse = `0.6`, focused pulse = `1.2`,
continuous fire inherits the active mode's density.

#### Floor Points Along Ray Trajectory

For each ray, the system walks along the ray path and deposits floor points at
variable intervals:

- **Skip condition**: ray skipped when `dir.Y > 0.3` (steeply downward rays would
  flood the floor with redundant points)
- **Step size**: base step scaled by `1.0 / density` — focused mode samples more densely
- Floor point Y is clamped to 0 (ground plane)

#### Passive Ping

| Parameter | Value |
|-----------|-------|
| Interval | 2.0 seconds |
| Ray count | 35 |
| Distribution | 360° sweep in XZ plane (horizontal ring) |
| Max range | 2.5 m |
| Energy cost | None |

The passive ping uses the player pose cached by `sonar_set_player()`, which must be
called once per frame before `sonar_update()`.

#### RNG

Linear Congruential Generator (LCG) — deterministic, no stdlib dependency:

| Parameter | Value |
|-----------|-------|
| Seed | 42 |
| Multiplier | 1 664 525 |
| Increment | 1 013 904 223 |

Outputs are mapped to `[0, 1)` floats for scatter and probability rolls.

#### Color Ranges (`ColorRangeF`)

Per-surface-type random color, sampled per point. Channel ranges are floats in `[0, 1]`
mapped from the 8-bit GDD 6.2 spec:

| Surface | R | G | B |
|---------|---|---|---|
| Wall    | 0 | 180–255 | 160–220 |
| Floor   | 0 | 160–240 | 120–180 |
| Ceiling | 30–70 | 120–180 | 180–255 |

(Values are in 0–255 range as specified in GDD 6.2; stored internally as normalized floats.)

---

### 13.2 M4b — HUD (`src/render/hud.c`)

A 2D orthographic overlay rendered after all 3D passes. No depth test; drawn in screen
space.

#### HUD Elements

| Element | Position | Description |
|---------|----------|-------------|
| Energy bar | Bottom-right | Filled bar proportional to current energy / 100 |
| Mode indicator | Above energy bar | Text label: "WIDE" or "FOCUSED" |
| Crosshair | Screen center | Dot + circle, r=5px, points every 15° |
| Title | Top-left | Static text "SONAR" |
| Depth | Top-left (below title) | Dynamic text "DEPTH: -XX M" (player Y) |
| FPS counter | Top-right | Smoothed FPS from `timer_fps()` |

#### Bitmap Font

- Internal 5×7 pixel bitmap font (no external asset)
- Character set: A–Z, 0–9, common punctuation (`:./-`)
- Rendered as GL_POINTS or GL_TRIANGLES quads per glyph pixel

#### Shaders

| File | Purpose |
|------|---------|
| `shaders/hud.vert` | Orthographic transform, pixel-space positions |
| `shaders/hud.frag` | Flat color output, no lighting |

---

### 13.3 M4b — Visual FX (`src/render/vfx.c`)

Post-process and overlay effects rendered as full-screen quad passes.

#### Scanlines

- Fragment shader darkens every 3rd screen row by factor **0.85**
- Applied as a multiplicative pass over the final composited image

#### Vignette

- Radial distance from screen center, squared
- Falloff factor: **3.2**
- Output clamped to `[0.15, 1.0]` — darkens corners, never fully black
- Shared shader file with HUD: `shaders/vignette.vert` / `shaders/vignette.frag`

#### Pulse Ripple

| Parameter | Value |
|-----------|-------|
| Trigger | SPACE (wide or focused pulse fire) |
| Duration | 0.4 seconds |
| Shape | Expanding ellipse |
| Y squish | 0.6× (horizontally elongated) |
| Fade | Opacity decreases linearly over duration |

---

### 13.4 M4 Gap Closure Summary

| Gap | Title | M4 Status |
|-----|-------|-----------|
| G1 | Single point vs scatter | Fixed in M4a |
| G2 | No floor points along ray | Fixed in M4a |
| G3 | No passive ping | Fixed in M4a |
| G4 | Fixed colors | Fixed in M4a |
| G5 | No visual FX | Fixed in M4b (scanlines, vignette, pulse ripple) |
| G6 | Echo sound not triggered | **Deferred to M5** — `sndEcho` still loaded but `spatial_play()` not called on sonar hit |

---

## 14. M5 — Sonar Visual Refactor

> **Milestone**: M5 (complete)
> **Date**: 2026-03-06
> **Commit**: `1ef2f6a`

M5 refactored the sonar rendering layer from the M4 baseline. No new game mechanics
were introduced; all changes are in the visual presentation pipeline.

---

### 14.1 Sonar Point Rendering (`src/render/sonar_fx.c`)

**Change from M4**: Points were previously rendered as billboard quads (two triangles
per point, camera-facing). M5 replaced these with `GL_POINTS`.

| Parameter | M4 (billboard) | M5 (GL_POINTS) |
|-----------|---------------|----------------|
| Primitive | `GL_TRIANGLES` (2 tris/point) | `GL_POINTS` |
| Point sizing | Uniform scale passed as uniform | `gl_PointSize` in vertex shader |
| Circle shape | Quad with alpha texture | Hard discard in fragment shader |
| Depth function | `GL_LESS` | `GL_LEQUAL` (see below) |
| Blend | `GL_ONE, GL_ONE` | `GL_ONE, GL_ONE` (unchanged) |

A **depth-only pre-pass** was introduced (Step 1 in the render loop): the world mesh
is drawn with `glColorMask(GL_FALSE, …)`, writing only the depth buffer. This enables
`GL_LEQUAL` to correctly occlude sonar points behind walls while allowing co-planar
surface points to render. See [ADR 0005](adr/0005-sonar-visual-design.md).

---

### 14.2 SonarPoint Struct Change

```c
// M4 struct
typedef struct { float x, y, z; uint32_t color; } SonarPoint;  // 16 bytes

// M5 struct (src/sonar/sonar.h)
typedef struct {
    float pos[3];    // world position
    float color[3];  // RGB floats [0, 1] — was packed uint32_t
    float age;       // seconds since spawn (reserved)
} SonarPoint;        // 28 bytes
```

Color changed from packed `uint32_t` to `float[3]` to avoid precision loss when
sampling fractional per-surface color ranges. The `age` field is reserved for
a future fade mechanic (open design question).

---

### 14.3 Laser Lines (`src/render/vfx.c`)

New feature: when LMB is held, cyan `GL_LINES` are drawn in 2D screen-space from
the gun muzzle to each sonar hit point added this frame.

The frame-start API (`sonar_frame_begin`, `sonar_get_frame_start`,
`sonar_get_write_head`) was added to `sonar.h` to expose this-frame point range
without requiring the VFX layer to track its own counters. See §2.11.

---

### 14.4 Gun Sprite (`src/render/vfx.c`)

New feature: a procedural 2D gun outline is rendered in the bottom-right corner
as batched `GL_TRIANGLES`. The sprite has two visual states: resting and active
(LMB held, brightened). No external texture asset is used.

---

### 14.5 Minimap (`src/render/hud.c`, `src/sonar/sonar.h`)

New feature: a fog-of-war minimap in the top-left corner of the HUD.

`sonar.c` maintains a flat `int[EXPLORE_GRID_H * EXPLORE_GRID_W]` explored grid.
A cell is marked explored when any sonar point's world position maps into it.
`hud.c` reads the grid via `sonar_get_explored_grid()` and draws explored cells
as 2×2 px squares in the HUD orthographic pass.

| Parameter | Value |
|-----------|-------|
| Grid size | 16 × 40 cells |
| Cell size (world) | 0.5 m × 0.5 m |
| World origin | X = −4.0 m, Z = −4.0 m |
| Constants | `EXPLORE_GRID_W`, `EXPLORE_GRID_H`, `EXPLORE_ORIGIN_X/Z`, `EXPLORE_CELL_SIZE` in `sonar.h` |

---

### 14.6 M5 Gap Status

| Gap | Title | M5 Status |
|-----|-------|-----------|
| G6 | Echo sound not triggered | **Still open** — carried to M6 |

---

## 15. M6 — World System + Entity System

> **Milestone**: M6 (specification complete; implementation in progress)
> **Date**: 2026-03-07

M6 introduces real level authoring and a typed entity system. No new sonar mechanics
are added; focus is entirely on world loading and interactive objects.

---

### 15.1 World System (`src/world/map.c`)

**Change from M5**: the procedural two-room test geometry is replaced by a full
cgltf `.glb` loader. See [ADR 0006](adr/0006-world-authoring-blender-gltf.md).

The loader performs a single cgltf traversal:
1. **Visual mesh nodes** (no prefix) → uploaded to GPU via existing `model_load()` path
2. **Collision nodes** (`col_` prefix) → extracted as flat float triangle array for physics
3. **Material inspection** — primitives with `mat_clue_*` materials tagged for special
   sonar color assignment
4. **Empty objects** → parsed by name prefix into `Entity` structs or player spawn

The procedural fallback (`map_load(NULL)`) is retained for development use.

### 15.2 Entity System (`src/world/entity.c`)

**New module.** See [ADR 0007](adr/0007-entity-system.md) for full spec.

Key implementation notes for M6:

- Creatures are **statically positioned** in M6 (no movement). Pathfinding deferred
  (see §10 Q7).
- The `age` field on `SonarPoint` is **first used** in M6 for creature reveal TTL (0.8s).
  `sonar_update()` must cull points with `age >= 0.8f` for creature-origin points.
  Non-creature points have `age = 0` and are never culled (existing behaviour).
- Password dial UI is a **minimal 4-slot digit overlay** rendered by `hud.c`.
  No new shader; uses existing text batch.

### 15.3 M6 Gap Status

| Gap | Title | M6 Status |
|-----|-------|-----------|
| G6 | Echo sound not triggered | `spatial_play()` wired to creature reveal events; echo on wall-hit still pending |

---

### 15.4 M6a Implementation Record

> **Commit**: `7a31030`  feat: M6a entity system + sonar TTL culling for creature reveal
> **Date**: 2026-03-11

#### `world/entity.h` — Types and API Surface

**`EntityType` enum** (`src/world/entity.h:13`):

| Value | Meaning |
|-------|---------|
| `ENTITY_CREATURE` | Moving (or static in M6) creature that emits ambient sound and passive sonar reveal |
| `ENTITY_DIAL`     | Password dial interactive object; stores a numeric `code` |
| `ENTITY_DOOR`     | Toggle-able door; stores a `target` entity id for activation signal |
| `ENTITY_SOUND`    | Ambient point-source sound emitter; no interaction |

**`Entity` struct** (`src/world/entity.h:23`): all fields populated by the map loader from glTF extras at level load time. Runtime-mutable fields (`sound_timer`) are initialised by `entity_init()`.

**Four public functions** — see [docs/api/entity.md](api/entity.md) for full signatures and parameter details.

#### `passive_reveal()` — Creature Mesh Outline Injection (`src/world/entity.c:40`)

Invoked inside `entity_update()` whenever a creature's `sound_timer` expires
(`e->type == ENTITY_CREATURE && e->mesh_index >= 0`). The function:

1. Retrieves the global collision vertex array via `map_get_collision_verts()` and
   `map_get_collision_tri_count()`.
2. Iterates up to `MAX_REVEAL_VERTS` (200) vertices from the flat array
   (`triCount * 3` total vertices, 9 floats per triangle).
3. For each vertex, constructs a `SonarPoint` with:
   - `pos` copied directly from the collision array (`verts[i*3+0..2]`)
   - `color` = `{1.0f, 0.55f, 0.1f}` (orange, per ADR 0007)
   - `age` = `0.001f` (`CREATURE_POINT_AGE`) — a nonzero sentinel that marks
     this point as transient and subject to TTL culling in `sonar_update()`
4. Calls `sonar_add_point(&p)` to insert the point directly into the circular buffer.

**Current limitation**: `e->mesh_index` is checked for `>= 0` as a guard but not yet
used to filter vertices to the creature's specific mesh range. All collision
geometry up to the 200-vertex cap is projected. Mesh-scoped filtering is deferred
to M6c when the glTF loader populates per-entity mesh ranges.

#### `sonar_add_point()` — New API (`src/sonar/sonar.h:84`, `src/sonar/sonar.c:412`)

```c
void sonar_add_point(const SonarPoint *p);
```

Inserts a fully-formed `SonarPoint` directly at the current `s_writeHead` position
in the circular buffer, then advances `s_writeHead` and increments `s_pointCount`
(capped at `MAX_SONAR_POINTS`). Unlike the internal `spawn_point()`, which always
writes `age = 0.0f`, `sonar_add_point()` preserves the caller-supplied `age`, making
it the intended cross-module injection path for transient points.

#### `sonar_update()` — Age-Based TTL Culling (`src/sonar/sonar.c:384`)

The TTL logic is appended after the existing passive-ping and cooldown updates:

```c
for (int i = 0; i < s_pointCount; i++) {
    if (s_points[i].age > 0.0f) {
        s_points[i].age += dt;
        if (s_points[i].age >= 0.8f)
            s_points[i].age = -1.0f;
    }
}
```

Sentinel values:

| `age` value | Meaning |
|-------------|---------|
| `== 0.0f`  | Permanent sonar point (normal wall/floor/ceiling hit) — never aged |
| `> 0.0f`   | Transient point (creature reveal) — accumulates dt each frame |
| `== -1.0f` | Dead / expired — skipped by renderer |

TTL threshold is **0.8 seconds**. When `age >= 0.8f`, the point is marked `-1.0f`
and its slot is logically vacant (overwriteable by the next `sonar_add_point()` or
`spawn_point()` call that wraps around the circular buffer).

#### `sonar_fx_render()` — Skip Dead Points (`src/render/sonar_fx.c:79`)

During GPU buffer packing, each point is tested before upload:

```c
if (points[i].age == -1.0f)
    continue;   /* TTL-expired creature reveal point — skip */
```

This is an exact float comparison against the sentinel; no epsilon is applied because
`-1.0f` is only ever written by the TTL cull branch in `sonar_update()`.

---

### 15.5 M6b Implementation Record

> **Commit**: `6a9e43e`  feat: M6b wire entity system into main.c + map.c entity API + cgltf stub
> **Date**: 2026-03-11

#### `main.c` — Entity System Wiring

Three call sites added:

| Call | Location | Surrounding context |
|------|----------|---------------------|
| `entity_init(map_get_entities(), map_get_entity_count())` | Init path, line 68 | After `map_load()` succeeds |
| `entity_update(dt, map_get_entities(), map_get_entity_count())` | Game loop, line 227 | After `sonar_update(dt)` |
| `entity_shutdown()` | Shutdown path, line 315 | After `physics_shutdown()`, before `map_shutdown()` |

The shutdown order (`physics_shutdown` → `entity_shutdown` → `map_shutdown`) ensures
that `entity_shutdown()` can still reference entity data before `map.c` frees it,
and that `map_shutdown()` runs last to keep collision pointers valid for the full
entity lifecycle.

#### `map.h` — New Entity API (`src/world/map.h:35`)

Two accessors added to expose the map's internal entity storage:

```c
Entity *map_get_entities(void);     /* mutable pointer to s_entities[32] */
int     map_get_entity_count(void); /* number of entities parsed */
```

These are intentionally mutable because `entity_init()` and `entity_update()` write
runtime state (`sound_timer`) back into the same array owned by `map.c`.

#### `map.c` — Static Entity Storage (`src/world/map.c:33`)

```c
static Entity  s_entities[32];
static int     s_entityCount = 0;
```

The cap of 32 entities matches the performance budget from §9. Both arrays are
zeroed in `map_shutdown()` (`memset(s_entities, 0, sizeof(s_entities))`) and
`s_entityCount` is reset to 0 at the top of `map_load()` before any parsing.

#### cgltf Stub — Design Intent (`src/world/map.c:150`)

When `map_load()` is called with a non-NULL path, it logs a warning and falls back
to the procedural two-room generator. The stub comment documents the planned
five-step glTF traversal:

1. `cgltf_parse_file()` → node traversal
2. Visual mesh nodes → `model_load()` upload
3. `col_` prefix nodes → collision triangle extraction
4. `mat_clue_*` materials → sonar color tagging
5. `entity_*` / player-spawn nodes → populate `s_entities[]`

The stub is marked `TODO M6c` and will be replaced once a `.glb` asset is available.
The procedural fallback is retained as a permanent development aid so the game loop
remains exercisable without authored level data.

---

## 16. M6c — Trigger, Stalker & Particle VFX

> **Milestone**: M6c (specification complete; implementation pending)
> **Date**: 2026-03-18

M6c introduces two new gameplay systems (Static Triggers, Stalker) and the supporting
particle VFX module. It also extends `map.c` with a `vis_*` mesh range buffer and
extends `SonarPoint` with a per-point TTL field and a fire counter API. No new sonar
firing modes are introduced.

---

### 16.1 New Modules

| Module | Source files | Depends on |
|--------|-------------|------------|
| `world/trigger` | `src/world/trigger.h`, `src/world/trigger.c` | `sonar/sonar`, `world/map`, `audio/spatial` |
| `world/stalker` | `src/world/stalker.h`, `src/world/stalker.c` | `sonar/sonar`, `sonar/raycast`, `world/map`, `render/vfx_particles` |
| `render/vfx_particles` | `src/render/vfx_particles.h`, `src/render/vfx_particles.c` | `core/` only |

Full module specifications: §2.16 (trigger), §2.17 (stalker), §2.18 (vfx_particles).

---

### 16.2 `world/trigger.c` — Reveal Logic

#### Zone Mode

`trigger_update()` iterates all unfired `TriggerState` entries each frame:

1. Compute player–trigger distance: `d = length(player_pos - entity.pos)`.
2. If `d ≤ radius` and `fired == 0` and `mode == TRIGGER_MODE_ZONE`:
   - If no trigger in the same `zone_id` has `zone_activated == 1` yet, set
     `zone_activated = 1` and `delay_timer = delay` for **all** triggers sharing
     this `zone_id`.
3. Each activated trigger with `delay_timer > 0` ticks down by `dt`; when
   `delay_timer ≤ 0`, call `trigger_reveal_mesh()` and set `fired = 1`.

#### Step Mode

For all unfired triggers with `mode == TRIGGER_MODE_STEP`:

1. Find the nearest unfired trigger to `player_pos` within `radius`.
2. If distance `≤ radius`, set that trigger's `delay_timer = delay` and
   `zone_activated = 1`; tick down and fire as above.

Only one step trigger fires per `trigger_update()` call (the nearest).

#### `trigger_reveal_mesh()` — Point Generation

```
for each triangle T in mesh range:
    generate 10 SonarPoints:
        P[0..2]  = T.v0, T.v1, T.v2                 (vertices)
        P[3..5]  = midpoints(e01, e12, e20)          (edge midpoints)
        P[6]     = centroid(T)                        (centroid)
        P[7..9]  = centroid(T) + rand_in_triangle(T) (jitter ±0.06 m)
    assign color = (0, 0.85, 0.75)
    70% of points: age=0.0, ttl=0.0   (permanent)
    30% of points: age=0.001, ttl=0.3 (flash, expires in 0.3 s)
    if total points would exceed 500: break triangle iteration
    sonar_add_point() for each
```

---

### 16.3 `world/stalker.c` — State Machine Implementation

#### DORMANT → APPROACHING

`stalker_update()` calls `sonar_get_fire_count()` each frame. When the returned
count differs from `s->last_fire_count`, a new sonar fire has occurred:

```c
int fires = sonar_get_fire_count();
if (fires != s->last_fire_count) {
    s->last_fire_count = fires;
    s->idle_timer = 0.0f;
    if (s->phase == STALKER_DORMANT)
        s->phase = STALKER_APPROACHING;
    s->current_dist -= s->step_dist;
    if (s->current_dist < s->step_dist) s->current_dist = s->step_dist;
}
```

#### APPROACHING → VISIBLE

When `current_dist ≤ step_dist` (minimum 1.5 m):

1. `compute_behind_pos()` resolves the world position → `s->appear_pos`.
2. `stalker_reveal_mesh(s)` injects red sonar outline (TTL 2.0 s).
3. `vfx_emit_shockwave(s->appear_pos, …)` spawns 64-particle ring.
4. `sound_play(s->sound_appear_buf, s->appear_pos, 1.0f)` triggers audio.
5. `s->reveal_timer = 2.0f`; `s->phase = STALKER_VISIBLE`.

#### VISIBLE → DEPARTING

Each frame in VISIBLE: `s->reveal_timer -= dt`. When `≤ 0`:

1. `vfx_emit_collapse(s->appear_pos, …)` spawns 48-particle collapse.
2. `sound_play(s->sound_depart_buf, s->appear_pos, 1.0f)` triggers audio.
3. `s->phase = STALKER_DEPARTING`.

#### DEPARTING → DORMANT

`current_dist` is lerped back to `start_dist` at rate 2.0 m/s in DEPARTING.
When `current_dist ≥ start_dist`: `s->phase = STALKER_DORMANT`.

Idle retreat: if `idle_timer ≥ retreat_time` while in APPROACHING, transition
directly to DEPARTING without entering VISIBLE.

---

### 16.4 `render/vfx_particles.c` — Rendering

The particle system slots into the existing render pipeline between Steps 3 (laser
lines) and 4 (fullscreen VFX):

```
renderer_begin_frame()
  ├─ 1. depth-only world pass
  ├─ 2. sonar_fx_render()
  ├─ 3. vfx_render_laser_lines() (if LMB)
  ├─ 3b. vfx_particles_render()              ← NEW
  ├─ 4. vfx_render_scanlines()
  │    vfx_render_pulse_ripple()
  ├─ 5. vfx_render_gun()
  └─ 6. hud_render()
renderer_end_frame()
```

`vfx_particles_update(dt)` is called in the game loop alongside `sonar_update(dt)`,
before the render block.

**GL state contract**:
- Primitive: `GL_POINTS` with `GL_PROGRAM_POINT_SIZE` enabled
- Blend: `glBlendFunc(GL_SRC_ALPHA, GL_ONE)` — additive alpha
- Depth: `glDepthFunc(GL_LEQUAL)` while rendering; restored to `GL_LESS` after
- No depth write: `glDepthMask(GL_FALSE)` scoped to particle draw

---

### 16.5 `map.c` — `vis_*` MeshRange Extension

The `map_load()` traversal gains a fourth pass (after visual mesh, collision, and
entity parsing):

4. **`vis_*` nodes** → flattened into `s_meshRangeBuf` as triangle soup (9 floats/tri);
   a `MeshRange` entry records the name, `tri_offset`, and `tri_count`. These meshes
   are **not** uploaded to the GPU render model and are **not** used for physics.

New public API (`src/world/map.h`):

```c
int              map_get_mesh_range_count(void);
const MeshRange *map_get_mesh_range_by_name(const char *name);
void             map_get_mesh_range_tris(int index,
                                         const float **out_verts,
                                         int         *out_count);
```

Limits:
- `MAX_MESH_RANGES = 32` — one per trigger/stalker entity (shared entity cap)
- `MAX_MESH_RANGE_TRIS = 8192` — combined across all `vis_*` meshes; ~230 KB

---

### 16.6 `sonar/sonar.c` — TTL Extension & Fire Counter

#### SonarPoint struct (`src/sonar/sonar.h`)

`float ttl` is appended to `SonarPoint`. Size increases from 28 bytes to **32 bytes**.
GPU upload in `sonar_fx_render()` packs only `pos[3]` and `color[3]` (unchanged);
`age` and `ttl` are CPU-only fields and are not included in the GPU vertex layout.

All existing call sites that construct `SonarPoint` literals must be updated to
zero-initialise `ttl` (i.e., add `.ttl = 0.0f` or use a designated initialiser).

#### `sonar_update()` TTL branch update

The M6a fixed-threshold branch (`>= 0.8f`) is replaced:

```c
for (int i = 0; i < s_pointCount; i++) {
    if (s_points[i].age > 0.0f) {
        s_points[i].age += dt;
        float thr = (s_points[i].ttl > 0.0f) ? s_points[i].ttl : 0.8f;
        if (s_points[i].age >= thr)
            s_points[i].age = -1.0f;
    }
}
```

#### `sonar_get_fire_count()` (`src/sonar/sonar.h`, `src/sonar/sonar.c`)

```c
int sonar_get_fire_count(void);
```

Backed by `static int s_fireCount`. Incremented once per `sonar_fire_pulse()` call
and once per frame inside `sonar_fire_continuous()` (not per-ray). Never reset.
Overflow wraps at `INT_MAX` (safe: `stalker_update()` uses delta comparison, not
absolute value).

---

### 16.7 M6c Initialization Order Extension

New modules initialize and shut down in the following order relative to existing systems:

```c
// Init additions (after entity_init):
vfx_particles_init();
trigger_init();
stalker_init();

// Game loop additions (after entity_update):
trigger_update(dt, cam.position);
stalker_update(dt, cam.position, cam.front);
vfx_particles_update(dt);

// Shutdown additions (before entity_shutdown):
stalker_shutdown();
trigger_shutdown();
vfx_particles_shutdown();
```

`trigger_shutdown()` and `stalker_shutdown()` must execute before `entity_shutdown()`
because they hold indices into the entity array. `vfx_particles_shutdown()` has no
entity dependency and may be ordered freely within the render-layer teardown.

---

## 17. M8 — Player Interaction, Door System & Minimap Toggle

> **Milestone**: M8 (complete)
> **Date**: 2026-03-18
> **Related ADR**: [0009-door-collision-toggle](adr/0009-door-collision-toggle.md)

M8 introduces the F-key entity interaction system, runtime door collision toggling,
and a centered minimap overlay panel with energy drain and player direction indicator.
The vignette fullscreen VFX was removed.

---

### 17.1 F-Key Interaction (`src/world/entity.c`)

#### `entity_find_nearest_interactable()`

```c
int entity_find_nearest_interactable(const Entity *entities, int count,
                                     const float pos[3], float maxDist);
```

Iterates all entities, filtering for `ENTITY_DIAL` and `ENTITY_DOOR` types. Computes
3D Euclidean distance (squared, for efficiency) and returns the index of the nearest
qualifying entity within `maxDist`. Returns `-1` if none found.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `maxDist` | 2.5 m | Interaction range; passed from `main.c` |

#### `main.c` Integration

```c
if (input_key_pressed(SDL_SCANCODE_F)) {
    int idx = entity_find_nearest_interactable(
        map_get_entities(), map_get_entity_count(),
        cam.position, 2.5f);
    if (idx >= 0)
        entity_activate(&map_get_entities()[idx]);
}
```

Placed before the sonar input block in the game loop.

---

### 17.2 Door Collision Toggle

See [ADR 0009](adr/0009-door-collision-toggle.md) for the full design rationale.

#### CollisionRange (`src/world/map.c`)

```c
#define MAX_COLLISION_RANGES 32

typedef struct {
    char name[32];    /* Blender col_* node name */
    int  start_tri;   /* first triangle index in collision buffer */
    int  tri_count;   /* number of triangles */
} CollisionRange;

static CollisionRange s_colRanges[MAX_COLLISION_RANGES];
static int            s_colRangeCount = 0;
```

Populated during `load_glb()` traversal: each `col_*` node's triangle range is
recorded after extraction. The `map_get_collision_range()` API performs a linear
scan by name.

#### Triangle Disable Mask

Both `physics.c` and `raycast.c` maintain independent `static uint8_t s_triDisabled[]`
arrays (capacity 8192 each). The collision and raycast inner loops skip entries where
`s_triDisabled[i] == 1`.

```c
void physics_set_tris_enabled(int start, int count, int enabled);
void raycast_set_tris_enabled(int start, int count, int enabled);
```

#### `entity_activate()` — DOOR Case

```c
case ENTITY_DOOR: {
    e->active = !e->active;
    int start, count;
    if (map_get_collision_range(e->target, &start, &count) == 0) {
        physics_set_tris_enabled(start, count, !e->active);
        raycast_set_tris_enabled(start, count, !e->active);
    }
    if (e->sound[0] != '\0') {
        const Sound *snd = find_cached_sound(e->sound);
        if (snd) spatial_play(snd, e->pos, 1.0f, 1.0f);
    }
    break;
}
```

`Entity.active` is a new runtime field (0 = closed, 1 = open). Both `physics` and
`raycast` must be toggled in the same call to maintain consistency.

The optional `sound` field triggers a **spatial audio cue** at the door's world
position on every toggle. The sound buffer is pre-loaded by `entity_init()` via the
shared `find_cached_sound()` / `load_and_cache_sound()` cache; no additional loading
occurs at interaction time. If the door entity has no `sound` property set (empty
string), the sound block is skipped silently.

#### Blender Convention

| Object | Naming | Purpose |
|--------|--------|---------|
| Collision mesh | `col_door_XX` | Door panel collision geometry |
| Entity empty | `entity_door_XX` | Door entity; `target` property = `"col_door_XX"` |

---

### 17.3 Minimap Overlay Panel (`src/render/hud.c`, `src/main.c`)

#### M Key Toggle + Energy Drain

```c
int showMinimap = 0;
#define MINIMAP_DRAIN 20.0f

if (input_key_pressed(SDL_SCANCODE_M))
    showMinimap = !showMinimap;

if (showMinimap && !energy_spend(MINIMAP_DRAIN * dt))
    showMinimap = 0;
```

Energy recharge is blocked while the minimap is active (`!sonarFired && !showMinimap`).

#### `hud_render()` Signature Extension

```c
void hud_render(int winW, int winH, float energy, int sonarMode,
                float fps, const float *camPos,
                int showMinimap, float playerYaw);
```

Two new parameters: `showMinimap` gates minimap rendering; `playerYaw` (radians,
from `atan2f(cam.front[0], cam.front[2])`) drives the direction indicator (line of 5 quads).

#### Centered Panel Rendering

When `showMinimap == 1`:

1. **Square panel**: `panelSz = min(winW, winH) * 0.7`, centered on screen.
   Background fill: near-black `(0, 0.01, 0.03)` at alpha 0.88.
2. **Deep blue border**: 5 px wide, colour `(0.04, 0.08, 0.45)`, with breathing
   alpha pulsation (`0.5 + 0.4 * sin(t * 5.0)`).
3. **Grid cells**: explored cells drawn with a **diagonal wave animation** — a sine
   wave propagates from bottom-left to top-right across the grid. Wave parameters:
   `waveSpeed = 4.0`, `waveFreq = 3.5`. Cell colours blend between dim blue
   `(0.03, 0.08, 0.25)` and bright blue `(0.15, 0.28, 0.70)` based on wave phase.
4. **Player dot**: bright blue `(0.3, 0.5, 1.0)`, size = `cellPx * 0.7` (minimum 3 px).
5. **Direction indicator**: 5 small quads forming a line from the player dot toward
   the facing direction. Each successive quad fades in alpha (`1 - s/(steps+1)`).
   Quad size = `cellPx * 0.25` (minimum 1.5 px). Line length = `cellPx * 2.5`
   (minimum 10 px).
6. **Below-panel energy bar**: width = 80 % of panel width, height 8 px, positioned
   12 px below the panel. Blue fill `(0.1, 0.3, 0.9)` above 30 % energy; orange
   `(0.8, 0.25, 0.1)` at or below 30 %. Deep blue border matching the panel.
   ENERGY text label to the left. Only visible while the minimap is open.

---

### 17.4 Vignette Removal

The `vfx_render_vignette(winW, winH)` call was removed from `main.c`. The vignette
shader code (`u_mode == 1` in `vignette.frag`) remains in the shader file but is no
longer invoked. The render pipeline step order is now:

```
4. vfx_render_scanlines()
   vfx_render_pulse_ripple()
5. vfx_render_gun()
6. hud_render()
```

---

## 18. M9 — Creature TTL Propagation, Door Audio & Blender Authoring Guide

> **Milestone**: M9 (complete)
> **Date**: 2026-03-24

M9 delivers two targeted bug fixes / feature additions to `src/world/entity.c` and
one new documentation deliverable. No new modules are introduced.

---

### 18.1 `passive_reveal()` — TTL Propagation Fix (`src/world/entity.c:72`)

**Problem**: `passive_reveal()` always constructed `SonarPoint` with `p.ttl = 0.0f`,
discarding the per-entity `ttl` field even when a designer had set a custom value.
All creature reveals therefore expired at the hardcoded 0.8-second default regardless
of configuration.

**Fix**: Line 72 changed from `p.ttl = 0.0f` to `p.ttl = e->ttl`. The entity's
`ttl` field is now forwarded directly to the `SonarPoint`.

**Behavior after fix**:

| `e->ttl` | Effective TTL |
|----------|---------------|
| `0.0f` (default) | 0.8 s (sonar system default, via `(p->ttl > 0.0f) ? p->ttl : 0.8f` in `sonar_update()`) |
| `> 0.0f` | Custom duration in seconds |

Designers can now set the `ttl` custom property on any `entity_creature_*` Empty in
Blender to control the orange point cloud lifetime independently per creature.

---

### 18.2 `entity_activate()` DOOR — Spatial Audio (`src/world/entity.c:210–213`)

**Feature**: When a door entity's `sound` field is non-empty, `entity_activate()` now
plays a spatial audio cue at the door's world position after toggling its collision
geometry.

**Implementation**:
1. `entity_init()` already calls `load_and_cache_sound()` for every entity with a
   non-empty `sound` field — no new loading path required.
2. The new sound-playback block in the DOOR case calls `find_cached_sound(e->sound)`
   to retrieve the pre-loaded buffer, then `spatial_play(snd, e->pos, 1.0f, 1.0f)`.
3. If the cache lookup fails (sound not loaded or load error at init time), the block
   is silently skipped — no error is emitted at interaction time.

The sound fires on **every toggle** (both open and close). Designers who need
distinct open/close sounds must use two door entities with separate `sound` properties
and coordinate their logic at the level design level.

---

### 18.3 Blender Level Authoring Guide (`docs/guides/blender-level-authoring.md`)

A comprehensive Blender operations guide was written for level designers. It covers:

- Workspace and export settings
- Node naming conventions (`col_*`, `vis_*`, `entity_*`, `player_spawn`)
- Material setup (`mat_clue_red`, `mat_clue_blue`)
- Custom property authoring for all entity types (creature, dial, door, trigger, stalker)
- Common authoring pitfalls and how to avoid them

The guide is the primary reference for level designers working with the Blender → glTF
pipeline and is cross-referenced from the GDD (§8 and §9.3).

