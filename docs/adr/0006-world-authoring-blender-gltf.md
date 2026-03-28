# ADR 0006 — World Authoring: Blender + glTF 2.0

> **Status**: Accepted
> **Date**: 2026-03-07
> **Owner**: CIO Agent

---

## Context

The original project plan called for a custom **Python/Tkinter tile editor** ported
from `doomer/editor.py`. The format supported per-tile floor/ceiling heights and
diagonal wall types (v2 tile format). The editor was specced but never implemented.

Before implementation began, the tile-grid approach was evaluated against SONAR's
core design constraint: **sonar echoes must produce spatially interesting, acoustically
rich point clouds**. This evaluation found the tile editor fundamentally incompatible:

- **Axis-aligned geometry** produces point clouds in regular orthogonal grids — visually
  repetitive, acoustically monotone, and readable at a glance. This breaks the sonar
  navigation tension; players can infer room structure too easily from uniform returns.
- **Tile grids** cannot represent props, organic shapes, extruded text, diagonal surfaces,
  or non-rectangular rooms without prohibitive editor complexity.
- **Narrative surfaces** (clue text, colored materials) cannot be authored at the tile
  level without a metadata layer of equivalent complexity to glTF extras.

By contrast, Blender geometry naturally produces the properties SONAR requires:
irregular surfaces, props at varied orientations, diagonal cuts, extruded text readable
only via sonar — all of which produce acoustically rich, spatially unique point clouds
that require sonar skill to interpret.

**cgltf** is already a project dependency (used in `render/model.c`), making glTF
adoption zero additional dependency cost.

## Decision

**All level geometry is authored in Blender and exported as `.glb` (glTF 2.0 binary).**
The tile editor is abandoned. cgltf is the sole level-loading library.

### Blender Node Naming Conventions

The level loader (`world/map.c`) interprets node names to separate visual geometry,
collision geometry, entity markers, and special surfaces:

| Prefix / Name | Type | Behaviour |
|---------------|------|-----------|
| *(no prefix)* | Visual mesh | Uploaded to GPU for rendering; not used for physics |
| `col_` | Collision mesh | Extracted as triangle soup for raycast and physics; **never rendered** |
| `entity_*` / `player_spawn` | Empty object | Entity marker; parsed by entity loader (see ADR 0007) |
| *(any mesh)* | Material `mat_clue_<color>` | Sonar hits generate points of that color (e.g. `mat_clue_red` → red points) |
| `player_spawn` | Empty object | Provides player initial position and yaw from world transform |

**Rationale for col_ prefix separation**: rendering and collision have different
fidelity requirements. Collision meshes can be simplified proxies (boxes, hulls)
while visual meshes carry full artistic detail. Keeping them separate in Blender
allows designers to iterate each independently.

### Clue Surfaces

Any mesh with a material named `mat_clue_<color>` is treated as a narrative surface.
Sonar hits on these triangles generate points of the designated color instead of the
standard surface color. Current palette:

| Material name | Sonar color (RGB normalized) | Semantic |
|---------------|------------------------------|----------|
| `mat_clue_red` | [1.0, 0.15, 0.15] | Clue / narrative hint |
| `mat_clue_blue` | [0.15, 0.45, 1.0] | Interactive / mechanical |

Additional `mat_clue_*` colors may be added in future levels. The loader must check
each primitive's material name against the `mat_clue_` prefix before applying default
surface color assignment.

### Player Spawn

The level must contain exactly one **Empty object** named `player_spawn`. The world
loader reads its translation (X, Y, Z) as the player's initial position and its
rotation around the Y axis as the initial yaw. No JSON metadata file is required for
spawn data.

## Consequences

### Positive
- No additional dependency (cgltf already in use)
- Blender is industry-standard; no custom editor to maintain
- Organic geometry produces rich sonar point clouds aligned with core game feel
- Material-level metadata (clue surfaces) is expressed natively in glTF without
  a separate JSON format
- Designers can use all Blender tools (modifiers, Boolean, particle systems, physics bake)

### Negative
- **`world/map.c` must be rewritten** from the current hardcoded procedural test geometry
  to a full cgltf traversal. This is the highest-engineering-cost consequence.
- The **explored-grid constants** (`EXPLORE_GRID_W`, `EXPLORE_GRID_H`,
  `EXPLORE_ORIGIN_X/Z`, `EXPLORE_CELL_SIZE`) were tuned for the procedural test level's
  known bounds. They must be re-evaluated once the first Blender level defines real
  map extents. Until then they are provisional (open question Q6 in TDD §10).
- Designers must know Blender naming conventions — a brief authoring guide is required.
- `.glb` files are binary; Git diffs are uninformative. Version control of levels
  requires designer discipline (descriptive commit messages, not relying on diffs).

---

## Changelog

| Date       | Change |
|------------|--------|
| 2026-03-07 | Initial ADR, status Accepted |
