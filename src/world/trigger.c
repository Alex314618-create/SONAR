/**
 * trigger.c — Static trigger entity system implementation
 *
 * Zone mode:  player enters radius of ANY trigger with zone_id X →
 *             all triggers in that zone activate with staggered delays.
 * Step mode:  closest unfired trigger within radius reveals on approach.
 *
 * Reveal: reads tris from MeshRange, injects sonar points with
 * 10 points/tri (3 vertex + 3 mid-edge + 1 centroid) + optional star field.
 *
 * Dependencies: sonar/sonar, world/map, audio/spatial, audio/sound, core/log
 */

#include "world/trigger.h"
#include "sonar/sonar.h"
#include "world/map.h"
#include "audio/spatial.h"
#include "audio/sound.h"
#include "core/log.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define MAX_TRIGGERS 32
#define MAX_POINTS_PER_TRIGGER 500

/* ── LCG RNG (same algorithm as sonar.c) ───────────────────── */
static unsigned int s_trigSeed = 12345;

static float trig_randf(void)
{
    s_trigSeed = s_trigSeed * 1664525u + 1013904223u;
    return (float)(s_trigSeed >> 8) / 16777216.0f;
}

typedef enum { TRIGGER_MODE_ZONE, TRIGGER_MODE_STEP } TriggerMode;

typedef struct {
    int           entity_index;
    int           fired;
    float         delay;
    float         delay_timer;
    float         radius;
    char          zone_id[32];
    TriggerMode   mode;
    int           zone_activated;
} TriggerState;

static TriggerState s_triggers[MAX_TRIGGERS];
static int          s_triggerCount;
static Entity      *s_entities;
static int          s_entityCount;

/* Cached trigger sounds */
#define MAX_TRIGGER_SOUNDS 16
static Sound s_trigSounds[MAX_TRIGGER_SOUNDS];
static int   s_trigSoundCount;

static const Sound *find_trigger_sound(const char *name)
{
    for (int i = 0; i < s_trigSoundCount; i++) {
        if (strcmp(s_trigSounds[i].name, name) == 0 && s_trigSounds[i].buffer != 0)
            return &s_trigSounds[i];
    }
    return NULL;
}

static const Sound *load_trigger_sound(const char *name)
{
    const Sound *existing = find_trigger_sound(name);
    if (existing) return existing;
    if (s_trigSoundCount >= MAX_TRIGGER_SOUNDS) return NULL;

    char path[128];
    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    size_t len = strlen(name);
    Sound snd;
    if (len > 4 && strcmp(name + len - 4, ".ogg") == 0)
        snd = sound_load_ogg(path);
    else
        snd = sound_load_wav(path);

    if (snd.buffer == 0) return NULL;
    s_trigSounds[s_trigSoundCount] = snd;
    return &s_trigSounds[s_trigSoundCount++];
}

/**
 * Inject sonar points from MeshRange triangles.
 * 10 points per triangle: 3 vertices + 3 mid-edges + 1 centroid = 7,
 * plus 3 extra jittered centroid points for density (total ~10).
 */
static void trigger_reveal_mesh(const Entity *e)
{
    if (e->mesh_index < 0) return;

    const float *verts = NULL;
    int triCount = 0;
    if (map_get_mesh_range_tris(e->mesh_index, &verts, &triCount) < 0)
        return;

    /* Default cyan color for triggers */
    float baseColor[3] = { 0.0f, 0.85f, 0.75f };
    float ttl = e->ttl;  /* 0 = permanent */
    float age = (ttl > 0.0f || ttl == 0.0f) ? 0.001f : 0.001f;

    /* For permanent points (ttl==0), set age=0 (permanent in sonar system) */
    if (ttl == 0.0f) age = 0.0f;

    int pointsEmitted = 0;

    for (int t = 0; t < triCount && pointsEmitted < MAX_POINTS_PER_TRIGGER; t++) {
        const float *v0 = &verts[t * 9 + 0];
        const float *v1 = &verts[t * 9 + 3];
        const float *v2 = &verts[t * 9 + 6];

        /* 3 vertex points */
        float pts[10][3];
        int pc = 0;

        pts[pc][0] = v0[0]; pts[pc][1] = v0[1]; pts[pc][2] = v0[2]; pc++;
        pts[pc][0] = v1[0]; pts[pc][1] = v1[1]; pts[pc][2] = v1[2]; pc++;
        pts[pc][0] = v2[0]; pts[pc][1] = v2[1]; pts[pc][2] = v2[2]; pc++;

        /* 3 mid-edge points */
        pts[pc][0] = (v0[0]+v1[0])*0.5f;
        pts[pc][1] = (v0[1]+v1[1])*0.5f;
        pts[pc][2] = (v0[2]+v1[2])*0.5f; pc++;

        pts[pc][0] = (v1[0]+v2[0])*0.5f;
        pts[pc][1] = (v1[1]+v2[1])*0.5f;
        pts[pc][2] = (v1[2]+v2[2])*0.5f; pc++;

        pts[pc][0] = (v0[0]+v2[0])*0.5f;
        pts[pc][1] = (v0[1]+v2[1])*0.5f;
        pts[pc][2] = (v0[2]+v2[2])*0.5f; pc++;

        /* Centroid */
        float cx = (v0[0]+v1[0]+v2[0]) / 3.0f;
        float cy = (v0[1]+v1[1]+v2[1]) / 3.0f;
        float cz = (v0[2]+v1[2]+v2[2]) / 3.0f;
        pts[pc][0] = cx; pts[pc][1] = cy; pts[pc][2] = cz; pc++;

        /* 3 jittered centroid for density */
        for (int j = 0; j < 3 && pc < 10; j++) {
            pts[pc][0] = cx + (trig_randf() - 0.5f) * 0.05f;
            pts[pc][1] = cy + (trig_randf() - 0.5f) * 0.05f;
            pts[pc][2] = cz + (trig_randf() - 0.5f) * 0.05f;
            pc++;
        }

        for (int j = 0; j < pc && pointsEmitted < MAX_POINTS_PER_TRIGGER; j++) {
            SonarPoint p;
            p.pos[0] = pts[j][0];
            p.pos[1] = pts[j][1];
            p.pos[2] = pts[j][2];

            /* Star field: 70% permanent, 30% brief flash (ttl 0.3s) */
            if (ttl == 0.0f && trig_randf() < 0.3f) {
                /* Flash point: bright, short-lived */
                p.color[0] = baseColor[0] * 1.4f;
                p.color[1] = baseColor[1] * 1.4f;
                p.color[2] = baseColor[2] * 1.4f;
                if (p.color[0] > 1.0f) p.color[0] = 1.0f;
                if (p.color[1] > 1.0f) p.color[1] = 1.0f;
                if (p.color[2] > 1.0f) p.color[2] = 1.0f;
                p.age = 0.001f;
                p.ttl = 0.3f;
            } else {
                p.color[0] = baseColor[0];
                p.color[1] = baseColor[1];
                p.color[2] = baseColor[2];
                p.age = age;
                p.ttl = ttl;
            }

            sonar_add_point(&p);
            pointsEmitted++;
        }
    }

    LOG_INFO("trigger: revealed %d points for entity '%s'",
             pointsEmitted, e->id);
}

static float dist_xz(const float a[3], const float b[3])
{
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dz*dz);
}

void trigger_init(Entity *entities, int count)
{
    s_entities = entities;
    s_entityCount = count;
    s_triggerCount = 0;
    s_trigSoundCount = 0;

    for (int i = 0; i < count && s_triggerCount < MAX_TRIGGERS; i++) {
        if (entities[i].type != ENTITY_TRIGGER) continue;

        TriggerState *ts = &s_triggers[s_triggerCount];
        memset(ts, 0, sizeof(TriggerState));
        ts->entity_index = i;
        ts->fired = 0;
        ts->zone_activated = 0;

        /* Read trigger-specific fields from Entity.
         * map.c parses extras into reused fields:
         *   target  → zone_id
         *   interval → delay
         *   code    → mode ("step" or "zone")
         *   radius  → proximity radius
         *   sound   → trigger sound */
        ts->radius = 3.0f;
        ts->delay = 0.0f;
        ts->mode = TRIGGER_MODE_ZONE;
        ts->zone_id[0] = '\0';

        Entity *e = &entities[i];
        if (e->target[0] != '\0')
            snprintf(ts->zone_id, sizeof(ts->zone_id), "%s", e->target);
        ts->delay = e->interval;
        if (strcmp(e->code, "step") == 0)
            ts->mode = TRIGGER_MODE_STEP;
        if (e->radius > 0.0f)
            ts->radius = e->radius;

        /* Preload sound */
        if (e->sound[0] != '\0')
            load_trigger_sound(e->sound);

        s_triggerCount++;
    }

    LOG_INFO("trigger: initialized %d triggers", s_triggerCount);
}

void trigger_update(float dt, const float player_pos[3])
{
    /* Zone mode: check if player entered any zone trigger's radius */
    for (int i = 0; i < s_triggerCount; i++) {
        TriggerState *ts = &s_triggers[i];
        if (ts->fired) continue;

        Entity *e = &s_entities[ts->entity_index];
        float d = dist_xz(player_pos, e->pos);

        if (ts->mode == TRIGGER_MODE_ZONE && ts->zone_id[0] != '\0') {
            /* Check if player is in radius of THIS trigger */
            if (d < ts->radius && !ts->zone_activated) {
                /* Activate ALL triggers with same zone_id */
                for (int j = 0; j < s_triggerCount; j++) {
                    TriggerState *other = &s_triggers[j];
                    if (!other->fired && !other->zone_activated &&
                        other->mode == TRIGGER_MODE_ZONE &&
                        strcmp(other->zone_id, ts->zone_id) == 0) {
                        other->zone_activated = 1;
                        other->delay_timer = other->delay;
                    }
                }
            }
        } else if (ts->mode == TRIGGER_MODE_STEP) {
            /* Step mode: reveal closest unfired when player in radius */
            if (d < ts->radius) {
                ts->zone_activated = 1;
                ts->delay_timer = 0.0f;
            }
        } else {
            /* Standalone zone trigger (no zone_id) */
            if (d < ts->radius && !ts->zone_activated) {
                ts->zone_activated = 1;
                ts->delay_timer = ts->delay;
            }
        }
    }

    /* Tick delay timers and fire */
    for (int i = 0; i < s_triggerCount; i++) {
        TriggerState *ts = &s_triggers[i];
        if (ts->fired || !ts->zone_activated) continue;

        ts->delay_timer -= dt;
        if (ts->delay_timer > 0.0f) continue;

        /* Fire! */
        ts->fired = 1;
        Entity *e = &s_entities[ts->entity_index];

        trigger_reveal_mesh(e);

        /* Play sound if configured */
        if (e->sound[0] != '\0') {
            const Sound *snd = find_trigger_sound(e->sound);
            if (snd) spatial_play(snd, e->pos, 1.0f, 1.0f);
        }
    }
}

void trigger_shutdown(void)
{
    for (int i = 0; i < s_trigSoundCount; i++)
        sound_destroy(&s_trigSounds[i]);
    s_trigSoundCount = 0;
    s_triggerCount = 0;
}
