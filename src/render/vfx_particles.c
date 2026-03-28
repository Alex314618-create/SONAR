/**
 * vfx_particles.c — Dedicated particle system implementation
 *
 * Uses GL_POINTS with per-particle alpha (7 floats: pos[3] + color[3] + alpha).
 * Additive blending, same distance-based point size as sonar shader.
 *
 * Dependencies: render/shader, core/log, glad
 */

#include "render/vfx_particles.h"
#include "render/shader.h"
#include "core/log.h"

#include <glad/gl.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_VFX_PARTICLES 2048
#define GPU_FLOATS_PER_PARTICLE 7  /* pos[3] + color[3] + alpha */

/* ── LCG RNG ─────────────────────────────────────────────── */
static unsigned int s_vfxSeed = 9999;

static float vfx_randf(void)
{
    s_vfxSeed = s_vfxSeed * 1664525u + 1013904223u;
    return (float)(s_vfxSeed >> 8) / 16777216.0f;
}

typedef struct {
    float pos[3];
    float vel[3];
    float color[3];
    float life;
    float max_life;
} VfxParticle;

static VfxParticle s_particles[MAX_VFX_PARTICLES];
static int         s_particleCount;

static uint32_t s_shader;
static uint32_t s_vao;
static uint32_t s_vbo;

int vfx_particles_init(void)
{
    s_shader = shader_load("shaders/particle.vert", "shaders/particle.frag");
    if (!s_shader) {
        LOG_ERROR("Failed to load particle shader");
        return -1;
    }

    s_particleCount = 0;

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(MAX_VFX_PARTICLES * GPU_FLOATS_PER_PARTICLE * sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    /* Position: location 0, 3 floats */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          GPU_FLOATS_PER_PARTICLE * sizeof(float),
                          (void *)0);

    /* Color: location 1, 3 floats */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          GPU_FLOATS_PER_PARTICLE * sizeof(float),
                          (void *)(3 * sizeof(float)));

    /* Alpha: location 2, 1 float */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
                          GPU_FLOATS_PER_PARTICLE * sizeof(float),
                          (void *)(6 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("VFX particles initialized (max %d)", MAX_VFX_PARTICLES);
    return 0;
}

static void emit_particle(const float pos[3], const float vel[3],
                           const float color[3], float lifetime)
{
    if (s_particleCount >= MAX_VFX_PARTICLES) return;

    VfxParticle *p = &s_particles[s_particleCount++];
    p->pos[0] = pos[0]; p->pos[1] = pos[1]; p->pos[2] = pos[2];
    p->vel[0] = vel[0]; p->vel[1] = vel[1]; p->vel[2] = vel[2];
    p->color[0] = color[0]; p->color[1] = color[1]; p->color[2] = color[2];
    p->life = lifetime;
    p->max_life = lifetime;
}

void vfx_particles_spawn_shockwave(const float center[3],
                                    const float color[3])
{
    /* 64 particles in horizontal ring, outward velocity 4.0 m/s */
    int count = 64;
    float speed = 4.0f;
    float lifetime = 0.6f;

    /* Red-orange tint */
    float col[3] = { 1.0f, 0.2f, 0.1f };
    if (color) {
        col[0] = color[0];
        col[1] = color[1] * 0.5f + 0.1f;
        col[2] = color[2] * 0.3f + 0.05f;
    }

    for (int i = 0; i < count; i++) {
        float angle = (float)i / (float)count * 2.0f * (float)M_PI;
        float vel[3] = {
            cosf(angle) * speed,
            0.0f,
            sinf(angle) * speed
        };
        emit_particle(center, vel, col, lifetime);
    }
}

void vfx_particles_spawn_collapse(const float center[3],
                                   const float color[3])
{
    /* 48 particles falling from stalker position with random drift */
    int count = 48;

    for (int i = 0; i < count; i++) {
        float offset[3] = {
            center[0] + (vfx_randf() - 0.5f) * 1.0f,
            center[1] + vfx_randf() * 1.5f,
            center[2] + (vfx_randf() - 0.5f) * 1.0f
        };
        float vel[3] = {
            (vfx_randf() - 0.5f) * 0.6f,
            -1.0f - vfx_randf() * 2.0f,
            (vfx_randf() - 0.5f) * 0.6f
        };
        float lifetime = 0.8f + vfx_randf() * 0.6f;
        emit_particle(offset, vel, color, lifetime);
    }
}

void vfx_particles_update(float dt)
{
    int alive = 0;
    for (int i = 0; i < s_particleCount; i++) {
        VfxParticle *p = &s_particles[i];
        p->life -= dt;
        if (p->life <= 0.0f) continue;

        /* Apply velocity + gravity */
        p->vel[1] -= 9.8f * dt;
        p->pos[0] += p->vel[0] * dt;
        p->pos[1] += p->vel[1] * dt;
        p->pos[2] += p->vel[2] * dt;

        /* Floor clamp */
        if (p->pos[1] < 0.0f) {
            p->pos[1] = 0.0f;
            p->vel[1] = 0.0f;
        }

        /* Compact alive particles */
        if (alive != i)
            s_particles[alive] = *p;
        alive++;
    }
    s_particleCount = alive;
}

void vfx_particles_render(const float *view, const float *proj,
                           const float *camPos)
{
    if (s_particleCount <= 0) return;

    /* Pack into GPU buffer */
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

    float chunkBuf[GPU_FLOATS_PER_PARTICLE * 256];
    int gpuCount = 0;
    int bufIdx = 0;

    for (int i = 0; i < s_particleCount; i++) {
        VfxParticle *p = &s_particles[i];
        float alpha = p->life / p->max_life;
        if (alpha < 0.0f) alpha = 0.0f;

        int off = bufIdx * GPU_FLOATS_PER_PARTICLE;
        chunkBuf[off + 0] = p->pos[0];
        chunkBuf[off + 1] = p->pos[1];
        chunkBuf[off + 2] = p->pos[2];
        chunkBuf[off + 3] = p->color[0];
        chunkBuf[off + 4] = p->color[1];
        chunkBuf[off + 5] = p->color[2];
        chunkBuf[off + 6] = alpha;
        bufIdx++;

        if (bufIdx == 256) {
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(gpuCount * GPU_FLOATS_PER_PARTICLE * sizeof(float)),
                            (GLsizeiptr)(bufIdx * GPU_FLOATS_PER_PARTICLE * sizeof(float)),
                            chunkBuf);
            gpuCount += bufIdx;
            bufIdx = 0;
        }
    }

    if (bufIdx > 0) {
        glBufferSubData(GL_ARRAY_BUFFER,
                        (GLintptr)(gpuCount * GPU_FLOATS_PER_PARTICLE * sizeof(float)),
                        (GLsizeiptr)(bufIdx * GPU_FLOATS_PER_PARTICLE * sizeof(float)),
                        chunkBuf);
        gpuCount += bufIdx;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Render with additive blending + alpha */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    shader_use(s_shader);
    shader_set_mat4(s_shader, "u_view", view);
    shader_set_mat4(s_shader, "u_proj", proj);
    shader_set_vec3(s_shader, "u_camPos", camPos);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_POINTS, 0, gpuCount);
    glBindVertexArray(0);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void vfx_particles_shutdown(void)
{
    if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_shader) { shader_destroy(s_shader); s_shader = 0; }
    s_particleCount = 0;
}
