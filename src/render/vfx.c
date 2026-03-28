/**
 * vfx.c — Fullscreen visual effects implementation
 *
 * Scanlines, vignette, pulse ripple rendered as fullscreen quads.
 * Gun sprite and laser lines rendered as 2D overlays.
 */

#include "render/vfx.h"
#include "render/shader.h"
#include "core/log.h"

#include <glad/gl.h>
#include <string.h>
#include <math.h>

/* ── Fullscreen quad (scanlines/vignette/pulse) ─────────────── */
static uint32_t s_shader;
static uint32_t s_vao;
static uint32_t s_vbo;

/* Pulse ripple state */
static float s_rippleTimer;
static int   s_rippleActive;

static const float s_quadVerts[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f,
};

/* ── Gun sprite (HUD shader) ───────────────────────────────── */
static uint32_t s_gunShader;
static uint32_t s_gunVao;
static uint32_t s_gunVbo;

#define GUN_MAX_VERTS 512
#define GUN_FLOATS_PER_VERT 6  /* x, y, r, g, b, a */
static float s_gunBatch[GUN_MAX_VERTS * GUN_FLOATS_PER_VERT];
static int   s_gunVertCount;

/* ── Laser lines (laser shader, 2D screen-space) ───────────── */
static uint32_t s_laserShader;
static uint32_t s_laserVao;
static uint32_t s_laserVbo;

#define MAX_LASER_LINES    500
#define LASER_FLOATS_PER_VERT 5  /* x, y, r, g, b */
#define LASER_FLOATS_PER_LINE 10 /* 2 verts */
static float s_laserBuf[MAX_LASER_LINES * LASER_FLOATS_PER_LINE];

/* ── Init / Shutdown ────────────────────────────────────────── */

int vfx_init(void)
{
    /* Fullscreen quad shader */
    s_shader = shader_load("shaders/vignette.vert", "shaders/vignette.frag");
    if (!s_shader) {
        LOG_ERROR("Failed to load VFX shader");
        return -1;
    }

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadVerts),
                 s_quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          2 * sizeof(float), (void *)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    s_rippleTimer  = 0.0f;
    s_rippleActive = 0;

    /* Gun sprite (HUD shader) */
    s_gunShader = shader_load("shaders/hud.vert", "shaders/hud.frag");
    if (!s_gunShader) {
        LOG_ERROR("Failed to load gun HUD shader");
        return -1;
    }

    glGenVertexArrays(1, &s_gunVao);
    glGenBuffers(1, &s_gunVbo);

    glBindVertexArray(s_gunVao);
    glBindBuffer(GL_ARRAY_BUFFER, s_gunVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(GUN_MAX_VERTS * GUN_FLOATS_PER_VERT * sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          GUN_FLOATS_PER_VERT * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          GUN_FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Laser line shader */
    s_laserShader = shader_load("shaders/laser.vert", "shaders/laser.frag");
    if (!s_laserShader) {
        LOG_ERROR("Failed to load laser shader");
        return -1;
    }

    glGenVertexArrays(1, &s_laserVao);
    glGenBuffers(1, &s_laserVbo);

    glBindVertexArray(s_laserVao);
    glBindBuffer(GL_ARRAY_BUFFER, s_laserVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(MAX_LASER_LINES * LASER_FLOATS_PER_LINE * sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    /* Position: location 0, 2 floats */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          LASER_FLOATS_PER_VERT * sizeof(float), (void *)0);
    /* Color: location 1, 3 floats */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          LASER_FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("VFX initialized");
    return 0;
}

void vfx_update(float dt)
{
    if (s_rippleActive) {
        s_rippleTimer += dt;
        if (s_rippleTimer >= 0.4f) {
            s_rippleActive = 0;
        }
    }
}

/* ── Fullscreen effects ─────────────────────────────────────── */

void vfx_render_scanlines(int winW, int winH)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_use(s_shader);
    float res[2] = {(float)winW, (float)winH};
    shader_set_vec2(s_shader, "u_resolution", res);
    shader_set_int(s_shader, "u_mode", 0);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void vfx_render_vignette(int winW, int winH)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_use(s_shader);
    float res[2] = {(float)winW, (float)winH};
    shader_set_vec2(s_shader, "u_resolution", res);
    shader_set_int(s_shader, "u_mode", 1);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void vfx_trigger_pulse_ripple(void)
{
    s_rippleActive = 1;
    s_rippleTimer  = 0.0f;
}

void vfx_render_pulse_ripple(int winW, int winH)
{
    if (!s_rippleActive) return;

    float t = s_rippleTimer / 0.4f;
    float radius = t * (float)winW * 0.5f;
    float alpha  = 1.0f - t;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    shader_use(s_shader);
    float res[2] = {(float)winW, (float)winH};
    shader_set_vec2(s_shader, "u_resolution", res);
    shader_set_int(s_shader, "u_mode", 2);
    shader_set_float(s_shader, "u_pulseRadius", radius);
    shader_set_float(s_shader, "u_pulseAlpha", alpha);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* ── Gun sprite ─────────────────────────────────────────────── */

static void gun_reset(void) { s_gunVertCount = 0; }

static void gun_quad(float x, float y, float w, float h,
                     float r, float g, float b, float a)
{
    if (s_gunVertCount + 6 > GUN_MAX_VERTS) return;
    float *v = s_gunBatch + s_gunVertCount * GUN_FLOATS_PER_VERT;

    v[0]=x;   v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x+w; v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x;   v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x+w; v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x+w; v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x;   v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a;

    s_gunVertCount += 6;
}

void vfx_get_gun_muzzle(int winW, int winH, int *outX, int *outY)
{
    *outX = winW - 50;
    *outY = winH - 80;
}

void vfx_render_gun(int winW, int winH, int lmbHeld)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gun_reset();

    /* Body: x=[winW-80, winW-20], y=[winH-100, winH-20] */
    float bx = (float)(winW - 80);
    float by = (float)(winH - 100);
    float bodyW = 60.0f, bodyH = 80.0f;

    /* Border: #1A1F26 */
    gun_quad(bx - 1, by - 1, bodyW + 2, bodyH + 2,
             0.102f, 0.122f, 0.149f, 1.0f);

    /* Body: #28303A */
    gun_quad(bx, by, bodyW, bodyH,
             0.157f, 0.188f, 0.227f, 1.0f);

    /* Emitter dish: center (winW-50, winH-80), radius 16px */
    float cx = (float)(winW - 50);
    float cy = (float)(winH - 80);
    float dishR = 16.0f;

    float er, eg, eb;
    if (lmbHeld) {
        er = 0.0f; eg = 1.0f; eb = 220.0f / 255.0f; /* #00FFDC */
    } else {
        er = 50.0f/255.0f; eg = 72.0f/255.0f; eb = 88.0f/255.0f; /* #324858 */
    }

    for (float dy = -dishR; dy <= dishR; dy += 2.0f) {
        float half = sqrtf(dishR * dishR - dy * dy);
        gun_quad(cx - half, cy + dy, half * 2.0f, 2.0f,
                 er, eg, eb, 1.0f);
    }

    /* Upload and draw */
    if (s_gunVertCount > 0 && winW > 0 && winH > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, s_gunVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(s_gunVertCount * GUN_FLOATS_PER_VERT * (int)sizeof(float)),
                        s_gunBatch);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        shader_use(s_gunShader);

        float ortho[16];
        memset(ortho, 0, sizeof(ortho));
        ortho[0]  =  2.0f / (float)winW;
        ortho[5]  = -2.0f / (float)winH;
        ortho[10] = -1.0f;
        ortho[12] = -1.0f;
        ortho[13] =  1.0f;
        ortho[15] =  1.0f;
        shader_set_mat4(s_gunShader, "u_ortho", ortho);

        glBindVertexArray(s_gunVao);
        glDrawArrays(GL_TRIANGLES, 0, s_gunVertCount);
        glBindVertexArray(0);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* ── Laser lines (screen-space, gun muzzle → new sonar points) ─ */

void vfx_render_laser_lines(
    const SonarPoint *points, int frameStart, int writeHead,
    int totalCapacity,
    const float *view, const float *proj,
    int winW, int winH,
    int gunX, int gunY)
{
    if (frameStart == writeHead) return;
    if (winW <= 0 || winH <= 0) return;

    int lineCount = 0;
    float gx = (float)gunX;
    float gy = (float)gunY;

    int idx = frameStart;
    while (idx != writeHead && lineCount < MAX_LASER_LINES) {
        const SonarPoint *p = &points[idx % totalCapacity];
        idx = (idx + 1) % totalCapacity;

        float px = p->pos[0], py = p->pos[1], pz = p->pos[2];

        /* view * pos (column-major) */
        float vx = view[0]*px + view[4]*py + view[8]*pz  + view[12];
        float vy = view[1]*px + view[5]*py + view[9]*pz  + view[13];
        float vz = view[2]*px + view[6]*py + view[10]*pz + view[14];
        float vw = view[3]*px + view[7]*py + view[11]*pz + view[15];

        /* proj * view_pos */
        float cx = proj[0]*vx + proj[4]*vy + proj[8]*vz  + proj[12]*vw;
        float cy = proj[1]*vx + proj[5]*vy + proj[9]*vz  + proj[13]*vw;
        float cw = proj[3]*vx + proj[7]*vy + proj[11]*vz + proj[15]*vw;

        if (cw <= 0.0f) continue; /* behind camera */
        float ndcX = cx / cw;
        float ndcY = cy / cw;
        if (ndcX < -1.0f || ndcX > 1.0f ||
            ndcY < -1.0f || ndcY > 1.0f) continue; /* off screen */

        float sx = (ndcX * 0.5f + 0.5f) * (float)winW;
        float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)winH;

        /* Derive laser color from point color (brightened 1.5x) */
        float lr = p->color[0] * 1.5f;
        float lg = p->color[1] * 1.5f;
        float lb = p->color[2] * 1.5f;
        if (lr > 1.0f) lr = 1.0f;
        if (lg > 1.0f) lg = 1.0f;
        if (lb > 1.0f) lb = 1.0f;

        /* Gun-end vertex: bright */
        float *v = s_laserBuf + lineCount * LASER_FLOATS_PER_LINE;
        v[0] = gx;
        v[1] = gy;
        v[2] = lr;
        v[3] = lg;
        v[4] = lb;

        /* Point-end vertex: dimmed */
        v[5] = sx;
        v[6] = sy;
        v[7] = lr * 0.3f;
        v[8] = lg * 0.3f;
        v[9] = lb * 0.3f;

        lineCount++;
    }

    if (lineCount == 0) return;

    /* One-shot diagnostic — remove once laser lines confirmed working */
    {
        static int s_laserLogged = 0;
        if (!s_laserLogged) {
            LOG_INFO("Laser: %d lines  gun=(%.0f,%.0f)  pt0=(%.0f,%.0f)  color=(%.2f,%.2f,%.2f)",
                     lineCount,
                     s_laserBuf[0], s_laserBuf[1],
                     s_laserBuf[5], s_laserBuf[6],
                     s_laserBuf[2], s_laserBuf[3], s_laserBuf[4]);
            s_laserLogged = 1;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_laserVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(lineCount * LASER_FLOATS_PER_LINE * sizeof(float)),
                    s_laserBuf);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);

    shader_use(s_laserShader);

    /* Orthographic: screen pixels to NDC */
    float ortho[16];
    memset(ortho, 0, sizeof(ortho));
    ortho[0]  =  2.0f / (float)winW;
    ortho[5]  = -2.0f / (float)winH;
    ortho[10] =  1.0f;
    ortho[12] = -1.0f;
    ortho[13] =  1.0f;
    ortho[15] =  1.0f;
    shader_set_mat4(s_laserShader, "u_ortho", ortho);

    glLineWidth(2.0f);
    glBindVertexArray(s_laserVao);
    glDrawArrays(GL_LINES, 0, lineCount * 2);
    glBindVertexArray(0);
    glLineWidth(1.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* ── Shutdown ───────────────────────────────────────────────── */

void vfx_shutdown(void)
{
    if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_shader) { shader_destroy(s_shader); s_shader = 0; }

    if (s_gunVbo) { glDeleteBuffers(1, &s_gunVbo); s_gunVbo = 0; }
    if (s_gunVao) { glDeleteVertexArrays(1, &s_gunVao); s_gunVao = 0; }
    if (s_gunShader) { shader_destroy(s_gunShader); s_gunShader = 0; }

    if (s_laserVbo) { glDeleteBuffers(1, &s_laserVbo); s_laserVbo = 0; }
    if (s_laserVao) { glDeleteVertexArrays(1, &s_laserVao); s_laserVao = 0; }
    if (s_laserShader) { shader_destroy(s_laserShader); s_laserShader = 0; }
}
