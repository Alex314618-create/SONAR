/**
 * physics.h — AABB-based collision detection and response
 *
 * Uses per-axis collide-and-slide against triangle soup.
 */
#pragma once

typedef struct {
    float min[3];
    float max[3];
} AABB;

/**
 * Initialize the physics system with collision geometry.
 *
 * @param tris      Triangle data: 9 floats per tri (3 verts * xyz)
 * @param triCount  Number of triangles
 * @return 0 on success, negative on error
 */
int physics_init(const float *tris, int triCount);

/**
 * Attempt to move from pos by velocity, with AABB collision.
 *
 * @param pos      Current position (vec3)
 * @param velocity Desired movement (vec3)
 * @param bounds   Player AABB (relative to pos)
 * @param outPos   Resulting position after collision (vec3)
 */
void physics_move(const float *pos, const float *velocity,
                  const AABB *bounds, float *outPos);

/**
 * Enable or disable a range of collision triangles.
 *
 * Disabled triangles are skipped during collision checks.
 * Used by the door system to toggle door collision on/off.
 *
 * @param start    First triangle index
 * @param count    Number of triangles
 * @param enabled  1 = enabled (default), 0 = disabled
 */
void physics_set_tris_enabled(int start, int count, int enabled);

/** Free collision data. */
void physics_shutdown(void);
