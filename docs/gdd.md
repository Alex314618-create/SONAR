# SONAR — Game Design Document

> **Version**: 0.9.2
> **Status**: Draft
> **Last Updated**: 2026-03-24
> **Owner**: CIO Agent

---

## Table of Contents
1. [Game Overview](#1-game-overview)
2. [Core Experience](#2-core-experience)
3. [Game Mechanics](#3-game-mechanics)
4. [World Design](#4-world-design)
5. [Audio Design](#5-audio-design)
6. [Visual Design](#6-visual-design)
7. [UI/UX](#7-uiux)
8. [World Authoring & Interactive Systems](#8-world-authoring--interactive-systems)
9. [Trigger & Stalker Systems](#9-trigger--stalker-systems)
10. [Narrative](#10-narrative)
11. [Technical Constraints](#11-technical-constraints)
12. [Open Questions](#12-open-questions)
13. [Changelog](#13-changelog)

---

## 1. Game Overview

| Field        | Value                                    |
|--------------|------------------------------------------|
| Title        | SONAR                                    |
| Genre        | First-person exploration / atmospheric horror |
| Platform     | PC (Windows primary)                     |
| Perspective  | First-person 3D                          |
| Core Fantasy | Navigating blind through darkness using only sound |

### 1.1 Elevator Pitch

You are trapped in an underground facility with no light. Your only tool is a
handheld sonar device that reveals the world through sound — each pulse maps
geometry as glowing particles, then fades. You must navigate, explore, and
escape using nothing but acoustic echolocation.

### 1.2 Unique Selling Points
- **Sound IS sight**: The world is invisible until you reveal it with sonar
- **Persistent particle mapping**: Sonar echoes leave permanent traces, building
  a pointillistic image of the world over time
- **Resource tension**: Sonar costs energy — fire too much and you're blind;
  fire too little and you're lost
- **Spatial audio**: Echoes bounce realistically, letting you "hear" room size
  and geometry before you see it

---

## 2. Core Experience

### 2.1 Player Emotion Curve
```
Start:    Disorientation, vulnerability
Early:    Discovery, cautious exploration
Mid:      Growing confidence, mapping mastery
Late:     Tension, urgency (narrative pressure)
End:      Relief, accomplishment
```

### 2.2 Core Loop
```
Navigate (move in darkness)
    → Pulse (fire sonar, spend energy)
        → Reveal (world geometry appears as particles)
            → Decide (which direction? conserve or explore?)
                → Navigate ...
```

### 2.3 Design Pillars
1. **Atmosphere over action**: This is not a shooter. Tension comes from darkness and the unknown.
2. **Sound is information**: Every audio cue is meaningful gameplay data.
3. **Restraint as mechanic**: The energy system forces deliberate, strategic use of sonar.

---

## 3. Game Mechanics

### 3.1 Sonar System

#### 3.1.1 Sonar Modes
| Mode     | Rays | Spread  | Energy Cost | Cooldown | Use Case                  |
|----------|------|---------|-------------|----------|---------------------------|
| Wide     | 120  | 30 deg  | 20          | 0.4s     | General area scanning     |
| Focused  | 250  | 12 deg  | 15          | 0.25s    | Detailed corridor mapping |

#### 3.1.2 Fire Types
- **Pulse** (SPACE): Single burst, mode-dependent parameters
- **Continuous** (Hold LMB): Streaming rays at 150/sec, constant energy drain (20/sec)
- **Passive**: Automatic low-power ping every 2 seconds, 35 rays, 2.5 unit range

#### 3.1.3 Sonar Points
- Points are **persistent** — they do not fade (core design decision)
- Stored in a circular buffer (max 65,536 points; oldest overwritten)
- Each point has: world position (x, y, z), color
- Points are rendered as **additive glowing dots** with distance-based size

### 3.2 Energy System
| Parameter      | Value |
|----------------|-------|
| Max Energy     | 100   |
| Recharge Rate  | 12/sec (when not firing AND minimap is closed) |
| Wide Cost      | 20 per pulse |
| Focused Cost   | 15 per pulse |
| Continuous Drain| 20/sec |

### 3.3 Movement
| Parameter       | Value      |
|-----------------|------------|
| Move Speed      | 3.0 units/sec |
| Sprint Multiplier | 1.8x    |
| Mouse Sensitivity | 0.003 rad/px |
| Max Pitch       | ~70 degrees |
| Head Bob        | Sinusoidal, amplitude 4px |
| Collision       | AABB against world geometry |

### 3.4 Mode Toggle
- Right mouse button toggles between Wide and Focused
- Visual indicator on HUD

---

## 4. World Design

### 4.1 Environment
- Underground facility / industrial complex
- Mix of corridors, large chambers, and tight passages
- Designed to reward exploration and punish recklessness

### 4.2 Level Structure
- Levels defined as **3D geometry** (glTF meshes) + **JSON metadata**
- Metadata includes: player spawn, story zones, teleport zones, win zone, color scheme

### 4.3 Story Zones
- Rectangular trigger areas in the level
- When player enters: display text overlay with fade-in/fade-out
- Up to 32 story zones per level

### 4.4 Teleport Zones
- Allow non-linear level connections
- Trigger area + destination position + optional facing direction
- 0.5 second cooldown to prevent re-triggering

### 4.5 Win Condition
- Designated win zone in the level
- Triggers end sequence: "SIGNAL SENT" → "EXTRACTION CONFIRMED" → fade to black

---

## 5. Audio Design

### 5.1 Philosophy
Audio is the **primary information channel** in SONAR. The player is blind;
they rely on sound for spatial awareness even when sonar points are sparse.

### 5.2 Sound Categories
| Category      | Description | Spatial? |
|---------------|-------------|----------|
| Sonar pulse   | Distinctive ping on each fire mode | Source: player |
| Sonar echo    | Reflections returning from surfaces | 3D positioned at hit points |
| Footsteps     | Player movement feedback | Source: player |
| Ambient drone | Low background tension | Non-spatial (stereo) |
| Environment   | Dripping water, creaking metal, distant echoes | 3D positioned |
| Story cues    | Text appearance sound | Non-spatial |
| UI feedback   | Mode switch, energy low warning | Non-spatial |

### 5.3 Spatial Audio Requirements
- **HRTF**: Use OpenAL Soft's HRTF for headphone users (recommended play mode)
- **Distance attenuation**: Inverse distance model, tuned per sound type
- **Reverb**: Environment-dependent reverb (large room = long tail, corridor = short)

### 5.4 [TBD] Sound Asset List
*To be defined when level design is finalized. Need sound designer input.*

---

## 6. Visual Design

### 6.1 Visual Philosophy

The world of SONAR is **completely dark**. The background clear color is
near-black (`glClearColor(0.008, 0.008, 0.02)`) — a cold, almost-black void.

Sonar echoes are the **only light source**. Nothing is visible until the player
fires. As the player explores, hit points accumulate into a pointillistic map
of the environment. Dense coverage makes surfaces appear brighter through
additive blending; sparse coverage leaves vague outlines. The world is literally
built by the player's sonar use.

This is not a stylistic choice — it is the core design mechanic. Every visual
system must preserve this contract: **nothing renders unless sonar made it**.
The only exceptions are 2D HUD elements (energy, minimap, gun sprite) which are
navigation aids, not environmental illumination.

### 6.2 Sonar Point Visual Specification

Each sonar echo point is rendered as a **1–3 px hard-edge circle** using
`GL_POINTS`. Size is distance-dependent: 3 px close, 1 px at range, never
sub-pixel. Points do not fade — they are permanent until the circular buffer
wraps.

**Color is assigned per point at spawn time, randomised within per-surface ranges:**

| Surface | R (0–255) | G (0–255) | B (0–255) | Visual character |
|---------|-----------|-----------|-----------|-----------------|
| Wall    | 0         | 180–255   | 160–220   | Cyan-green, bright |
| Floor   | 0         | 160–240   | 120–180   | Teal-green, slightly warmer |
| Ceiling | 30–70     | 120–180   | 180–255   | Blue-violet, cooler |

Color randomisation uses a deterministic LCG RNG (seed 42) seeded at startup.
This ensures reproducible visuals for the same firing pattern.

**Continuous fire ray distribution**: rays use **uniform disk sampling** (polar
coordinates: `r = √rand`, `θ = rand · 2π`) projected onto the cone's cross-section.
This avoids the center-clustering artifact of naive rectangular random offsets and
produces even surface coverage across the scanned area.

**Blending**: `GL_ONE + GL_ONE` additive — overlapping points add their colors.
Dense scan areas accumulate brightness, providing a natural visual density cue:
bright regions = well-mapped surfaces.

**Depth**: Sonar points are rendered with `GL_LEQUAL` (see ADR 0005) after a
depth-only pre-pass of the world geometry. Points behind walls are correctly
occluded; co-planar points on wall faces are correctly visible.

### 6.3 Laser Lines (Continuous Fire Feedback)

When the player holds LMB (continuous sonar fire), a laser line is drawn from
the **gun muzzle** (fixed screen-space position, bottom-right corner) to each
sonar hit point added during the current frame.

- Lines are 2D screen-space primitives (`GL_LINES` in orthographic projection).
- They disappear the moment LMB is released (only this-frame's points are shown).
- Color: bright cyan (`RGB 0, 1, 0.9`), no depth test — always visible.
- Maximum 500 lines per frame (beyond that, excess points are skipped silently).

The laser lines provide real-time feedback of where the sonar is hitting, making
the continuous fire mode feel like an active scanning instrument rather than an
invisible ray emitter.

### 6.4 VFX Overlay Layer

Two fullscreen effects are composited in order after the 3D passes (the vignette
effect was removed in M8 to avoid dimming the already-dark sonar aesthetic):

| Effect | Description | Implementation |
|--------|-------------|----------------|
| Scanlines | Every 3rd screen row darkened by factor 0.85 | Fragment shader, fullscreen quad |
| Pulse ripple | Expanding ellipse on sonar fire, 0.4 s duration, fades linearly | Fragment shader, triggered per-fire |
| Gun sprite | Procedural 2D gun outline, bottom-right, brightens on LMB hold | Batched colored quads (`GL_TRIANGLES`) |

All VFX are rendered **after** sonar points and **before** the HUD.

The scanline effect reinforces the CRT/oscilloscope aesthetic. The pulse ripple
gives tactile feedback for discrete sonar pulses. The gun sprite anchors the
player's spatial reference for laser line origins.

### 6.5 Minimap

A fog-of-war explored-area map, toggled by **M key**. When active, the minimap
is displayed as a **centered square panel** occupying 70 % of the smaller window
dimension. The surrounding game view remains visible behind the panel. The panel
has a deep blue colour theme with animated wave effects.

| Parameter | Value |
|-----------|-------|
| Grid dimensions | 16 cells wide × 40 cells tall |
| Cell size | 0.5 m × 0.5 m world space |
| Origin (world) | X = −4.0 m, Z = −4.0 m |
| Panel size | 70 % of `min(winW, winH)`, centered on screen |
| Panel background | Near-black `(0, 0.01, 0.03)` at α = 0.88 |
| Border | 5 px deep blue `(0.04, 0.08, 0.45)` with breathing alpha pulsation |
| Grid cell animation | Diagonal wave: sine propagation from bottom-left to top-right; `waveSpeed = 4.0`, `waveFreq = 3.5`; cells blend between dim blue `(0.03, 0.08, 0.25)` and bright blue `(0.15, 0.28, 0.70)` |
| Player dot | Bright blue `(0.3, 0.5, 1.0)` |
| Direction indicator | Line of 5 small quads extending from the player dot toward the facing direction; each quad fades in alpha with distance |
| Energy drain | 20.0 energy/s while active; auto-closes when energy reaches 0 |
| Energy bar (panel) | Positioned below the panel; width = 80 % of panel width; blue fill `(0.1, 0.3, 0.9)`, turns orange `(0.8, 0.25, 0.1)` below 30 % energy; visible only while minimap is open |
| Explored condition | Cell marked when any sonar point lands within it |

A cell is rendered as a small colored square when `explored[z][x] == 1`. Unexplored
cells are not rendered (invisible = dark, consistent with visual philosophy). The
player's current cell is highlighted to show present position. A direction line of
five progressively fading quads indicates the player's facing direction.

The explored grid is maintained by `sonar.c` and queried by `hud.c` via
`sonar_get_explored_grid()`. It is a flat `int[EXPLORE_GRID_H * EXPLORE_GRID_W]`
array (1 = explored, 0 = not).

### 6.6 HUD Typography Specification

All HUD text is rendered using **DM Mono** (monospace, open-source) via **stb_truetype**,
baked into a single `512 × 512` OpenGL texture (`GL_R8` format — red channel only).

| Parameter | Value |
|-----------|-------|
| Font file | `assets/fonts/DMMono-Regular.ttf` |
| Atlas size | 512 × 512 px (`GL_R8`) |
| Small size | 15 px — labels, values (DEPTH, FPS, mode) |
| Large size | 22 px — title ("SONAR") |
| Atlas layout | Small glyphs in upper 256 rows; large glyphs in lower 256 rows |
| Texture filtering | `GL_NEAREST` (min and mag) — pixel-sharp glyph edges, no bilinear blur |

**Rendering pipeline**: text glyphs are batched into a separate vertex buffer
(8 floats per vertex: `pos.xy + uv.xy + color.rgba`) and drawn with the
`text.vert/frag` shader, which samples the `GL_R8` atlas as an alpha mask.
This is separate from the solid-color quad batch used for bars, minimap, and crosshair.

**Layout**:
| Region | Content |
|--------|---------|
| Top-left | "SONAR" (22 px) + depth (15 px) |
| Top-right | FPS counter (15 px) |
| Bottom-right | Mode indicator (15 px) + energy bar |
| Center (overlay, M key) | Minimap panel (see §6.5) |

### 6.7 View Bob

While the player is walking, a sinusoidal **view bob** is applied to the camera
to simulate natural head movement.

| Parameter | Value |
|-----------|-------|
| Y-axis amplitude | 0.028 units |
| X-axis amplitude | 0.009 units |
| Frequency | 7.5 Hz |
| Phase advance | `bobPhase += 7.5 · 2π · dt` per frame (while moving) |
| Y offset | `+amplitude_Y · sin(bobPhase)` |
| X offset | `+amplitude_X · sin(bobPhase · 0.5)` |

**Important**: the bob offset is applied to `cam.position` only for the purpose of
computing the view matrix. The camera's stored position (used for collision detection,
audio listener, and HUD depth readout) is **never modified**. The offset is added
before `camera_view_matrix()` and discarded immediately after.

---

## 7. UI/UX

### 7.1 HUD Elements
| Element        | Position       | Description                |
|----------------|----------------|----------------------------|
| Title          | Top-left       | "SONAR" branding           |
| Depth          | Below title    | "DEPTH: -XX M"             |
| Energy bar     | Bottom-right   | Horizontal bar with border |
| Mode indicator | Above energy   | "WIDE" or "FOCUSED"        |
| FPS counter    | Top-right      | Debug, togglable           |
| Crosshair      | Center         | Circular dot pattern       |
| Minimap        | Centered overlay | Fog-of-war explored cells (square panel, 70 % of smaller dimension) |
| Story text     | Bottom-center  | Fade-in/out overlay        |

### 7.2 Controls
| Input          | Action               |
|----------------|----------------------|
| W/S            | Move forward/back    |
| A/D            | Strafe left/right    |
| Mouse X        | Turn left/right      |
| Mouse Y        | Look up/down         |
| LMB (hold)     | Continuous sonar fire|
| RMB            | Toggle sonar mode    |
| SHIFT          | Sprint               |
| F              | Interact with nearest entity (door / dial); max range 2.5 m |
| M              | Toggle minimap overlay (centered panel; drains 20 energy/s; auto-closes at 0) |
| ESC            | Quit                 |

---

## 8. World Authoring & Interactive Systems

### 8.1 Level Design Philosophy

Levels are authored in **Blender** and exported as glTF 2.0 binary (`.glb`).
The choice of tool is not incidental — it is a design constraint.

**Sonar-richness is a first-class design criterion.** A level is well-designed if
scanning a room produces a visually interesting, spatially readable point cloud.
This means:
- Varied surface orientations: diagonals, curves, and angled cuts produce richer
  returns than flat orthogonal walls
- Deliberate props and details: a crate against a wall, a pipe running overhead,
  extruded text on a door — each reveals itself progressively as the player scans
- Non-rectangular rooms: L-shapes, recesses, and irregular floor plans are
  preferable to boxes

Designers should ask: *"What does this room look like through sonar?"* before
*"What does this room look like visually?"* — because the sonar map is the only
map the player will ever have.

### 8.2 Sonar Color Language

Players learn to read sonar point color as meaning. The color palette is consistent
across all levels:

| Color | What it means |
|-------|---------------|
| Teal / cyan-green | Environment — walls, floor, ceiling. Background geometry. |
| Red (bright) | **Look here.** Clue surface: a number, a word, a symbol. |
| Blue | **Interactive.** Something can be activated — a dial, a door panel. |
| Orange (fades quickly) | **Something moved.** A creature just made a sound. |
| Dim teal | Passive ping return — very close geometry, low-energy ambient echo. |

No HUD tooltip or tutorial explains this table. Players discover it through experience.
The design goal is that a player who has survived two rooms knows intuitively that
red means "pay attention" and orange means "turn around."

### 8.3 Password Dial Mechanic

Password dials are the primary puzzle mechanic. The flow:

1. The player scans a wall and finds **red sonar text** — a four-digit number rendered
   in extruded 3D geometry, visible only via sonar. The color means "this is a clue."
2. Elsewhere in the level, a **blue object** (the dial) is visible. Blue means
   "interactive." The player approaches and presses **F**.
3. A minimal UI appears: four digit slots. The player enters the code.
4. On correct input: the target door opens, an audio cue confirms success.
5. On incorrect input: the dial rejects, no penalty, player can retry.

No in-game instruction tells the player what dials do or that red text is a code.
The color language is the only hint. This is intentional — the game rewards
attention and pattern recognition, not following tutorial prompts.

### 8.4 Creature Passive Reveal

Creatures inhabit the levels but are never visible in the conventional sense —
the game has no lighting, and creatures are not sonar-scanned by default.

The only way to perceive a creature is when it **makes a sound**.

At the moment a creature emits its ambient audio, its geometry is briefly traced
in **orange sonar points**. The outline appears for less than a second, then fades.
The player hears a sound, turns toward it, and sees a shape dissolve.

Design intent:
- **Impermanence as threat**: the player never has a stable image of what they are
  sharing the level with. Memory and reaction determine survival.
- **Color as signal**: orange is distinct from all environment colors. Even a
  peripheral glimpse is recognisable.
- **No radar**: the minimap does not show creatures. Only direct acoustic encounters
  produce information.

The **duration** of the orange point cloud is configurable per creature via the `ttl`
custom property on the entity Empty in Blender. When `ttl = 0` (the default), the
sonar system applies its standard 0.8-second lifetime. Setting `ttl > 0` overrides
this to a custom duration in seconds, letting designers make some creatures feel more
or less substantial. See [Blender Level Authoring Guide](guides/blender-level-authoring.md)
for authoring details.

---

## 9. Trigger & Stalker Systems

### 9.1 Static Triggers

Static triggers are invisible activation zones placed in the level by the designer.
When a trigger fires, it **reveals a pre-authored mesh** as a burst of sonar points,
producing geometry that could not otherwise be seen. Triggers are **one-shot** — once
fired they do not repeat.

#### Trigger Modes

| Mode | Behaviour |
|------|-----------|
| **Zone** | Player enters any trigger whose `zone_id` matches → all triggers sharing that `zone_id` activate simultaneously (each after its own `delay` offset) |
| **Step** | Player enters range → only the nearest unfired trigger in the level activates |

Zone mode is designed for scripted reveals: an entire fossil corridor or wall fresco
appearing all at once when the player crosses a threshold. Step mode is suited to
progressive discovery: walking along a path lights up waypoints one at a time.

#### Trigger Reveal Sequence

When a trigger activates (after its `delay` timer elapses):

1. `trigger_reveal_mesh()` extracts triangles from the trigger's associated `vis_*` mesh
   (resolved via `mesh_ref` Blender property, stored in `map.c` as a `MeshRange`).
2. For each triangle, **10 sonar points** are generated:
   - 3 vertices
   - 3 edge midpoints
   - 1 centroid
   - 3 jitter points (random offset within triangle bounds, ±0.06 m)
3. All points are clamped to a **maximum of 500 per trigger** to protect the sonar
   buffer budget. If the mesh has more than 50 triangles, triangles are sampled
   uniformly at the required density.
4. Points use the default trigger color **cyan `(0, 0.85, 0.75)`** unless overridden
   by the designer's `color` custom property.

#### Starfield Effect

Trigger reveal points use a **two-tier TTL** to create a flash-then-settle visual:

| Tier | Proportion | TTL | Behaviour |
|------|-----------|-----|-----------|
| Permanent | 70 % | `ttl = 0` (default — never expires) | Persist in circular buffer as ordinary sonar points |
| Bright flash | 30 % | `ttl = 0.3s` | Rendered at full brightness, then expire |

The result is a bright flash on reveal that fades to a permanent dim outline —
establishing the geometry as a lasting map feature.

#### Example Use Cases

- **Ancient fossil corridor**: player crosses a doorway threshold → Zone trigger fires
  all fossil outlines simultaneously across the entire corridor.
- **Cave paintings**: Step trigger chain — each painting reveals as the player walks
  toward it.
- **Hidden inscription**: single Zone trigger with `delay = 1.2s` to build suspense
  before the text appears.

#### Budget Constraints

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Max points per trigger | 500 | Prevents a single trigger consuming >0.8 % of the 65 536-point sonar buffer |
| Points per triangle | 10 | Sufficient density for readable outlines; higher counts do not add legibility |
| Max triggers per level | 32 (shared entity cap) | Enforced by loader (`MAX_ENTITIES = 32`) |

---

### 9.2 Stalker

The Stalker is an invisible entity that responds to the player's use of the sonar.
It is never present at level start. It approaches from behind when the player fires
the sonar and retreats when the player stops.

The Stalker is not an enemy in the traditional sense — it cannot harm the player.
Its purpose is **psychological pressure**: the player's primary tool (sonar) is also
what summons it. Aggressive scanning has a cost.

#### State Machine

```
DORMANT ──(sonar fired)──► APPROACHING ──(dist ≤ step_dist)──► VISIBLE(2 s)
   ▲                                                                  │
   └───────(retreat_time elapsed)──── DEPARTING ◄────────────────────┘
```

| Phase | Condition to enter | Behaviour |
|-------|--------------------|-----------|
| **DORMANT** | Initial state; after full retreat | Stalker is absent. No sonar points generated. |
| **APPROACHING** | Player fires sonar | Each sonar fire event moves Stalker `step_dist` closer along the behind-vector |
| **VISIBLE** | `current_dist ≤ step_dist` (minimum distance reached) | Stalker mesh revealed as red sonar outline for 2.0 s; shockwave VFX spawned |
| **DEPARTING** | `reveal_timer` elapses (2.0 s) OR `idle_timer ≥ retreat_time` | Stalker retreats to `start_dist`; collapse VFX spawned |

#### Sonar-Based Approach

- Each sonar fire increments `last_fire_count`; on change, `current_dist -= step_dist`
- `step_dist` default: **1.5 m**
- `start_dist` default: **12.0 m** (designer-configurable)
- Minimum distance: **1.5 m** (clamped — Stalker cannot overlap the player)
- `retreat_time` default: **15 s** of sonar inactivity before retreat begins

#### Behind-Position Computation (`compute_behind_pos`)

1. Take the XZ-projection of the player's forward vector; negate it to get the
   behind-vector.
2. Walk `current_dist` metres along the behind-vector from the player's position.
3. Perform a ray check against the collision mesh. If the straight-line position
   is inside geometry, slide the position along the wall normal until a clear
   point with **0.5 m safety margin** is found.
4. Lock Y to world floor level (the Stalker is always ground-level).

#### Sonar Reveal

When the Stalker enters VISIBLE, `stalker_reveal_mesh()` generates an outline of
the Stalker mesh at `appear_pos`:

- Color: **red `(1.0, 0.15, 0.15)`**
- TTL: **2.0 s** (all points expire; the Stalker leaves no permanent trace)
- Point generation follows the same 10-points-per-triangle scheme as Static Triggers
- Mesh source: `vis_*` node referenced by `mesh_ref` Blender property

#### VFX Events

| Event | Effect | Parameters |
|-------|--------|------------|
| Stalker appears (VISIBLE) | **Shockwave ring** at `appear_pos` | 64 particles, red-orange `(1.0, 0.4, 0.1)`, horizontal ring, expand outward at 4 m/s, lifetime 0.6 s |
| Stalker departs (DEPARTING) | **Sandfall collapse** at last `appear_pos` | 48 particles, gravity 9.8 m/s² downward, random horizontal drift ±0.3 m/s, lifetime 0.8–1.4 s (randomised per particle) |

#### Audio

| Event | Sound |
|-------|-------|
| VISIBLE entered | `sound_appear` asset path (designer-defined) |
| DEPARTING entered | `sound_depart` asset path (designer-defined) |

---

### 9.3 Blender Custom Property Reference

All trigger and Stalker entities are authored as **Empty objects** in Blender with
custom properties in the Object Properties panel. The map loader reads these from
glTF extras.

#### Trigger Entity Properties (`entity_trigger_<id>`)

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `mesh_ref` | string | — | Name of the `vis_*` mesh node to reveal |
| `zone_id` | string | `""` | Zone identifier; triggers with the same `zone_id` fire together (Zone mode) |
| `delay` | float | `0.0` | Seconds after zone activation before this trigger fires |
| `radius` | float | `2.0` | Activation radius in metres |
| `ttl` | float | `0.0` | Per-point TTL override (0 = starfield default: 70% permanent / 30% × 0.3 s) |
| `sound` | string | `""` | Asset path for reveal sound, relative to `assets/sounds/` |
| `mode` | string | `"zone"` | `"zone"` or `"step"` |

#### Stalker Entity Properties (`entity_stalker_<id>`)

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `mesh_ref` | string | — | Name of the `vis_*` mesh node used for the reveal outline |
| `sound_appear` | string | `""` | Sound to play when Stalker enters VISIBLE phase |
| `sound_depart` | string | `""` | Sound to play when Stalker enters DEPARTING phase |
| `start_dist` | float | `12.0` | Initial and retreat distance from player, metres |
| `step_dist` | float | `1.5` | Distance closed per sonar fire event, metres |
| `retreat_time` | float | `15.0` | Seconds of sonar inactivity before retreat, seconds |

#### Door Entity Properties (`entity_door_<id>`)

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `target` | string | — | Name of the `col_door_*` collision mesh node whose triangles are toggled on F-key interaction |
| `sound` | string | `""` | Sound asset to play on open/close |

When the player presses **F** within 2.5 m of a door entity, `entity_activate()`
toggles the door's collision triangles on/off via `physics_set_tris_enabled()` and
`raycast_set_tris_enabled()`. An open door allows both player movement and sonar
rays to pass through.

If the `sound` property is set, a **spatial audio cue** is played at the door's world
position on every open/close toggle. The sound is pre-loaded during `entity_init()`
and played via `spatial_play()`, giving the player an acoustic confirmation of the
interaction and a positional reference in the darkness.

For complete Blender authoring instructions, including naming conventions and custom
property setup, see the [Blender Level Authoring Guide](guides/blender-level-authoring.md).

---

## 10. Narrative

### 10.1 Setting
An abandoned underground research facility ("Site Omega, Sublevel 4").
Power is offline. No personnel remain. Something happened here.

### 10.2 Story Delivery
- Environmental storytelling through level geometry
- Text triggers at key locations (story zones)
- No cutscenes, no voice acting — text and atmosphere only

### 10.3 Story Beats (Default Level)
1. Entry: "SITE OMEGA - SUBLEVEL 4. Power offline. Deploying acoustic mapper."
2. Corridor: "Emergency lighting failed. No signs of personnel."
3. Lab wing: "Lab equipment overturned. They left in a hurry."
4. Server room: "Data cores removed. Deliberate."
5. Deep shaft: "Structural damage. Proceed with caution."
6. Beacon room: "Emergency beacon located. Signal sent. Extraction confirmed."

---

## 11. Technical Constraints

- **Max sonar points**: 65,536 (circular buffer)
- **Target framerate**: 60 FPS minimum
- **Screen resolution**: 960x600 default (resizable [TBD])
- **Max map complexity**: [TBD, depends on renderer performance]
- **OpenGL version**: 3.3 Core (no compute shaders)

---

## 12. Open Questions

| # | Question | Status | Owner |
|---|----------|--------|-------|
| 1 | Should sonar points eventually fade to encourage re-scanning? | Open | Architect |
| 2 | Will there be enemies or hazards beyond darkness itself? | Open | Architect |
| 3 | Should levels be procedurally generated or hand-crafted? | Open | Architect |
| 4 | Multiplayer / co-op as a stretch goal? | Open | Architect |
| 5 | Should the sonar device be upgradeable (progression)? | Open | Architect |

---

## 13. Changelog

| Date       | Version | Changes                  |
|------------|---------|--------------------------|
| 2026-03-05 | 0.1.0   | Initial draft from prototype analysis |
| 2026-03-06 | 0.5.0   | M5: Expand Section 6 — visual philosophy, precise sonar point spec, laser lines, VFX overlay layer, minimap; update color ranges to match implementation |
| 2026-03-06 | 0.6.0   | M6 controls/polish: remove SPACE pulse (replaced by LMB-only); add disk sampling note to §6.2; add §6.6 HUD typography (DM Mono TTF, dual batch); add §6.7 view bob spec |
| 2026-03-07 | 0.7.0   | M6 world/entity: add §8 World Authoring & Interactive Systems (Blender workflow, sonar color language, password dial, creature reveal); renumber §8–§11 → §9–§12; add E key to controls; link ADR 0006/0007 |
| 2026-03-18 | 0.8.0   | M6 trigger/stalker: add §9 Trigger & Stalker Systems (§9.1 Static Triggers — zone/step modes, starfield TTL, 10pts/tri, 500pt cap; §9.2 Stalker — 4-phase state machine, sonar-approach, compute_behind_pos, shockwave/collapse VFX; §9.3 Blender custom property reference); renumber §9–§12 → §10–§13 |
| 2026-03-18 | 0.9.0   | M8: F-key interaction (2.5 m range); door collision toggle via CollisionRange; minimap overlay with M key (centered 70 % panel, deep blue theme, diagonal wave animation, direction line of 5 quads, below-panel energy bar, 20 energy/s drain); remove vignette VFX; add door entity properties to §9.3 |
| 2026-03-18 | 0.9.1   | M8 doc audit: correct §6.5 minimap from fullscreen overlay to centered square panel (70 % of shorter axis); replace direction chevron with 5-quad direction line; add diagonal wave animation parameters; add below-panel energy bar spec; add GL_NEAREST font filtering to §6.6; update §6.4 to note vignette removal; update §3.2 energy recharge condition to include minimap blocking |
| 2026-03-24 | 0.9.2   | M9: §8.4 creature point cloud lifetime now configurable via `ttl` custom property (0 = default 0.8 s); §9.3 door entities now play spatial audio on toggle when `sound` is configured; reference new Blender Level Authoring Guide |
