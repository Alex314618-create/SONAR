# ADR 0005 — Sonar Visual Design: Render Primitives and Blending

> **Status**: Accepted
> **Date**: 2026-03-06
> **Deciders**: Architect, Engineer
> **Related**: [TDD §4](../tdd.md#4-rendering-pipeline), [GDD §6](../gdd.md#6-visual-design)
> **Implemented in**: `src/render/sonar_fx.c`, `src/render/vfx.c`

---

## Context

M5 refactored the sonar visual layer from the M3/M4 prototype. Four independent
rendering decisions required explicit justification because each carries trade-offs
affecting correctness, performance, and visual identity:

1. How to draw sonar echo points (primitive type)
2. How to draw laser feedback lines (coordinate space)
3. How to handle depth testing for sonar points (depth function)
4. How to blend overlapping sonar points (blend equation)

---

## Decision 1 — GL_POINTS (1–3 px hard circles) instead of sprite quads / billboards

### Rejected alternative

Billboard quads: render each sonar point as two triangles (a small screen-aligned quad)
with a circular texture or procedural soft circle. This was the approach documented
in TDD §4.3 prior to M5.

### Decision

Use `GL_POINTS` with distance-scaled `gl_PointSize` and a hard-edge circle clip in the
fragment shader (discard if `length(gl_PointCoord - 0.5) > 0.5`).

### Rationale

| Factor | GL_POINTS (chosen) | Billboard quad |
|--------|-------------------|----------------|
| GPU data per point | 28 bytes (pos + color) | same data + 4 verts/quad |
| Draw call overhead | 1 draw call, `glDrawArrays` | 1 draw call, instanced |
| Vertex shader work | identity — GPU sizes the point | billboard matrix per vert |
| Visual character | sharp, pixelated at close range | soft/blurry if textured |
| Aesthetic fit | matches CRT/oscilloscope look | looks like particles in a game |
| Implementation complexity | minimal | requires instanced quad geometry |

With 65 536 points, billboard quads would require a separate index/vertex buffer
for the quad geometry. GL_POINTS collapses this to a single vertex per point.
The sharp, 1–3 px appearance reinforces the "raw acoustic data" aesthetic: these
are sample hits, not glowing particles.

**Point size formula** (vertex shader):
```glsl
float dist = length(u_camPos - a_pos);
gl_PointSize = clamp(3.0 / dist, 1.0, 3.0);
```
This gives 3 px at close range, 1 px at distance — never sub-pixel, never
oversize.

---

## Decision 2 — Laser lines in pure 2D screen-space instead of 3D geometry

### Rejected alternative

Render laser lines as 3D line geometry from the player's position to each hit
point, subject to the standard view/projection pipeline and depth testing.

### Decision

Project each this-frame sonar hit point through the current view/projection
matrices to obtain its NDC position, convert to screen pixels, then draw a
`GL_LINES` primitive in a 2D orthographic pass from gun muzzle pixel to
projected hit pixel. No depth test is applied.

### Rationale

1. **The gun muzzle has no stable 3D world position.** The gun sprite is drawn in
   screen-space at a fixed bottom-right offset. A 3D line would not connect
   visually to the sprite regardless of player position.

2. **Laser lines are feedback, not world geometry.** They show "this is where my
   shot went this frame." Occlusion by walls would make them invisible exactly
   when the shot passed through geometry — defeating their purpose.

3. **Simpler depth management.** 3D lines require their own depth bias or
   `glPolygonOffset` to avoid z-fighting with sonar points at the hit surface.
   Screen-space lines avoid depth entirely.

4. **Performance.** The line set is small (at most `CONTINUOUS_RATE × dt` new
   points per frame, capped at 500 lines). CPU projection of N points is
   negligible.

**Implementation** (`src/render/vfx.c`):
```c
// Project world hit pos to screen coords
vec4 clip = proj × view × vec4(hit.pos, 1.0);
float sx = (clip.x/clip.w * 0.5 + 0.5) * winW;
float sy = (1.0 - (clip.y/clip.w * 0.5 + 0.5)) * winH;
// Draw GL_LINES from (gunX, gunY) to (sx, sy) in ortho space
```

---

## Decision 3 — GL_LEQUAL depth function for sonar point rendering

### Rejected alternative

Keep the default `GL_LESS` depth function throughout the entire frame, including
the sonar point pass.

### Decision

Switch to `GL_LEQUAL` before drawing sonar points; restore `GL_LESS` afterward.

```c
// sonar_fx_render() in src/render/sonar_fx.c
glDepthFunc(GL_LEQUAL);
glDrawArrays(GL_POINTS, 0, count);
glDepthFunc(GL_LESS);
```

### Rationale

M5 introduced a **depth-only pre-pass**: the world geometry is rendered first
with `glColorMask(GL_FALSE, …)` — no color output, only the depth buffer is
populated. This lets the GPU cull sonar points hidden behind walls before any
color blending occurs.

The problem: sonar hit points lie **exactly on the surface** they hit. Their
depth values (computed from the same world geometry) are numerically identical to
the values written by the pre-pass. With `GL_LESS`, `depth == depth` fails and
every sonar point on a wall face is discarded.

`GL_LEQUAL` passes fragments whose depth is ≤ the stored value, so co-planar
sonar points are correctly visible. The pre-pass still culls fully occluded
points (those behind walls from the camera's perspective).

**Why not `glDepthMask(GL_FALSE)` + `GL_LESS`?** That would make sonar points
ignore depth entirely — they would render through walls.

---

## Decision 4 — GL_ONE + GL_ONE additive blending

### Rejected alternative

Standard alpha blending: `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` — the default
for transparent objects.

### Decision

Use `glBlendFunc(GL_ONE, GL_ONE)` for the sonar point pass.

### Rationale

| Property | Additive (GL_ONE, GL_ONE) | Alpha blend |
|----------|--------------------------|-------------|
| Color of overlapping points | adds (gets brighter) | mixes (stays same saturation) |
| Bright clusters from density | yes — dense areas glow | no — blends toward average |
| Black background needed? | yes (additive over non-black washes out) | no |
| Order-dependent artifacts | none | yes — requires back-to-front sort |
| Alpha channel required | no | yes |

Additive blending is the correct model for **emissive / luminous** objects: two
lights in the same spot make more light. Dense sonar coverage on a surface should
read as a brighter, more "lit" area. This is how the world becomes legible — high
scan density reveals shape through brightness accumulation.

The near-black background (`glClearColor(0.008, 0.008, 0.02)`) is essential: any
non-black base color would saturate and lose hue discrimination. The depth pre-pass
also means only visible points contribute, preventing additive over-saturation
from occluded geometry.

**No sort required**: additive blending is commutative — order of point rendering
does not affect the final color.

---

## Consequences

- Sonar points appear as 1–3 px hard circles. Close-range surfaces show distinct
  sample positions; distant surfaces appear as continuous-looking point clouds.
- Dense scan areas glow brighter than sparse areas — a natural visual density cue.
- Laser lines are always fully visible regardless of geometry (intentional).
- The depth pre-pass adds one geometry draw call per frame but enables correct
  wall occlusion of sonar points without artifacts.
- `GL_LEQUAL` must be carefully scoped; any pass that writes to the depth buffer
  after the sonar pass must reset to `GL_LESS`.
