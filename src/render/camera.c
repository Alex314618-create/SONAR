/**
 * camera.c — First-person camera implementation
 */

#include "render/camera.h"

#include <math.h>

#define CAMERA_SENSITIVITY 0.1f

static void camera_update_vectors(Camera *cam)
{
    float yawRad   = glm_rad(cam->yaw);
    float pitchRad = glm_rad(cam->pitch);

    cam->front[0] = cosf(yawRad) * cosf(pitchRad);
    cam->front[1] = sinf(pitchRad);
    cam->front[2] = sinf(yawRad) * cosf(pitchRad);
    glm_vec3_normalize(cam->front);

    vec3 worldUp = {0.0f, 1.0f, 0.0f};
    glm_vec3_cross(cam->front, worldUp, cam->right);
    glm_vec3_normalize(cam->right);

    glm_vec3_cross(cam->right, cam->front, cam->up);
    glm_vec3_normalize(cam->up);
}

void camera_init(Camera *cam, float posX, float posY, float posZ,
                 float yaw, float pitch)
{
    cam->position[0] = posX;
    cam->position[1] = posY;
    cam->position[2] = posZ;
    cam->yaw         = yaw;
    cam->pitch       = pitch;
    cam->fov         = 75.0f;
    cam->nearPlane   = 0.1f;
    cam->farPlane    = 100.0f;

    camera_update_vectors(cam);
}

void camera_update(Camera *cam, float dx, float dy)
{
    cam->yaw   += dx * CAMERA_SENSITIVITY;
    cam->pitch -= dy * CAMERA_SENSITIVITY;  /* invert Y: moving mouse up -> look up */

    if (cam->pitch >  89.0f) cam->pitch =  89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    camera_update_vectors(cam);
}

void camera_view_matrix(const Camera *cam, mat4 dest)
{
    vec3 center;
    glm_vec3_add((float *)cam->position, (float *)cam->front, center);
    glm_lookat((float *)cam->position, center, (float *)cam->up, dest);
}

void camera_proj_matrix(const Camera *cam, float aspect, mat4 dest)
{
    glm_perspective(glm_rad(cam->fov), aspect, cam->nearPlane,
                    cam->farPlane, dest);
}
