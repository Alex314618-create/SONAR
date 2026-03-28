/**
 * window.h — SDL2/OpenGL window management
 *
 * Creates and manages the application window, GL context, and swap chain.
 */
#pragma once

/**
 * Initialize the window subsystem.
 * Creates an SDL2 window with an OpenGL 3.3 Core context.
 *
 * @param width   Window width in pixels
 * @param height  Window height in pixels
 * @param title   Window title string
 * @return 0 on success, negative on error
 */
int window_init(int width, int height, const char *title);

/** Shut down the window subsystem and release all resources. */
void window_shutdown(void);

/** Swap the front and back buffers (present the frame). */
void window_swap(void);

/** @return non-zero if the window should close. */
int window_should_close(void);

/** Set the should-close flag (e.g. on SDL_QUIT or ESC). */
void window_set_should_close(int close);

/**
 * Get the current window size in pixels.
 *
 * @param[out] width   Receives window width
 * @param[out] height  Receives window height
 */
void window_get_size(int *width, int *height);
