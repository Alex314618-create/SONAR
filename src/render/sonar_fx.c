/**
 * sonar_fx.c — GPU sonar point renderer implementation
 *
 * Uses GL_POINTS with gl_PointSize for distance-based sizing.
 * Additive blending (GL_ONE, GL_ONE) for glowing overlap effect.
 * Circular falloff computed in fragment shader.
 */

#include "render/sonar_fx.h"
#include "render/shader.h"
#include "core/log.h"

#include <glad/gl.h>
#include <string.h>

/* Per-point GPU data: position (3f) + color (3f) */
#define SONAR_GPU_FLOATS_PER_POINT 6

static uint32_t s_shader;
static uint32_t s_vao;
static uint32_t s_vbo;
static int      s_capacity;

int sonar_fx_init(void)
{
    s_shader = shader_load("shaders/sonar.vert", "shaders/sonar.frag");
    if (!s_shader) {
        LOG_ERROR("Failed to load sonar shader");
        return -1;
    }

    s_capacity = MAX_SONAR_POINTS;

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

    /* Allocate buffer (will be updated each frame via glBufferSubData) */
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(s_capacity * SONAR_GPU_FLOATS_PER_POINT * sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    /* Position: location 0, 3 floats */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          SONAR_GPU_FLOATS_PER_POINT * sizeof(float),
                          (void *)0);

    /* Color: location 1, 3 floats */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          SONAR_GPU_FLOATS_PER_POINT * sizeof(float),
                          (void *)(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Enable programmable point size */
    glEnable(GL_PROGRAM_POINT_SIZE);

    LOG_INFO("Sonar FX initialized (max %d points)", s_capacity);
    return 0;
}

void sonar_fx_render(const SonarPoint *points, int count,
                     const float *view, const float *proj,
                     const float *camPos)
{
    if (count <= 0)
        return;

    if (count > s_capacity)
        count = s_capacity;

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

    /* Pack points into GPU buffer, skipping dead points (age == -1.0f) */
    {
        float chunkBuf[6 * 1024];
        int gpuCount = 0;
        int bufIdx = 0;

        for (int i = 0; i < count; i++) {
            /* Skip TTL-expired creature reveal points */
            if (points[i].age == -1.0f)
                continue;

            int off = bufIdx * SONAR_GPU_FLOATS_PER_POINT;
            chunkBuf[off + 0] = points[i].pos[0];
            chunkBuf[off + 1] = points[i].pos[1];
            chunkBuf[off + 2] = points[i].pos[2];
            chunkBuf[off + 3] = points[i].color[0];
            chunkBuf[off + 4] = points[i].color[1];
            chunkBuf[off + 5] = points[i].color[2];
            bufIdx++;

            /* Flush chunk when full */
            if (bufIdx == 1024) {
                glBufferSubData(GL_ARRAY_BUFFER,
                                (GLintptr)(gpuCount * SONAR_GPU_FLOATS_PER_POINT * sizeof(float)),
                                (GLsizeiptr)(bufIdx * SONAR_GPU_FLOATS_PER_POINT * sizeof(float)),
                                chunkBuf);
                gpuCount += bufIdx;
                bufIdx = 0;
            }
        }

        /* Flush remaining */
        if (bufIdx > 0) {
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(gpuCount * SONAR_GPU_FLOATS_PER_POINT * sizeof(float)),
                            (GLsizeiptr)(bufIdx * SONAR_GPU_FLOATS_PER_POINT * sizeof(float)),
                            chunkBuf);
            gpuCount += bufIdx;
        }

        count = gpuCount;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Render with additive blending; LEQUAL so points exactly on wall surfaces
     * are not rejected by the depth buffer from the depth-only pre-pass. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    shader_use(s_shader);
    shader_set_mat4(s_shader, "u_view", view);
    shader_set_mat4(s_shader, "u_proj", proj);
    shader_set_vec3(s_shader, "u_camPos", camPos);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_POINTS, 0, count);
    glBindVertexArray(0);

    /* Restore state */
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void sonar_fx_shutdown(void)
{
    if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_shader) { shader_destroy(s_shader); s_shader = 0; }
}
