# SONAR — Entity Module API Reference

> **Version**: 0.3.0
> **Status**: Accepted
> **Last Updated**: 2026-03-24
> **Owner**: CIO Agent
> **Source**: `src/world/entity.h` / `src/world/entity.c`
> **Related ADR**: [ADR 0007 — Entity System](../adr/0007-entity-system.md)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Types](#2-types)
   - 2.1 [EntityType](#21-entitytype)
   - 2.2 [Entity](#22-entity)
   - 2.3 [Entity Field Reuse Conventions](#23-entity-field-reuse-conventions)
3. [Functions](#3-functions)
   - 3.1 [entity_init](#31-entity_init)
   - 3.2 [entity_update](#32-entity_update)
   - 3.3 [entity_activate](#33-entity_activate)
   - 3.4 [entity_shutdown](#34-entity_shutdown)
4. [Cross-Module Dependency: sonar_add_point](#4-cross-module-dependency-sonar_add_point)
5. [Passive Reveal Data Flow](#5-passive-reveal-data-flow)
6. [Changelog](#6-changelog)

---

## 1. Overview

The `world/entity` module manages typed runtime entities parsed from a level's glTF
extras at load time. It drives two behaviours each frame:

- **Ambient sound emission** — entities with a configured `interval` count down a
  timer and play a 3D-positioned sound via `audio/spatial` when the timer expires.
- **Passive sonar reveal** — creature entities additionally inject orange `SonarPoint`
  records into the sonar circular buffer on each sound event, making nearby creature
  geometry briefly visible without requiring the player to fire a pulse.

The module depends on `sonar/sonar`, `world/map`, `audio/spatial`, `audio/sound`,
and `core/log`. It must be initialised after `map_load()` and shut down before
`map_shutdown()`.

---

## 2. Types

### 2.1 `EntityType`

```c
typedef enum {
    ENTITY_CREATURE,
    ENTITY_DIAL,
    ENTITY_DOOR,
    ENTITY_SOUND,
    ENTITY_TRIGGER,
    ENTITY_STALKER,
} EntityType;
```

| Value | Description |
|-------|-------------|
| `ENTITY_CREATURE` | A creature (enemy or ambient fauna). Emits ambient sound at regular intervals and injects orange sonar points tracing its collision mesh outline on each sound event (passive reveal). Static position in M6; pathfinding deferred. |
| `ENTITY_DIAL`     | A password dial object. Stores a numeric `code` (up to 15 chars). Player interaction (`entity_activate`) logs activation; dial input UI is a future milestone. |
| `ENTITY_DOOR`     | A door that can be toggled open/closed. Stores a `target` entity id to which it sends an activation signal. Door animation is a future milestone. |
| `ENTITY_SOUND`    | A stationary ambient sound emitter with no interaction. Plays its sound at the configured interval. No sonar reveal. |
| `ENTITY_TRIGGER`  | A one-shot environmental reveal. When the player enters its radius, injects permanent (or timed) sonar points from a `vis_*` mesh. Managed entirely by `world/trigger`; `entity_update()` skips this type. See [docs/api/trigger.md](trigger.md). |
| `ENTITY_STALKER`  | A threat entity that appears behind the player when sonar is used. Managed entirely by `world/stalker`; `entity_update()` skips this type. See [docs/api/stalker.md](stalker.md). |

Values correspond to Blender node name prefixes used during glTF authoring
(e.g., nodes named `creature_01`, `dial_vault`, `door_lab`, `sound_drip`,
`trigger_altar`, `stalker_01`).

---

### 2.2 `Entity`

```c
typedef struct {
    EntityType  type;
    float       pos[3];
    float       yaw;
    char        id[32];
    char        sound[64];
    char        code[16];
    char        target[32];
    float       interval;
    char        mesh_ref[32];
    float       ttl;
    float       radius;
    /* runtime state */
    float       sound_timer;
    int         mesh_index;
} Entity;
```

| Field | Type | Description |
|-------|------|-------------|
| `type` | `EntityType` | Discriminant tag; determines behaviour in `entity_update()` and `entity_activate()`. |
| `pos[3]` | `float[3]` | World-space position (x, y, z). Used as the 3D audio source position for `spatial_play()`. |
| `yaw` | `float` | Initial facing angle in radians. Informational in M6; used for door/creature orientation in future milestones. |
| `id[32]` | `char[]` | Unique string identifier (e.g. `"creature_01"`). Used for activation-signal routing via `target`. |
| `sound[64]` | `char[]` | Asset path relative to `assets/sounds/` (e.g. `"creature_growl.ogg"`). Empty string means no sound. |
| `code[16]` | `char[]` | Dial numeric code (e.g. `"4712"`). Also reused by trigger (mode: `"zone"` or `"step"`) and stalker (retreat time as decimal string). See §2.3. |
| `target[32]` | `char[]` | Activation-signal target entity id. Reused by trigger as `zone_id` and stalker as `sound_depart` asset name. See §2.3. |
| `interval` | `float` | Sound repeat interval in seconds for creatures/sounds. Reused by trigger as activation delay and stalker as `start_dist`. `<= 0` means no periodic emission. |
| `mesh_ref[32]` | `char[]` | Name of the `vis_*` mesh node in the glTF file used for reveal (trigger, stalker, creature). Populated by the glTF loader; empty for procedural levels. |
| `ttl` | `float` | Sonar point lifetime override. `0.0f` = permanent (normal sonar points); `> 0.0f` = custom TTL in seconds used when injecting reveal points. |
| `radius` | `float` | Trigger proximity radius (meters) or stalker `step_dist` (meters per sonar fire). `0.0f` = use subsystem default. |
| `sound_timer` | `float` | **Runtime.** Countdown to next sound event. Initialised to `interval` by `entity_init()`; decremented each frame by `entity_update()`. |
| `mesh_index` | `int` | **Runtime.** Index into the `MeshRange` array returned by `map_get_mesh_range_tris()`. `-1` means no vis mesh assigned. |

**Ownership**: the `Entity` array is owned by `world/map` (`s_entities[32]`).
`entity_init()` and `entity_update()` operate on a caller-supplied pointer to that array.

---

### 2.3 Entity Field Reuse Conventions

Several fields in `Entity` are repurposed by specialised subsystems (`trigger`, `stalker`)
that read from the same struct rather than defining separate types. The map loader
populates these fields from glTF extras using the conventions below.

| Field | `ENTITY_TRIGGER` | `ENTITY_STALKER` |
|-------|-----------------|-----------------|
| `sound` | Trigger activation sound (asset name) | Appear sound (asset name) |
| `target` | `zone_id` — groups triggers that activate together | Depart sound (asset name) |
| `interval` | Activation delay in seconds (stagger within a zone) | `start_dist` — initial distance behind player (meters) |
| `code` | Mode string: `"zone"` or `"step"` | Retreat time as decimal string (e.g. `"15.0"`) |
| `radius` | Proximity trigger radius (meters; default 3.0 m) | `step_dist` — distance reduction per sonar fire (meters; default 1.5 m) |
| `mesh_ref` | `vis_*` node name for reveal mesh | `vis_*` node name for stalker silhouette mesh |
| `ttl` | Sonar point TTL (`0` = permanent reveal) | Not read; stalker always uses 2.0 s TTL |

**Rationale**: a single `Entity` array avoids parallel storage; subsystems extract
their parameters during `_init()` into internal state structs, so the reuse is
one-time and read-only after init.

---

## 3. Functions

### 3.1 `entity_init`

```c
void entity_init(Entity *entities, int count);
```

Initialises runtime state for all entities and pre-loads their sound assets.

| Parameter | Description |
|-----------|-------------|
| `entities` | Pointer to the entity array; typically the value returned by `map_get_entities()`. |
| `count`    | Number of entities in the array; typically `map_get_entity_count()`. |

**Side effects**:
- Resets the internal sound cache (`s_soundCount = 0`).
- Sets each `entity->sound_timer = entity->interval`, so the first sound fires after
  exactly one interval rather than immediately.
- For each entity with a non-empty `sound` field, calls `sound_load_ogg()` or
  `sound_load_wav()` (determined by file extension) and caches the resulting `Sound`
  buffer. Failed loads are logged but do not abort initialisation.

**Call site**: `main.c` immediately after `map_load()` succeeds (line 68).

**No return value.** Does not fail silently — sound load errors are emitted via
`LOG_ERROR`.

---

### 3.2 `entity_update`

```c
void entity_update(float dt, Entity *entities, int count);
```

Ticks entity logic once per frame.

| Parameter | Description |
|-----------|-------------|
| `dt`       | Frame delta time in seconds. |
| `entities` | Pointer to entity array. |
| `count`    | Number of entities. |

**Per-entity logic**:

`ENTITY_TRIGGER` and `ENTITY_STALKER` are **skipped entirely** by this function
(`continue` on type check at `src/world/entity.c:134`). Their lifecycle is managed
by `world/trigger` and `world/stalker` respectively, which have their own `_update()`
calls in `main.c`. Mixing their update logic into `entity_update()` would create
ordering conflicts with subsystem state machines.

For all other types, if `interval <= 0.0f`: also skipped (no periodic behaviour).

For entities with `interval > 0.0f`:
1. Decrement `entity->sound_timer` by `dt`.
2. If `sound_timer > 0`: continue (timer still running).
3. On expiry (`sound_timer <= 0`):
   - If `sound` is non-empty: look up the cached `Sound` buffer and call
     `spatial_play(snd, e->pos, 1.0f, 1.0f)`.
   - Reset `sound_timer = interval`.
   - If `type == ENTITY_CREATURE && mesh_index >= 0`: call `passive_reveal(e)`.

**Side effects**: may call `sonar_add_point()` (via `passive_reveal`) for creature
entities. This writes into the sonar circular buffer owned by `sonar/sonar`.

**Call site**: `main.c` game loop, after `sonar_update(dt)` (line 227).

---

### 3.3 `entity_activate`

```c
void entity_activate(Entity *e);
```

Processes a player interaction (F-key press) with a single entity.

| Parameter | Description |
|-----------|-------------|
| `e` | The entity the player is interacting with. |

**Dispatch table**:

| `e->type` | Effect |
|-----------|--------|
| `ENTITY_DIAL` | `LOG_INFO("Dial activated: code=%s", e->code)` — dial input UI is a future milestone. |
| `ENTITY_DOOR` | Toggles `e->active` (0↔1); calls `physics_set_tris_enabled()` and `raycast_set_tris_enabled()` via `map_get_collision_range(e->target, …)`; if `e->sound` is non-empty, plays a spatial audio cue at `e->pos` via `find_cached_sound()` + `spatial_play()`. |
| All other types | No-op. |

**No return value.**

**DOOR side effects**:
- Collision and raycast geometry toggled atomically for the named `col_*` mesh range.
- Spatial audio played on every toggle (both open and close) when `e->sound` is set.
  The sound buffer is sourced from the pre-loaded cache populated by `entity_init()`;
  no file I/O occurs at interaction time.
- `LOG_INFO("Door %s: %s", e->id, e->active ? "OPEN" : "CLOSED")` always emitted.

---

### 3.4 `entity_shutdown`

```c
void entity_shutdown(void);
```

Releases all cached sound buffers loaded during `entity_init()`.

Calls `sound_destroy()` on each entry in `s_sounds[]` and resets `s_soundCount`
to 0. The entity array itself is owned by `world/map` and is not freed here.

**Call site**: `main.c` shutdown path, after `physics_shutdown()` and before
`map_shutdown()` (line 315). This ordering ensures that the entity module relinquishes
its audio resources before map geometry is freed.

---

## 4. Cross-Module Dependency: `sonar_add_point`

`entity.c` calls `sonar_add_point()` from `sonar/sonar.h` to inject creature reveal
points. This is the **only** permitted direction for cross-module sonar writes from
the world layer.

```c
/* src/world/entity.c — passive_reveal() */
SonarPoint p;
p.pos[0]   = verts[i * 3 + 0];
p.pos[1]   = verts[i * 3 + 1];
p.pos[2]   = verts[i * 3 + 2];
p.color[0] = 1.0f;   /* orange */
p.color[1] = 0.55f;
p.color[2] = 0.1f;
p.age      = 0.001f; /* CREATURE_POINT_AGE — marks point as transient */
p.ttl      = e->ttl; /* 0 = sonar default 0.8 s; > 0 = custom lifetime */
sonar_add_point(&p);
```

`sonar_add_point()` inserts `p` at the current write-head and advances the write-head
modulo `MAX_SONAR_POINTS`. The `age` value of `0.001f` is the sentinel that causes
`sonar_update()` to accumulate elapsed time each frame and mark the point dead
(`age = -1.0f`) once `age` reaches the TTL threshold.

The TTL threshold is resolved in `sonar_update()` as:
```c
float threshold = (p->ttl > 0.0f) ? p->ttl : 0.8f;
```
When `e->ttl == 0.0f` (the default), the 0.8-second default applies. When `e->ttl > 0.0f`,
that value is used instead, enabling per-creature custom point cloud lifetimes.

Normal sonar points (spawned by `spawn_point()` inside `sonar.c`) always have
`age = 0.0f` and are never aged — the TTL loop only processes `age > 0.0f`.

**Dependency graph**:
```
world/entity  ──calls──►  sonar/sonar (sonar_add_point)
world/entity  ──calls──►  world/map   (map_get_collision_verts, map_get_collision_tri_count)
world/entity  ──calls──►  audio/spatial (spatial_play)
world/entity  ──calls──►  audio/sound   (sound_load_ogg, sound_load_wav, sound_destroy)
world/entity  ──calls──►  core/log      (LOG_INFO, LOG_ERROR)
```

Note: this makes `world/entity` an exception to the world-layer isolation rule
documented in `docs/api/world.md §1`. The dependency on `sonar/` is intentional and
recorded in ADR 0007.

---

## 5. Passive Reveal Data Flow

The sequence from creature sound event to screen-visible orange points:

```
[entity_update, frame N]
  │
  ├─ sound_timer expires for ENTITY_CREATURE (mesh_index >= 0)
  │
  ├─ spatial_play()  →  3D audio: creature growl emitted in world space
  │
  └─ passive_reveal(e)
       │
       ├─ map_get_collision_verts()  →  const float* (9 floats per triangle)
       ├─ map_get_collision_tri_count()  →  int triCount
       │
       └─ loop: up to min(triCount*3, 200) vertices
            │
            └─ sonar_add_point(&p)
                 │  p.age   = 0.001f (transient sentinel)
                 │  p.ttl   = e->ttl (0 = default 0.8 s, >0 = custom)
                 │  p.color = orange {1.0, 0.55, 0.1}
                 ▼
            s_points[s_writeHead] = p   (sonar circular buffer)

[sonar_update, frame N+k]
  │
  └─ for each point with age > 0.0f:
       age += dt
       threshold = (p.ttl > 0.0f) ? p.ttl : 0.8f
       if age >= threshold  →  age = -1.0f  (mark dead)

[sonar_fx_render, frame N+k]
  │
  └─ pack GPU buffer
       if points[i].age == -1.0f  →  skip (dead point)
       else  →  upload to VBO for rendering
```

**TTL summary**: creature reveal point lifetime is determined by `e->ttl` at the
time `passive_reveal()` runs. When `e->ttl == 0.0f` (default), points are visible
for up to **0.8 seconds**. When `e->ttl > 0.0f`, points expire at that custom
duration instead. After expiry, points are culled from the render pass and their
buffer slots are recycled by the next `sonar_add_point()` or `spawn_point()` call
that wraps around the circular buffer.

---

## 6. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-11 | 0.1.0   | Initial API documentation for M6a/M6b implementation |
| 2026-03-18 | 0.2.0   | Add ENTITY_TRIGGER and ENTITY_STALKER; add mesh_ref, ttl, radius fields; add §2.3 field reuse conventions; document entity_update skip logic for trigger/stalker types |
| 2026-03-24 | 0.3.0   | M9: §4 update `passive_reveal()` snippet — `p.ttl = e->ttl` propagation fix; update TTL threshold description to reflect per-point custom TTL; §3.3 `entity_activate()` DOOR case updated with spatial audio side effect documentation; §5 data flow and TTL summary updated |
