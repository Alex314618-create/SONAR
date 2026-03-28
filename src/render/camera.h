/**
 * camera.h — First-person camera with mouse look
 *
 * Euler-angle based FPS camera. Provides view and projection matrices
 * for use with the renderer.
 */
#pragma once

#include <cglm/cglm.h>

typedef struct Camera {
    vec3  position;
    vec3  front;
    vec3  up;
    vec3  right;
    float yaw;       /* degrees, 0 = +X, 90 = +Z */
    float pitch;     /* degrees, clamped [-89, 89] */
    float fov;       /* vertical FOV in degrees */
    float nearPlane;
    float farPlane;
} Camera;

/**
 * Initialize a camera at the given position with default orientation.
 *
 * @param cam  Camera to initialize
 * @param posX, posY, posZ  Initial position
 * @param yaw   Initial yaw in degrees
 * @param pitch Initial pitch in degrees
 */
void camera_init(Camera *cam, float posX, float posY, float posZ,
                 float yaw, float pitch);

/**
 * Update camera orientation from mouse delta.
 *
 * @param cam  Camera to update
 * @param dx   Mouse horizontal delta (pixels)
 * @param dy   Mouse vertical delta (pixels)
 */
void camera_update(Camera *cam, float dx, float dy);

/** Compute the view matrix and store in dest. */
void camera_view_matrix(const Camera *cam, mat4 dest);

/**
 * Compute the projection matrix and store in dest.
 *
 * @param cam    Camera with FOV and near/far settings
 * @param aspect Viewport aspect ratio (width / height)
 * @param dest   Output matrix
 */
void camera_proj_matrix(const Camera *cam, float aspect, mat4 dest);
