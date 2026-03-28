/**
 * map.h — Level loading and procedural test level generation
 *
 * Generates renderable geometry and collision data for the game world.
 */
#pragma once

#include "render/model.h"
#include "world/entity.h"

/**
 * Load or generate a map.
 *
 * @param path  Path to map file, or NULL for procedural test level
 * @return 0 on success, negative on error
 */
int map_load(const char *path);

/** Get the renderable model for the current map. */
const Model *map_get_render_model(void);

/** Get collision triangle data (9 floats per tri: 3 verts * xyz). */
const float *map_get_collision_verts(void);

/** Get number of collision triangles. */
int map_get_collision_tri_count(void);

/** Get player spawn position (vec3). */
const float *map_get_player_spawn(void);

/** Get player initial yaw in degrees. */
float map_get_player_yaw(void);

/**
 * @brief Check if a collision triangle belongs to a clue surface.
 *
 * If tri_index falls within a ClueRange, writes the clue RGB to out_rgb.
 *
 * @param tri_index  Triangle index in the collision array.
 * @param out_rgb    Output color (3 floats), written only if clue found.
 * @return 1 if clue surface, 0 otherwise.
 */
int map_get_clue_color(int tri_index, float *out_rgb);

/** Get mutable pointer to entity array (populated by map loader). */
Entity *map_get_entities(void);

/** Get number of entities parsed from the current map. */
int map_get_entity_count(void);

/**
 * @brief Get triangle data for a MeshRange (vis_* meshes).
 *
 * MeshRanges are visual-only mesh data loaded from vis_* glTF nodes,
 * stored in a separate buffer from collision data.
 *
 * @param index      MeshRange index (entity->mesh_index)
 * @param out_verts  Output pointer to triangle vertex data (9 floats per tri)
 * @param out_count  Output triangle count
 * @return 0 on success, -1 if index invalid
 */
int map_get_mesh_range_tris(int index, const float **out_verts, int *out_count);

/**
 * @brief Look up the collision triangle range for a named col_* node.
 *
 * Used by the door system to find which collision triangles to
 * enable/disable when a door opens or closes.
 *
 * @param name       Collision node name (e.g. "col_door_01")
 * @param out_start  Output: first triangle index
 * @param out_count  Output: number of triangles
 * @return 0 on success, -1 if name not found
 */
int map_get_collision_range(const char *name, int *out_start, int *out_count);

/** Free all map resources. */
void map_shutdown(void);
