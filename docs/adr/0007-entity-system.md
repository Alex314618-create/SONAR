# ADR 0007 — Lightweight Entity System

> **Status**: Accepted
> **Date**: 2026-03-07
> **Owner**: CIO Agent

---

## Context

SONAR's levels require runtime objects that are not representable as static glTF
geometry. Two categories are needed:

1. **Creatures** — objects with meshes that move or are positioned dynamically.
   They emit ambient sounds. When they produce sound, their geometry is briefly
   revealed via passive sonar (orange points, short TTL). The player never sees
   creatures directly — only their sonar outline at the moment of sound.

2. **Interactive objects** — stationary triggers requiring player interaction
   (approach + press E). Examples: password dials, doors.

A full Entity Component System (ECS) is unnecessary overhead for the number of
entities expected (≤32 per level, per performance budget). A flat typed struct
with a union is sufficient.

## Decision

Introduce `world/entity.c` / `world/entity.h` as a lightweight entity system.
Entities are defined in the glTF file via Blender Empty objects with structured
node names and cgltf `extras` custom properties. The loader (`world/map.c`)
parses these at level load time.

### Entity Types and Blender Node Names

| Type tag | Blender node name | Description |
|----------|-------------------|-------------|
| `ENTITY_CREATURE` | `entity_creature_<id>` | Has mesh + optional ambient audio. On sound-emit event: project mesh vertices to world space → insert orange `SonarPoint`s (TTL 0.8s). |
| `ENTITY_DIAL` | `entity_dial_<id>` | Password dial. Stores correct code. Player presses F to open input UI. Activates on correct input sequence; sends signal to `target` entity. |
| `ENTITY_DOOR` | `entity_door_<id>` | Opens or closes on activation signal from another entity (e.g. from a dial). |
| `ENTITY_SOUND` | `entity_sound_<id>` | Ambient spatial sound source. No mesh. Loops a sound at world position on `interval` seconds. |
| *(spawn)* | `player_spawn` | Player spawn. Not an `Entity` struct; handled directly by `map_load()`. Listed here for naming-convention completeness. |

### Custom Properties (glTF Extras)

Entity behaviour parameters are stored as Blender custom properties, exported as
glTF `extras` JSON, and parsed by the entity loader:

| Property | Type | Applies to | Description |
|----------|------|------------|-------------|
| `code` | string | `ENTITY_DIAL` | Correct password sequence (e.g. `"1234"`) |
| `target` | string | `ENTITY_DIAL`, `ENTITY_DOOR` | Entity ID to send activation signal to |
| `sound` | string | `ENTITY_CREATURE`, `ENTITY_SOUND` | Path relative to `assets/sounds/` |
| `interval` | float | `ENTITY_CREATURE`, `ENTITY_SOUND` | Seconds between ambient sound repeats |

### Entity Struct

```c
typedef enum {
    ENTITY_CREATURE,
    ENTITY_DIAL,
    ENTITY_DOOR,
    ENTITY_SOUND,
} EntityType;

typedef struct {
    EntityType  type;
    float       pos[3];       // world-space position
    float       yaw;          // initial facing, radians
    char        id[32];       // e.g. "creature_01"
    char        sound[64];    // asset path relative to assets/sounds/, may be empty
    char        code[16];     // dial code, may be empty
    char        target[32];   // signal target entity id, may be empty
    float       interval;     // sound repeat interval, seconds (0 = no repeat)
    /* runtime state */
    float       sound_timer;  // countdown to next ambient sound
    int         mesh_index;   // index into map collision mesh list (-1 if no mesh)
} Entity;
```

### Module API — `world/entity`

| Function | Description |
|----------|-------------|
| `entity_init(Entity *entities, int count)` | Initialize runtime state (reset timers, load sound buffers) for all entities |
| `entity_update(float dt, Entity *entities, int count)` | Tick sound timers; trigger passive sonar reveal on sound events; send activation signals |
| `entity_activate(Entity *e)` | Process player interaction: check dial code, toggle door, etc. |
| `entity_shutdown()` | Free entity resources (sound buffers, etc.) |

### Passive Sonar Reveal — Data Flow

When a creature plays its ambient sound, `entity_update()` injects orange sonar
points tracing the creature's mesh outline:

```
creature.sound_timer reaches 0
    → play sound at creature.pos via audio/spatial
    → reset creature.sound_timer = creature.interval
    → for each vertex in map_get_collision_mesh(creature.mesh_index):
        world_pos = transform(vertex, creature.pos, creature.yaw)
        SonarPoint p = { world_pos, {1.0, 0.55, 0.1}, age = 0.0f }
        sonar_add_point(&p)       // orange, TTL 0.8s via age field
    → player sees brief orange outline of creature shape dissolve over ~0.8s
```

The TTL mechanism reuses the existing `age` field on `SonarPoint`. Points with
`age >= TTL_CREATURE` (0.8 s) are culled by `sonar_update()` before upload to
the GPU. This is the first consumer of the `age` field (previously reserved).

### Sonar Color Semantics

The complete sonar color table, including entity colors:

| Source | RGB (normalized) | Meaning |
|--------|-----------------|---------|
| Wall (default) | [0, 0.75, 0.70] | Environment |
| Floor | [0, 0.68, 0.55] | Environment |
| Ceiling | [0.22, 0.60, 0.95] | Environment |
| `mat_clue_red` surface | [1.0, 0.15, 0.15] | Clue / narrative hint |
| `mat_clue_blue` surface | [0.15, 0.45, 1.0] | Interactive / mechanical |
| Creature reveal | [1.0, 0.55, 0.10] | Entity (orange) |
| Passive ping | [0, 0.55, 0.45] | Dim teal (proximity only) |

**Note**: this table supersedes the per-surface random color ranges specified in
TDD §13.1 for `mat_clue_*` surfaces. Random ranges still apply to ordinary
geometry (wall, floor, ceiling).

## Consequences

### Positive
- Entity definitions live in the glTF file — no separate JSON format for game objects
- Flat struct is simple to implement, iterate, and debug
- Passive sonar reveal is mechanically coherent: creatures are perceptible only
  through acoustic events, consistent with the game's core design pillar
- Color semantics create a learnable visual language (teal = environment,
  red = clue, blue = interactive, orange = creature)

### Negative
- `entity_update()` must be called every frame; creature mesh vertex iteration is
  O(N_verts) per reveal event — capped by performance budget (200 verts max, see TDD §9)
- The `age`-based TTL mechanism requires `sonar_update()` to cull aged points;
  this is a new responsibility for the sonar module
- No pathfinding for M6 (creatures are static; movement deferred, see TDD §10 Q7)
- Password dial requires a small UI input sub-system not yet designed

---

## Changelog

| Date       | Change |
|------------|--------|
| 2026-03-07 | Initial ADR, status Accepted |
