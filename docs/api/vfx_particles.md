# SONAR — VFX Particles Module API Reference

> **Version**: 0.1.0
> **Status**: Accepted
> **Last Updated**: 2026-03-18
> **Owner**: CIO Agent
> **Source**: `src/render/vfx_particles.h` / `src/render/vfx_particles.c`
> **Related**: [docs/api/stalker.md §6](stalker.md#6-vfx_particles-integration)

---

## Table of Contents

1. [Overview](#1-overview)
2. [GPU Layout](#2-gpu-layout)
3. [Particle Lifecycle](#3-particle-lifecycle)
4. [Functions](#4-functions)
   - 4.1 [vfx_particles_init](#41-vfx_particles_init)
   - 4.2 [vfx_particles_spawn_shockwave](#42-vfx_particles_spawn_shockwave)
   - 4.3 [vfx_particles_spawn_collapse](#43-vfx_particles_spawn_collapse)
   - 4.4 [vfx_particles_update](#44-vfx_particles_update)
   - 4.5 [vfx_particles_render](#45-vfx_particles_render)
   - 4.6 [vfx_particles_shutdown](#46-vfx_particles_shutdown)
5. [Shader Specification](#5-shader-specification)
6. [GL State Contract](#6-gl-state-contract)
7. [Changelog](#7-changelog)

---

## 1. Overview

`render/vfx_particles` is a dedicated **world-space particle system** used
exclusively for stalker appearance and retreat effects. It is architecturally
separate from the sonar point buffer (`sonar/sonar`) because particles require
additional per-particle state that the sonar buffer does not model:

| Property | Sonar buffer | vfx_particles |
|----------|-------------|---------------|
| Velocity | No | Yes (3D) |
| Gravity | No | Yes (9.8 m/s²) |
| Alpha fade | No | Yes (linear over lifetime) |
| Lifetime | Age-based TTL | Per-particle `life` / `max_life` |
| Persistence | Circular buffer (persistent) | Pool (compacted on death) |

The module owns its own VAO, VBO, and shader (`particle.vert` / `particle.frag`).
It renders as `GL_POINTS` with `GL_SRC_ALPHA, GL_ONE` blending (soft additive —
particle alpha multiplies into the blend rather than being purely additive).

**Caller**: `world/stalker` only. No other module currently spawns particles.

---

## 2. GPU Layout

Each active particle uploads **7 floats** per point to the VBO:

```
Offset  Size  Attribute  Location  Description
──────  ────  ─────────  ────────  ──────────────────────────────────
  0     3×f   pos        0         World-space position (x, y, z)
  12    3×f   color      1         RGB base color (normalized [0,1])
  24    1×f   alpha      2         Alpha = life / max_life (linear fade)
──────────────────────────────────────────────────────────────────────
Total: 7 floats × 4 bytes = 28 bytes / particle
```

VBO stride: `7 × sizeof(float)` = 28 bytes.

The CPU-side `VfxParticle` struct (internal) carries additional fields not
uploaded to the GPU:

```c
typedef struct {
    float pos[3];     // also uploaded
    float vel[3];     // CPU only — integrated each frame
    float color[3];   // also uploaded
    float life;       // remaining lifetime, seconds
    float max_life;   // initial lifetime (for alpha = life/max_life)
} VfxParticle;
```

**Buffer capacity**: `MAX_VFX_PARTICLES` = 2048 particles. New particles beyond
this cap are silently dropped (`emit_particle` is a no-op when full).

GPU buffer is allocated as `GL_DYNAMIC_DRAW` with full capacity at init time.
Uploaded in 256-particle chunks via `glBufferSubData` each render call.

---

## 3. Particle Lifecycle

```
vfx_particles_spawn_*(center, color)
    │
    └─ emit_particle(pos, vel, color, lifetime)
         │  CPU: stored in s_particles[s_particleCount++]
         ▼
vfx_particles_update(dt)   [called each frame]
    │
    ├─ for each alive particle:
    │    vel.y -= 9.8 * dt          (gravity)
    │    pos += vel * dt            (Euler integration)
    │    if pos.y < 0: pos.y = 0,   (floor clamp)
    │                  vel.y = 0
    │    life -= dt
    │
    └─ compact: dead particles (life <= 0) are removed
         s_particleCount = number of alive particles

vfx_particles_render(view, proj, camPos)   [called each frame]
    │
    ├─ pack GPU buffer: [pos[3], color[3], alpha] per particle
    │    alpha = life / max_life
    │
    └─ glDrawArrays(GL_POINTS, 0, gpuCount)
```

Particles are **compacted in-place** during `vfx_particles_update()`: alive
particles are shifted toward the front of the array using a read/write index
pattern. This avoids fragmentation without a free-list.

---

## 4. Functions

### 4.1 `vfx_particles_init`

```c
int vfx_particles_init(void);
```

Initialises the particle system: loads the particle shader, creates VAO/VBO,
and configures vertex attribute pointers.

**Returns**: `0` on success, `-1` if shader loading fails.

**VAO layout** (set once at init):

| Attrib | Location | Components | Offset | Description |
|--------|----------|------------|--------|-------------|
| pos    | 0 | 3 floats | 0 bytes | World position |
| color  | 1 | 3 floats | 12 bytes | RGB color |
| alpha  | 2 | 1 float  | 24 bytes | Fade alpha |

**Side effects**: allocates GPU buffer (`MAX_VFX_PARTICLES × 28` bytes).
Logs: `VFX particles initialized (max 2048)` via `LOG_INFO`.

---

### 4.2 `vfx_particles_spawn_shockwave`

```c
void vfx_particles_spawn_shockwave(const float center[3],
                                    const float color[3]);
```

Spawns 64 particles arranged in a horizontal ring expanding outward from
`center`. Used when the stalker appears.

| Parameter | Description |
|-----------|-------------|
| `center` | World position of the effect origin (vec3) |
| `color`  | Base color (vec3); tinted internally for warm-red aesthetic |

**Particle parameters**:

| Property | Value |
|----------|-------|
| Count | 64 |
| Distribution | Evenly spaced angles: `θ = i/64 × 2π` |
| Velocity | `{cos(θ) × 4.0, 0.0, sin(θ) × 4.0}` m/s (horizontal ring) |
| Lifetime | 0.6 seconds |
| Color formula | `{color.r, color.g × 0.5 + 0.1, color.b × 0.3 + 0.05}` |

The color formula produces a red-orange ring when called with the stalker's
red base color `{1.0, 0.15, 0.15}`.

---

### 4.3 `vfx_particles_spawn_collapse`

```c
void vfx_particles_spawn_collapse(const float center[3],
                                   const float color[3]);
```

Spawns 48 particles that fall downward with gravity and random horizontal drift,
simulating the stalker dissolving into sand. Used when the stalker retreats.

| Parameter | Description |
|-----------|-------------|
| `center` | World position of the effect origin (vec3) |
| `color`  | Base color used as-is (vec3) |

**Particle parameters**:

| Property | Value |
|----------|-------|
| Count | 48 |
| Spawn offset | `center.xz ± rand(0, 0.5)`, `center.y + rand(0, 1.5)` |
| Velocity | `{rand(±0.3), -(1.0 + rand(0, 2.0)), rand(±0.3)}` m/s |
| Lifetime | 0.8 + rand(0, 0.6) seconds (variable per particle) |
| Gravity | 9.8 m/s² downward (applied in `update`) |
| Color | Passed through unchanged |

Random values use the module's internal LCG RNG (seed 9999, same algorithm as
`sonar.c` but separate state).

---

### 4.4 `vfx_particles_update`

```c
void vfx_particles_update(float dt);
```

Integrates all active particles for one frame.

| Parameter | Description |
|-----------|-------------|
| `dt` | Frame delta time in seconds |

**Per-particle** (in-place compaction loop):
1. Decrement `life -= dt`
2. If `life <= 0`: mark dead (skip in compaction)
3. Apply gravity: `vel.y -= 9.8 × dt`
4. Integrate: `pos += vel × dt`
5. Floor clamp: if `pos.y < 0`: set `pos.y = 0`, `vel.y = 0`

After iteration, `s_particleCount` equals the count of alive particles. Dead
particles' slots are reclaimed immediately.

---

### 4.5 `vfx_particles_render`

```c
void vfx_particles_render(const float *view, const float *proj,
                           const float *camPos);
```

Uploads active particle data to the GPU and issues a single draw call.
Early-exits if `s_particleCount == 0`.

| Parameter | Description |
|-----------|-------------|
| `view`   | View matrix (column-major `float[16]`, passed as `mat4` uniform `u_view`) |
| `proj`   | Projection matrix (column-major `float[16]`, passed as `mat4` uniform `u_proj`) |
| `camPos` | Camera world position (vec3, passed as `u_camPos` for distance-based point sizing) |

**Upload strategy**: particles are packed into a 256-particle staging buffer
(`chunkBuf[7 × 256]`) and flushed to the VBO via `glBufferSubData` in 256-particle
chunks. A final partial flush handles the remainder. This avoids mapping the
entire buffer for small particle counts.

**Uniforms set**:

| Uniform | Type | Value |
|---------|------|-------|
| `u_view`   | `mat4` | Camera view matrix |
| `u_proj`   | `mat4` | Projection matrix |
| `u_camPos` | `vec3` | Camera world position |

**Draw call**: `glDrawArrays(GL_POINTS, 0, gpuCount)`

---

### 4.6 `vfx_particles_shutdown`

```c
void vfx_particles_shutdown(void);
```

Releases all GPU resources and resets internal state.

**Side effects**:
- `glDeleteBuffers(1, &s_vbo)` — frees VBO
- `glDeleteVertexArrays(1, &s_vao)` — frees VAO
- `shader_destroy(s_shader)` — deletes shader program
- `s_particleCount = 0`

Safe to call with uninitialized handles (guards on non-zero IDs).

---

## 5. Shader Specification

**Files**: `shaders/particle.vert` / `shaders/particle.frag`

### Vertex Shader (`particle.vert`)

| Input (attrib) | Location | Type | Description |
|----------------|----------|------|-------------|
| `a_pos`   | 0 | `vec3` | World position |
| `a_color` | 1 | `vec3` | RGB color |
| `a_alpha` | 2 | `float` | Fade alpha |

| Uniform | Type | Description |
|---------|------|-------------|
| `u_view`   | `mat4` | View matrix |
| `u_proj`   | `mat4` | Projection matrix |
| `u_camPos` | `vec3` | Camera position for distance-based point size |

**Point size formula** (matches `sonar_point.vert` for visual consistency):
```glsl
float dist = length(u_camPos - a_pos);
gl_PointSize = clamp(3.0 / dist, 1.0, 4.0);
```

Passes `v_color` (vec3) and `v_alpha` (float) to fragment stage.

### Fragment Shader (`particle.frag`)

- **Circle clip**: `if (length(gl_PointCoord - 0.5) > 0.5) discard`
  (same hard-circle technique as `sonar_point.frag`)
- **Output**: `fragColor = vec4(v_color, v_alpha)`

The `v_alpha` channel interacts with `GL_SRC_ALPHA, GL_ONE` blending:
particles fade from full brightness to transparent over their lifetime.

---

## 6. GL State Contract

`vfx_particles_render()` modifies and restores the following GL state:

| State | During render | Restored to |
|-------|---------------|-------------|
| `GL_BLEND` | Enabled | Disabled |
| `glBlendFunc` | `GL_SRC_ALPHA, GL_ONE` | Previous state (caller's responsibility) |
| `GL_DEPTH_MASK` | `GL_FALSE` (no depth writes) | `GL_TRUE` |
| `glDepthFunc` | `GL_LEQUAL` | `GL_LESS` |

**Render ordering**: `vfx_particles_render()` should be called **after** the
sonar point pass (`sonar_fx_render`) and **before** the HUD pass, so particles
appear composited over the scene but behind UI elements.

---

## 7. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-18 | 0.1.0   | Initial API documentation for render/vfx_particles module |
