# ADR 0008 — Trigger and Stalker Entities

> **Status**: Accepted
> **Date**: 2026-03-18
> **Owner**: CIO Agent

---

## Context

M7 introduces two new entity types and the infrastructure required to support them:

- **Trigger** — a stateless zone detector. When the player enters its radius it
  fires a signal to a target entity, optionally with a delay and a repeat mode.
  Triggers are purely spatial; they have no mesh and no audio.

- **Stalker** — an entity that tracks the player by emitting a sequence of
  positional audio cues (footsteps or ambient sounds) at increasing proximity.
  Like creatures (ADR 0007), stalkers are revealed passively via sonar only
  when they produce sound. Unlike creatures they are dynamic: their effective
  position advances toward the player over time.

Supporting these two types requires three infrastructure changes:

1. A way to store visual-only geometry (vis_* meshes) without polluting the
   physics/collision buffers used by the existing ray-cast system.
2. Per-point TTL on `SonarPoint` so that different entity types can emit points
   with different display durations without a global constant.
3. A standalone VFX particle system for effects (e.g. trigger-pulse ring,
   stalker arrival burst) that need velocity, gravity, and alpha fade — none of
   which the sonar point buffer supports.

---

## Decision

### 1. Trigger and Stalker as Independent Modules

`trigger.c` / `trigger.h` and `stalker.c` / `stalker.h` are introduced as
independent files under `world/`. They are not extensions of `entity.c`.

`entity_update()` explicitly skips `ENTITY_TRIGGER` and `ENTITY_STALKER` entries
in the entity array so that the core entity loop (sound timers, creature passive
reveal) remains unaware of trigger/stalker logic. Each module owns its own update
function called from the main game loop.

Rationale: `entity.c` handles only generic entity concerns. Trigger and stalker
logic is voluminous and divergent enough that merging it into `entity.c` would
violate single-responsibility and complicate future removal or replacement.

### 2. Parallel State Arrays

Runtime state for triggers and stalkers is stored in dedicated arrays indexed by
entity index:

```c
TriggerState s_triggers[MAX_ENTITIES];   // trigger.c
StalkerState s_stalkers[MAX_ENTITIES];   // stalker.c
```

These arrays are not embedded in the `Entity` struct.

Rationale: The `Entity` struct (ADR 0007) is intentionally minimal — a
data-transfer object populated by `map.c` from glTF extras. Trigger and stalker
each carry substantial runtime state (cooldown counters, advance timers, audio
source handles, alert phases) that is irrelevant to every other entity type.
Embedding it would bloat `Entity` by ~80 bytes per entry regardless of type.

### 3. MeshRange Independent Buffer for Visual Geometry

Visual-only meshes (node name prefix `vis_*`) are loaded into a separate buffer:

```c
MeshRange s_meshRangeBuf[MAX_MESH_RANGES];   // capacity: 32 ranges
float     s_meshRangeTris[MAX_MESH_RANGE_TRIS * 9]; // capacity: 8192 triangles
```

`vis_*` meshes are never inserted into the collision buffer (`s_collisionBuf`)
used by the existing ray-cast and physics systems.

Rationale: Inserting decorative geometry into the collision buffer would cause
ray-cast hits on invisible surfaces and corrupt pathfinding distance queries
planned for M8. The separate buffer keeps physics data pure. `MAX_MESH_RANGES=32`
and `MAX_MESH_RANGE_TRIS=8192` are derived from the level art budget (TDD §9).

### 4. Per-Point TTL on SonarPoint

A `float ttl` field is added to `SonarPoint`:

```c
typedef struct {
    float pos[3];    // world-space XYZ
    float col[3];    // RGB normalized
    float age;       // seconds since emission
    float ttl;       // display duration: 0 → use default 0.8s (backward-compatible)
} SonarPoint;       // 32 bytes (was 28)
```

Cull condition in `sonar_update()`:

```c
float effective_ttl = (p->ttl > 0.0f) ? p->ttl : TTL_DEFAULT;
if (p->age >= effective_ttl) { /* cull */ }
```

This supports three distinct TTL regimes without new fields or separate buffers:

| Consumer | ttl value | Effective duration |
|----------|-----------|--------------------|
| Trigger permanent marker | 0.0 with age reset to 0 each frame | Persistent |
| Stalker reveal | 2.0 | 2.0 s |
| Star-field flicker | 0.3 | 0.3 s |
| Legacy (creature, wall) | 0.0 | 0.8 s (default) |

The four-byte size increase is acceptable; the sonar point pool is 65 536 entries
× 32 bytes = 2 MiB, within the GPU buffer budget (TDD §13.3).

### 5. VFX Particle System as Independent Module

A standalone `render/vfx_particles.c` / `vfx_particles.h` module is introduced.
It maintains its own pool:

```c
VfxParticle s_particles[MAX_VFX_PARTICLES];  // capacity: 2048
```

Each `VfxParticle` carries: `pos[3]`, `vel[3]`, `col[4]` (RGBA), `age`, `lifetime`.
The update step applies velocity + gravity per particle and fades alpha linearly
with `age / lifetime`. The module renders through a dedicated GL draw call
separate from the sonar point draw call.

Rationale: Sonar points are static (no velocity, no alpha), and consuming them
for particles would exhaust the 65 536-point budget during heavy trigger/stalker
scenes. Particle lifetimes and blending modes also differ from sonar semantics.
A separate pool with capacity 2048 is sufficient for all planned M7 effects and
has negligible memory footprint (~160 KiB).

### 6. Entity Field Reuse for Trigger and Stalker Parameters

Rather than extending the `Entity` struct, trigger- and stalker-specific
parameters are mapped onto existing fields. `map.c` applies type-specific
interpretation when parsing glTF extras.

#### Trigger field mapping

| Entity field | Trigger meaning |
|--------------|-----------------|
| `target[32]` | `zone_id` — ID of the zone group this trigger belongs to |
| `interval`   | `delay` — seconds before the signal fires after player enters radius |
| `code[16]`   | `mode` — activation mode string (`"once"`, `"repeat"`, `"toggle"`) |
| `radius`     | `radius` — detection radius in world units |

#### Stalker field mapping

| Entity field   | Stalker meaning |
|----------------|-----------------|
| `sound[64]`    | `sound_appear` — audio asset played at each advance step |
| `target[32]`   | `sound_depart` — audio asset played when stalker retreats |
| `interval`     | `start_dist` — initial distance from player at spawn (world units) |
| `radius`       | `step_dist` — distance closed per advance event (world units) |
| `code[16]`     | `retreat_time` — seconds of silence before stalker retreats (string→float) |

`map.c` reads the `type` field before parsing extras and branches accordingly.
Fields left unused by a given type are zero-initialized.

### 7. Zone Grouping via Shared zone_id

Zones — logical groups of triggers that share enable/disable state — are not
represented by a dedicated `ENTITY_ZONE` type. Instead, triggers belonging to
the same zone share an identical `zone_id` string in their `target` field.
`trigger.c` maintains a flat lookup table of zone states keyed by `zone_id`.

Rationale: A separate Zone entity would require an additional Blender Empty per
group, increase the entity count, and add a second layer of signal routing. The
implicit grouping by shared string is sufficient for M7 level complexity (≤4
zones per level) and requires no schema changes.

---

## Consequences

### Positive

- `entity.c` remains unchanged; M7 entities do not risk regressing M6 behavior.
- `Entity` struct size is stable; all new runtime state is isolated in
  module-local arrays.
- Per-point TTL enables nuanced sonar vocabulary (persistent markers, slow-fade
  stalker reveals, fast star flickers) with zero API breakage.
- VFX particles are budget-neutral with respect to the sonar point pool; heavy
  particle effects cannot crowd out navigation-critical sonar geometry.
- Field reuse keeps `map.c` extras parsing consolidated and avoids a struct
  versioning problem.

### Negative

- Parallel state arrays (`TriggerState[MAX_ENTITIES]`, `StalkerState[MAX_ENTITIES]`)
  are indexed by entity index; if entity order in the glTF changes between map
  reloads, state arrays must be re-initialized from scratch — no incremental update.
- `SonarPoint` grows from 28 to 32 bytes; any serialized or network-transmitted
  point buffer (not currently planned) would require a format bump.
- The `retreat_time` stalker parameter is stored as a string in `code[16]` and
  parsed to float at load time — a type mismatch that is invisible to the Blender
  artist until runtime.
- `vis_*` mesh naming convention must be enforced manually in Blender; a mesh
  with a missing prefix silently enters the collision buffer.

### Risks

- `MAX_MESH_RANGE_TRIS=8192` may be insufficient for levels with dense decorative
  geometry. If exceeded at load time, `map_load()` must return an error rather
  than silently truncating triangles.
- The implicit zone grouping by `zone_id` string provides no editor validation.
  A typo in one trigger's `target` field silently creates a new singleton zone.
- Stalker advance logic runs on a timer in `stalker_update()`; if the game loop
  stalls (e.g. asset streaming), accumulated `dt` may advance the stalker multiple
  steps in one frame. `stalker_update()` must clamp `dt` to a maximum step size.

---

## Changelog

| Date       | Change |
|------------|--------|
| 2026-03-18 | Initial ADR, status Accepted |
