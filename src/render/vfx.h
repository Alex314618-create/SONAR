/**
 * vfx.h — Fullscreen visual effects (scanlines, vignette, pulse ripple,
 *          gun sprite, laser lines)
 *
 * All effects render as fullscreen quads or 2D overlays with fragment shaders.
 * Call after 3D rendering and before hud_render.
 */
#pragma once

#include "sonar/sonar.h"

/** Initialize VFX renderer (shaders, VAOs). @return 0 on success */
int vfx_init(void);

/** Update VFX state (pulse ripple timer). Call each frame. */
void vfx_update(float dt);

/** Render CRT scanline overlay. */
void vfx_render_scanlines(int winW, int winH);

/** Render vignette darkening overlay. */
void vfx_render_vignette(int winW, int winH);

/** Trigger a pulse ripple effect (call when sonar pulse fires). */
void vfx_trigger_pulse_ripple(void);

/** Render pulse ripple if active. */
void vfx_render_pulse_ripple(int winW, int winH);

/** Render 2D gun sprite in the bottom-right corner. */
void vfx_render_gun(int winW, int winH, int lmbHeld);

/** Returns the gun muzzle screen position (laser line origin). */
void vfx_get_gun_muzzle(int winW, int winH, int *outX, int *outY);

/**
 * Draw laser lines from gun muzzle to each sonar point added this frame.
 * Only call when LMB is held.
 */
void vfx_render_laser_lines(
    const SonarPoint *points, int frameStart, int writeHead,
    int totalCapacity,
    const float *view, const float *proj,
    int winW, int winH,
    int gunX, int gunY
);

/** Release VFX GPU resources. */
void vfx_shutdown(void);
