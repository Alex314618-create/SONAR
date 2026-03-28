/**
 * sonar_fx.h — GPU sonar point renderer
 *
 * Renders SonarPoint arrays as instanced point sprites with
 * additive blending and circular falloff.
 */
#pragma once

#include "sonar/sonar.h"

/** Initialize sonar FX renderer (shader, VAO, VBO). @return 0 on success */
int sonar_fx_init(void);

/**
 * Upload and render sonar points.
 *
 * @param points     Array of SonarPoint
 * @param count      Number of points to render
 * @param view       View matrix (mat4)
 * @param proj       Projection matrix (mat4)
 * @param camPos     Camera position (vec3)
 */
void sonar_fx_render(const SonarPoint *points, int count,
                     const float *view, const float *proj,
                     const float *camPos);

/** Release GPU resources. */
void sonar_fx_shutdown(void);
