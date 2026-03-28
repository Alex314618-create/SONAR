/**
 * sonar.c — Sonar firing logic implementation
 *
 * Rays are distributed in a cone using a Fibonacci spiral (pulse) or
 * random angles (continuous). Hit points are stored in a circular buffer
 * with scatter variants (wall, floor-at-base, ceiling, floor-along-ray)
 * ported from the 2D prototype (doomer/sonar.c).
 */

#include "sonar/sonar.h"
#include "sonar/raycast.h"
#include "sonar/energy.h"
#include "world/map.h"
#include "core/log.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Mode parameters ────────────────────────────────────────── */
#define WIDE_RAYS         120
#define WIDE_SPREAD_DEG   30.0f
#define WIDE_COST         20.0f
#define WIDE_COOLDOWN     0.4f
#define WIDE_DENSITY      1.0f

#define FOCUSED_RAYS      250
#define FOCUSED_SPREAD_DEG 12.0f
#define FOCUSED_COST      15.0f
#define FOCUSED_COOLDOWN  0.25f
#define FOCUSED_DENSITY   1.0f

#define CONTINUOUS_RATE_WIDE    320.0f
#define CONTINUOUS_RATE_FOCUSED 720.0f
#define CONTINUOUS_DRAIN  20.0f
#define CONT_WIDE_DENSITY    0.6f
#define CONT_FOCUSED_DENSITY 1.2f

#define PASSIVE_RAYS      35
#define PASSIVE_RANGE     2.5f
#define PASSIVE_INTERVAL  2.0f

/* Floor and ceiling Y in world space (must match map.c) */
#define FLOOR_Y   0.0f
#define CEILING_Y 3.0f

/* ── LCG RNG (ported from doomer/utils.c) ───────────────────── */
static unsigned int s_seed = 42;

static float randf_local(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return (float)(s_seed >> 8) / 16777216.0f;
}

/* ── Color ranges (GDD section 6.2) ─────────────────────────── */
typedef struct { float rMin, rMax, gMin, gMax, bMin, bMax; } ColorRangeF;

static const ColorRangeF CR_WALL    = {  0.f/255,   0.f/255,  180.f/255, 255.f/255,  160.f/255, 220.f/255 };
static const ColorRangeF CR_FLOOR   = {  0.f/255,   0.f/255,  160.f/255, 240.f/255,  120.f/255, 180.f/255 };
static const ColorRangeF CR_CEILING = { 30.f/255,  70.f/255,  120.f/255, 180.f/255,  180.f/255, 255.f/255 };

static void color_from_range(const ColorRangeF *cr, float *out)
{
    out[0] = cr->rMin + randf_local() * (cr->rMax - cr->rMin);
    out[1] = cr->gMin + randf_local() * (cr->gMax - cr->gMin);
    out[2] = cr->bMin + randf_local() * (cr->bMax - cr->bMin);
}

/* ── Explored grid ───────────────────────────────────────────── */
static int s_explored[EXPLORE_GRID_H * EXPLORE_GRID_W];

const int *sonar_get_explored_grid(void) { return s_explored; }

/* ── Point buffer ────────────────────────────────────────────── */
static SonarPoint s_points[MAX_SONAR_POINTS];
static int        s_pointCount;
static int        s_writeHead;
static int        s_frameStart;

static void spawn_point(const float pos[3], const float color[3])
{
    SonarPoint *p = &s_points[s_writeHead];
    p->pos[0]   = pos[0];
    p->pos[1]   = pos[1];
    p->pos[2]   = pos[2];
    p->color[0] = color[0];
    p->color[1] = color[1];
    p->color[2] = color[2];
    p->age      = 0.0f;
    p->ttl      = 0.0f;

    s_writeHead = (s_writeHead + 1) % MAX_SONAR_POINTS;
    if (s_pointCount < MAX_SONAR_POINTS)
        s_pointCount++;

    /* Mark explored grid */
    int gx = (int)((pos[0] - EXPLORE_ORIGIN_X) / EXPLORE_CELL_SIZE);
    int gz = (int)((pos[2] - EXPLORE_ORIGIN_Z) / EXPLORE_CELL_SIZE);
    if (gx >= 0 && gx < EXPLORE_GRID_W && gz >= 0 && gz < EXPLORE_GRID_H)
        s_explored[gz * EXPLORE_GRID_W + gx] = 1;
}

/* ── Gun / sonar state ───────────────────────────────────────── */
static SonarMode  s_mode;
static float      s_cooldownTimer;
static float      s_continuousAccum;
static float      s_passiveTimer;
static int        s_fireCount;

/* Cached player state for passive ping */
static float s_playerPos[3];
static float s_playerForward[3];
static float s_playerUp[3];
static float s_playerRight[3];

/* ── Clue color helper ────────────────────────────────────────── */
static float clamp01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void clue_disperse(const float base[3], float *out)
{
    /* Red clue (base ≈ [1, 0.15, 0.15]): wide red-orange dispersion
     * so individual points alternate between red and orange, giving
     * a particulate, non-uniform appearance. */
    if (base[0] > 0.8f && base[1] < 0.4f && base[2] < 0.4f) {
        out[0] = 1.0f;
        out[1] = clamp01(0.30f + randf_local() * 0.25f);  /* 0.30–0.55 */
        out[2] = clamp01(0.05f + randf_local() * 0.10f);  /* 0.05–0.15 */
        return;
    }

    /* Default dispersion for other clue colors */
    out[0] = clamp01(base[0] + (randf_local() - 0.5f) * 0.16f);
    out[1] = clamp01(base[1] + (randf_local() - 0.5f) * 0.16f);
    out[2] = clamp01(base[2] + (randf_local() - 0.5f) * 0.16f);
}

/* ── Point scatter logic (ported from cast_sonar_ray_d) ──────── */
/* clueColor: if non-NULL, use this as base color with dispersion */
static void spawn_wall_scatter(const float hitPos[3], float density,
                               const float *clueColor)
{
    /* a) Wall point — always, +/-0.04 XZ jitter */
    float wallCol[3];
    if (clueColor)
        clue_disperse(clueColor, wallCol);
    else
        color_from_range(&CR_WALL, wallCol);
    {
        float wp[3] = {
            hitPos[0] + (randf_local() * 0.08f - 0.04f),
            hitPos[1],
            hitPos[2] + (randf_local() * 0.08f - 0.04f)
        };
        spawn_point(wp, wallCol);
    }

    /* b) Second wall point — 50% * density, +/-0.05 XZ jitter */
    if (randf_local() < 0.5f * density) {
        float wp2[3] = {
            hitPos[0] + (randf_local() * 0.10f - 0.05f),
            hitPos[1],
            hitPos[2] + (randf_local() * 0.10f - 0.05f)
        };
        float wallCol2[3];
        if (clueColor)
            clue_disperse(clueColor, wallCol2);
        else
            color_from_range(&CR_WALL, wallCol2);
        spawn_point(wp2, wallCol2);
    }

    /* c) Floor point near wall base — 60% * density, XZ +/-0.15
     * Skip for clue surfaces and focused mode (avoids sandwich lines) */
    if (!clueColor && s_mode != SONAR_MODE_FOCUSED && randf_local() < 0.6f * density) {
        float fcol[3];
        color_from_range(&CR_FLOOR, fcol);
        float fp[3] = {
            hitPos[0] + (randf_local() * 0.30f - 0.15f),
            FLOOR_Y,
            hitPos[2] + (randf_local() * 0.30f - 0.15f)
        };
        spawn_point(fp, fcol);
    }

    /* d) Ceiling point — 40% * density, XZ +/-0.15
     * Skip for clue surfaces and focused mode (avoids sandwich lines) */
    if (!clueColor && s_mode != SONAR_MODE_FOCUSED && randf_local() < 0.4f * density) {
        float ccol[3];
        color_from_range(&CR_CEILING, ccol);
        float cp[3] = {
            hitPos[0] + (randf_local() * 0.30f - 0.15f),
            CEILING_Y,
            hitPos[2] + (randf_local() * 0.30f - 0.15f)
        };
        spawn_point(cp, ccol);
    }

    /* e) Clue surface density boost: 6 extra scatter points so clue
     * shapes appear denser and more continuous than plain walls. */
    if (clueColor) {
        for (int extra = 0; extra < 6; extra++) {
            float ep[3] = {
                hitPos[0] + (randf_local() - 0.5f) * 0.06f,
                hitPos[1] + (randf_local() - 0.5f) * 0.06f,
                hitPos[2] + (randf_local() - 0.5f) * 0.06f
            };
            SonarPoint p;
            p.pos[0] = ep[0]; p.pos[1] = ep[1]; p.pos[2] = ep[2];
            clue_disperse(clueColor, p.color);
            p.age = 0.0f;
            sonar_add_point(&p);
        }
    }
}

/* Sample floor points along a ray trajectory. */
static void spawn_floor_along_ray(const float *origin, const float *dir,
                                  float hitDist, float density)
{
    if (dir[1] > 0.3f)
        return;

    float maxD = (hitDist > 0.0f) ? hitDist : SONAR_MAX_RANGE;
    if (maxD > 10.0f) maxD = 10.0f;

    float d = 0.5f + randf_local() * 0.3f;
    while (d < maxD) {
        if (randf_local() < 0.6f) {
            float fcol[3];
            color_from_range(&CR_FLOOR, fcol);
            float fp[3] = {
                origin[0] + dir[0] * d + (randf_local() * 0.20f - 0.10f),
                FLOOR_Y,
                origin[2] + dir[2] * d + (randf_local() * 0.20f - 0.10f)
            };
            spawn_point(fp, fcol);
        }
        d += (0.3f + 0.5f * (d / maxD)) * (1.0f / density) + randf_local() * 0.3f;
    }
}

/* ── Ray cone distribution (Fibonacci spiral) ───────────────── */
static int fire_rays(const float *origin, const float *forward,
                     const float *up, const float *right,
                     int numRays, float spreadDeg, float density)
{
    float spreadRad = spreadDeg * (float)(M_PI / 180.0);
    int hits = 0;

    for (int i = 0; i < numRays; i++) {
        float t      = (float)i / (float)numRays;
        float angle  = 2.0f * (float)M_PI * (float)i * 0.618033988749895f;
        float radius = spreadRad * sqrtf(t);

        float offX = cosf(angle) * radius;
        float offY = sinf(angle) * radius;

        float dir[3] = {
            forward[0] + right[0] * offX + up[0] * offY,
            forward[1] + right[1] * offX + up[1] * offY,
            forward[2] + right[2] * offX + up[2] * offY
        };

        float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len < 1e-6f) continue;
        dir[0] /= len;
        dir[1] /= len;
        dir[2] /= len;

        float hitPos[3], hitNorm[3];
        int triIdx = -1;
        float dist = raycast_cast(origin, dir, SONAR_MAX_RANGE, hitPos, hitNorm, &triIdx);

        if (dist > 0.0f) {
            float clueRgb[3];
            const float *cluePtr = NULL;
            if (triIdx >= 0 && map_get_clue_color(triIdx, clueRgb))
                cluePtr = clueRgb;
            spawn_wall_scatter(hitPos, density, cluePtr);
            hits++;
        }
        spawn_floor_along_ray(origin, dir, dist, density);
    }

    return hits;
}

/* ── Passive ping — 35 rays, 360 deg XZ sweep, 2.5 unit range ─ */
static void sonar_passive_ping(void)
{
    for (int i = 0; i < PASSIVE_RAYS; i++) {
        float angle = (float)i / (float)PASSIVE_RAYS * 2.0f * (float)M_PI;
        angle += (randf_local() - 0.5f) * 0.3f;

        float dir[3] = { cosf(angle), 0.0f, sinf(angle) };

        float hitPos[3], hitNorm[3];
        float dist = raycast_cast(s_playerPos, dir, PASSIVE_RANGE, hitPos, hitNorm, NULL);
        if (dist > 0.0f) {
            float col[3];
            color_from_range(&CR_WALL, col);
            spawn_point(hitPos, col);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────── */

void sonar_init(void)
{
    memset(s_points, 0, sizeof(s_points));
    memset(s_explored, 0, sizeof(s_explored));
    s_pointCount      = 0;
    s_writeHead       = 0;
    s_frameStart      = 0;
    s_mode            = SONAR_MODE_WIDE;
    s_cooldownTimer   = 0.0f;
    s_continuousAccum = 0.0f;
    s_passiveTimer    = 0.0f;
    s_fireCount       = 0;
    memset(s_playerPos,     0, sizeof(s_playerPos));
    memset(s_playerForward, 0, sizeof(s_playerForward));
    memset(s_playerUp,      0, sizeof(s_playerUp));
    memset(s_playerRight,   0, sizeof(s_playerRight));
    s_playerForward[2] = 1.0f;
    s_playerUp[1]      = 1.0f;
    s_playerRight[0]   = 1.0f;
}

void sonar_frame_begin(void)
{
    s_frameStart = s_writeHead;
}

int sonar_get_frame_start(void)
{
    return s_frameStart;
}

int sonar_get_write_head(void)
{
    return s_writeHead;
}

void sonar_toggle_mode(void)
{
    s_mode = (s_mode == SONAR_MODE_WIDE) ? SONAR_MODE_FOCUSED : SONAR_MODE_WIDE;
}

SonarMode sonar_get_mode(void)
{
    return s_mode;
}

void sonar_set_player(const float *pos, const float *forward,
                      const float *up, const float *right)
{
    s_playerPos[0] = pos[0];     s_playerPos[1] = pos[1];     s_playerPos[2] = pos[2];
    s_playerForward[0] = forward[0]; s_playerForward[1] = forward[1]; s_playerForward[2] = forward[2];
    s_playerUp[0] = up[0];      s_playerUp[1] = up[1];      s_playerUp[2] = up[2];
    s_playerRight[0] = right[0]; s_playerRight[1] = right[1]; s_playerRight[2] = right[2];
}

int sonar_fire_pulse(const float *origin, const float *forward,
                     const float *up, const float *right)
{
    if (s_cooldownTimer > 0.0f)
        return 0;

    int   rays;
    float spread, cost, cooldown;

    if (s_mode == SONAR_MODE_WIDE) {
        rays     = WIDE_RAYS;
        spread   = WIDE_SPREAD_DEG;
        cost     = WIDE_COST;
        cooldown = WIDE_COOLDOWN;
    } else {
        rays     = FOCUSED_RAYS;
        spread   = FOCUSED_SPREAD_DEG;
        cost     = FOCUSED_COST;
        cooldown = FOCUSED_COOLDOWN;
    }

    if (!energy_spend(cost))
        return 0;

    s_cooldownTimer = cooldown;
    s_fireCount++;
    return fire_rays(origin, forward, up, right, rays, spread, 1.0f);
}

int sonar_fire_continuous(const float *origin, const float *forward,
                          const float *up, const float *right, float dt)
{
    float drain = CONTINUOUS_DRAIN * dt;
    if (!energy_spend(drain))
        return 0;

    s_fireCount++;

    float rate = (s_mode == SONAR_MODE_WIDE) ? CONTINUOUS_RATE_WIDE : CONTINUOUS_RATE_FOCUSED;
    s_continuousAccum += rate * dt;
    int raysThisFrame = (int)s_continuousAccum;
    s_continuousAccum -= (float)raysThisFrame;

    if (raysThisFrame < 1)
        return 0;

    float spread  = (s_mode == SONAR_MODE_WIDE) ? WIDE_SPREAD_DEG  : FOCUSED_SPREAD_DEG;
    float density = (s_mode == SONAR_MODE_WIDE) ? CONT_WIDE_DENSITY : CONT_FOCUSED_DENSITY;

    /* Random ray angles (not Fibonacci) for organic scanning feel */
    float spreadRad = spread * (float)(M_PI / 180.0);
    int hits = 0;
    for (int i = 0; i < raysThisFrame; i++) {
        float phi = randf_local() * 2.0f * (float)M_PI;
        float r   = sqrtf(randf_local()) * spreadRad;
        float az  = r * cosf(phi);
        float el  = r * sinf(phi);
        float dir[3] = {
            forward[0] + right[0]*az + up[0]*el,
            forward[1] + right[1]*az + up[1]*el,
            forward[2] + right[2]*az + up[2]*el
        };
        float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len < 1e-6f) continue;
        dir[0] /= len; dir[1] /= len; dir[2] /= len;

        float hitPos[3], hitNorm[3];
        int triIdx = -1;
        float dist = raycast_cast(origin, dir, SONAR_MAX_RANGE, hitPos, hitNorm, &triIdx);
        if (dist > 0.0f) {
            /* One-time debug: log first triangle hit for clue diagnosis */
            static int loggedOnce = 0;
            if (!loggedOnce && triIdx >= 0) {
                float testRgb[3];
                int isClue = map_get_clue_color(triIdx, testRgb);
                LOG_INFO("sonar hit tri=%d isClue=%d rgb=[%.2f,%.2f,%.2f]",
                         triIdx, isClue, testRgb[0], testRgb[1], testRgb[2]);
                loggedOnce = 1;
            }

            float clueRgb[3];
            const float *cluePtr = NULL;
            if (triIdx >= 0 && map_get_clue_color(triIdx, clueRgb))
                cluePtr = clueRgb;
            spawn_wall_scatter(hitPos, density, cluePtr);
            hits++;
        }
        if (s_mode == SONAR_MODE_WIDE)
            spawn_floor_along_ray(origin, dir, dist, density);
    }
    return hits;
}

void sonar_update(float dt)
{
    if (s_cooldownTimer > 0.0f) {
        s_cooldownTimer -= dt;
        if (s_cooldownTimer < 0.0f)
            s_cooldownTimer = 0.0f;
    }

    /* Passive ping every 2 seconds using cached player pose */
    s_passiveTimer += dt;
    if (s_passiveTimer >= PASSIVE_INTERVAL) {
        s_passiveTimer = 0.0f;
        sonar_passive_ping();
    }

    /* Age and TTL-cull transient points.
     * Points with age == 0.0f are permanent (normal sonar) — left untouched.
     * Points with age > 0.0f are transient — aged and marked dead when
     * they exceed their TTL (per-point ttl if >0, else default 0.8s). */
    for (int i = 0; i < s_pointCount; i++) {
        if (s_points[i].age > 0.0f) {
            s_points[i].age += dt;
            float limit = s_points[i].ttl > 0.0f ? s_points[i].ttl : 0.8f;
            if (s_points[i].age >= limit)
                s_points[i].age = -1.0f;
        }
    }
}

void sonar_add_point(const SonarPoint *p)
{
    s_points[s_writeHead] = *p;
    s_writeHead = (s_writeHead + 1) % MAX_SONAR_POINTS;
    if (s_pointCount < MAX_SONAR_POINTS)
        s_pointCount++;
}

const SonarPoint *sonar_get_points(void)
{
    return s_points;
}

int sonar_get_point_count(void)
{
    return s_pointCount;
}

void sonar_clear(void)
{
    s_pointCount = 0;
    s_writeHead  = 0;
}

int sonar_get_fire_count(void)
{
    return s_fireCount;
}

void sonar_shutdown(void)
{
    sonar_clear();
    s_cooldownTimer   = 0.0f;
    s_continuousAccum = 0.0f;
    s_passiveTimer    = 0.0f;
}
