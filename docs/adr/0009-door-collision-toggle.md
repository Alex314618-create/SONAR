# ADR 0009 — Dynamic Door Collision Toggle via Triangle Disable Mask

> **Status**: Accepted
> **Date**: 2026-03-18

---

## Context

SONAR levels require doors that the player can open and close at runtime. The
physics AABB solver (`physics.c`) and the sonar raycast system (`raycast.c`) both
operate against a **static** triangle soup loaded from glTF `col_*` mesh nodes at
level initialisation. Rebuilding this buffer at runtime is expensive and
architecturally disruptive.

Door collision geometry is a small, named subset of the overall collision buffer.
Requirements:

1. When a door opens, its triangles must no longer block player movement or sonar rays.
2. When it closes, those same triangles must resume blocking both.
3. The change must be instantaneous (no animation phase for M8).
4. Physics and sonar must agree — a door transparent to movement must also be
   transparent to sonar.

---

## Decision

### 1. Per-Triangle Disable Mask

Each of `physics.c` and `raycast.c` maintains a file-static byte array parallel to
its internal triangle buffer:

```c
// physics.c
static uint8_t s_triDisabled[MAX_PHYSICS_TRIS];   // capacity: 8192

// raycast.c
static uint8_t s_triDisabled[MAX_RAYCAST_TRIS];   // capacity: 8192
```

A value of `1` means the triangle is disabled and skipped during collision testing
or ray intersection. `0` is active (default). Masks are zeroed at initialisation.

### 2. set_tris_enabled API

```c
void physics_set_tris_enabled(int start, int count, int enabled);
void raycast_set_tris_enabled(int start, int count, int enabled);
```

Each function writes `enabled ? 0 : 1` into `s_triDisabled[start .. start + count - 1]`.
Callers must invoke both functions with the same arguments to maintain consistency.

### 3. CollisionRange — Named Triangle Ranges

`map.c` records the triangle range of every `col_*` node during glTF traversal:

```c
#define MAX_COLLISION_RANGES 32
typedef struct { char name[32]; int start_tri; int tri_count; } CollisionRange;
```

`map_get_collision_range(name, &start, &count)` performs a linear scan. This lookup
occurs only at interaction time (player presses F), not per-frame.

### 4. Door Entity `target` Field

`entity_door_<id>` objects carry a `target` custom property whose value is the exact
Blender node name of the associated `col_door_*` mesh. `entity_activate()` resolves
`e->target` via `map_get_collision_range()` to obtain the range, then calls both
`physics_set_tris_enabled` and `raycast_set_tris_enabled`.

### 5. Blender Convention

| Object | Type | Naming |
|--------|------|--------|
| Collision mesh | Mesh | `col_door_XX` |
| Entity empty | Empty + extras | `entity_door_XX` with `target = "col_door_XX"` |

---

## Consequences

### Positive

- **No collision buffer rebuild**: toggling is O(tri_count) memset, negligible for
  typical door geometry (< 20 triangles).
- **Unified disable semantic**: physics and raycast use the same pattern; an open door
  is transparent to both movement and sonar rays.
- **No Entity struct schema change**: the `target` field already exists (ADR 0007).
- **Zero per-frame overhead when no doors exist**: the disabled check is always
  not-taken and branch-predictor-friendly.

### Negative

- **Duplicated state**: `physics.c` and `raycast.c` each maintain independent
  `s_triDisabled[]` arrays. A caller that forgets to update both will produce
  visible inconsistency.
- **Linear O(n) skip per frame**: both collision loops check the flag for each
  triangle. A BVH with disabled-node pruning would eliminate this but is deferred.
- **Hard capacity limit**: `MAX_*_TRIS = 8192`. Levels approaching this limit
  leave no headroom for door meshes.
- **No partial-open states**: the toggle is binary (fully open or closed).

---

## Changelog

| Date       | Change |
|------------|--------|
| 2026-03-18 | Initial ADR, status Accepted |
