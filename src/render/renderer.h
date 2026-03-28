/**
 * renderer.h — Core OpenGL renderer
 *
 * Manages global GL state, loads the basic shader, and provides
 * frame begin/end helpers.
 */
#pragma once

#include <stdint.h>

/** Initialize the renderer (GL state, load basic shader). @return 0 on success */
int renderer_init(void);

/** Begin a new frame: clear buffers, set viewport. */
void renderer_begin_frame(void);

/** End the current frame: swap buffers. */
void renderer_end_frame(void);

/** Shut down the renderer and release resources. */
void renderer_shutdown(void);

/** @return GL program handle for the basic shader. */
uint32_t renderer_get_basic_shader(void);
