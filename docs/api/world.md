# SONAR — World Module API Reference

> **Version**: 0.2.0
> **Status**: Accepted
> **Last Updated**: 2026-03-18
> **Owner**: CIO Agent
> **Source**: `src/world/`

---

## Table of Contents

1. [Overview](#1-overview)
2. [world/map](#2-worldmap)
3. [world/physics](#3-worldphysics)
4. [MeshRange System](#4-meshrange-system)
5. [Changelog](#5-changelog)

---

## 1. Overview

The `world/` layer manages level data and physics simulation. It depends only on
`core/` and `render/` (for the `Model` type used to store renderable geometry).
It must not depend on `audio/`, `sonar/`, or `ui/`.

**Initialization order** (from `main.c`):
```c
map_load(NULL);                                         // load/generate level
physics_init(map_get_collision_verts(),
             map_get_collision_tri_count());             // bind collision data
```

**Shutdown order**:
```c
physics_shutdown();
map_shutdown();
```

### Module Dependency

```
world/map  ──depends on──►  render/model  (for Model type)
world/physics  ──uses──►  collision data provided by world/map at init time
```

---

## 2. world/map

**File**: `src/world/map.h` / `src/world/map.c`

**Purpose**: Load or procedurally generate a level. Provides renderable geometry
(as a `Model`) and collision geometry (as a flat triangle array) to the rest of
the engine. Also stores the player spawn point.

### 2.1 Data Model

A map exposes two representations of its geometry:

| Representation | Type | Consumer |
|----------------|------|----------|
| Render model | `const Model *` | `render/renderer` via `main.c` |
| Collision verts | `const float *` (9 floats per triangle) | `world/physics` |

The render model and collision mesh are currently generated from the same procedural
geometry, meaning visual and collision boundaries are identical. Separating them
is tracked as an open question in `docs/tdd.md`.

### 2.2 Functions

---

#### `map_load`
```c
int map_load(const char *path);
```
Loads or generates a level.

| Parameter | Description |
|-----------|-------------|
| `path`    | Filesystem path to a map file, or `NULL` to generate the procedural two-room test level |

**Returns**: `0` on success, negative on error.

**Current behavior**: When `path == NULL`, generates a hard-coded two-room box
level with a connecting doorway. Room dimensions and player spawn are fixed.
File-based loading is planned for a future milestone.

---

#### `map_get_render_model`
```c
const Model *map_get_render_model(void);
```
**Returns**: Pointer to the internal `Model` for rendering. Valid until `map_shutdown()`
is called. Do not free or modify the returned pointer.

---

#### `map_get_collision_verts`
```c
const float *map_get_collision_verts(void);
```
**Returns**: Pointer to a flat array of collision triangle vertices, laid out as:
```
[v0x, v0y, v0z,  v1x, v1y, v1z,  v2x, v2y, v2z,  ...]  // 9 floats per triangle
```
Valid until `map_shutdown()`. Pass directly to `physics_init()`.

---

#### `map_get_collision_tri_count`
```c
int map_get_collision_tri_count(void);
```
**Returns**: Number of collision triangles in the current map.

---

#### `map_get_player_spawn`
```c
const float *map_get_player_spawn(void);
```
**Returns**: Pointer to a `vec3` (3 floats) representing the player's initial
world-space position. Used to initialize the camera in `main.c`.

---

#### `map_get_player_yaw`
```c
float map_get_player_yaw(void);
```
**Returns**: The player's initial yaw angle in degrees. Used alongside
`map_get_player_spawn()` to initialize `Camera`.

---

#### `map_get_entities`
```c
Entity *map_get_entities(void);
```
**Returns**: Mutable pointer to the internal `s_entities[32]` array, populated
by the glTF loader or left zeroed for procedural levels. Valid until `map_shutdown()`.

The pointer is intentionally mutable because `entity_init()` and `entity_update()`
write runtime state (`sound_timer`) back into the same array.

---

#### `map_get_entity_count`
```c
int map_get_entity_count(void);
```
**Returns**: Number of entities parsed from the current map (0 for procedural levels).

---

#### `map_get_mesh_range_tris`
```c
int map_get_mesh_range_tris(int index, const float **out_verts, int *out_count);
```
Retrieves triangle vertex data for a named `vis_*` mesh stored in the MeshRange table.
Used by `world/trigger` and `world/stalker` to project entity shapes as sonar points.

| Parameter | Type | Description |
|-----------|------|-------------|
| `index`     | `int`            | MeshRange index — corresponds to `Entity.mesh_index` populated at load time |
| `out_verts` | `const float **` | Output: pointer to the flat triangle array (9 floats per triangle: 3 vertices × xyz) |
| `out_count` | `int *`          | Output: number of triangles in this MeshRange |

**Returns**: `0` on success, `-1` if `index` is out of range or the MeshRange table
is empty (e.g., procedural level with no `vis_*` nodes).

**Memory**: the returned pointer is into a map-owned buffer. It is valid until
`map_shutdown()` is called. Do not free the pointer.

---

#### `map_shutdown`
```c
void map_shutdown(void);
```
Frees all map resources: destroys the render model via `model_destroy()`, frees the
collision vertex array, and frees the MeshRange buffer. After this call, all pointers
returned by `map_get_*` functions are invalid.

---

## 3. world/physics

**File**: `src/world/physics.h` / `src/world/physics.c`

**Purpose**: AABB-based collision detection and response. Implements a
**collide-and-slide** algorithm: movement is resolved per-axis against a
triangle soup, allowing smooth sliding along walls and floors.

### 3.1 Algorithm Summary

`physics_move()` attempts movement in three independent axis passes (X, then Z,
then Y). For each axis, it sweeps the player AABB along that axis and tests
against all collision triangles. On overlap, the movement along that axis is
cancelled while the other axes proceed normally. This produces smooth wall-sliding
without sticking.

### 3.2 Types

#### `AABB`
```c
typedef struct {
    float min[3];   // Minimum corner (x, y, z) relative to player position
    float max[3];   // Maximum corner (x, y, z) relative to player position
} AABB;
```
The AABB is defined relative to the player's position. Example player bounds:
```c
AABB playerBounds = { {-0.3f, 0.0f, -0.3f}, {0.3f, 1.75f, 0.3f} };
```

### 3.3 Functions

---

#### `physics_init`
```c
int physics_init(const float *tris, int triCount);
```
Stores a reference to the collision triangle data for use in `physics_move()`.

| Parameter  | Description |
|------------|-------------|
| `tris`     | Triangle array: 9 floats per triangle (`v0x,v0y,v0z, v1x,..., v2z`) |
| `triCount` | Number of triangles |

**Returns**: `0` on success, negative on error.

**Note**: The physics system does not copy `tris` — it holds a pointer. The caller
(currently `map_load` output via `main.c`) must ensure the triangle data outlives
the physics system. In practice, `map_shutdown()` must be called after
`physics_shutdown()`.

---

#### `physics_move`
```c
void physics_move(const float *pos, const float *velocity,
                  const AABB *bounds, float *outPos);
```
Attempts to move the entity from `pos` by `velocity`, resolving AABB collisions
against the stored triangle soup using per-axis collide-and-slide.

| Parameter  | Type | Description |
|------------|------|-------------|
| `pos`      | `const float[3]` | Current world-space position |
| `velocity` | `const float[3]` | Desired movement vector (units/frame, pre-scaled by dt) |
| `bounds`   | `const AABB *`   | Player AABB relative to `pos` |
| `outPos`   | `float[3]`       | Output: resulting position after collision response |

**Post-condition**: `outPos` will be inside valid geometry (not intersecting any
collision triangle) within the tolerance of the collision epsilon.

---

#### `physics_shutdown`
```c
void physics_shutdown(void);
```
Clears the internal triangle pointer and count. No heap memory is freed (the
triangle data is owned by `world/map`).

---

## 4. MeshRange System

The `MeshRange` system provides per-entity visual mesh data for sonar reveal
operations. It is distinct from the collision triangle soup used by `world/physics`.

### 4.1 Purpose and Separation

| Buffer | Source in glTF | Consumer |
|--------|---------------|----------|
| Collision tris (`map_get_collision_verts`) | `col_*` prefix nodes | `world/physics`, `sonar/raycast` |
| MeshRange tris (`map_get_mesh_range_tris`) | `vis_*` prefix nodes | `world/trigger`, `world/stalker` |

Collision meshes are used for physics and ray intersection. `vis_*` meshes are
**visual-only** approximations of entity shapes — typically lower-poly, sized to
project well as sonar point clouds. They are never uploaded to the GPU as render
geometry.

### 4.2 `vis_*` Naming Convention

In Blender, any mesh object with a `vis_` prefix is treated as a MeshRange node:

| Blender name | Behaviour |
|--------------|-----------|
| `vis_altar` | Parsed as MeshRange; associated with the entity whose `mesh_ref` matches `"vis_altar"` |
| `vis_stalker_body` | MeshRange for stalker silhouette mesh |
| `col_walls` | Collision mesh — **not** a MeshRange |
| *(no prefix)* | Visual render mesh — **not** a MeshRange |

The glTF loader processes `vis_*` nodes **before** `entity_*` nodes in its traversal
so that by the time entity extras are parsed, the MeshRange table is already populated
and `mesh_index` values can be assigned.

### 4.3 Storage and Capacity

Internally, `map.c` maintains a flat array of MeshRange records, each storing:
- A pointer (offset) into a shared `vis_*` vertex buffer
- A triangle count

The shared buffer holds all `vis_*` triangle data contiguously. MeshRange indices
(`Entity.mesh_index`) are sequential integers assigned during load; they have no
stable identity across level reloads.

**Capacity**: limited by the same 32-entity cap enforced by `s_entities[32]`.
One `vis_*` node may be shared by multiple entities (same `mesh_ref`).

---

## 5. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-06 | 0.1.0   | Initial API documentation extracted from M2 implementation |
| 2026-03-18 | 0.2.0   | Add map_get_entities, map_get_entity_count, map_get_mesh_range_tris; add §4 MeshRange system; rename §4 Changelog to §5 |
