/**
 * audio.h — OpenAL device/context lifecycle and listener management
 *
 * Manages the single OpenAL device and context. Enables HRTF when available.
 */
#pragma once

/**
 * Open the default audio device, create an OpenAL context, and enable HRTF.
 *
 * @return 0 on success, -1 on failure
 */
int audio_init(void);

/**
 * Update the OpenAL listener position and orientation each frame.
 *
 * @param pos      World-space eye position (vec3)
 * @param forward  Camera front vector (vec3)
 * @param up       Camera up vector (vec3)
 */
void audio_set_listener(const float *pos, const float *forward, const float *up);

/** Destroy the OpenAL context and close the device. */
void audio_shutdown(void);
