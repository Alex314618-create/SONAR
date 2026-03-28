# Blender Level Authoring Guide — SONAR

> **Version**: 1.0
> **Date**: 2026-03-24
> **Audience**: Level designer (Blender)

---

## 1. Overview

This guide covers how to set up all gameplay entities in Blender for SONAR levels.
The engine loads `.glb` files (glTF 2.0 binary) and parses node names + custom
properties to create runtime entities.

**Core principle**: Meshes define geometry, Empties define entities. The engine
distinguishes them by naming prefix.

---

## 2. Spatial Conventions

| Reference | Z Value |
|-----------|---------|
| Floor | 0.0 |
| Player eye height | 1.5 |
| Standard ceiling | 3.0 |

- `player_spawn` Empty should be placed at Z=1.5 (eye height).
- All positions are in meters (1 Blender unit = 1 meter).

---

## 3. Node Naming Convention

| Prefix | Object Type | Purpose | Example |
|--------|-------------|---------|---------|
| *(none)* | Mesh | Rendered visual geometry | `hallway_wall` |
| `col_*` | Mesh | Collision only (invisible) | `col_hallway` |
| `col_door_*` | Mesh | Door collision (toggled at runtime) | `col_door_01` |
| `vis_*` | Mesh | Entity reveal mesh (MeshRange) | `vis_creature_fish` |
| `entity_creature_*` | Empty | Creature entity | `entity_creature_01` |
| `entity_trigger_*` | Empty | Trigger / gallery element | `entity_trigger_paint_01` |
| `entity_stalker_*` | Empty | Stalker entity | `entity_stalker_01` |
| `entity_door_*` | Empty | Door entity | `entity_door_01` |
| `entity_dial_*` | Empty | Password dial entity | `entity_dial_01` |
| `entity_sound_*` | Empty | Ambient sound source | `entity_sound_drip` |
| `player_spawn` | Empty | Player spawn point | `player_spawn` |

### Important Rules

- **`vis_*` meshes are NOT rendered**. They exist only as point-cloud data for
  sonar reveals. Keep them simple (low poly).
- **`col_*` meshes are NOT rendered**. They define invisible collision walls.
- **Regular meshes** (no prefix) are rendered but have no collision unless they
  also have a `col_*` counterpart.
- Entity Empties must have **Custom Properties** set in Blender's Object
  Properties panel (N panel > Custom Properties, or Properties > Object > Custom Properties).

---

## 4. Material Conventions

| Material Name | Sonar Color | Purpose |
|--------------|-------------|---------|
| `mat_clue_red` | Red | Narrative clue (text, symbols) |
| `mat_clue_blue` | Blue | Interactive object indicator |
| *(any other)* | Cyan (default) | Normal environment surface |

Apply these materials to **rendered meshes** (not col_* or vis_*) where you want
colored sonar feedback.

---

## 5. Setting Up a Creature

Creatures are passive entities that periodically emit sound and briefly reveal
themselves as orange sonar point clouds.

### Steps

1. **Model the creature mesh** and name it `vis_creature_XX` (e.g., `vis_creature_fish`).
2. **Triangle limit**: Keep under **66 triangles** (engine cap: 200 vertices per reveal).
3. **Position** the mesh where the creature should appear in the level.
4. **Create an Empty** at the same position, name it `entity_creature_XX`.
5. **Add Custom Properties** to the Empty:

| Property | Type | Required | Description | Example |
|----------|------|----------|-------------|---------|
| `sound` | String | YES | Sound file in `assets/sounds/` | `creature_growl.ogg` |
| `interval` | Float | YES | Seconds between sound/reveal events | `5.0` |
| `mesh_ref` | String | YES | Name of the `vis_*` node | `vis_creature_fish` |
| `ttl` | Float | no | Point cloud lifetime in seconds (default: 0.8) | `1.5` |

### Runtime Behavior

Every `interval` seconds:
1. The creature's sound plays at its 3D position (spatial audio).
2. Orange sonar points appear tracing the `vis_*` mesh outline.
3. Points fade after `ttl` seconds (default 0.8s).

### Example

```
vis_creature_fish      (Mesh, ~40 tris, positioned at the pool)
entity_creature_01     (Empty, same position)
    sound     = "fish_splash.ogg"
    interval  = 6.0
    mesh_ref  = "vis_creature_fish"
    ttl       = 1.2
```

---

## 6. Setting Up a Gallery (Trigger Zone)

A gallery is a group of triggers sharing a `zone_id`. When the player enters
ANY trigger's radius, ALL triggers in the same zone activate with staggered
delays, revealing artworks one by one as permanent cyan point clouds.

### Steps

1. **Model each artwork** mesh: `vis_painting_01`, `vis_sculpture_01`, etc.
2. **Position** them where they should appear in the gallery.
3. **Create an Empty** for each, named `entity_trigger_XX`.
4. **Add Custom Properties**:

| Property | Type | Required | Description | Example |
|----------|------|----------|-------------|---------|
| `zone_id` | String | YES | Shared group ID (same for all in gallery) | `gallery_A` |
| `delay` | Float | YES | Reveal delay in seconds (stagger these) | `0.0` |
| `mode` | String | no | `"zone"` (default) or `"step"` | `zone` |
| `radius` | Float | no | Detection distance in meters (default: 3.0) | `5.0` |
| `mesh_ref` | String | YES | The `vis_*` mesh to reveal | `vis_painting_01` |
| `sound` | String | no | Sound to play on reveal | `reveal_whoosh.ogg` |
| `ttl` | Float | no | 0 = permanent (default), >0 = transient | `0` |

### Zone Mode Example (3-Painting Gallery)

```
vis_painting_01              (Mesh, gallery wall left)
vis_painting_02              (Mesh, gallery wall center)
vis_painting_03              (Mesh, gallery wall right)

entity_trigger_paint_01      (Empty, near painting 1)
    zone_id   = "hall_A"
    delay     = 0.0
    mesh_ref  = "vis_painting_01"
    radius    = 5.0

entity_trigger_paint_02      (Empty, near painting 2)
    zone_id   = "hall_A"
    delay     = 0.4
    mesh_ref  = "vis_painting_02"

entity_trigger_paint_03      (Empty, near painting 3)
    zone_id   = "hall_A"
    delay     = 0.8
    mesh_ref  = "vis_painting_03"
```

**Result**: Player walks within 5m of any trigger in "hall_A" ->
painting 1 appears immediately -> 0.4s later painting 2 -> 0.8s later painting 3.
All appear as permanent cyan point clouds with starfield sparkle effect.

### Step Mode Example (Corridor Progressive Reveal)

For sequential discovery along a path, use `mode = "step"`:

```
entity_trigger_step_01       (Empty, corridor section 1)
    mode      = "step"
    mesh_ref  = "vis_statue_01"
    radius    = 3.0

entity_trigger_step_02       (Empty, corridor section 2)
    mode      = "step"
    mesh_ref  = "vis_statue_02"
    radius    = 3.0
```

**Result**: Player approaches step_01 -> statue_01 reveals. Walk further ->
approach step_02 -> statue_02 reveals. Each trigger fires only once.

---

## 7. Setting Up a Door

Doors have collision meshes that can be toggled on/off by player interaction (F key).

### Steps

1. **Model the door collision** mesh, name it `col_door_XX`.
2. **Create an Empty** at the door position, name it `entity_door_XX`.
3. **Add Custom Properties**:

| Property | Type | Required | Description | Example |
|----------|------|----------|-------------|---------|
| `target` | String | YES | Name of the `col_door_*` node | `col_door_01` |
| `sound` | String | no | Open/close sound effect | `door_creak.ogg` |

### Example

```
col_door_01            (Mesh, door-shaped collision blocker)
entity_door_01         (Empty, at the door)
    target  = "col_door_01"
    sound   = "door_stone.ogg"
```

**Result**: Player walks up to the door, presses F -> collision disables (door "opens"),
sound plays. Press F again -> collision restores (door "closes").

### Tips

- The `col_door_*` mesh defines the physical barrier. Make it match the doorway shape.
- The entity Empty should be placed where the player would naturally stand to interact
  (interaction range: 2.5m).

---

## 8. Setting Up a Stalker

Stalkers are threatening creatures that appear behind the player when sonar is used.
They close distance with each sonar fire.

### Steps

1. **Model the stalker mesh**, name it `vis_stalker_XX`.
2. **Create an Empty**, name it `entity_stalker_XX`.
   Position doesn't matter — the stalker spawns behind the player.
3. **Add Custom Properties**:

| Property | Type | Required | Description | Default |
|----------|------|----------|-------------|---------|
| `mesh_ref` | String | YES | `vis_*` mesh name | — |
| `sound_appear` | String | no | Sound when stalker appears | *(none)* |
| `sound_depart` | String | no | Sound when stalker retreats | *(none)* |
| `start_dist` | Float | no | Initial distance behind player (meters) | 8.0 |
| `step_dist` | Float | no | Distance closed per sonar fire (meters) | 1.5 |
| `retreat_time` | Float | no | Seconds of no sonar before retreat | 15.0 |

### Example

```
vis_stalker_shadow          (Mesh, humanoid silhouette, ~50 tris)
entity_stalker_01           (Empty, anywhere in level)
    mesh_ref      = "vis_stalker_shadow"
    sound_appear  = "stalker_breath.ogg"
    sound_depart  = "stalker_sand.ogg"
    start_dist    = 10.0
    step_dist     = 2.0
    retreat_time  = 12.0
```

**Behavior**: When player fires sonar, stalker appears 10m behind as red point cloud.
Each additional sonar fire moves it 2m closer. If player stops using sonar for 12s,
stalker retreats with a sand-collapse effect.

---

## 9. Setting Up Ambient Sound Sources

Ambient sounds play at fixed positions in the world, looping at a set interval.

### Steps

1. **Create an Empty** at the sound position, name it `entity_sound_XX`.
2. **Add Custom Properties**:

| Property | Type | Required | Description | Example |
|----------|------|----------|-------------|---------|
| `sound` | String | YES | Sound file in `assets/sounds/` | `water_drip.ogg` |
| `interval` | Float | YES | Repeat interval in seconds | `3.0` |

---

## 10. Export Checklist

1. **File > Export > glTF 2.0 (.glb)**
2. Format: **glTF Binary (.glb)**
3. Include:
   - [x] **Custom Properties** (critical — entities won't load without this)
4. Transform: **+Y Up** (default, do not change)
5. Save to: `assets/maps/your_level.glb`

### Pre-Export Verification

- [ ] All entity Empties have correct naming prefix (`entity_*`)
- [ ] All reveal meshes have `vis_*` prefix
- [ ] All collision meshes have `col_*` prefix
- [ ] Custom Properties are set on Empties (not on meshes)
- [ ] `mesh_ref` values match actual `vis_*` node names exactly
- [ ] Door `target` values match actual `col_door_*` node names exactly
- [ ] Sound files exist in `assets/sounds/`
- [ ] `player_spawn` Empty exists at Z=1.5

---

## 11. Quick Reference — All Custom Properties

| Entity Type | Property | Type | Required | Default | Notes |
|-------------|----------|------|----------|---------|-------|
| **creature** | `sound` | String | YES | — | Audio file path |
| | `interval` | Float | YES | — | Sound repeat period (s) |
| | `mesh_ref` | String | YES | — | `vis_*` mesh name |
| | `ttl` | Float | no | 0.8 | Point lifetime (s) |
| **trigger** | `zone_id` | String | YES | — | Group ID for zone mode |
| | `delay` | Float | YES | — | Staggered reveal delay (s) |
| | `mode` | String | no | `"zone"` | `"zone"` or `"step"` |
| | `radius` | Float | no | 3.0 | Detection distance (m) |
| | `mesh_ref` | String | YES | — | `vis_*` mesh name |
| | `sound` | String | no | — | Reveal sound |
| | `ttl` | Float | no | 0 | 0=permanent, >0=transient |
| **door** | `target` | String | YES | — | `col_door_*` node name |
| | `sound` | String | no | — | Open/close sound |
| **stalker** | `mesh_ref` | String | YES | — | `vis_*` mesh name |
| | `sound_appear` | String | no | — | Appear sound |
| | `sound_depart` | String | no | — | Retreat sound |
| | `start_dist` | Float | no | 8.0 | Initial distance (m) |
| | `step_dist` | Float | no | 1.5 | Close per sonar fire (m) |
| | `retreat_time` | Float | no | 15.0 | Idle before retreat (s) |
| **sound** | `sound` | String | YES | — | Audio file path |
| | `interval` | Float | YES | — | Repeat period (s) |
| **dial** | `code` | String | YES | — | Correct dial code |
| **player_spawn** | *(none)* | — | — | — | Position + rotation only |

---

## 12. Engine Limits

| Resource | Max | Notes |
|----------|-----|-------|
| Total entities | 32 | All entity types combined |
| MeshRange entries | 32 | All `vis_*` meshes combined |
| MeshRange triangles | 8192 | Total across all `vis_*` meshes |
| Collision triangles | 8192 | Total `col_*` triangles |
| CollisionRange entries | 32 | Named `col_*` node groups |
| Creature reveal vertices | 200 | Per creature per reveal event |
| Sonar points | 65536 | Ring buffer, oldest overwritten |
| Entity sounds | 16 | Unique sound files cached |
