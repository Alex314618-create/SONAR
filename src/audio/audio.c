/**
 * audio.c — OpenAL device/context lifecycle implementation
 */

#include "audio/audio.h"
#include "core/log.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <string.h>

static ALCdevice  *s_device  = NULL;
static ALCcontext *s_context = NULL;

int audio_init(void)
{
    s_device = alcOpenDevice(NULL);
    if (!s_device) {
        LOG_ERROR("OpenAL: failed to open audio device");
        return -1;
    }

    /* Request HRTF context if supported */
    ALCint attrs[5];
    int attrCount = 0;

    if (alcIsExtensionPresent(s_device, "ALC_SOFT_HRTF")) {
        attrs[attrCount++] = ALC_HRTF_SOFT;
        attrs[attrCount++] = ALC_TRUE;
        LOG_INFO("OpenAL: HRTF extension available, enabling");
    }
    attrs[attrCount] = 0;

    s_context = alcCreateContext(s_device, attrCount > 0 ? attrs : NULL);
    if (!s_context) {
        LOG_ERROR("OpenAL: failed to create context");
        alcCloseDevice(s_device);
        s_device = NULL;
        return -1;
    }

    if (!alcMakeContextCurrent(s_context)) {
        LOG_ERROR("OpenAL: failed to make context current");
        alcDestroyContext(s_context);
        alcCloseDevice(s_device);
        s_context = NULL;
        s_device  = NULL;
        return -1;
    }

    /* Set listener defaults */
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    ALfloat orient[6] = {0.0f, 0.0f, -1.0f,  /* forward */
                         0.0f, 1.0f,  0.0f}; /* up      */
    alListenerfv(AL_ORIENTATION, orient);
    alListenerf(AL_GAIN, 1.0f);

    LOG_INFO("OpenAL initialized: %s", alGetString(AL_RENDERER));
    return 0;
}

void audio_set_listener(const float *pos, const float *forward, const float *up)
{
    alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);

    ALfloat orient[6] = {
        forward[0], forward[1], forward[2],
        up[0],      up[1],      up[2]
    };
    alListenerfv(AL_ORIENTATION, orient);
}

void audio_shutdown(void)
{
    alcMakeContextCurrent(NULL);

    if (s_context) {
        alcDestroyContext(s_context);
        s_context = NULL;
    }
    if (s_device) {
        alcCloseDevice(s_device);
        s_device = NULL;
    }

    LOG_INFO("OpenAL shutdown");
}
