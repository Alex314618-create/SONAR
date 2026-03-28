/**
 * physics.c — AABB collision detection and response
 *
 * Implements per-axis collide-and-slide using AABB vs triangle SAT test.
 */

#include "world/physics.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define MAX_PHYSICS_TRIS 8192

static float   *s_tris = NULL;
static int      s_triCount = 0;
static uint8_t  s_triDisabled[MAX_PHYSICS_TRIS];

/* --- AABB-Triangle overlap test (Tomas Akenine-Moller SAT) --- */

static void vec3_sub(float *out, const float *a, const float *b)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static void vec3_cross(float *out, const float *a, const float *b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float vec3_dot(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* Project triangle vertices onto axis and check overlap with AABB projection.
 * center = AABB center, halfSize = AABB half-extents.
 * v0,v1,v2 are triangle vertices relative to AABB center. */
static int sat_test_axis(const float *axis, const float *halfSize,
                         const float *v0, const float *v1, const float *v2)
{
    /* AABB projection radius */
    float r = halfSize[0] * fabsf(axis[0])
            + halfSize[1] * fabsf(axis[1])
            + halfSize[2] * fabsf(axis[2]);

    /* Triangle vertex projections */
    float p0 = vec3_dot(axis, v0);
    float p1 = vec3_dot(axis, v1);
    float p2 = vec3_dot(axis, v2);

    float triMin = p0, triMax = p0;
    if (p1 < triMin) triMin = p1;
    if (p2 < triMin) triMin = p2;
    if (p1 > triMax) triMax = p1;
    if (p2 > triMax) triMax = p2;

    /* Separating axis found if intervals don't overlap */
    if (triMin > r || triMax < -r) return 0;
    return 1;
}

/* Returns 1 if AABB overlaps triangle, 0 if separated. */
static int aabb_tri_overlap(const float *center, const float *halfSize,
                            const float *t0, const float *t1, const float *t2)
{
    /* Move triangle to AABB-centered coordinates */
    float v0[3], v1[3], v2[3];
    vec3_sub(v0, t0, center);
    vec3_sub(v1, t1, center);
    vec3_sub(v2, t2, center);

    /* Triangle edges */
    float e0[3], e1[3], e2[3];
    vec3_sub(e0, v1, v0);
    vec3_sub(e1, v2, v1);
    vec3_sub(e2, v0, v2);

    /* 9 edge cross-product axes (AABB face normals x triangle edges) */
    float axes[3];
    float aabbAxes[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    float *edges[3] = {e0, e1, e2};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            vec3_cross(axes, aabbAxes[i], edges[j]);
            /* Skip degenerate axes */
            if (fabsf(axes[0]) < 1e-8f && fabsf(axes[1]) < 1e-8f && fabsf(axes[2]) < 1e-8f)
                continue;
            if (!sat_test_axis(axes, halfSize, v0, v1, v2)) return 0;
        }
    }

    /* 3 AABB face normals */
    if (!sat_test_axis(aabbAxes[0], halfSize, v0, v1, v2)) return 0;
    if (!sat_test_axis(aabbAxes[1], halfSize, v0, v1, v2)) return 0;
    if (!sat_test_axis(aabbAxes[2], halfSize, v0, v1, v2)) return 0;

    /* 1 triangle normal */
    float triNorm[3];
    vec3_cross(triNorm, e0, e1);
    if (!sat_test_axis(triNorm, halfSize, v0, v1, v2)) return 0;

    return 1;
}

/* Returns 1 if AABB at testPos overlaps any wall triangle.
 * Triangles whose geometric normal is mostly vertical (|ny| > 0.7)
 * are classified as floor/ceiling and skipped, so the player can
 * walk freely across floor triangles without being blocked. */
static int check_wall_collision(const float *testPos, const AABB *bounds)
{
    float center[3] = {
        testPos[0] + (bounds->min[0] + bounds->max[0]) * 0.5f,
        testPos[1] + (bounds->min[1] + bounds->max[1]) * 0.5f,
        testPos[2] + (bounds->min[2] + bounds->max[2]) * 0.5f
    };
    float halfSize[3] = {
        (bounds->max[0] - bounds->min[0]) * 0.5f,
        (bounds->max[1] - bounds->min[1]) * 0.5f,
        (bounds->max[2] - bounds->min[2]) * 0.5f
    };

    for (int i = 0; i < s_triCount; i++) {
        if (s_triDisabled[i]) continue;

        const float *t = &s_tris[i * 9];

        /* Compute geometric normal */
        float e1[3], e2[3], fn[3];
        vec3_sub(e1, &t[3], &t[0]);
        vec3_sub(e2, &t[6], &t[0]);
        vec3_cross(fn, e1, e2);
        float len = sqrtf(fn[0]*fn[0] + fn[1]*fn[1] + fn[2]*fn[2]);
        if (len < 1e-8f) continue;
        fn[1] /= len;   /* only need Y component for horizontal/vertical check */

        /* |ny| > 0.7 → floor or ceiling → skip */
        if (fabsf(fn[1]) > 0.7f) continue;

        /* Wall triangle — test overlap */
        if (aabb_tri_overlap(center, halfSize, &t[0], &t[3], &t[6])) {
            return 1;
        }
    }
    return 0;
}

int physics_init(const float *tris, int triCount)
{
    physics_shutdown();

    if (!tris || triCount <= 0) return -1;

    size_t size = (size_t)triCount * 9 * sizeof(float);
    s_tris = (float *)malloc(size);
    if (!s_tris) {
        LOG_ERROR("Failed to allocate physics collision data");
        return -1;
    }
    memcpy(s_tris, tris, size);
    s_triCount = triCount;
    memset(s_triDisabled, 0, sizeof(s_triDisabled));

    LOG_INFO("Physics initialized: %d collision tris", triCount);
    return 0;
}

void physics_set_tris_enabled(int start, int count, int enabled)
{
    uint8_t val = enabled ? 0 : 1;
    for (int i = start; i < start + count && i < s_triCount; i++) {
        if (i >= 0)
            s_triDisabled[i] = val;
    }
}

void physics_move(const float *pos, const float *velocity,
                  const AABB *bounds, float *outPos)
{
    outPos[0] = pos[0];
    outPos[1] = pos[1];
    outPos[2] = pos[2];

    /* Per-axis slide: try X, then Z (skip Y — no gravity yet).
     * Only wall triangles are checked; floor/ceiling are ignored so
     * the player can walk freely across multiple floor triangles. */
    float testPos[3];

    /* Try X */
    testPos[0] = outPos[0] + velocity[0];
    testPos[1] = outPos[1];
    testPos[2] = outPos[2];
    if (!check_wall_collision(testPos, bounds)) {
        outPos[0] = testPos[0];
    }

    /* Try Z */
    testPos[0] = outPos[0];
    testPos[1] = outPos[1];
    testPos[2] = outPos[2] + velocity[2];
    if (!check_wall_collision(testPos, bounds)) {
        outPos[2] = testPos[2];
    }
}

void physics_shutdown(void)
{
    free(s_tris);
    s_tris = NULL;
    s_triCount = 0;
}
