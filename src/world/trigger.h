/**
 * trigger.h — Static trigger entity system
 *
 * One-time environmental reveals with two modes:
 *   - Zone: timed playback of grouped triggers sharing a zone_id
 *   - Step: approach-to-reveal closest unfired trigger
 *
 * Reveals inject sonar points from vis_* MeshRange data.
 */
#pragma once

#include "world/entity.h"

/**
 * Initialize trigger states from entity array.
 * Scans for ENTITY_TRIGGER, builds state table, preloads sounds.
 *
 * @param entities  Entity array from map loader
 * @param count     Entity count
 */
void trigger_init(Entity *entities, int count);

/**
 * Tick trigger logic each frame.
 *
 * @param dt          Frame delta time
 * @param player_pos  Player world position (vec3)
 */
void trigger_update(float dt, const float player_pos[3]);

/** Free trigger resources. */
void trigger_shutdown(void);
