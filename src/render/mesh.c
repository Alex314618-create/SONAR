/**
 * mesh.c — VAO/VBO/EBO wrapper implementation
 */

#include "render/mesh.h"

#include <glad/gl.h>
#include <string.h>

Mesh mesh_create(const float *verts, int vertCount,
                 const uint32_t *indices, int idxCount)
{
    Mesh m;
    memset(&m, 0, sizeof(m));

    m.vertexCount = vertCount;
    m.indexCount   = indices ? idxCount : 0;

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);

    glBindVertexArray(m.vao);

    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vertCount * 6 * sizeof(float)),
                 verts, GL_STATIC_DRAW);

    /* location 0: position (vec3) */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    /* location 1: normal (vec3) */
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    if (indices) {
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizeiptr)(idxCount * sizeof(uint32_t)),
                     indices, GL_STATIC_DRAW);
    }

    glBindVertexArray(0);

    return m;
}

void mesh_draw(const Mesh *m)
{
    glBindVertexArray(m->vao);

    if (m->indexCount > 0) {
        glDrawElements(GL_TRIANGLES, m->indexCount, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, m->vertexCount);
    }

    glBindVertexArray(0);
}

void mesh_destroy(Mesh *m)
{
    if (m->ebo) glDeleteBuffers(1, &m->ebo);
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    if (m->vao) glDeleteVertexArrays(1, &m->vao);
    memset(m, 0, sizeof(*m));
}
