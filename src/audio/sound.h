/**
 * sound.h — Sound asset loading and management
 *
 * Loads .wav and .ogg files fully into OpenAL buffers.
 * Sounds are decoded into memory at load time (no streaming).
 */
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t buffer;    /* OpenAL buffer handle */
    float    duration;  /* Length in seconds */
    char     name[64];  /* Filename (for debug) */
} Sound;

/**
 * Load a WAV file into an OpenAL buffer via dr_wav.
 *
 * @param path  Path to the .wav file
 * @return Sound with valid buffer on success; buffer == 0 on failure
 */
Sound sound_load_wav(const char *path);

/**
 * Load an OGG/Vorbis file into an OpenAL buffer via stb_vorbis.
 *
 * @param path  Path to the .ogg file
 * @return Sound with valid buffer on success; buffer == 0 on failure
 */
Sound sound_load_ogg(const char *path);

/** Delete the OpenAL buffer and zero the struct. */
void sound_destroy(Sound *s);
