/**
 * model.c — glTF 2.0 model loading implementation via cgltf
 */

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "render/model.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static void compute_flat_normals(float *verts, int vertCount)
{
    for (int i = 0; i + 2 < vertCount; i += 3) {
        float *v0 = &verts[i * 6];
        float *v1 = &verts[(i + 1) * 6];
        float *v2 = &verts[(i + 2) * 6];

        float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
        float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

        float nx = e1[1] * e2[2] - e1[2] * e2[1];
        float ny = e1[2] * e2[0] - e1[0] * e2[2];
        float nz = e1[0] * e2[1] - e1[1] * e2[0];

        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        if (len > 1e-8f) {
            nx /= len;
            ny /= len;
            nz /= len;
        }

        v0[3] = nx; v0[4] = ny; v0[5] = nz;
        v1[3] = nx; v1[4] = ny; v1[5] = nz;
        v2[3] = nx; v2[4] = ny; v2[5] = nz;
    }
}

Model model_load(const char *path)
{
    Model model;
    memset(&model, 0, sizeof(model));

    cgltf_options options;
    memset(&options, 0, sizeof(options));

    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        LOG_ERROR("cgltf_parse_file failed: %s", path);
        return model;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        LOG_ERROR("cgltf_load_buffers failed: %s", path);
        cgltf_free(data);
        return model;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        LOG_ERROR("cgltf_validate failed: %s", path);
        cgltf_free(data);
        return model;
    }

    /* Count triangle primitives */
    int primCount = 0;
    for (cgltf_size mi = 0; mi < data->meshes_count; mi++) {
        for (cgltf_size pi = 0; pi < data->meshes[mi].primitives_count; pi++) {
            if (data->meshes[mi].primitives[pi].type == cgltf_primitive_type_triangles) {
                primCount++;
            }
        }
    }

    if (primCount == 0) {
        LOG_ERROR("No triangle primitives in: %s", path);
        cgltf_free(data);
        return model;
    }

    model.meshes = (Mesh *)calloc((size_t)primCount, sizeof(Mesh));
    if (!model.meshes) {
        LOG_ERROR("Failed to allocate meshes");
        cgltf_free(data);
        return model;
    }
    model.meshCount = primCount;

    int meshIdx = 0;
    for (cgltf_size mi = 0; mi < data->meshes_count; mi++) {
        for (cgltf_size pi = 0; pi < data->meshes[mi].primitives_count; pi++) {
            cgltf_primitive *prim = &data->meshes[mi].primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            /* Find POSITION and NORMAL accessors */
            cgltf_accessor *posAcc = NULL;
            cgltf_accessor *normAcc = NULL;
            for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                if (prim->attributes[ai].type == cgltf_attribute_type_position)
                    posAcc = prim->attributes[ai].data;
                else if (prim->attributes[ai].type == cgltf_attribute_type_normal)
                    normAcc = prim->attributes[ai].data;
            }

            if (!posAcc) continue;

            int vertCount = (int)posAcc->count;

            /* Read indices if present */
            int idxCount = 0;
            int finalVertCount = vertCount;

            if (prim->indices) {
                idxCount = (int)prim->indices->count;

                /* Unpack into flat vertex array (de-index) */
                finalVertCount = idxCount;
                float *verts = (float *)malloc((size_t)finalVertCount * 6 * sizeof(float));
                if (!verts) continue;
                memset(verts, 0, (size_t)finalVertCount * 6 * sizeof(float));

                for (int i = 0; i < idxCount; i++) {
                    uint32_t idx = (uint32_t)cgltf_accessor_read_index(prim->indices, (cgltf_size)i);
                    float pos[3] = {0};
                    cgltf_accessor_read_float(posAcc, idx, pos, 3);
                    verts[i * 6 + 0] = pos[0];
                    verts[i * 6 + 1] = pos[1];
                    verts[i * 6 + 2] = pos[2];

                    if (normAcc) {
                        float norm[3] = {0};
                        cgltf_accessor_read_float(normAcc, idx, norm, 3);
                        verts[i * 6 + 3] = norm[0];
                        verts[i * 6 + 4] = norm[1];
                        verts[i * 6 + 5] = norm[2];
                    }
                }

                if (!normAcc) {
                    compute_flat_normals(verts, finalVertCount);
                }

                model.meshes[meshIdx] = mesh_create(verts, finalVertCount, NULL, 0);
                free(verts);
            } else {
                /* Non-indexed */
                float *verts = (float *)malloc((size_t)vertCount * 6 * sizeof(float));
                if (!verts) continue;
                memset(verts, 0, (size_t)vertCount * 6 * sizeof(float));

                for (int i = 0; i < vertCount; i++) {
                    float pos[3] = {0};
                    cgltf_accessor_read_float(posAcc, (cgltf_size)i, pos, 3);
                    verts[i * 6 + 0] = pos[0];
                    verts[i * 6 + 1] = pos[1];
                    verts[i * 6 + 2] = pos[2];

                    if (normAcc) {
                        float norm[3] = {0};
                        cgltf_accessor_read_float(normAcc, (cgltf_size)i, norm, 3);
                        verts[i * 6 + 3] = norm[0];
                        verts[i * 6 + 4] = norm[1];
                        verts[i * 6 + 5] = norm[2];
                    }
                }

                if (!normAcc) {
                    compute_flat_normals(verts, vertCount);
                }

                model.meshes[meshIdx] = mesh_create(verts, vertCount, NULL, 0);
                free(verts);
            }

            meshIdx++;
        }
    }

    cgltf_free(data);
    LOG_INFO("Loaded model: %s (%d meshes)", path, model.meshCount);
    return model;
}

void model_draw(const Model *m, uint32_t shader)
{
    (void)shader;
    for (int i = 0; i < m->meshCount; i++) {
        mesh_draw(&m->meshes[i]);
    }
}

void model_destroy(Model *m)
{
    for (int i = 0; i < m->meshCount; i++) {
        mesh_destroy(&m->meshes[i]);
    }
    free(m->meshes);
    m->meshes = NULL;
    m->meshCount = 0;
}
