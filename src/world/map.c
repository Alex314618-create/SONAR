/**
 * map.c — Level loading: cgltf glTF loader + procedural fallback
 *
 * When path != NULL, parses a .glb file via cgltf:
 *   - Visual mesh nodes (no prefix)    -> GPU render model
 *   - col_* nodes                      -> collision triangles
 *   - player_spawn empty               -> spawn position + yaw
 *   - entity_* empties                 -> Entity structs
 *
 * When path == NULL, generates a procedural two-room test level.
 *
 * Dependencies: render/mesh, render/model, world/entity, core/log, cgltf
 */

#include "world/map.h"
#include "core/log.h"
#include "cgltf.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_VERTS           2048
#define MAX_COLLISION_TRIS  8192
#define MAX_VISUAL_VERTS    65536
#define MAX_ENTITIES        32

static Model   s_model;
static float  *s_collisionVerts;
static int     s_collisionTriCount;
static float   s_playerSpawn[3];
static float   s_playerYaw;
static int     s_loaded = 0;
static int     s_glbCollision = 0; /* 1 if collision data is from static buf */

static Entity  s_entities[MAX_ENTITIES];
static int     s_entityCount = 0;

/* Static collision buffer for glTF-loaded levels */
static float   s_collisionBuf[MAX_COLLISION_TRIS * 9];

/* Clue surface ranges — maps tri index ranges to special colors */
#define MAX_CLUE_RANGES 32
typedef struct { int start; int end; float color[3]; } ClueRange;
static ClueRange s_clueRanges[MAX_CLUE_RANGES];
static int       s_clueRangeCount = 0;

/* CollisionRange: tracks triangle ranges for named col_* nodes (door system) */
#define MAX_COLLISION_RANGES 32
typedef struct { char name[32]; int start_tri; int tri_count; } CollisionRange;
static CollisionRange s_colRanges[MAX_COLLISION_RANGES];
static int            s_colRangeCount = 0;

/* MeshRange: visual-only meshes from vis_* nodes (separate from collision) */
#define MAX_MESH_RANGES     32
#define MAX_MESH_RANGE_TRIS 8192
typedef struct { char name[32]; int start_tri; int tri_count; } MeshRange;
static MeshRange s_meshRanges[MAX_MESH_RANGES];
static int       s_meshRangeCount = 0;
static float     s_meshRangeBuf[MAX_MESH_RANGE_TRIS * 9];
static int       s_meshRangeTotalTris = 0;

/* ═══════════════════════════════════════════════════════════════════
 * Procedural test level (preserved from original map.c)
 * ═══════════════════════════════════════════════════════════════════ */

static int add_quad(float *buf,
                    const float a[3], const float b[3],
                    const float c[3], const float d[3],
                    const float n[3])
{
    int vi = 0;
    buf[vi++] = a[0]; buf[vi++] = a[1]; buf[vi++] = a[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    buf[vi++] = b[0]; buf[vi++] = b[1]; buf[vi++] = b[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    buf[vi++] = c[0]; buf[vi++] = c[1]; buf[vi++] = c[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    buf[vi++] = a[0]; buf[vi++] = a[1]; buf[vi++] = a[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    buf[vi++] = c[0]; buf[vi++] = c[1]; buf[vi++] = c[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    buf[vi++] = d[0]; buf[vi++] = d[1]; buf[vi++] = d[2];
    buf[vi++] = n[0]; buf[vi++] = n[1]; buf[vi++] = n[2];
    return vi;
}

static int add_wall(float *buf,
                    float x0, float z0, float x1, float z1,
                    float yBot, float yTop,
                    float nx, float ny, float nz)
{
    float a[3] = {x0, yBot, z0};
    float b[3] = {x1, yBot, z1};
    float c[3] = {x1, yTop, z1};
    float d[3] = {x0, yTop, z0};
    float n[3] = {nx, ny, nz};
    return add_quad(buf, a, b, c, d, n);
}

static int add_floor_ceil(float *buf,
                          float x0, float z0, float x1, float z1,
                          float y, float ny)
{
    float n[3] = {0.0f, ny, 0.0f};
    if (ny > 0) {
        float a[3] = {x0, y, z0};
        float b[3] = {x1, y, z0};
        float c[3] = {x1, y, z1};
        float d[3] = {x0, y, z1};
        return add_quad(buf, a, b, c, d, n);
    } else {
        float a[3] = {x0, y, z1};
        float b[3] = {x1, y, z1};
        float c[3] = {x1, y, z0};
        float d[3] = {x0, y, z0};
        return add_quad(buf, a, b, c, d, n);
    }
}

static int generate_test_level(float *buf)
{
    int vi = 0;
    float yBot = 0.0f, yTop = 3.0f;

    /* Room 1: x=[-3,3], z=[-3,3] */
    vi += add_floor_ceil(&buf[vi], -3, -3, 3, 3, yBot, 1.0f);
    vi += add_floor_ceil(&buf[vi], -3, -3, 3, 3, yTop, -1.0f);
    vi += add_wall(&buf[vi], -3,-3, 3,-3, yBot,yTop, 0,0,1);
    vi += add_wall(&buf[vi], -3,3, -3,-3, yBot,yTop, 1,0,0);
    vi += add_wall(&buf[vi], 3,-3, 3,3, yBot,yTop, -1,0,0);
    vi += add_wall(&buf[vi], -1,3, -3,3, yBot,yTop, 0,0,-1);
    vi += add_wall(&buf[vi], 3,3, 1,3, yBot,yTop, 0,0,-1);

    /* Corridor: x=[-1,1], z=[3,9] */
    vi += add_floor_ceil(&buf[vi], -1, 3, 1, 9, yBot, 1.0f);
    vi += add_floor_ceil(&buf[vi], -1, 3, 1, 9, yTop, -1.0f);
    vi += add_wall(&buf[vi], -1,9, -1,3, yBot,yTop, 1,0,0);
    vi += add_wall(&buf[vi], 1,3, 1,9, yBot,yTop, -1,0,0);

    /* Room 2: x=[-3,3], z=[9,15] */
    vi += add_floor_ceil(&buf[vi], -3, 9, 3, 15, yBot, 1.0f);
    vi += add_floor_ceil(&buf[vi], -3, 9, 3, 15, yTop, -1.0f);
    vi += add_wall(&buf[vi], 3,15, -3,15, yBot,yTop, 0,0,-1);
    vi += add_wall(&buf[vi], -3,15, -3,9, yBot,yTop, 1,0,0);
    vi += add_wall(&buf[vi], 3,9, 3,15, yBot,yTop, -1,0,0);
    vi += add_wall(&buf[vi], -3,9, -1,9, yBot,yTop, 0,0,1);
    vi += add_wall(&buf[vi], 1,9, 3,9, yBot,yTop, 0,0,1);

    return vi;
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: transform a local-space vertex by a 4x4 column-major matrix
 * ═══════════════════════════════════════════════════════════════════ */

static void transform_point(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0]*in[0] + m[4]*in[1] + m[ 8]*in[2] + m[12];
    out[1] = m[1]*in[0] + m[5]*in[1] + m[ 9]*in[2] + m[13];
    out[2] = m[2]*in[0] + m[6]*in[1] + m[10]*in[2] + m[14];
}

static void transform_normal(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0]*in[0] + m[4]*in[1] + m[ 8]*in[2];
    out[1] = m[1]*in[0] + m[5]*in[1] + m[ 9]*in[2];
    out[2] = m[2]*in[0] + m[6]*in[1] + m[10]*in[2];
    float len = sqrtf(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (len > 1e-8f) {
        out[0] /= len;
        out[1] /= len;
        out[2] /= len;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: flat normals for triangles that lack NORMAL attribute
 * ═══════════════════════════════════════════════════════════════════ */

static void compute_flat_normals(float *verts, int vertCount)
{
    for (int i = 0; i + 2 < vertCount; i += 3) {
        float *v0 = &verts[i * 6];
        float *v1 = &verts[(i + 1) * 6];
        float *v2 = &verts[(i + 2) * 6];

        float e1[3] = { v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2] };
        float e2[3] = { v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2] };

        float nx = e1[1]*e2[2] - e1[2]*e2[1];
        float ny = e1[2]*e2[0] - e1[0]*e2[2];
        float nz = e1[0]*e2[1] - e1[1]*e2[0];

        float len = sqrtf(nx*nx + ny*ny + nz*nz);
        if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }

        v0[3] = nx; v0[4] = ny; v0[5] = nz;
        v1[3] = nx; v1[4] = ny; v1[5] = nz;
        v2[3] = nx; v2[4] = ny; v2[5] = nz;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: parse extras JSON for entity properties
 * Simple key-value extraction without a full JSON parser.
 * ═══════════════════════════════════════════════════════════════════ */

static void parse_extra_string(const char *json, const char *key,
                               char *out, int maxlen)
{
    out[0] = '\0';
    if (!json || !key) return;

    /* Search for "key" : "value" pattern */
    char needle[80];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *found = strstr(json, needle);
    if (!found) return;

    /* Skip past key and find the colon then opening quote */
    found += strlen(needle);
    while (*found && (*found == ' ' || *found == ':' || *found == '\t')) found++;
    if (*found != '"') return;
    found++; /* skip opening quote */

    int i = 0;
    while (*found && *found != '"' && i < maxlen - 1) {
        out[i++] = *found++;
    }
    out[i] = '\0';
}

static float parse_extra_float(const char *json, const char *key, float fallback)
{
    if (!json || !key) return fallback;

    char needle[80];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *found = strstr(json, needle);
    if (!found) return fallback;

    found += strlen(needle);
    while (*found && (*found == ' ' || *found == ':' || *found == '\t')) found++;

    /* Value might be a number (not quoted) or a quoted string */
    if (*found == '"') found++;

    char buf[32];
    int i = 0;
    while (*found && *found != ',' && *found != '}' && *found != '"' && i < 31) {
        buf[i++] = *found++;
    }
    buf[i] = '\0';

    if (i == 0) return fallback;
    return (float)atof(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: extract collision triangles from a single primitive.
 * Reads NORMAL attribute to detect outward-facing normals and flips
 * winding order when the stored normal opposes the geometric normal.
 * Returns number of triangles written.
 * ═══════════════════════════════════════════════════════════════════ */

static void write_tri_with_flip(float *dst, float v[3][3],
                                cgltf_accessor *normAcc,
                                const float worldMat[16],
                                cgltf_size idx0)
{
    if (normAcc) {
        float localN[3] = {0};
        cgltf_accessor_read_float(normAcc, idx0, localN, 3);
        float storedN[3];
        transform_normal(worldMat, localN, storedN);

        float e1[3] = { v[1][0]-v[0][0], v[1][1]-v[0][1], v[1][2]-v[0][2] };
        float e2[3] = { v[2][0]-v[0][0], v[2][1]-v[0][1], v[2][2]-v[0][2] };
        float geomN[3] = {
            e1[1]*e2[2] - e1[2]*e2[1],
            e1[2]*e2[0] - e1[0]*e2[2],
            e1[0]*e2[1] - e1[1]*e2[0]
        };

        float dot = geomN[0]*storedN[0] + geomN[1]*storedN[1] + geomN[2]*storedN[2];
        if (dot < 0.0f) {
            float tmp[3];
            memcpy(tmp,   v[1], sizeof(tmp));
            memcpy(v[1],  v[2], sizeof(tmp));
            memcpy(v[2],  tmp,  sizeof(tmp));
        }
    }

    for (int vi = 0; vi < 3; vi++) {
        dst[vi * 3 + 0] = v[vi][0];
        dst[vi * 3 + 1] = v[vi][1];
        dst[vi * 3 + 2] = v[vi][2];
    }
}

static int extract_collision_tris_prim(const float worldMat[16],
                                       cgltf_primitive *prim,
                                       float *dst, int maxTris)
{
    if (prim->type != cgltf_primitive_type_triangles) return 0;

    cgltf_accessor *posAcc = NULL;
    cgltf_accessor *normAcc = NULL;
    for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
        if (prim->attributes[ai].type == cgltf_attribute_type_position)
            posAcc = prim->attributes[ai].data;
        else if (prim->attributes[ai].type == cgltf_attribute_type_normal)
            normAcc = prim->attributes[ai].data;
    }
    if (!posAcc) return 0;

    int totalTris = 0;

    if (prim->indices) {
        int idxCount = (int)prim->indices->count;
        for (int i = 0; i + 2 < idxCount; i += 3) {
            if (totalTris >= maxTris) return totalTris;

            float v[3][3];
            cgltf_size idx0 = cgltf_accessor_read_index(prim->indices, (cgltf_size)i);
            for (int vi = 0; vi < 3; vi++) {
                cgltf_size idx = cgltf_accessor_read_index(prim->indices, (cgltf_size)(i + vi));
                float local[3] = {0};
                cgltf_accessor_read_float(posAcc, idx, local, 3);
                transform_point(worldMat, local, v[vi]);
            }

            write_tri_with_flip(&dst[totalTris * 9], v, normAcc, worldMat, idx0);
            totalTris++;
        }
    } else {
        int vertCount = (int)posAcc->count;
        for (int i = 0; i + 2 < vertCount; i += 3) {
            if (totalTris >= maxTris) return totalTris;

            float v[3][3];
            for (int vi = 0; vi < 3; vi++) {
                float local[3] = {0};
                cgltf_accessor_read_float(posAcc, (cgltf_size)(i + vi), local, 3);
                transform_point(worldMat, local, v[vi]);
            }

            write_tri_with_flip(&dst[totalTris * 9], v, normAcc, worldMat,
                                (cgltf_size)i);
            totalTris++;
        }
    }

    return totalTris;
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: extract visual mesh with pos+normal, world-transformed.
 * Writes interleaved [px,py,pz, nx,ny,nz] into dst.
 * Returns vertex count.
 * ═══════════════════════════════════════════════════════════════════ */

static int extract_visual_verts(cgltf_node *node, float *dst, int maxVerts)
{
    float worldMat[16];
    cgltf_node_transform_world(node, worldMat);

    cgltf_mesh *mesh = node->mesh;
    int totalVerts = 0;

    for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
        cgltf_primitive *prim = &mesh->primitives[pi];
        if (prim->type != cgltf_primitive_type_triangles) continue;

        cgltf_accessor *posAcc = NULL;
        cgltf_accessor *normAcc = NULL;
        for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
            if (prim->attributes[ai].type == cgltf_attribute_type_position)
                posAcc = prim->attributes[ai].data;
            else if (prim->attributes[ai].type == cgltf_attribute_type_normal)
                normAcc = prim->attributes[ai].data;
        }
        if (!posAcc) continue;

        int startVert = totalVerts;

        if (prim->indices) {
            int idxCount = (int)prim->indices->count;
            for (int i = 0; i < idxCount; i++) {
                if (totalVerts >= maxVerts) return totalVerts;
                cgltf_size idx = cgltf_accessor_read_index(prim->indices, (cgltf_size)i);
                float local[3] = {0};
                cgltf_accessor_read_float(posAcc, idx, local, 3);
                float world[3];
                transform_point(worldMat, local, world);
                int off = totalVerts * 6;
                dst[off + 0] = world[0];
                dst[off + 1] = world[1];
                dst[off + 2] = world[2];

                if (normAcc) {
                    float localN[3] = {0};
                    cgltf_accessor_read_float(normAcc, idx, localN, 3);
                    float worldN[3];
                    transform_normal(worldMat, localN, worldN);
                    dst[off + 3] = worldN[0];
                    dst[off + 4] = worldN[1];
                    dst[off + 5] = worldN[2];
                } else {
                    dst[off + 3] = 0.0f;
                    dst[off + 4] = 0.0f;
                    dst[off + 5] = 0.0f;
                }
                totalVerts++;
            }
        } else {
            int vertCount = (int)posAcc->count;
            for (int i = 0; i < vertCount; i++) {
                if (totalVerts >= maxVerts) return totalVerts;
                float local[3] = {0};
                cgltf_accessor_read_float(posAcc, (cgltf_size)i, local, 3);
                float world[3];
                transform_point(worldMat, local, world);
                int off = totalVerts * 6;
                dst[off + 0] = world[0];
                dst[off + 1] = world[1];
                dst[off + 2] = world[2];

                if (normAcc) {
                    float localN[3] = {0};
                    cgltf_accessor_read_float(normAcc, (cgltf_size)i, localN, 3);
                    float worldN[3];
                    transform_normal(worldMat, localN, worldN);
                    dst[off + 3] = worldN[0];
                    dst[off + 4] = worldN[1];
                    dst[off + 5] = worldN[2];
                } else {
                    dst[off + 3] = 0.0f;
                    dst[off + 4] = 0.0f;
                    dst[off + 5] = 0.0f;
                }
                totalVerts++;
            }
        }

        /* Generate flat normals if attribute was missing */
        if (!normAcc && totalVerts > startVert) {
            compute_flat_normals(&dst[startVert * 6], totalVerts - startVert);
        }
    }

    return totalVerts;
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: record a clue range if primitive has mat_clue_* material
 * ═══════════════════════════════════════════════════════════════════ */

static void record_clue_range(cgltf_primitive *prim, int startTri, int endTri)
{
    const char *matName = prim->material ? prim->material->name : NULL;
    if (!matName || strncmp(matName, "mat_clue_", 9) != 0) return;
    if (s_clueRangeCount >= MAX_CLUE_RANGES) return;
    if (startTri == endTri) return;

    ClueRange *cr = &s_clueRanges[s_clueRangeCount++];
    cr->start = startTri;
    cr->end   = endTri;

    if (strncmp(matName, "mat_clue_red", 12) == 0) {
        cr->color[0] = 1.0f; cr->color[1] = 0.15f; cr->color[2] = 0.15f;
    } else if (strncmp(matName, "mat_clue_blue", 13) == 0) {
        cr->color[0] = 0.15f; cr->color[1] = 0.45f; cr->color[2] = 1.0f;
    } else {
        cr->color[0] = 1.0f; cr->color[1] = 1.0f; cr->color[2] = 1.0f;
    }

    LOG_INFO("map_load: clue range [%d,%d) material='%s'", startTri, endTri, matName);
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf helper: extract vis_* mesh triangles into s_meshRangeBuf.
 * Returns number of triangles written.
 * ═══════════════════════════════════════════════════════════════════ */

static int extract_vis_tris(cgltf_node *node, float *dst, int maxTris)
{
    float worldMat[16];
    cgltf_node_transform_world(node, worldMat);

    cgltf_mesh *mesh = node->mesh;
    int totalTris = 0;

    for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
        cgltf_primitive *prim = &mesh->primitives[pi];
        if (prim->type != cgltf_primitive_type_triangles) continue;

        cgltf_accessor *posAcc = NULL;
        for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
            if (prim->attributes[ai].type == cgltf_attribute_type_position)
                posAcc = prim->attributes[ai].data;
        }
        if (!posAcc) continue;

        if (prim->indices) {
            int idxCount = (int)prim->indices->count;
            for (int i = 0; i + 2 < idxCount; i += 3) {
                if (totalTris >= maxTris) return totalTris;
                for (int vi = 0; vi < 3; vi++) {
                    cgltf_size idx = cgltf_accessor_read_index(prim->indices,
                                        (cgltf_size)(i + vi));
                    float local[3] = {0};
                    cgltf_accessor_read_float(posAcc, idx, local, 3);
                    float world[3];
                    transform_point(worldMat, local, world);
                    dst[totalTris * 9 + vi * 3 + 0] = world[0];
                    dst[totalTris * 9 + vi * 3 + 1] = world[1];
                    dst[totalTris * 9 + vi * 3 + 2] = world[2];
                }
                totalTris++;
            }
        } else {
            int vertCount = (int)posAcc->count;
            for (int i = 0; i + 2 < vertCount; i += 3) {
                if (totalTris >= maxTris) return totalTris;
                for (int vi = 0; vi < 3; vi++) {
                    float local[3] = {0};
                    cgltf_accessor_read_float(posAcc, (cgltf_size)(i + vi),
                                              local, 3);
                    float world[3];
                    transform_point(worldMat, local, world);
                    dst[totalTris * 9 + vi * 3 + 0] = world[0];
                    dst[totalTris * 9 + vi * 3 + 1] = world[1];
                    dst[totalTris * 9 + vi * 3 + 2] = world[2];
                }
                totalTris++;
            }
        }
    }

    return totalTris;
}

/* ═══════════════════════════════════════════════════════════════════
 * cgltf glTF loader — the real map_load path for path != NULL
 * ═══════════════════════════════════════════════════════════════════ */

static int load_glb(const char *path)
{
    cgltf_options opts;
    memset(&opts, 0, sizeof(opts));

    cgltf_data *data = NULL;
    cgltf_result res = cgltf_parse_file(&opts, path, &data);
    if (res != cgltf_result_success) {
        LOG_ERROR("map_load: cgltf_parse_file failed for %s", path);
        return -1;
    }

    res = cgltf_load_buffers(&opts, data, path);
    if (res != cgltf_result_success) {
        LOG_ERROR("map_load: cgltf_load_buffers failed for %s", path);
        cgltf_free(data);
        return -1;
    }

    /* Accumulators */
    s_collisionTriCount = 0;
    s_entityCount = 0;
    s_clueRangeCount = 0;
    s_colRangeCount = 0;
    s_meshRangeCount = 0;
    s_meshRangeTotalTris = 0;
    int hasSpawn = 0;

    /* Visual mesh: accumulate into a temp buffer, then create GPU mesh */
    float *visualBuf = (float *)malloc((size_t)MAX_VISUAL_VERTS * 6 * sizeof(float));
    if (!visualBuf) {
        LOG_ERROR("map_load: failed to allocate visual vertex buffer");
        cgltf_free(data);
        return -1;
    }
    int visualVertCount = 0;

    /* Traverse all nodes */
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        cgltf_node *node = &data->nodes[i];
        const char *name = node->name ? node->name : "";

        /* --- Player spawn --- */
        if (strcmp(name, "player_spawn") == 0) {
            float worldMat[16];
            cgltf_node_transform_world(node, worldMat);
            /* Translation is in columns 12,13,14 of column-major matrix */
            s_playerSpawn[0] = worldMat[12];
            s_playerSpawn[1] = worldMat[13];
            s_playerSpawn[2] = worldMat[14];

            /* Extract yaw from rotation quaternion */
            if (node->has_rotation) {
                float qx = node->rotation[0];
                float qy = node->rotation[1];
                float qz = node->rotation[2];
                float qw = node->rotation[3];
                float yawRad = atan2f(2.0f*(qw*qy + qx*qz),
                                      1.0f - 2.0f*(qy*qy + qz*qz));
                s_playerYaw = yawRad * (float)(180.0 / M_PI);
            } else {
                s_playerYaw = 0.0f;
            }

            hasSpawn = 1;
            LOG_INFO("map_load: player_spawn at (%.2f, %.2f, %.2f) yaw=%.1f",
                     s_playerSpawn[0], s_playerSpawn[1], s_playerSpawn[2],
                     s_playerYaw);
        }
        /* --- Collision mesh (per-primitive, with normal flip + clue detection) --- */
        else if (strncmp(name, "col_", 4) == 0 && node->mesh) {
            float worldMat[16];
            cgltf_node_transform_world(node, worldMat);
            int nodeTris = 0;
            int nodeStartTri = s_collisionTriCount;

            LOG_INFO("=== COL NODE '%s': %zu primitives ===", name,
                     node->mesh->primitives_count);
            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; pi++) {
                cgltf_primitive *prim = &node->mesh->primitives[pi];
                const char *matName = prim->material ? prim->material->name : NULL;
                LOG_INFO("  prim %zu: material='%s'", pi,
                         matName ? matName : "(null)");
                int remaining = MAX_COLLISION_TRIS - s_collisionTriCount;
                if (remaining <= 0) {
                    LOG_ERROR("map_load: collision tri limit reached");
                    break;
                }
                int startTri = s_collisionTriCount;
                int tris = extract_collision_tris_prim(worldMat, prim,
                            &s_collisionBuf[s_collisionTriCount * 9], remaining);
                s_collisionTriCount += tris;
                nodeTris += tris;
                record_clue_range(prim, startTri, s_collisionTriCount);

                /* Fallback: if material didn't trigger a clue range but
                 * the node name contains clue_red / clue_blue, force it */
                if (startTri < s_collisionTriCount &&
                    s_clueRangeCount > 0 &&
                    s_clueRanges[s_clueRangeCount - 1].start != startTri) {
                    /* record_clue_range didn't fire — check node name */
                    if (strstr(name, "clue_red") && s_clueRangeCount < MAX_CLUE_RANGES) {
                        ClueRange *cr = &s_clueRanges[s_clueRangeCount++];
                        cr->start = startTri;
                        cr->end   = s_collisionTriCount;
                        cr->color[0] = 1.0f; cr->color[1] = 0.30f; cr->color[2] = 0.08f;
                        LOG_INFO("  -> fallback clue_red by node name [%d,%d)",
                                 startTri, s_collisionTriCount);
                    } else if (strstr(name, "clue_blue") && s_clueRangeCount < MAX_CLUE_RANGES) {
                        ClueRange *cr = &s_clueRanges[s_clueRangeCount++];
                        cr->start = startTri;
                        cr->end   = s_collisionTriCount;
                        cr->color[0] = 0.15f; cr->color[1] = 0.45f; cr->color[2] = 1.0f;
                        LOG_INFO("  -> fallback clue_blue by node name [%d,%d)",
                                 startTri, s_collisionTriCount);
                    }
                }
            }

            /* Record CollisionRange for this node (used by door system) */
            if (nodeTris > 0 && s_colRangeCount < MAX_COLLISION_RANGES) {
                CollisionRange *cr = &s_colRanges[s_colRangeCount++];
                snprintf(cr->name, sizeof(cr->name), "%s", name);
                cr->start_tri = nodeStartTri;
                cr->tri_count = nodeTris;
            }

            LOG_INFO("map_load: collision node '%s' -> %d tris (total %d)",
                     name, nodeTris, s_collisionTriCount);
        }
        /* --- Entity --- */
        else if (strncmp(name, "entity_", 7) == 0) {
            if (s_entityCount >= MAX_ENTITIES) {
                LOG_ERROR("map_load: entity limit (%d) reached, skipping %s",
                          MAX_ENTITIES, name);
                continue;
            }

            Entity *e = &s_entities[s_entityCount];
            memset(e, 0, sizeof(Entity));

            /* Determine type from name */
            if (strncmp(name + 7, "creature_", 9) == 0) {
                e->type = ENTITY_CREATURE;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else if (strncmp(name + 7, "dial_", 5) == 0) {
                e->type = ENTITY_DIAL;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else if (strncmp(name + 7, "door_", 5) == 0) {
                e->type = ENTITY_DOOR;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else if (strncmp(name + 7, "sound_", 6) == 0) {
                e->type = ENTITY_SOUND;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else if (strncmp(name + 7, "trigger_", 8) == 0) {
                e->type = ENTITY_TRIGGER;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else if (strncmp(name + 7, "stalker_", 8) == 0) {
                e->type = ENTITY_STALKER;
                snprintf(e->id, sizeof(e->id), "%s", name + 7);
            } else {
                LOG_ERROR("map_load: unknown entity type: %s", name);
                continue;
            }

            /* Position from world transform */
            float worldMat[16];
            cgltf_node_transform_world(node, worldMat);
            e->pos[0] = worldMat[12];
            e->pos[1] = worldMat[13];
            e->pos[2] = worldMat[14];

            /* Yaw from rotation */
            if (node->has_rotation) {
                float qx = node->rotation[0];
                float qy = node->rotation[1];
                float qz = node->rotation[2];
                float qw = node->rotation[3];
                e->yaw = atan2f(2.0f*(qw*qy + qx*qz),
                                1.0f - 2.0f*(qy*qy + qz*qz));
            }

            /* Parse extras JSON for custom properties */
            if (node->extras.start_offset != node->extras.end_offset) {
                /* cgltf stores extras as a range into the JSON buffer */
                cgltf_size len = node->extras.end_offset - node->extras.start_offset;
                char *json = (char *)malloc(len + 1);
                if (json) {
                    memcpy(json, (const char *)data->json + node->extras.start_offset, len);
                    json[len] = '\0';

                    parse_extra_string(json, "sound", e->sound, sizeof(e->sound));
                    parse_extra_string(json, "code", e->code, sizeof(e->code));
                    parse_extra_string(json, "target", e->target, sizeof(e->target));
                    e->interval = parse_extra_float(json, "interval", 0.0f);
                    parse_extra_string(json, "mesh_ref", e->mesh_ref,
                                       sizeof(e->mesh_ref));
                    e->ttl = parse_extra_float(json, "ttl", 0.0f);
                    e->radius = parse_extra_float(json, "radius", 0.0f);

                    /* Trigger/Stalker: parse type-specific extras into
                     * reused Entity fields:
                     *   Trigger: target→zone_id, interval→delay,
                     *            code→mode, sound→sound, radius→radius
                     *   Stalker: sound→sound_appear, target→sound_depart,
                     *            interval→start_dist, code→retreat_time */
                    if (e->type == ENTITY_TRIGGER) {
                        parse_extra_string(json, "zone_id", e->target,
                                           sizeof(e->target));
                        e->interval = parse_extra_float(json, "delay", 0.0f);
                        parse_extra_string(json, "mode", e->code,
                                           sizeof(e->code));
                    } else if (e->type == ENTITY_STALKER) {
                        parse_extra_string(json, "sound_appear", e->sound,
                                           sizeof(e->sound));
                        parse_extra_string(json, "sound_depart", e->target,
                                           sizeof(e->target));
                        e->interval = parse_extra_float(json, "start_dist",
                                                        8.0f);
                        /* step_dist stored as radius, retreat_time as code */
                        e->radius = parse_extra_float(json, "step_dist",
                                                      1.5f);
                        snprintf(e->code, sizeof(e->code), "%.1f",
                                 parse_extra_float(json, "retreat_time",
                                                   15.0f));
                    }

                    free(json);
                }
            }

            /* Resolve mesh_ref → mesh_index from MeshRange table */
            e->mesh_index = -1;
            if (e->mesh_ref[0] != '\0') {
                for (int mi = 0; mi < s_meshRangeCount; mi++) {
                    if (strcmp(s_meshRanges[mi].name, e->mesh_ref) == 0) {
                        e->mesh_index = mi;
                        break;
                    }
                }
                if (e->mesh_index < 0) {
                    LOG_ERROR("map_load: entity '%s' mesh_ref '%s' not found",
                              name, e->mesh_ref);
                }
            }

            LOG_INFO("map_load: entity '%s' type=%d pos=(%.2f,%.2f,%.2f) mesh=%d",
                     e->id, e->type, e->pos[0], e->pos[1], e->pos[2],
                     e->mesh_index);

            s_entityCount++;
        }
        /* --- Visual-only mesh (vis_* prefix) → MeshRange buffer --- */
        else if (strncmp(name, "vis_", 4) == 0 && node->mesh) {
            if (s_meshRangeCount >= MAX_MESH_RANGES) {
                LOG_ERROR("map_load: mesh range limit reached, skipping %s", name);
                continue;
            }
            int remaining = MAX_MESH_RANGE_TRIS - s_meshRangeTotalTris;
            if (remaining <= 0) {
                LOG_ERROR("map_load: mesh range tri buffer full, skipping %s", name);
                continue;
            }
            int tris = extract_vis_tris(node,
                           &s_meshRangeBuf[s_meshRangeTotalTris * 9], remaining);
            if (tris > 0) {
                MeshRange *mr = &s_meshRanges[s_meshRangeCount];
                snprintf(mr->name, sizeof(mr->name), "%s", name);
                mr->start_tri = s_meshRangeTotalTris;
                mr->tri_count = tris;
                s_meshRangeTotalTris += tris;
                LOG_INFO("map_load: mesh range '%s' -> %d tris (index %d)",
                         name, tris, s_meshRangeCount);
                s_meshRangeCount++;
            }
        }
        /* --- Visual mesh (no prefix) --- */
        else if (node->mesh) {
            int remaining = MAX_VISUAL_VERTS - visualVertCount;
            if (remaining <= 0) {
                LOG_ERROR("map_load: visual vert limit reached, skipping %s", name);
                continue;
            }
            int verts = extract_visual_verts(node,
                            &visualBuf[visualVertCount * 6], remaining);
            visualVertCount += verts;
            LOG_INFO("map_load: visual node '%s' -> %d verts (total %d)",
                     name, verts, visualVertCount);

            /* Also add clue-material primitives to collision so sonar can detect them */
            float worldMat[16];
            cgltf_node_transform_world(node, worldMat);
            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; pi++) {
                cgltf_primitive *prim = &node->mesh->primitives[pi];
                const char *matName = prim->material ? prim->material->name : NULL;
                if (!matName || strncmp(matName, "mat_clue_", 9) != 0) continue;

                int colRemaining = MAX_COLLISION_TRIS - s_collisionTriCount;
                if (colRemaining <= 0) break;
                int startTri = s_collisionTriCount;
                int tris = extract_collision_tris_prim(worldMat, prim,
                            &s_collisionBuf[s_collisionTriCount * 9], colRemaining);
                s_collisionTriCount += tris;
                record_clue_range(prim, startTri, s_collisionTriCount);
                LOG_INFO("map_load: visual clue '%s' -> %d collision tris", name, tris);
            }
        }
    }

    /* Build render model from accumulated visual verts */
    if (visualVertCount > 0) {
        Mesh mesh = mesh_create(visualBuf, visualVertCount, NULL, 0);
        s_model.meshes = (Mesh *)malloc(sizeof(Mesh));
        if (s_model.meshes) {
            s_model.meshes[0] = mesh;
            s_model.meshCount = 1;
        }
    } else {
        LOG_ERROR("map_load: no visual geometry found in %s", path);
    }
    free(visualBuf);

    /* Point collision data to the static buffer */
    s_collisionVerts = s_collisionBuf;
    s_glbCollision = 1;

    /* Default spawn if not found */
    if (!hasSpawn) {
        LOG_ERROR("map_load: no player_spawn node found, using origin");
        s_playerSpawn[0] = 0.0f;
        s_playerSpawn[1] = 1.6f;
        s_playerSpawn[2] = 0.0f;
        s_playerYaw = 0.0f;
    }

    cgltf_free(data);
    LOG_INFO("map_load: loaded %s — %d visual verts, %d collision tris, %d entities",
             path, visualVertCount, s_collisionTriCount, s_entityCount);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

int map_get_clue_color(int tri_index, float *out_rgb)
{
    for (int i = 0; i < s_clueRangeCount; i++) {
        if (tri_index >= s_clueRanges[i].start && tri_index < s_clueRanges[i].end) {
            out_rgb[0] = s_clueRanges[i].color[0];
            out_rgb[1] = s_clueRanges[i].color[1];
            out_rgb[2] = s_clueRanges[i].color[2];
            return 1;
        }
    }
    return 0;
}

Entity *map_get_entities(void)
{
    return s_entities;
}

int map_get_entity_count(void)
{
    return s_entityCount;
}

int map_get_collision_range(const char *name, int *out_start, int *out_count)
{
    for (int i = 0; i < s_colRangeCount; i++) {
        if (strcmp(s_colRanges[i].name, name) == 0) {
            *out_start = s_colRanges[i].start_tri;
            *out_count = s_colRanges[i].tri_count;
            return 0;
        }
    }
    return -1;
}

int map_get_mesh_range_tris(int index, const float **out_verts, int *out_count)
{
    if (index < 0 || index >= s_meshRangeCount) return -1;
    *out_verts = &s_meshRangeBuf[s_meshRanges[index].start_tri * 9];
    *out_count = s_meshRanges[index].tri_count;
    return 0;
}

int map_load(const char *path)
{
    if (s_loaded) {
        map_shutdown();
    }

    if (path != NULL) {
        int result = load_glb(path);
        if (result == 0) {
            s_loaded = 1;
            return 0;
        }
        LOG_ERROR("map_load: glTF load failed, falling back to procedural");
    }

    /* Procedural test level fallback */
    s_entityCount = 0;
    s_glbCollision = 0;

    float *vertBuf = (float *)malloc(MAX_VERTS * 6 * sizeof(float));
    if (!vertBuf) {
        LOG_ERROR("Failed to allocate map vertex buffer");
        return -1;
    }

    int totalFloats = generate_test_level(vertBuf);
    int vertCount = totalFloats / 6;

    Mesh mesh = mesh_create(vertBuf, vertCount, NULL, 0);
    s_model.meshes = (Mesh *)malloc(sizeof(Mesh));
    if (!s_model.meshes) {
        LOG_ERROR("Failed to allocate model meshes");
        free(vertBuf);
        return -1;
    }
    s_model.meshes[0] = mesh;
    s_model.meshCount = 1;

    int triCount = vertCount / 3;
    s_collisionTriCount = triCount;
    s_collisionVerts = (float *)malloc((size_t)triCount * 9 * sizeof(float));
    if (!s_collisionVerts) {
        LOG_ERROR("Failed to allocate collision data");
        free(vertBuf);
        return -1;
    }

    for (int t = 0; t < triCount; t++) {
        for (int v = 0; v < 3; v++) {
            int srcIdx = (t * 3 + v) * 6;
            int dstIdx = t * 9 + v * 3;
            s_collisionVerts[dstIdx + 0] = vertBuf[srcIdx + 0];
            s_collisionVerts[dstIdx + 1] = vertBuf[srcIdx + 1];
            s_collisionVerts[dstIdx + 2] = vertBuf[srcIdx + 2];
        }
    }

    free(vertBuf);

    s_playerSpawn[0] = 0.0f;
    s_playerSpawn[1] = 1.6f;
    s_playerSpawn[2] = 0.0f;
    s_playerYaw = 90.0f;

    s_loaded = 1;
    LOG_INFO("Map loaded (procedural): %d tris", triCount);
    return 0;
}

const Model *map_get_render_model(void)
{
    return &s_model;
}

const float *map_get_collision_verts(void)
{
    return s_collisionVerts;
}

int map_get_collision_tri_count(void)
{
    return s_collisionTriCount;
}

const float *map_get_player_spawn(void)
{
    return s_playerSpawn;
}

float map_get_player_yaw(void)
{
    return s_playerYaw;
}

void map_shutdown(void)
{
    if (!s_loaded) return;

    model_destroy(&s_model);

    /* Only free collision verts if they were malloc'd (procedural path) */
    if (!s_glbCollision) {
        free(s_collisionVerts);
    }
    s_collisionVerts = NULL;
    s_collisionTriCount = 0;
    s_glbCollision = 0;
    s_entityCount = 0;
    memset(s_entities, 0, sizeof(s_entities));
    s_clueRangeCount = 0;
    s_colRangeCount = 0;
    s_meshRangeCount = 0;
    s_meshRangeTotalTris = 0;
    s_loaded = 0;
}
