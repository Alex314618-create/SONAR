# ADR 0004 — M3 Sonar Firing Mechanism Gaps vs Prototype

> **Status**: Accepted — Partially resolved (5/6 gaps closed in M4; Gap 6 deferred to M5)
> **Date**: 2026-03-06
> **Last Updated**: 2026-03-06
> **Owner**: CIO Agent
> **Type**: Gap Log (not a decision record — documents known implementation delta)

---

## Context

M3 delivered a functional sonar system (`sonar/raycast`, `sonar/energy`, `sonar/sonar`,
`render/sonar_fx`) and audio system (`audio/audio`, `audio/sound`, `audio/spatial`).
The reference for expected behavior is the 2D prototype (`render.c`, `sonar.c`).

At M3 close, six gaps exist between the prototype's sonar firing behavior and the
current 3D implementation. These gaps affect visual density, organic feel, and
audio feedback. All gaps are targeted for closure in **M4**.

---

## Gap 1 — Single Hit Point vs Scatter + Secondary Points

> **Status**: ✅ Fixed in M4a

**Prototype behavior**:
- On each ray hit, the prototype spawns **1–2 wall points** at the hit position plus
  small random scatter offsets.
- Probabilistically spawns additional **floor and ceiling points** near the hit
  (capped scatter radius, random offset within surface plane).
- Result: each sonar burst produces a visually dense, organic cluster of points.

**M3 behavior**:
- Exactly **1 clean point** was recorded at the precise hit position.
- No scatter, no secondary points.

**M4 resolution**: Implemented in `src/sonar/sonar.c`. Primary wall point (±0.04 XZ jitter)
plus probabilistic secondary wall (50%·density), floor-at-base (60%·density), and ceiling
(40%·density) points per hit. LCG RNG used for all probability rolls.

---

## Gap 2 — No Floor Points Along Ray Trajectory

> **Status**: ✅ Fixed in M4a

**Prototype behavior**:
- For each ray, the prototype samples **dense floor points along the ray's
  horizontal trajectory**, not just at the wall hit.
- This gives strong ground-plane presence — the player can see the floor in front
  of them even when no wall is near.

**M3 behavior**:
- Only the wall (or geometry) hit point was recorded.
- No floor sampling along the ray path.

**M4 resolution**: Implemented in `src/sonar/sonar.c`. After each raycast, the system walks
along the ray depositing floor points at variable-step intervals. Step size is scaled by
`1.0 / density`. Rays with `dir.Y > 0.3` are skipped to avoid flooding the floor with
downward-angled ray samples.

---

## Gap 3 — No Passive Ping

> **Status**: ✅ Fixed in M4a

**Prototype behavior**:
- Every **2 seconds**, the prototype automatically fires a **35-ray, 360° passive ping**
  at low power (no energy cost or minimal cost).
- Provides ambient world awareness without player input.

**M3 behavior**:
- Sonar fired only on explicit player input (SPACE, LMB).
- No automatic pings.

**M4 resolution**: Implemented in `src/sonar/sonar.c`. `sonar_passive_ping()` fires every
2.0 s: 35 rays distributed in a 360° horizontal (XZ) ring, 2.5 m max range, zero energy
cost. Player pose is cached via `sonar_set_player()`, which must be called each frame before
`sonar_update()`.

---

## Gap 4 — Fixed Colors vs ColorRange Randomness

> **Status**: ✅ Fixed in M4a

**Prototype behavior**:
- Each surface type (wall, floor, ceiling) has a `ColorRange` struct: separate
  `[min, max]` bounds for R, G, B channels.
- On each hit, a random color is sampled within the surface's range.
- Result: surfaces have a consistent hue but natural color variation, making large
  surfaces look alive rather than flat.

**M3 behavior**:
- Hardcoded fixed colors were assigned per surface type.
- No per-hit randomness.

**M4 resolution**: Implemented `ColorRangeF` struct in `src/sonar/sonar.c`. Three surface
ranges are defined per GDD 6.2 (Wall: G=180–255 B=160–220; Floor: G=160–240 B=120–180;
Ceiling: R=30–70 G=120–180 B=180–255). LCG RNG samples a random color within range for
every point spawned.

---

## Gap 5 — No Visual FX

> **Status**: ✅ Fixed in M4b (partial — see note)

**Prototype behavior**:
- **Device sprite**: A gun/emitter sprite rendered at screen center.
- **Laser lines**: Short line segments emitted from the device toward hit points on fire.
- **Pulse ripple**: An expanding ring effect at the device origin on pulse fire.
- **Ambient dust**: Slowly drifting background particles to convey atmosphere.

**M3 behavior**:
- None of these effects existed.

**M4 resolution**: Implemented in `src/render/vfx.c`:
- **Scanlines**: fragment shader darkens every 3rd row by 0.85
- **Vignette**: radial falloff factor 3.2, clamped to [0.15, 1.0]
- **Pulse ripple**: 0.4s expanding ellipse (Y squish 0.6×), triggered on SPACE

**Note**: Device sprite, laser lines, and ambient dust were not implemented in M4.
These are lower-priority cosmetic effects; no blocking gap logged for them.

---

## Gap 6 — Echo Sound Not Triggered

> **Status**: ⏳ Deferred to M5

**Prototype behavior**:
- On each sonar hit, a short **echo sound** (`sndEcho`) is played at the hit
  position in 3D space. Hit echoes arriving from different directions give the player
  auditory spatial information.

**M3 behavior** (unchanged in M4):
- `sndEcho` is loaded into an OpenAL buffer during audio init.
- **No playback path exists** — the sonar system does not call into `audio/` on hit.

**Impact**: The core audio feedback loop of sonar echolocation is absent. The player
cannot hear echoes, which is a central mechanic of the game concept.

**M5 action**: Pass hit positions from `sonar_fire_pulse()` back to `main.c` (or via
a callback/event queue). Play `sndEcho` at each hit position using `spatial_play()`
with distance-based gain.

---

## Consequences

### Positive
- All gaps are fully documented with prototype references, current behavior, impact
  assessment, and a concrete action.
- M4 closed 5/6 gaps, bringing the sonar visual density and organic feel to parity
  with the prototype.

### Negative
- Gap 6 (echo audio) remains open. The sonar experience is still missing its core
  acoustic feedback loop. This is the highest-priority item for M5.

---

## Resolution (M4)

| Gap | Title | Status |
|-----|-------|--------|
| G1 | Single point vs scatter | ✅ Fixed in M4a |
| G2 | No floor points along ray | ✅ Fixed in M4a |
| G3 | No passive ping | ✅ Fixed in M4a |
| G4 | Fixed colors | ✅ Fixed in M4a |
| G5 | No visual FX | ✅ Fixed in M4b (scanlines, vignette, pulse ripple) |
| G6 | Echo sound not triggered | ⏳ Deferred to M5 |

Device sprite, laser lines, and ambient dust (originally listed under G5) were not
implemented in M4. They are cosmetic and do not block core gameplay. No new gap entry
is created; they may be addressed opportunistically in M5 or later.

---

## Changelog

| Date       | Change |
|------------|--------|
| 2026-03-06 | Initial gap log at M3 close |
| 2026-03-06 | M4 update: mark G1–G5 resolved; defer G6 to M5; add Resolution table |
