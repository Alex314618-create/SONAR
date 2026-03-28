# SONAR — Trigger Module API Reference

> **Version**: 0.1.0
> **Status**: Accepted
> **Last Updated**: 2026-03-18
> **Owner**: CIO Agent
> **Source**: `src/world/trigger.h` / `src/world/trigger.c`
> **Related**: [docs/api/entity.md §2.3](entity.md#23-entity-field-reuse-conventions), [ADR 0007](../adr/0007-entity-system.md)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Types](#2-types)
   - 2.1 [TriggerMode](#21-triggermode)
   - 2.2 [TriggerState (internal)](#22-triggerstate-internal)
3. [Functions](#3-functions)
   - 3.1 [trigger_init](#31-trigger_init)
   - 3.2 [trigger_update](#32-trigger_update)
   - 3.3 [trigger_shutdown](#33-trigger_shutdown)
4. [Zone Activation Flow](#4-zone-activation-flow)
5. [Mesh Reveal Algorithm](#5-mesh-reveal-algorithm)
6. [Entity Field Mapping](#6-entity-field-mapping)
7. [Changelog](#7-changelog)

---

## 1. Overview

The `world/trigger` module manages **one-shot environmental reveals**: static
trigger entities that inject sonar points into the sonar buffer when the player
enters their proximity radius. Once fired, a trigger never activates again.

Two activation modes are supported:

- **Zone** (`TRIGGER_MODE_ZONE`): entering any trigger's radius activates the
  entire group sharing the same `zone_id`, with staggered per-trigger delays.
  Enables reveals that unfold sequentially as the player approaches an area.
- **Step** (`TRIGGER_MODE_STEP`): the closest unfired trigger within radius
  fires immediately, one at a time, creating a trail of incremental reveals.

Reveal injects permanent (or optionally timed) cyan sonar points sampled from
a `vis_*` MeshRange at 10 points per triangle (3 vertices + 3 mid-edges +
1 centroid + 3 jittered centroid variants for density). Points persist in the
sonar buffer until overwritten by the circular buffer.

**Dependency summary**:
```
world/trigger  ──reads──►  world/entity   (Entity array, ENTITY_TRIGGER)
world/trigger  ──calls──►  world/map      (map_get_mesh_range_tris)
world/trigger  ──calls──►  sonar/sonar    (sonar_add_point)
world/trigger  ──calls──►  audio/spatial  (spatial_play)
world/trigger  ──calls──►  audio/sound    (sound_load_ogg, sound_load_wav, sound_destroy)
world/trigger  ──calls──►  core/log       (LOG_INFO)
```

**`entity_update()` does not process `ENTITY_TRIGGER` entities** — the trigger
module has its own update path. See [docs/api/entity.md §3.2](entity.md#32-entity_update).

---

## 2. Types

### 2.1 `TriggerMode`

```c
typedef enum { TRIGGER_MODE_ZONE, TRIGGER_MODE_STEP } TriggerMode;
```

| Value | Description |
|-------|-------------|
| `TRIGGER_MODE_ZONE` | Group activation: any trigger entering radius activates all triggers sharing the same `zone_id` with their individual delays |
| `TRIGGER_MODE_STEP` | Incremental activation: fires the closest unfired trigger when player enters its radius; one trigger per approach |

Mode is determined by the `code` field of the `Entity` extras:
- `"step"` → `TRIGGER_MODE_STEP`
- Any other value (including empty) → `TRIGGER_MODE_ZONE`

### 2.2 `TriggerState` (internal)

`TriggerState` is a file-static struct in `trigger.c`. It is not exposed in the
public header but is documented here for traceability.

| Field | Type | Description |
|-------|------|-------------|
| `entity_index` | `int` | Index into the `Entity` array passed to `trigger_init()` |
| `fired` | `int` | Non-zero once the trigger has activated (permanent — never reset) |
| `delay` | `float` | Activation delay in seconds (from `Entity.interval`) |
| `delay_timer` | `float` | Countdown until the trigger fires after zone activation |
| `radius` | `float` | Proximity radius in meters (from `Entity.radius`; default 3.0 m) |
| `zone_id[32]` | `char[]` | Group identifier (from `Entity.target`); empty for standalone triggers |
| `mode` | `TriggerMode` | `ZONE` or `STEP` |
| `zone_activated` | `int` | Non-zero when the delay countdown has been started |

**Capacity**: up to 32 triggers (`MAX_TRIGGERS`). Sound cache: up to 16 sounds
(`MAX_TRIGGER_SOUNDS`).

---

## 3. Functions

### 3.1 `trigger_init`

```c
void trigger_init(Entity *entities, int count);
```

Scans the entity array for `ENTITY_TRIGGER` entries, builds the internal
`TriggerState` table, and pre-loads all referenced sound assets.

| Parameter | Description |
|-----------|-------------|
| `entities` | Entity array from `map_get_entities()` |
| `count`    | Entity count from `map_get_entity_count()` |

**Field extraction** (from `Entity` extras — see §6 for full mapping):

| Entity field | TriggerState field |
|--------------|--------------------|
| `target`   | `zone_id` |
| `interval` | `delay` |
| `code`     | `mode` (`"step"` → STEP, else ZONE) |
| `radius`   | `radius` (default 3.0 m if `<= 0`) |
| `sound`    | Pre-loaded into sound cache |

**Side effects**: stores a pointer to the `entities` array internally for use
by `trigger_update()`. The caller must ensure the array remains valid until
`trigger_shutdown()`.

Logs: `trigger: initialized N triggers` via `LOG_INFO`.

---

### 3.2 `trigger_update`

```c
void trigger_update(float dt, const float player_pos[3]);
```

Ticks trigger logic once per frame. Tests player proximity, manages zone activation,
counts down delays, and fires reveals.

| Parameter | Description |
|-----------|-------------|
| `dt`          | Frame delta time in seconds |
| `player_pos`  | Player world position (vec3); only XZ components are used for proximity tests |

**Logic summary** (two sequential passes):

**Pass 1 — Zone activation check:**
- For each unfired trigger:
  - Compute XZ distance from player to `Entity.pos`
  - **ZONE with `zone_id`**: if `d < radius` and not yet activated, set
    `zone_activated = 1` and `delay_timer = delay` on **all** triggers sharing
    the same `zone_id`
  - **STEP**: if `d < radius`, set `zone_activated = 1` and `delay_timer = 0`
    (fires immediately on next pass)
  - **Standalone ZONE** (no `zone_id`): if `d < radius`, activate self only

**Pass 2 — Delay countdown and fire:**
- For each activated, unfired trigger:
  - Decrement `delay_timer -= dt`
  - If `delay_timer > 0`: continue waiting
  - If `delay_timer <= 0`: fire
    - Set `fired = 1`
    - Call `trigger_reveal_mesh(e)` — injects sonar points
    - If `sound` non-empty: `spatial_play()` at entity position

**Side effects**: calls `sonar_add_point()` (may inject up to
`MAX_POINTS_PER_TRIGGER` = 500 points per trigger per frame).

---

### 3.3 `trigger_shutdown`

```c
void trigger_shutdown(void);
```

Releases all cached sound buffers and resets internal counters. The `Entity`
array pointer is cleared. All `TriggerState` records are logically invalidated.

**Side effects**: calls `sound_destroy()` on each cached sound buffer.

---

## 4. Zone Activation Flow

```
player enters trigger_A radius (zone_id="altar")
         │
         ▼
  Pass 1 detects: d < trigger_A.radius
         │
         ├─ Find ALL triggers with zone_id "altar"
         │    trigger_A: delay=0.0s  → zone_activated=1, delay_timer=0.0
         │    trigger_B: delay=0.5s  → zone_activated=1, delay_timer=0.5
         │    trigger_C: delay=1.2s  → zone_activated=1, delay_timer=1.2
         │
  [frame N+1] Pass 2 countdown:
         ├─ trigger_A: delay_timer=0  → FIRE → reveal + sound
         ├─ trigger_B: delay_timer=0.5-dt  → waiting
         └─ trigger_C: delay_timer=1.2-dt  → waiting

  [frame N+30] Pass 2:
         ├─ trigger_B: delay_timer≤0  → FIRE
         └─ trigger_C: delay_timer>0  → waiting

  [frame N+70] Pass 2:
         └─ trigger_C: delay_timer≤0  → FIRE

All three triggers now fired=1; will never activate again.
```

**Key invariant**: once `fired = 1`, a trigger is permanently inert regardless
of continued player proximity.

---

## 5. Mesh Reveal Algorithm

`trigger_reveal_mesh()` samples **10 points per triangle** from the MeshRange:

```
for each triangle (v0, v1, v2) in MeshRange:
    │
    ├─ 3 vertex points:     v0, v1, v2
    ├─ 3 mid-edge points:   (v0+v1)/2, (v1+v2)/2, (v0+v2)/2
    ├─ 1 centroid:          (v0+v1+v2)/3
    └─ 3 jittered centroid: centroid ± rand(0, 0.05) per axis
                                              = 10 points/triangle
    │
    └─ inject via sonar_add_point()
         color: cyan {0.0, 0.85, 0.75}
         age / ttl: determined by Entity.ttl
```

**TTL logic**:
- `Entity.ttl == 0.0f`: permanent reveal — `age = 0.0f` (sonar system never ages)
- `Entity.ttl > 0.0f`: timed reveal — `age = 0.001f`, `ttl = Entity.ttl`

**Star-field effect** (permanent reveals only): 30% of points get a brightened
color (`base × 1.4`, clamped to 1.0) and a short flash TTL of 0.3 seconds,
creating a sparkling initial appearance over a stable permanent constellation.

**Cap**: `MAX_POINTS_PER_TRIGGER` = 500 points. If the mesh has more triangles
than the cap allows, early triangles are preferred.

---

## 6. Entity Field Mapping

| `Entity` field | Trigger interpretation |
|----------------|------------------------|
| `pos[3]`     | World position for distance tests and `spatial_play()` |
| `id[32]`     | Identifier for logging |
| `sound[64]`  | Activation sound asset (relative to `assets/sounds/`) |
| `target[32]` | `zone_id` — groups triggers that activate together |
| `interval`   | Activation delay in seconds within a zone |
| `code[16]`   | Mode: `"step"` for step mode; else zone mode |
| `radius`     | Proximity radius in meters (default 3.0 m if `<= 0`) |
| `mesh_index` | MeshRange index for `map_get_mesh_range_tris()` |
| `ttl`        | Sonar point TTL (`0` = permanent) |

---

## 7. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-18 | 0.1.0   | Initial API documentation for world/trigger module |
