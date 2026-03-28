/**
 * entity.h — Lightweight typed entity system
 *
 * Provides runtime entities for creatures, dials, doors, and ambient
 * sound sources. Parsed from glTF extras at level load time.
 * See ADR 0007 for design rationale.
 */
#pragma once

/**
 * Entity type tags, matching Blender node name prefixes.
 */
typedef enum {
    ENTITY_CREATURE,
    ENTITY_DIAL,
    ENTITY_DOOR,
    ENTITY_SOUND,
    ENTITY_TRIGGER,
    ENTITY_STALKER,
} EntityType;

/**
 * Runtime entity instance. Populated by map loader from glTF extras.
 */
typedef struct {
    EntityType  type;
    float       pos[3];       /* world-space position */
    float       yaw;          /* initial facing, radians */
    char        id[32];       /* e.g. "creature_01" */
    char        sound[64];    /* asset path relative to assets/sounds/, may be empty */
    char        code[16];     /* dial code, may be empty */
    char        target[32];   /* activation signal target entity id, may be empty */
    float       interval;     /* sound repeat interval in seconds (0 = no repeat) */
    char        mesh_ref[32]; /* vis_* mesh name for reveal (trigger/stalker/creature) */
    float       ttl;          /* sonar point lifetime override (0 = permanent) */
    float       radius;       /* trigger/interaction radius (0 = use default) */
    /* runtime state */
    float       sound_timer;  /* countdown to next ambient sound */
    int         mesh_index;   /* index into MeshRange array (-1 = no mesh) */
    int         active;       /* runtime toggle: door open/closed, etc. */
} Entity;

/**
 * @brief Initialize runtime state for all entities.
 *
 * Resets sound timers to their configured interval so the first
 * sound fires after one full interval elapses.
 *
 * @param entities  Array of entities populated by the map loader.
 * @param count     Number of entities in the array.
 */
void entity_init(Entity *entities, int count);

/**
 * @brief Tick entity logic each frame.
 *
 * Decrements sound timers; on expiry plays the entity's sound at its
 * position and, for creatures, injects orange sonar points tracing
 * the creature mesh outline (passive reveal, TTL 0.8 s).
 *
 * @param dt        Frame delta time in seconds.
 * @param entities  Array of entities.
 * @param count     Number of entities.
 */
void entity_update(float dt, Entity *entities, int count);

/**
 * @brief Process player interaction (F-key) with an entity.
 *
 * Dial: logs activation with code. Door: logs toggle. Other types: no-op.
 *
 * @param e  Entity the player is interacting with.
 */
void entity_activate(Entity *e);

/**
 * @brief Find the nearest interactable entity within range.
 *
 * Searches for ENTITY_DIAL and ENTITY_DOOR types within maxDist
 * of the given position.
 *
 * @param entities  Array of entities.
 * @param count     Number of entities.
 * @param pos       Player position (vec3).
 * @param maxDist   Maximum interaction distance.
 * @return Index of nearest interactable entity, or -1 if none in range.
 */
int entity_find_nearest_interactable(const Entity *entities, int count,
                                     const float pos[3], float maxDist);

/**
 * @brief Free entity resources.
 *
 * Currently a no-op; reserved for future sound buffer cleanup.
 */
void entity_shutdown(void);
