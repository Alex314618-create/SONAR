/**
 * hud.h — 2D heads-up display overlay
 *
 * Draws energy bar, mode indicator, crosshair, title, depth,
 * and FPS counter as a 2D overlay using orthographic projection.
 */
#pragma once

/**
 * Initialize HUD renderer (shader, VAO, VBO, font data).
 * @return 0 on success, negative on error
 */
int hud_init(void);

/**
 * Render the full HUD overlay. Call after all 3D rendering.
 *
 * @param winW        Window width in pixels
 * @param winH        Window height in pixels
 * @param energy      Energy level (0..100)
 * @param sonarMode   0 = WIDE, 1 = FOCUSED
 * @param fps         Current frames per second
 * @param camPos      Camera world position (vec3, float[3])
 * @param showMinimap 1 = render minimap, 0 = hide
 * @param playerYaw   Player facing yaw in radians (XZ plane, from cam.front)
 */
void hud_render(int winW, int winH, float energy, int sonarMode,
                float fps, const float *camPos,
                int showMinimap, float playerYaw);

/** Release HUD GPU resources. */
void hud_shutdown(void);
