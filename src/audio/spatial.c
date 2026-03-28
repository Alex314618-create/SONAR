/**
 * spatial.c — 3D sound source pool implementation
 */

#include "audio/spatial.h"
#include "core/log.h"

#include <AL/al.h>

#include <string.h>

#define SOURCE_POOL_SIZE 32

static ALuint s_sources[SOURCE_POOL_SIZE];
static int    s_initialized = 0;

int spatial_init(void)
{
    alGenSources(SOURCE_POOL_SIZE, s_sources);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        LOG_ERROR("spatial_init: alGenSources failed (0x%x)", err);
        return -1;
    }

    /* Apply defaults to every source */
    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        alSourcef(s_sources[i], AL_GAIN,         1.0f);
        alSourcef(s_sources[i], AL_PITCH,        1.0f);
        alSourcef(s_sources[i], AL_ROLLOFF_FACTOR, 1.0f);
        alSourcef(s_sources[i], AL_REFERENCE_DISTANCE, 1.0f);
        alSourcef(s_sources[i], AL_MAX_DISTANCE, 50.0f);
        alSourcei(s_sources[i], AL_LOOPING,      AL_FALSE);
    }

    s_initialized = 1;
    LOG_INFO("Spatial audio initialized (%d sources)", SOURCE_POOL_SIZE);
    return 0;
}

void spatial_shutdown(void)
{
    if (!s_initialized) return;

    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        alSourceStop(s_sources[i]);
        alSourcei(s_sources[i], AL_BUFFER, 0);
    }
    alDeleteSources(SOURCE_POOL_SIZE, s_sources);
    memset(s_sources, 0, sizeof(s_sources));
    s_initialized = 0;

    LOG_INFO("Spatial audio shutdown");
}

/* Find a source that is not currently playing. Returns index or -1. */
static int grab_free_source(void)
{
    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        ALint state;
        alGetSourcei(s_sources[i], AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING && state != AL_PAUSED) {
            return i;
        }
    }
    return -1;
}

void spatial_play(const Sound *s, const float pos[3], float gain, float pitch)
{
    if (!s_initialized || !s->buffer) return;

    int idx = grab_free_source();
    if (idx < 0) {
        LOG_INFO("spatial_play: no free sources");
        return;
    }

    ALuint src = s_sources[idx];
    alSourcei(src,  AL_BUFFER,            (ALint)s->buffer);
    alSource3f(src, AL_POSITION,          pos[0], pos[1], pos[2]);
    alSource3f(src, AL_VELOCITY,          0.0f, 0.0f, 0.0f);
    alSourcef(src,  AL_GAIN,              gain);
    alSourcef(src,  AL_PITCH,             pitch);
    alSourcei(src,  AL_SOURCE_RELATIVE,   AL_FALSE);
    alSourcePlay(src);
}

void spatial_play_ambient(const Sound *s, float gain, float pitch)
{
    if (!s_initialized || !s->buffer) return;

    int idx = grab_free_source();
    if (idx < 0) {
        LOG_INFO("spatial_play_ambient: no free sources");
        return;
    }

    ALuint src = s_sources[idx];
    alSourcei(src,  AL_BUFFER,            (ALint)s->buffer);
    alSource3f(src, AL_POSITION,          0.0f, 0.0f, 0.0f);
    alSource3f(src, AL_VELOCITY,          0.0f, 0.0f, 0.0f);
    alSourcef(src,  AL_GAIN,              gain);
    alSourcef(src,  AL_PITCH,             pitch);
    alSourcef(src,  AL_ROLLOFF_FACTOR,    0.0f);  /* no attenuation */
    alSourcei(src,  AL_SOURCE_RELATIVE,   AL_TRUE);
    alSourcePlay(src);
}

void spatial_update(void)
{
    /* Sources auto-release when playback ends (AL_STOPPED state).
     * grab_free_source() picks them up on next play call.
     * Nothing explicit needed here, but the call exists for future use
     * (streaming, priority management, etc.). */
}
