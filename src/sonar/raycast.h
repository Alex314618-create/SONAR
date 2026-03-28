/**
 * raycast.h — Ray-triangle intersection against world collision mesh
 *
 * Casts rays against the triangle soup from map_get_collision_verts().
 */
#pragma once

/**
 * Initialize the raycast system with collision geometry.
 *
 * @param tris      Triangle data: 9 floats per tri (3 verts * xyz)
 * @param triCount  Number of triangles
 */
void raycast_init(const float *tris, int triCount);

/**
 * Cast a single ray against the collision mesh.
 *
 * @param origin      Ray origin (vec3)
 * @param dir         Normalized ray direction (vec3)
 * @param maxDist     Maximum ray distance
 * @param outHit      Output hit position (vec3), written only on hit
 * @param outNormal   Output surface normal at hit (vec3), written only on hit
 * @param outTriIndex Output triangle index in collision array, or NULL if not needed
 * @return distance to hit, or -1.0f if no hit within maxDist
 */
float raycast_cast(const float *origin, const float *dir, float maxDist,
                   float *outHit, float *outNormal, int *outTriIndex);

/**
 * Enable or disable a range of collision triangles for raycasting.
 *
 * Disabled triangles are skipped during ray intersection tests.
 * Used by the door system to allow sonar rays through open doors.
 *
 * @param start    First triangle index
 * @param count    Number of triangles
 * @param enabled  1 = enabled (default), 0 = disabled
 */
void raycast_set_tris_enabled(int start, int count, int enabled);

/** Release raycast resources. */
void raycast_shutdown(void);
