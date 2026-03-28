/**
 * sonar.h — Sonar firing logic
 *
 * Fires rays in a cone from the player, producing SonarPoint hits
 * stored in a persistent circular buffer.
 */
#pragma once

#include <cglm/cglm.h>

#define MAX_SONAR_POINTS   65536
#define SONAR_MAX_RANGE    25.0f

typedef struct {
    float pos[3];
    float color[3];
    float age;
    float ttl;     /* custom TTL: 0 = default 0.8s for transient, >0 = custom */
} SonarPoint;

typedef enum {
    SONAR_MODE_WIDE,
    SONAR_MODE_FOCUSED
} SonarMode;

/** Initialize the sonar system. */
void sonar_init(void);

/**
 * Call once per frame, BEFORE any sonar firing.
 * Records the current write-head as the start of this frame's new points.
 */
void sonar_frame_begin(void);

/** Returns the write-head value recorded by the last sonar_frame_begin(). */
int sonar_get_frame_start(void);

/** Returns the current write-head (= total points mod MAX_SONAR_POINTS). */
int sonar_get_write_head(void);

/**
 * Cache the player's world pose for use by the passive ping.
 * Call once per frame before sonar_update().
 */
void sonar_set_player(const float *pos, const float *forward,
                      const float *up, const float *right);

/** Toggle between wide and focused modes. */
void sonar_toggle_mode(void);

/** @return current sonar mode. */
SonarMode sonar_get_mode(void);

/**
 * Fire a sonar pulse from the given position/direction.
 * @return number of new points added, or 0 if no energy
 */
int sonar_fire_pulse(const float *origin, const float *forward,
                     const float *up, const float *right);

/**
 * Fire continuous sonar (streaming rays). Call each frame while held.
 * @return number of new points added
 */
int sonar_fire_continuous(const float *origin, const float *forward,
                          const float *up, const float *right, float dt);

/** Update sonar state (age points, etc). Call each frame. */
void sonar_update(float dt);

/** @return pointer to the sonar point buffer (circular). */
const SonarPoint *sonar_get_points(void);

/** @return total number of active points (up to MAX_SONAR_POINTS). */
int sonar_get_point_count(void);

/**
 * @brief Insert a fully-formed sonar point into the circular buffer.
 *
 * Used by the entity system to inject creature reveal points with
 * custom color and age values.
 *
 * @param p  Point to insert (pos, color, age are all used as-is).
 */
void sonar_add_point(const SonarPoint *p);

/** Clear all sonar points. */
void sonar_clear(void);

/** @return total number of sonar fire events (pulse + continuous frames). */
int sonar_get_fire_count(void);

/** Shutdown sonar system. */
void sonar_shutdown(void);

/* ── Explored grid (minimap) ─────────────────────────────── */
#define EXPLORE_GRID_W    16
#define EXPLORE_GRID_H    40
#define EXPLORE_ORIGIN_X  (-4.0f)
#define EXPLORE_ORIGIN_Z  (-4.0f)
#define EXPLORE_CELL_SIZE  0.5f

/** Returns pointer to the flat explored grid [H][W], 1=explored 0=not. */
const int *sonar_get_explored_grid(void);
