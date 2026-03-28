/**
 * vfx_particles.h — Dedicated particle system for stalker VFX
 *
 * Separate from sonar buffer: particles have velocity, gravity, and
 * alpha fade. Two effects: shockwave ring and sand collapse.
 */
#pragma once

/**
 * Initialize particle system (shader, VAO, VBO).
 * @return 0 on success, -1 on failure
 */
int vfx_particles_init(void);

/**
 * Spawn a shockwave ring at a world position.
 * 64 particles expanding outward in a horizontal ring.
 *
 * @param center  World position (vec3)
 * @param color   Base color (vec3)
 */
void vfx_particles_spawn_shockwave(const float center[3],
                                    const float color[3]);

/**
 * Spawn a sand collapse effect at a world position.
 * Particles fall with gravity + random horizontal drift.
 *
 * @param center  World position (vec3)
 * @param color   Base color (vec3)
 */
void vfx_particles_spawn_collapse(const float center[3],
                                   const float color[3]);

/**
 * Update all active particles (velocity, gravity, lifetime).
 * @param dt  Frame delta time
 */
void vfx_particles_update(float dt);

/**
 * Render active particles as GL_POINTS with alpha fade.
 *
 * @param view    View matrix (mat4)
 * @param proj    Projection matrix (mat4)
 * @param camPos  Camera position (vec3)
 */
void vfx_particles_render(const float *view, const float *proj,
                           const float *camPos);

/** Release GPU resources. */
void vfx_particles_shutdown(void);
