/**
 * mesh.h — VAO/VBO/EBO wrapper for GPU mesh data
 *
 * Vertex format: position(vec3, loc 0) + normal(vec3, loc 1) = 6 floats, stride 24 bytes.
 */
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t vao, vbo, ebo;
    int indexCount, vertexCount;
} Mesh;

/**
 * Create a mesh from interleaved vertex data and optional index data.
 *
 * @param verts      Interleaved [px,py,pz, nx,ny,nz] vertex data
 * @param vertCount  Number of vertices
 * @param indices    Index array (NULL for non-indexed draw)
 * @param idxCount   Number of indices (ignored if indices is NULL)
 * @return Mesh with valid GL handles
 */
Mesh mesh_create(const float *verts, int vertCount,
                 const uint32_t *indices, int idxCount);

/** Draw the mesh (bind VAO, issue draw call, unbind). */
void mesh_draw(const Mesh *m);

/** Delete GPU resources and zero all fields. */
void mesh_destroy(Mesh *m);
