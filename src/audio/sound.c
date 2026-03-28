/**
 * sound.c — Sound asset loading implementation (dr_wav + stb_vorbis)
 */

/* dr_wav: single-header library, define implementation here */
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

/* stb_vorbis: single-file library, include the full implementation */
#include "stb_vorbis.c"

#include "audio/sound.h"
#include "core/log.h"

#include <AL/al.h>

#include <stdlib.h>
#include <string.h>

/* Copy up to (len-1) chars of src into dst and null-terminate. */
static void safe_strncpy(char *dst, const char *src, int len)
{
    int i;
    for (i = 0; i < len - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

Sound sound_load_wav(const char *path)
{
    Sound s;
    memset(&s, 0, sizeof(s));
    safe_strncpy(s.name, path, (int)sizeof(s.name));

    drwav wav;
    if (!drwav_init_file(&wav, path, NULL)) {
        LOG_ERROR("sound_load_wav: cannot open %s", path);
        return s;
    }

    /* Decode all frames to 16-bit PCM */
    drwav_uint64 frameCount = wav.totalPCMFrameCount;
    drwav_int16 *pcm = (drwav_int16 *)malloc(
        (size_t)frameCount * wav.channels * sizeof(drwav_int16));
    if (!pcm) {
        LOG_ERROR("sound_load_wav: malloc failed for %s", path);
        drwav_uninit(&wav);
        return s;
    }

    drwav_uint64 decoded = drwav_read_pcm_frames_s16(&wav, frameCount, pcm);
    if (decoded != frameCount) {
        LOG_ERROR("sound_load_wav: partial read %s (%llu/%llu frames)",
                  path, (unsigned long long)decoded, (unsigned long long)frameCount);
    }

    ALenum format = (wav.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, format, pcm,
                 (ALsizei)(decoded * wav.channels * sizeof(drwav_int16)),
                 (ALsizei)wav.sampleRate);

    s.buffer   = (uint32_t)buf;
    s.duration = (float)decoded / (float)wav.sampleRate;

    free(pcm);
    drwav_uninit(&wav);

    LOG_INFO("Loaded WAV: %s (%.2fs, %dch, %dHz)",
             path, s.duration, wav.channels, wav.sampleRate);
    return s;
}

Sound sound_load_ogg(const char *path)
{
    Sound s;
    memset(&s, 0, sizeof(s));
    safe_strncpy(s.name, path, (int)sizeof(s.name));

    int channels = 0, sampleRate = 0;
    short *pcm = NULL;
    int sampleCount = stb_vorbis_decode_filename(path, &channels, &sampleRate, &pcm);

    if (sampleCount < 0 || !pcm) {
        LOG_ERROR("sound_load_ogg: cannot decode %s (error %d)", path, sampleCount);
        return s;
    }

    ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, format, pcm,
                 (ALsizei)(sampleCount * channels * sizeof(short)),
                 (ALsizei)sampleRate);

    s.buffer   = (uint32_t)buf;
    s.duration = (float)sampleCount / (float)sampleRate;

    free(pcm);

    LOG_INFO("Loaded OGG: %s (%.2fs, %dch, %dHz)", path, s.duration, channels, sampleRate);
    return s;
}

void sound_destroy(Sound *s)
{
    if (s->buffer) {
        ALuint buf = (ALuint)s->buffer;
        alDeleteBuffers(1, &buf);
    }
    memset(s, 0, sizeof(*s));
}
