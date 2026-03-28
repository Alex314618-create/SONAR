/**
 * stalker.h — Stalker entity system
 *
 * Threatening creature that appears behind the player when sonar is used.
 * Closes distance with each sonar fire, retreats when sonar stops.
 *
 * State machine:
 *   DORMANT → (sonar used) → APPROACHING → (sonar) → VISIBLE (2s TTL)
 *   APPROACHING → (no sonar for N seconds) → DEPARTING → DORMANT
 */
#pragma once

#include "world/entity.h"

/**
 * Initialize stalker states from entity array.
 *
 * @param entities  Entity array from map loader
 * @param count     Entity count
 */
void stalker_init(Entity *entities, int count);

/**
 * Tick stalker logic each frame.
 *
 * @param dt             Frame delta time
 * @param player_pos     Player world position (vec3)
 * @param player_forward Player forward direction (vec3)
 */
void stalker_update(float dt, const float player_pos[3],
                    const float player_forward[3]);

/** Free stalker resources. */
void stalker_shutdown(void);
