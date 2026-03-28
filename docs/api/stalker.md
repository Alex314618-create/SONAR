# SONAR — Stalker Module API Reference

> **Version**: 0.1.0
> **Status**: Accepted
> **Last Updated**: 2026-03-18
> **Owner**: CIO Agent
> **Source**: `src/world/stalker.h` / `src/world/stalker.c`
> **Related**: [docs/api/entity.md §2.3](entity.md#23-entity-field-reuse-conventions), [docs/api/vfx_particles.md](vfx_particles.md), [ADR 0007](../adr/0007-entity-system.md)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Types](#2-types)
   - 2.1 [StalkerPhase](#21-stalkerphase)
   - 2.2 [StalkerState (internal)](#22-stalkerstate-internal)
3. [Functions](#3-functions)
   - 3.1 [stalker_init](#31-stalker_init)
   - 3.2 [stalker_update](#32-stalker_update)
   - 3.3 [stalker_shutdown](#33-stalker_shutdown)
4. [State Machine](#4-state-machine)
5. [compute_behind_pos Algorithm](#5-compute_behind_pos-algorithm)
6. [vfx_particles Integration](#6-vfx_particles-integration)
7. [Entity Field Mapping](#7-entity-field-mapping)
8. [Changelog](#8-changelog)

---

## 1. Overview

The `world/stalker` module implements the **Stalker** — a threat entity that
appears behind the player whenever sonar is used. Each sonar fire event
(pulse or continuous frame) brings the stalker closer. If the player stops
using sonar for long enough, the stalker retreats.

The stalker is never visible through normal vision; it is revealed only as a
**red sonar point outline** (`{1.0, 0.15, 0.15}`) projected from its `vis_*`
mesh, with a 2-second TTL. Each appearance is accompanied by a shockwave
particle effect (`vfx_particles_spawn_shockwave`) and a positioned sound.
Retreat triggers a sand-collapse particle effect (`vfx_particles_spawn_collapse`).

The stalker polls `sonar_get_fire_count()` each frame to detect new fires without
requiring an event callback — fire count is a monotonically increasing integer
in `sonar.c` incremented on each `sonar_fire_pulse()` or `sonar_fire_continuous()`
call.

**Dependency summary**:
```
world/stalker  ──reads──►  world/entity      (Entity array, ENTITY_STALKER)
world/stalker  ──reads──►  sonar/sonar       (sonar_get_fire_count, sonar_add_point)
world/stalker  ──calls──►  sonar/raycast     (raycast_cast — wall avoidance)
world/stalker  ──calls──►  world/map         (map_get_mesh_range_tris)
world/stalker  ──calls──►  render/vfx_particles (spawn_shockwave, spawn_collapse)
world/stalker  ──calls──►  audio/spatial     (spatial_play)
world/stalker  ──calls──►  audio/sound       (sound_load_ogg, sound_load_wav, sound_destroy)
world/stalker  ──calls──►  core/log          (LOG_INFO)
```

**`entity_update()` does not process `ENTITY_STALKER` entities** — the stalker
module has its own update path. See [docs/api/entity.md §3.2](entity.md#32-entity_update).

**Capacity**: up to 4 simultaneous stalkers (`MAX_STALKERS`).

---

## 2. Types

### 2.1 `StalkerPhase`

```c
typedef enum {
    STALKER_DORMANT,
    STALKER_APPROACHING,
    STALKER_VISIBLE,
    STALKER_DEPARTING
} StalkerPhase;
```

| Value | Description |
|-------|-------------|
| `STALKER_DORMANT`    | Waiting for first sonar use. No visual or audio presence. |
| `STALKER_APPROACHING` | Sonar has been used. The stalker exists conceptually behind the player at `current_dist` but is not yet visible. Each new sonar fire reduces `current_dist` by `step_dist`. |
| `STALKER_VISIBLE`    | A sonar fire occurred while approaching: the stalker appears at `appear_pos` as a red mesh outline (TTL 2 s). Reveal repeats on each subsequent sonar fire. |
| `STALKER_DEPARTING`  | No sonar for `retreat_time` seconds: the stalker dissolves (collapse VFX, depart sound). Lasts `STALKER_DEPART_DURATION` (0.8 s) then returns to DORMANT. |

### 2.2 `StalkerState` (internal)

`StalkerState` is a file-static struct in `stalker.c`, not exposed in the header.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `entity_index` | `int` | — | Index into the Entity array |
| `phase` | `StalkerPhase` | `DORMANT` | Current state machine phase |
| `current_dist` | `float` | `start_dist` | Current distance behind player (meters) |
| `start_dist` | `float` | 8.0 m | Initial distance; resets to this on DORMANT entry |
| `step_dist` | `float` | 1.5 m | Distance reduction per sonar fire event |
| `retreat_time` | `float` | 15.0 s | Idle time (no sonar) before retreating |
| `reveal_timer` | `float` | — | Countdown for VISIBLE phase TTL (2.0 s) |
| `idle_timer` | `float` | 0 | Accumulated time since last sonar fire |
| `last_fire_count` | `int` | — | `sonar_get_fire_count()` snapshot; delta = new fires |
| `appear_pos[3]` | `float[3]` | — | World position where stalker is rendered |
| `depart_timer` | `float` | — | Countdown for DEPARTING phase (0.8 s) |
| `sound_appear[64]` | `char[]` | — | Appear sound name (from `Entity.sound`) |
| `sound_depart[64]` | `char[]` | — | Depart sound name (from `Entity.target`) |

---

## 3. Functions

### 3.1 `stalker_init`

```c
void stalker_init(Entity *entities, int count);
```

Scans the entity array for `ENTITY_STALKER` entries, initialises internal
`StalkerState` records, and pre-loads sound assets.

| Parameter | Description |
|-----------|-------------|
| `entities` | Entity array from `map_get_entities()` |
| `count`    | Entity count from `map_get_entity_count()` |

**Field extraction** (see §7 for full mapping):

| `Entity` field | `StalkerState` field | Default if field empty/zero |
|----------------|---------------------|----------------------------|
| `interval`     | `start_dist`         | 8.0 m |
| `radius`       | `step_dist`          | 1.5 m |
| `code`         | `retreat_time` (parsed as float via `atof`) | 15.0 s |
| `sound`        | `sound_appear`       | (no sound) |
| `target`       | `sound_depart`       | (no sound) |

`current_dist` is initialised to `start_dist`. `last_fire_count` is initialised
to the current value of `sonar_get_fire_count()` so that fires before init are
not counted.

Logs: `stalker: initialized N stalkers` via `LOG_INFO`.

---

### 3.2 `stalker_update`

```c
void stalker_update(float dt, const float player_pos[3],
                    const float player_forward[3]);
```

Ticks stalker state machines once per frame.

| Parameter | Description |
|-----------|-------------|
| `dt`             | Frame delta time in seconds |
| `player_pos`     | Player world position (vec3) |
| `player_forward` | Player forward direction, normalized (vec3) |

**Per-stalker logic** (dispatched by `phase`):

| Phase | On new sonar fires (`newFires > 0`) | On no sonar fires |
|-------|-------------------------------------|-------------------|
| `DORMANT` | → `APPROACHING`; reset `current_dist = start_dist` | stay DORMANT |
| `APPROACHING` | Reduce `current_dist -= step_dist × newFires` (min 1.5 m); compute `appear_pos`; → `VISIBLE`; reveal + shockwave + appear sound | Accumulate `idle_timer`; if `idle_timer >= retreat_time` → `DEPARTING` (collapse + depart sound) |
| `VISIBLE` | Reduce `current_dist`; recompute `appear_pos`; re-reveal + shockwave; reset `reveal_timer = 2.0s` | Accumulate `idle_timer`; tick `reveal_timer -= dt`; if expired → back to `APPROACHING` (or `DEPARTING` if idle too long) |
| `DEPARTING` | — | Tick `depart_timer -= dt`; if expired → `DORMANT` |

**`last_fire_count` update**: updated at the end of each phase that handles
sonar fires, so `newFires` is always the delta since the last processed frame.

**Side effects**:
- Calls `stalker_reveal_mesh()` → `sonar_add_point()` (up to 500 points)
- Calls `vfx_particles_spawn_shockwave()` on appear
- Calls `vfx_particles_spawn_collapse()` on depart
- Calls `spatial_play()` for appear/depart sounds

---

### 3.3 `stalker_shutdown`

```c
void stalker_shutdown(void);
```

Releases all cached sound buffers and resets internal state.

**Side effects**: calls `sound_destroy()` on each cached sound buffer.

---

## 4. State Machine

```
         ┌──────────────────────────────────────────┐
         │                 DORMANT                   │
         │   Waiting for first sonar use.            │
         └──────────────────┬───────────────────────┘
                            │ sonar fire detected
                            ▼
         ┌──────────────────────────────────────────┐
         │              APPROACHING                  │◄──────────────┐
         │   current_dist starts at start_dist.      │               │
         │   Each sonar fire: dist -= step_dist.     │               │
         │   Idle timer accumulates when no fires.   │               │
         └────────┬─────────────────┬────────────────┘               │
                  │ sonar fire       │ idle_timer >= retreat_time     │
                  │ (dist reduced)   │                                │
                  ▼                  ▼                                │
     ┌────────────────────┐   ┌──────────────────────┐               │
     │      VISIBLE       │   │     DEPARTING         │               │
     │  Mesh revealed as  │   │  Collapse VFX +       │               │
     │  red outline at    │   │  depart sound.        │               │
     │  appear_pos.       │   │  Timer: 0.8 s.        │               │
     │  TTL: 2.0 s.       │   └──────────┬────────────┘               │
     │                    │              │ timer expires               │
     │ sonar fire → re-   │              └────────────────────────────►┘
     │ reveal + shockwave │                         (DORMANT; dist reset)
     │                    │
     │ reveal_timer≤0 AND │
     │ idle < retreat:    │
     └───────┬────────────┘
             │
             ▼  → APPROACHING (or DEPARTING if idle_timer >= retreat_time)
```

**Minimum distance clamp**: `current_dist` is never reduced below **1.5 m**
regardless of fire count, preventing the stalker from overlapping the player.

---

## 5. `compute_behind_pos` Algorithm

`compute_behind_pos()` calculates the stalker's world position directly behind
the player at `distance` meters, while respecting wall geometry.

```
Input: player_pos, player_forward, distance

1. Flatten player_forward to XZ plane:
   fwd_xz = { player_forward.x, 0, player_forward.z }
   normalize fwd_xz (fallback: {0, 0, -1} if near-zero length)

2. Compute behind direction:
   behind = { -fwd_xz.x, 0, -fwd_xz.z }

3. Raycast backward to detect walls:
   hitDist = raycast_cast(player_pos, behind, distance + 0.5m)

4. Resolve actual distance:
   if hitDist > 0 AND hitDist < distance + 0.5m:
       actualDist = max(hitDist - 0.5m, 1.0m)   // 0.5m wall margin
   else:
       actualDist = distance

5. Output position:
   out = player_pos + behind * actualDist
   out.y = player_pos.y   // same Y — no vertical offset
```

The 0.5 m wall margin ensures the stalker appears in front of the wall surface
rather than clipping into it. The 1.0 m floor prevents negative or
zero-distance positions in extremely tight corridors.

---

## 6. `vfx_particles` Integration

The stalker module is the **only** caller of `vfx_particles` in the current
codebase. Two effects are used:

| Event | Function | Particles |
|-------|----------|-----------|
| Stalker appears (`APPROACHING` → `VISIBLE`) | `vfx_particles_spawn_shockwave(appear_pos, STALKER_COLOR)` | 64 particles in horizontal ring, outward at 4.0 m/s, 0.6 s lifetime |
| Stalker retreats (`→ DEPARTING`) | `vfx_particles_spawn_collapse(appear_pos, STALKER_COLOR)` | 48 particles falling with gravity + random horizontal drift, 0.8–1.4 s lifetime |

`STALKER_COLOR` = `{1.0f, 0.15f, 0.15f}` (red). The shockwave function applies
an internal tint formula (`col.g = color.g × 0.5 + 0.1`, `col.b = color.b × 0.3 + 0.05`)
that produces a red-orange ring distinct from the flat red outline points.

Both calls happen **in the same frame** as the mesh reveal — the shockwave
appears simultaneously with the sonar outline.

See [docs/api/vfx_particles.md](vfx_particles.md) for full particle system documentation.

---

## 7. Entity Field Mapping

| `Entity` field | Stalker interpretation |
|----------------|------------------------|
| `pos[3]`       | Entity origin (used as mesh translation base for reveal offset) |
| `id[32]`       | Identifier for logging |
| `sound[64]`    | Appear sound asset (relative to `assets/sounds/`) → `sound_appear` |
| `target[32]`   | Depart sound asset (relative to `assets/sounds/`) → `sound_depart` |
| `interval`     | `start_dist` in meters (default 8.0 m if `<= 0`) |
| `radius`       | `step_dist` in meters per sonar fire (default 1.5 m if `<= 0`) |
| `code[16]`     | `retreat_time` as decimal string — parsed with `atof()` (default 15.0 s) |
| `mesh_index`   | MeshRange index for silhouette mesh via `map_get_mesh_range_tris()` |

---

## 8. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-18 | 0.1.0   | Initial API documentation for world/stalker module |
