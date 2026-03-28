/**
 * entity.c — Lightweight typed entity system implementation
 *
 * Handles sound timer ticking, passive sonar reveal for creatures,
 * and player interaction dispatch (dial / door).
 *
 * Dependencies: sonar/sonar, world/map, audio/spatial, core/log
 */

#include "world/entity.h"
#include "sonar/sonar.h"
#include "world/map.h"
#include "world/physics.h"
#include "sonar/raycast.h"
#include "audio/spatial.h"
#include "audio/sound.h"
#include "core/log.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* Maximum vertices to project for passive creature reveal */
#define MAX_REVEAL_VERTS 200

/* Maximum entities with sounds that we cache buffers for */
#define MAX_ENTITY_SOUNDS 32

/* Cached sound buffers loaded during entity_init */
static Sound s_sounds[MAX_ENTITY_SOUNDS];
static int   s_soundCount;

/* Creature sonar point color: orange per ADR 0007 */
static const float CREATURE_COLOR[3] = { 1.0f, 0.55f, 0.1f };

/* Initial age for creature points — triggers TTL logic (age > 0) */
#define CREATURE_POINT_AGE 0.001f

/**
 * Inject orange sonar points for a creature's collision mesh vertices.
 * Called when the creature emits its ambient sound.
 */
static void passive_reveal(const Entity *e)
{
    const float *verts = NULL;
    int triCount = 0;

    /* Use per-entity MeshRange if available, else fall back to collision */
    if (e->mesh_index >= 0) {
        if (map_get_mesh_range_tris(e->mesh_index, &verts, &triCount) < 0)
            return;
    } else {
        verts = map_get_collision_verts();
        triCount = map_get_collision_tri_count();
    }

    if (!verts || triCount <= 0)
        return;

    int totalVerts = triCount * 3;
    int processed = 0;

    for (int i = 0; i < totalVerts && processed < MAX_REVEAL_VERTS; i++) {
        SonarPoint p;
        p.pos[0] = verts[i * 3 + 0];
        p.pos[1] = verts[i * 3 + 1];
        p.pos[2] = verts[i * 3 + 2];
        p.color[0] = CREATURE_COLOR[0];
        p.color[1] = CREATURE_COLOR[1];
        p.color[2] = CREATURE_COLOR[2];
        p.age = CREATURE_POINT_AGE;
        p.ttl = e->ttl;   /* 0 = sonar default 0.8s, >0 = custom */
        sonar_add_point(&p);
        processed++;
    }
}

/**
 * Find a cached sound by name, or return NULL if not loaded.
 */
static const Sound *find_cached_sound(const char *name)
{
    for (int i = 0; i < s_soundCount; i++) {
        if (strcmp(s_sounds[i].name, name) == 0 && s_sounds[i].buffer != 0)
            return &s_sounds[i];
    }
    return NULL;
}

/**
 * Load and cache a sound by its asset-relative path (e.g. "creature_growl.wav").
 * Builds the full path as "assets/sounds/<name>".
 */
static const Sound *load_and_cache_sound(const char *name)
{
    const Sound *existing = find_cached_sound(name);
    if (existing)
        return existing;

    if (s_soundCount >= MAX_ENTITY_SOUNDS) {
        LOG_ERROR("entity: sound cache full, cannot load %s", name);
        return NULL;
    }

    char path[128];
    snprintf(path, sizeof(path), "assets/sounds/%s", name);

    /* Determine format by extension */
    size_t len = strlen(name);
    Sound snd;
    if (len > 4 && strcmp(name + len - 4, ".ogg") == 0)
        snd = sound_load_ogg(path);
    else
        snd = sound_load_wav(path);

    if (snd.buffer == 0) {
        LOG_ERROR("entity: failed to load sound %s", path);
        return NULL;
    }

    s_sounds[s_soundCount] = snd;
    return &s_sounds[s_soundCount++];
}

void entity_init(Entity *entities, int count)
{
    s_soundCount = 0;

    for (int i = 0; i < count; i++) {
        entities[i].sound_timer = entities[i].interval;

        /* Pre-load sounds referenced by entities */
        if (entities[i].sound[0] != '\0')
            load_and_cache_sound(entities[i].sound);
    }
}

void entity_update(float dt, Entity *entities, int count)
{
    for (int i = 0; i < count; i++) {
        Entity *e = &entities[i];

        /* Trigger/Stalker have their own update systems */
        if (e->type == ENTITY_TRIGGER || e->type == ENTITY_STALKER)
            continue;

        if (e->interval <= 0.0f)
            continue;

        e->sound_timer -= dt;
        if (e->sound_timer > 0.0f)
            continue;

        /* Timer expired — play sound if configured */
        if (e->sound[0] != '\0') {
            const Sound *snd = find_cached_sound(e->sound);
            if (snd)
                spatial_play(snd, e->pos, 1.0f, 1.0f);
        }

        /* Reset timer */
        e->sound_timer = e->interval;

        /* Creature passive sonar reveal */
        if (e->type == ENTITY_CREATURE && e->mesh_index >= 0) {
            passive_reveal(e);
        }
    }
}

int entity_find_nearest_interactable(const Entity *entities, int count,
                                     const float pos[3], float maxDist)
{
    int best = -1;
    float bestDist = maxDist * maxDist;

    for (int i = 0; i < count; i++) {
        if (entities[i].type != ENTITY_DIAL && entities[i].type != ENTITY_DOOR)
            continue;

        float dx = entities[i].pos[0] - pos[0];
        float dy = entities[i].pos[1] - pos[1];
        float dz = entities[i].pos[2] - pos[2];
        float dist2 = dx*dx + dy*dy + dz*dz;

        if (dist2 < bestDist) {
            bestDist = dist2;
            best = i;
        }
    }

    return best;
}

void entity_activate(Entity *e)
{
    switch (e->type) {
    case ENTITY_DIAL:
        LOG_INFO("Dial activated: code=%s", e->code);
        /* TODO: open dial input UI */
        break;
    case ENTITY_DOOR: {
        e->active = !e->active;
        int start = 0, count = 0;
        if (e->target[0] != '\0' &&
            map_get_collision_range(e->target, &start, &count) == 0) {
            physics_set_tris_enabled(start, count, !e->active);
            raycast_set_tris_enabled(start, count, !e->active);
        }
        if (e->sound[0] != '\0') {
            const Sound *snd = find_cached_sound(e->sound);
            if (snd) spatial_play(snd, e->pos, 1.0f, 1.0f);
        }
        LOG_INFO("Door %s: %s", e->id, e->active ? "OPEN" : "CLOSED");
        break;
    }
    default:
        break;
    }
}

void entity_shutdown(void)
{
    for (int i = 0; i < s_soundCount; i++)
        sound_destroy(&s_sounds[i]);
    s_soundCount = 0;
}
