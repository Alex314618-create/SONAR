/**
 * input.h — Keyboard and mouse input handling
 *
 * Polls SDL events, tracks key states with edge detection,
 * and accumulates mouse movement deltas.
 */
#pragma once

/** Initialize the input subsystem. @return 0 on success, negative on error */
int input_init(void);

/** Shut down the input subsystem. */
void input_shutdown(void);

/** Poll events and update input state. Call once per frame. */
void input_update(void);

/** @return non-zero if the given SDL scancode is currently held down. */
int input_key_down(int scancode);

/** @return non-zero if the given SDL scancode was just pressed this frame. */
int input_key_pressed(int scancode);

/**
 * Get accumulated mouse movement since last frame.
 *
 * @param[out] dx  Horizontal delta in pixels
 * @param[out] dy  Vertical delta in pixels
 */
void input_mouse_delta(int *dx, int *dy);

/** @return non-zero if the given mouse button (SDL_BUTTON_LEFT, etc) is held. */
int input_mouse_down(int button);

/** @return non-zero if the given mouse button was just pressed this frame. */
int input_mouse_pressed(int button);

/**
 * Enable or disable mouse capture (relative mouse mode).
 *
 * @param captured  Non-zero to capture, zero to release
 */
void input_set_mouse_captured(int captured);
