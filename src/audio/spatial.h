/**
 * spatial.h — 3D sound source pool and playback
 *
 * Manages a fixed pool of 32 OpenAL sources for positional and ambient audio.
 */
#pragma once

#include "audio/sound.h"

/**
 * Allocate 32 OpenAL sources.
 *
 * @return 0 on success, -1 on failure
 */
int spatial_init(void);

/** Stop all active sources and delete the pool. */
void spatial_shutdown(void);

/**
 * Play a sound at a 3D world position. Grabs a free source from the pool.
 * If no source is free, the call is silently dropped.
 *
 * @param s     Sound to play
 * @param pos   World-space position (vec3)
 * @param gain  Volume multiplier (1.0 = full)
 * @param pitch Pitch multiplier (1.0 = normal)
 */
void spatial_play(const Sound *s, const float pos[3], float gain, float pitch);

/**
 * Play a non-positional (ambient/stereo) sound. No distance attenuation.
 *
 * @param s     Sound to play
 * @param gain  Volume multiplier
 * @param pitch Pitch multiplier
 */
void spatial_play_ambient(const Sound *s, float gain, float pitch);

/** Reclaim sources whose playback has finished back into the free pool. */
void spatial_update(void);
