/**
 * stalker.c — Stalker entity system implementation
 *
 * Behind-player creature that responds to sonar usage.
 * Reads sonar fire count to detect new fires, computes position
 * behind player, and reveals mesh as red outline via sonar points.
 *
 * Dependencies: sonar/sonar, sonar/raycast, world/map,
 *               render/vfx_particles, audio/spatial, audio/sound, core/log
 */

#include "world/stalker.h"
#include "sonar/sonar.h"
#include "sonar/raycast.h"
#include "world/map.h"
#include "render/vfx_particles.h"
#include "audio/spatial.h"
#include "audio/sound.h"
#include "core/log.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define MAX_STALKERS 4
#define MAX_STALKER_REVEAL_POINTS 500

typedef enum {
    STALKER_DORMANT,
    STALKER_APPROACHING,
    STALKER_VISIBLE,
    STALKER_DEPARTING
} StalkerPhase;

typedef struct {
    int           entity_index;
    StalkerPhase  phase;
    float         current_dist;
    float         start_dist;
    float         step_dist;
    float         retreat_time;
    float         reveal_timer;
    float         idle_timer;
    int           last_fire_count;
    float         appear_pos[3];
    float         depart_timer;
    char          sound_appear[64];
    char          sound_depart[64];
} StalkerState;

static StalkerState s_stalkers[MAX_STALKERS];
static int          s_stalkerCount;
static Entity      *s_entities;

/* Cached sounds */
#define MAX_STALKER_SOUNDS 8
static Sound s_stalkSounds[MAX_STALKER_SOUNDS];
static int   s_stalkSoundCount;

/* Stalker reveal color: red outline */
static const float STALKER_COLOR[3] = { 1.0f, 0.15f, 0.15f };
#define STALKER_REVEAL_TTL 2.0f
#define STALKER_DEPART_DURATION 0.8f

static const Sound *find_stalk_sound(const char *name)
{
    for (int i = 0; i < s_stalkSoundCount; i++) {
        if (strcmp(s_stalkSounds[i].name, name) == 0 && s_stalkSounds[i].buffer != 0)
            return &s_stalkSounds[i];
    }
    return NULL;
}

static const Sound *load_stalk_sound(const char *name)
{
    const Sound *existing = find_stalk_sound(name);
    if (existing) return existing;
    if (s_stalkSoundCount >= MAX_STALKER_SOUNDS) return NULL;

    char path[128];
    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    size_t len = strlen(name);
    Sound snd;
    if (len > 4 && strcmp(name + len - 4, ".ogg") == 0)
        snd = sound_load_ogg(path);
    else
        snd = sound_load_wav(path);

    if (snd.buffer == 0) return NULL;
    s_stalkSounds[s_stalkSoundCount] = snd;
    return &s_stalkSounds[s_stalkSoundCount++];
}

/**
 * Compute position behind the player at a given distance.
 * XZ only (same Y as player). Raycasts backward to avoid wall clipping.
 */
static void compute_behind_pos(const float player_pos[3],
                                const float player_forward[3],
                                float distance, float out[3])
{
    /* Flatten forward to XZ plane */
    float fwd_xz[3] = { player_forward[0], 0.0f, player_forward[2] };
    float len = sqrtf(fwd_xz[0]*fwd_xz[0] + fwd_xz[2]*fwd_xz[2]);
    if (len < 1e-6f) {
        fwd_xz[0] = 0.0f;
        fwd_xz[2] = -1.0f;
        len = 1.0f;
    }
    fwd_xz[0] /= len;
    fwd_xz[2] /= len;

    /* Behind direction */
    float behind[3] = { -fwd_xz[0], 0.0f, -fwd_xz[2] };

    /* Raycast backward to check for walls */
    float hitPos[3], hitNorm[3];
    float hitDist = raycast_cast(player_pos, behind, distance + 0.5f,
                                  hitPos, hitNorm, NULL);

    float actualDist = distance;
    if (hitDist > 0.0f && hitDist < distance + 0.5f) {
        actualDist = hitDist - 0.5f; /* 0.5m wall margin */
        if (actualDist < 1.0f) actualDist = 1.0f;
    }

    out[0] = player_pos[0] + behind[0] * actualDist;
    out[1] = player_pos[1];
    out[2] = player_pos[2] + behind[2] * actualDist;
}

/**
 * Reveal stalker mesh as red sonar points at appear_pos.
 * Translates mesh vertices from entity origin to appear position.
 */
static void stalker_reveal_mesh(const StalkerState *ss)
{
    const Entity *e = &s_entities[ss->entity_index];
    if (e->mesh_index < 0) return;

    const float *verts = NULL;
    int triCount = 0;
    if (map_get_mesh_range_tris(e->mesh_index, &verts, &triCount) < 0)
        return;

    /* Translation offset: from entity origin to appear position */
    float offset[3] = {
        ss->appear_pos[0] - e->pos[0],
        ss->appear_pos[1] - e->pos[1],
        ss->appear_pos[2] - e->pos[2]
    };

    int pointsEmitted = 0;

    for (int t = 0; t < triCount && pointsEmitted < MAX_STALKER_REVEAL_POINTS; t++) {
        const float *v0 = &verts[t * 9 + 0];
        const float *v1 = &verts[t * 9 + 3];
        const float *v2 = &verts[t * 9 + 6];

        /* 10 points per tri: 3 vertex + 3 mid-edge + 1 centroid + 3 jittered */
        float pts[10][3];
        int pc = 0;

        /* Vertices */
        pts[pc][0] = v0[0]+offset[0]; pts[pc][1] = v0[1]+offset[1]; pts[pc][2] = v0[2]+offset[2]; pc++;
        pts[pc][0] = v1[0]+offset[0]; pts[pc][1] = v1[1]+offset[1]; pts[pc][2] = v1[2]+offset[2]; pc++;
        pts[pc][0] = v2[0]+offset[0]; pts[pc][1] = v2[1]+offset[1]; pts[pc][2] = v2[2]+offset[2]; pc++;

        /* Mid-edges */
        pts[pc][0] = (v0[0]+v1[0])*0.5f+offset[0];
        pts[pc][1] = (v0[1]+v1[1])*0.5f+offset[1];
        pts[pc][2] = (v0[2]+v1[2])*0.5f+offset[2]; pc++;

        pts[pc][0] = (v1[0]+v2[0])*0.5f+offset[0];
        pts[pc][1] = (v1[1]+v2[1])*0.5f+offset[1];
        pts[pc][2] = (v1[2]+v2[2])*0.5f+offset[2]; pc++;

        pts[pc][0] = (v0[0]+v2[0])*0.5f+offset[0];
        pts[pc][1] = (v0[1]+v2[1])*0.5f+offset[1];
        pts[pc][2] = (v0[2]+v2[2])*0.5f+offset[2]; pc++;

        /* Centroid */
        pts[pc][0] = (v0[0]+v1[0]+v2[0])/3.0f+offset[0];
        pts[pc][1] = (v0[1]+v1[1]+v2[1])/3.0f+offset[1];
        pts[pc][2] = (v0[2]+v1[2]+v2[2])/3.0f+offset[2]; pc++;

        for (int j = 0; j < pc && pointsEmitted < MAX_STALKER_REVEAL_POINTS; j++) {
            SonarPoint p;
            p.pos[0] = pts[j][0];
            p.pos[1] = pts[j][1];
            p.pos[2] = pts[j][2];
            p.color[0] = STALKER_COLOR[0];
            p.color[1] = STALKER_COLOR[1];
            p.color[2] = STALKER_COLOR[2];
            p.age = 0.001f;
            p.ttl = STALKER_REVEAL_TTL;
            sonar_add_point(&p);
            pointsEmitted++;
        }
    }
}

void stalker_init(Entity *entities, int count)
{
    s_entities = entities;
    s_stalkerCount = 0;
    s_stalkSoundCount = 0;

    for (int i = 0; i < count && s_stalkerCount < MAX_STALKERS; i++) {
        if (entities[i].type != ENTITY_STALKER) continue;

        StalkerState *ss = &s_stalkers[s_stalkerCount];
        memset(ss, 0, sizeof(StalkerState));
        ss->entity_index = i;
        ss->phase = STALKER_DORMANT;
        ss->start_dist = 8.0f;
        ss->step_dist = 1.5f;
        ss->retreat_time = 15.0f;
        ss->current_dist = ss->start_dist;
        ss->last_fire_count = sonar_get_fire_count();

        Entity *e = &entities[i];

        /* Read stalker-specific fields from Entity.
         * map.c parses extras into reused fields:
         *   interval → start_dist
         *   radius   → step_dist
         *   code     → retreat_time as string
         *   sound    → sound_appear
         *   target   → sound_depart */
        if (e->interval > 0.0f) ss->start_dist = e->interval;
        if (e->radius > 0.0f) ss->step_dist = e->radius;

        /* Parse retreat_time from code field */
        if (e->code[0] != '\0') {
            float rt = (float)atof(e->code);
            if (rt > 0.0f) ss->retreat_time = rt;
        }

        ss->current_dist = ss->start_dist;

        snprintf(ss->sound_appear, sizeof(ss->sound_appear), "%s", e->sound);
        snprintf(ss->sound_depart, sizeof(ss->sound_depart), "%s", e->target);

        /* Preload sounds */
        if (ss->sound_appear[0] != '\0') load_stalk_sound(ss->sound_appear);
        if (ss->sound_depart[0] != '\0') load_stalk_sound(ss->sound_depart);

        s_stalkerCount++;
    }

    LOG_INFO("stalker: initialized %d stalkers", s_stalkerCount);
}

void stalker_update(float dt, const float player_pos[3],
                    const float player_forward[3])
{
    int currentFires = sonar_get_fire_count();

    for (int i = 0; i < s_stalkerCount; i++) {
        StalkerState *ss = &s_stalkers[i];
        Entity *e = &s_entities[ss->entity_index];
        int newFires = currentFires - ss->last_fire_count;

        switch (ss->phase) {

        case STALKER_DORMANT:
            if (newFires > 0) {
                ss->phase = STALKER_APPROACHING;
                ss->idle_timer = 0.0f;
                ss->current_dist = ss->start_dist;
                ss->last_fire_count = currentFires;
                LOG_INFO("stalker: '%s' → APPROACHING", e->id);
            }
            break;

        case STALKER_APPROACHING:
            if (newFires > 0) {
                /* Each sonar fire brings it closer */
                ss->current_dist -= ss->step_dist * (float)newFires;
                if (ss->current_dist < 1.5f) ss->current_dist = 1.5f;
                ss->idle_timer = 0.0f;
                ss->last_fire_count = currentFires;

                /* Transition to visible */
                compute_behind_pos(player_pos, player_forward,
                                   ss->current_dist, ss->appear_pos);

                ss->phase = STALKER_VISIBLE;
                ss->reveal_timer = STALKER_REVEAL_TTL;

                /* Reveal mesh + shockwave */
                stalker_reveal_mesh(ss);
                vfx_particles_spawn_shockwave(ss->appear_pos, STALKER_COLOR);

                /* Play appear sound */
                if (ss->sound_appear[0] != '\0') {
                    const Sound *snd = find_stalk_sound(ss->sound_appear);
                    if (snd) spatial_play(snd, ss->appear_pos, 1.0f, 1.0f);
                }

                LOG_INFO("stalker: '%s' → VISIBLE at dist=%.1f",
                         e->id, ss->current_dist);
            } else {
                ss->idle_timer += dt;
                if (ss->idle_timer >= ss->retreat_time) {
                    ss->phase = STALKER_DEPARTING;
                    ss->depart_timer = STALKER_DEPART_DURATION;

                    /* Compute depart position */
                    compute_behind_pos(player_pos, player_forward,
                                       ss->current_dist, ss->appear_pos);

                    /* Sand collapse VFX */
                    vfx_particles_spawn_collapse(ss->appear_pos,
                                                  STALKER_COLOR);

                    /* Play depart sound */
                    if (ss->sound_depart[0] != '\0') {
                        const Sound *snd = find_stalk_sound(ss->sound_depart);
                        if (snd)
                            spatial_play(snd, ss->appear_pos, 1.0f, 1.0f);
                    }

                    LOG_INFO("stalker: '%s' → DEPARTING", e->id);
                }
                ss->last_fire_count = currentFires;
            }
            break;

        case STALKER_VISIBLE:
            ss->reveal_timer -= dt;
            if (newFires > 0) {
                /* New sonar use while visible: close distance, re-reveal */
                ss->current_dist -= ss->step_dist * (float)newFires;
                if (ss->current_dist < 1.5f) ss->current_dist = 1.5f;

                compute_behind_pos(player_pos, player_forward,
                                   ss->current_dist, ss->appear_pos);
                stalker_reveal_mesh(ss);
                vfx_particles_spawn_shockwave(ss->appear_pos, STALKER_COLOR);

                ss->reveal_timer = STALKER_REVEAL_TTL;
                ss->idle_timer = 0.0f;
                ss->last_fire_count = currentFires;
            } else {
                ss->idle_timer += dt;
            }

            if (ss->reveal_timer <= 0.0f) {
                /* Reveal expired, go back to approaching */
                ss->phase = STALKER_APPROACHING;
                ss->last_fire_count = currentFires;

                /* Check if should retreat instead */
                if (ss->idle_timer >= ss->retreat_time) {
                    ss->phase = STALKER_DEPARTING;
                    ss->depart_timer = STALKER_DEPART_DURATION;
                    vfx_particles_spawn_collapse(ss->appear_pos,
                                                  STALKER_COLOR);
                    if (ss->sound_depart[0] != '\0') {
                        const Sound *snd = find_stalk_sound(ss->sound_depart);
                        if (snd)
                            spatial_play(snd, ss->appear_pos, 1.0f, 1.0f);
                    }
                }
            }
            break;

        case STALKER_DEPARTING:
            ss->depart_timer -= dt;
            if (ss->depart_timer <= 0.0f) {
                ss->phase = STALKER_DORMANT;
                ss->current_dist = ss->start_dist;
                ss->idle_timer = 0.0f;
                ss->last_fire_count = currentFires;
                LOG_INFO("stalker: '%s' → DORMANT", e->id);
            }
            break;
        }
    }
}

void stalker_shutdown(void)
{
    for (int i = 0; i < s_stalkSoundCount; i++)
        sound_destroy(&s_stalkSounds[i]);
    s_stalkSoundCount = 0;
    s_stalkerCount = 0;
}
