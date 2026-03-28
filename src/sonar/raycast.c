/**
 * raycast.c — Ray-triangle intersection (Moller-Trumbore)
 *
 * Brute-force against all triangles. Sufficient for the current
 * low-poly procedural levels (~100 tris).
 */

#include "sonar/raycast.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

#define RAY_EPSILON 1e-6f
#define MAX_RAYCAST_TRIS 8192

static const float *s_tris;
static int          s_triCount;
static uint8_t      s_triDisabled[MAX_RAYCAST_TRIS];

void raycast_init(const float *tris, int triCount)
{
    s_tris = tris;
    s_triCount = triCount;
    memset(s_triDisabled, 0, sizeof(s_triDisabled));
}

static float vec3_dot(const float *a, const float *b)
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void vec3_cross(const float *a, const float *b, float *out)
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static void vec3_sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static void vec3_normalize(float *v)
{
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > RAY_EPSILON) {
        float inv = 1.0f / len;
        v[0] *= inv;
        v[1] *= inv;
        v[2] *= inv;
    }
}

float raycast_cast(const float *origin, const float *dir, float maxDist,
                   float *outHit, float *outNormal, int *outTriIndex)
{
    float bestT = maxDist + 1.0f;
    int bestTri = -1;

    for (int i = 0; i < s_triCount; i++) {
        if (s_triDisabled[i]) continue;

        const float *v0 = &s_tris[i * 9 + 0];
        const float *v1 = &s_tris[i * 9 + 3];
        const float *v2 = &s_tris[i * 9 + 6];

        float e1[3], e2[3];
        vec3_sub(v1, v0, e1);
        vec3_sub(v2, v0, e2);

        float h[3];
        vec3_cross(dir, e2, h);
        float a = vec3_dot(e1, h);

        if (a > -RAY_EPSILON && a < RAY_EPSILON)
            continue;

        float f = 1.0f / a;
        float s[3];
        vec3_sub(origin, v0, s);
        float u = f * vec3_dot(s, h);
        if (u < 0.0f || u > 1.0f)
            continue;

        float q[3];
        vec3_cross(s, e1, q);
        float v = f * vec3_dot(dir, q);
        if (v < 0.0f || u + v > 1.0f)
            continue;

        float t = f * vec3_dot(e2, q);
        if (t > RAY_EPSILON && t < bestT) {
            bestT = t;
            bestTri = i;
        }
    }

    if (bestTri < 0 || bestT > maxDist)
        return -1.0f;

    if (outHit) {
        outHit[0] = origin[0] + dir[0] * bestT;
        outHit[1] = origin[1] + dir[1] * bestT;
        outHit[2] = origin[2] + dir[2] * bestT;
    }

    if (outNormal) {
        const float *v0 = &s_tris[bestTri * 9 + 0];
        const float *v1 = &s_tris[bestTri * 9 + 3];
        const float *v2 = &s_tris[bestTri * 9 + 6];
        float e1[3], e2[3];
        vec3_sub(v1, v0, e1);
        vec3_sub(v2, v0, e2);
        vec3_cross(e1, e2, outNormal);
        vec3_normalize(outNormal);
    }

    if (outTriIndex)
        *outTriIndex = bestTri;

    return bestT;
}

void raycast_set_tris_enabled(int start, int count, int enabled)
{
    uint8_t val = enabled ? 0 : 1;
    for (int i = start; i < start + count && i < s_triCount; i++) {
        if (i >= 0)
            s_triDisabled[i] = val;
    }
}

void raycast_shutdown(void)
{
    s_tris = NULL;
    s_triCount = 0;
}
