/**
 * hud.c — 2D heads-up display overlay implementation
 *
 * Draws HUD elements in two batches:
 *   1. Colored quads (energy bar, crosshair, minimap) via hud.vert/frag
 *   2. Text glyphs (stb_truetype DM Mono TTF atlas)  via text.vert/frag
 *
 * Orthographic projection: (0,0) top-left.
 */

#include "render/hud.h"
#include "render/shader.h"
#include "sonar/sonar.h"
#include "core/log.h"

#include <glad/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── stb_truetype ────────────────────────────────────────────── */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* ── Colored-quad batch (existing system) ────────────────────── */

#define MAX_HUD_VERTS   16384
#define FLOATS_PER_VERT 6   /* x, y, r, g, b, a */

static uint32_t s_shader;
static uint32_t s_vao;
static uint32_t s_vbo;

static float s_batch[MAX_HUD_VERTS * FLOATS_PER_VERT];
static int   s_vertCount;

/* ── Font atlas ──────────────────────────────────────────────── */

#define FONT_ATLAS_W   512
#define FONT_ATLAS_H   512
#define FONT_SIZE_SM   15.0f
#define FONT_SIZE_LG   22.0f

static unsigned char   s_atlasBitmap[FONT_ATLAS_W * FONT_ATLAS_H];
static stbtt_bakedchar s_chars_sm[96];  /* ASCII 32..127 */
static stbtt_bakedchar s_chars_lg[96];
static GLuint          s_fontTex;
static uint32_t        s_textShader;

/* ── Text batch ──────────────────────────────────────────────── */

#define MAX_TEXT_VERTS        8192
#define TEXT_FLOATS_PER_VERT  8   /* x, y, u, v, r, g, b, a */

static float  s_textBatch[MAX_TEXT_VERTS * TEXT_FLOATS_PER_VERT];
static int    s_textVertCount;
static GLuint s_textVao, s_textVbo;

/* ── Batch helpers (colored quads) ───────────────────────────── */

static void batch_reset(void)
{
    s_vertCount = 0;
}

static void batch_quad(float x, float y, float w, float h,
                       float r, float g, float b, float a)
{
    if (s_vertCount + 6 > MAX_HUD_VERTS) return;

    float *v = s_batch + s_vertCount * FLOATS_PER_VERT;

    v[0]=x;   v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x+w; v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x;   v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;

    v[0]=x+w; v[1]=y;   v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x+w; v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a; v+=6;
    v[0]=x;   v[1]=y+h; v[2]=r; v[3]=g; v[4]=b; v[5]=a;

    s_vertCount += 6;
}

static void batch_flush(int winW, int winH)
{
    if (s_vertCount == 0 || winW <= 0 || winH <= 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(s_vertCount * FLOATS_PER_VERT * (int)sizeof(float)),
                    s_batch);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shader_use(s_shader);

    float ortho[16];
    memset(ortho, 0, sizeof(ortho));
    ortho[0]  =  2.0f / (float)winW;
    ortho[5]  = -2.0f / (float)winH;
    ortho[10] = -1.0f;
    ortho[12] = -1.0f;
    ortho[13] =  1.0f;
    ortho[15] =  1.0f;
    shader_set_mat4(s_shader, "u_ortho", ortho);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, s_vertCount);
    glBindVertexArray(0);
}

/* ── Text helpers (TTF glyphs) ───────────────────────────────── */

typedef enum { FONT_SM, FONT_LG } FontSize;

static void text_reset(void)
{
    s_textVertCount = 0;
}

static void text_push_vert(float x, float y, float u, float v,
                           float r, float g, float b, float a)
{
    if (s_textVertCount >= MAX_TEXT_VERTS) return;
    float *p = s_textBatch + s_textVertCount * TEXT_FLOATS_PER_VERT;
    p[0] = x; p[1] = y; p[2] = u; p[3] = v;
    p[4] = r; p[5] = g; p[6] = b; p[7] = a;
    s_textVertCount++;
}

static void draw_text_ttf(float x, float y, const char *text,
                           float r, float g, float b, float a,
                           FontSize size)
{
    stbtt_bakedchar *chars = (size == FONT_LG) ? s_chars_lg : s_chars_sm;
    /* y offset into atlas for the large font bake (second half) */
    int yOff = (size == FONT_LG) ? 256 : 0;
    (void)yOff; /* atlas offset is baked into the char UVs already */

    float cx = x;
    float cy = y;

    for (const char *p = text; *p; p++) {
        int c = (unsigned char)*p;
        if (c < 32 || c > 127) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(chars, FONT_ATLAS_W, FONT_ATLAS_H,
                           c - 32, &cx, &cy, &q, 1);

        /* Two triangles per glyph */
        text_push_vert(q.x0, q.y0, q.s0, q.t0, r, g, b, a);
        text_push_vert(q.x1, q.y0, q.s1, q.t0, r, g, b, a);
        text_push_vert(q.x0, q.y1, q.s0, q.t1, r, g, b, a);

        text_push_vert(q.x1, q.y0, q.s1, q.t0, r, g, b, a);
        text_push_vert(q.x1, q.y1, q.s1, q.t1, r, g, b, a);
        text_push_vert(q.x0, q.y1, q.s0, q.t1, r, g, b, a);
    }
}

static void text_flush(int winW, int winH)
{
    if (s_textVertCount == 0 || winW <= 0 || winH <= 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, s_textVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(s_textVertCount * TEXT_FLOATS_PER_VERT * (int)sizeof(float)),
                    s_textBatch);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_fontTex);
    shader_use(s_textShader);

    float ortho[16];
    memset(ortho, 0, sizeof(ortho));
    ortho[0]  =  2.0f / (float)winW;
    ortho[5]  = -2.0f / (float)winH;
    ortho[10] = -1.0f;
    ortho[12] = -1.0f;
    ortho[13] =  1.0f;
    ortho[15] =  1.0f;
    shader_set_mat4(s_textShader, "u_ortho", ortho);
    shader_set_int(s_textShader, "u_tex", 0);

    glBindVertexArray(s_textVao);
    glDrawArrays(GL_TRIANGLES, 0, s_textVertCount);
    glBindVertexArray(0);
}

/* ── Public API ──────────────────────────────────────────────── */

int hud_init(void)
{
    /* ── Colored-quad shader + VAO ────────────────────────── */
    s_shader = shader_load("shaders/hud.vert", "shaders/hud.frag");
    if (!s_shader) {
        LOG_ERROR("Failed to load HUD shader");
        return -1;
    }

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(MAX_HUD_VERTS * FLOATS_PER_VERT * (int)sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERT * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* ── Text shader + VAO ────────────────────────────────── */
    s_textShader = shader_load("shaders/text.vert", "shaders/text.frag");
    if (!s_textShader) {
        LOG_ERROR("Failed to load text shader");
        return -1;
    }

    glGenVertexArrays(1, &s_textVao);
    glGenBuffers(1, &s_textVbo);

    glBindVertexArray(s_textVao);
    glBindBuffer(GL_ARRAY_BUFFER, s_textVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(MAX_TEXT_VERTS * TEXT_FLOATS_PER_VERT * (int)sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    /* location 0: pos (vec2) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          TEXT_FLOATS_PER_VERT * sizeof(float), (void *)0);
    /* location 1: uv (vec2) */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          TEXT_FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));
    /* location 2: color (vec4) */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
                          TEXT_FLOATS_PER_VERT * sizeof(float),
                          (void *)(4 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* ── Bake font atlas ──────────────────────────────────── */
    FILE *f = fopen("assets/fonts/DMMono-Regular.ttf", "rb");
    if (!f) {
        LOG_ERROR("Cannot open DMMono-Regular.ttf");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long ttfLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ttfBuf = (unsigned char *)malloc((size_t)ttfLen);
    if (!ttfBuf) {
        fclose(f);
        LOG_ERROR("malloc failed for TTF data");
        return -1;
    }
    fread(ttfBuf, 1, (size_t)ttfLen, f);
    fclose(f);

    memset(s_atlasBitmap, 0, sizeof(s_atlasBitmap));

    /* Bake small font into top half of atlas (rows 0..255) */
    stbtt_BakeFontBitmap(ttfBuf, 0, FONT_SIZE_SM,
                         s_atlasBitmap,
                         FONT_ATLAS_W, FONT_ATLAS_H / 2,
                         32, 96, s_chars_sm);

    /* Bake large font into bottom half of atlas (rows 256..511) */
    stbtt_BakeFontBitmap(ttfBuf, 0, FONT_SIZE_LG,
                         s_atlasBitmap + (FONT_ATLAS_H / 2) * FONT_ATLAS_W,
                         FONT_ATLAS_W, FONT_ATLAS_H / 2,
                         32, 96, s_chars_lg);

    /* Fix UV.y for large chars: stbtt baked them as if atlas height = 256,
     * but the actual atlas is 512 and they sit in the bottom half. */
    for (int i = 0; i < 96; i++) {
        s_chars_lg[i].y0 = (unsigned short)(s_chars_lg[i].y0 + FONT_ATLAS_H / 2);
        s_chars_lg[i].y1 = (unsigned short)(s_chars_lg[i].y1 + FONT_ATLAS_H / 2);
    }

    free(ttfBuf);

    /* Upload atlas to GPU */
    glGenTextures(1, &s_fontTex);
    glBindTexture(GL_TEXTURE_2D, s_fontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 FONT_ATLAS_W, FONT_ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, s_atlasBitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    LOG_INFO("HUD initialized (TTF atlas %dx%d)", FONT_ATLAS_W, FONT_ATLAS_H);
    return 0;
}

void hud_render(int winW, int winH, float energy, int sonarMode,
                float fps, const float *camPos,
                int showMinimap, float playerYaw)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    batch_reset();
    text_reset();

    /* ── a) Energy bar — bottom-right ────────────────────── */
    {
        int barW = 120, barH = 10;
        float bx = (float)(winW - barW - 20);
        float by = (float)(winH - 30);

        batch_quad(bx - 2.0f, by - 2.0f,
                   (float)(barW + 4), (float)(barH + 4),
                   0.0f, 0.0f, 0.0f, 0.7f);

        float br = 0.0f, bg = 100.0f/255.0f, bb = 90.0f/255.0f;
        batch_quad(bx, by, (float)barW, 1.0f, br, bg, bb, 1.0f);
        batch_quad(bx, by + barH - 1, (float)barW, 1.0f, br, bg, bb, 1.0f);
        batch_quad(bx, by, 1.0f, (float)barH, br, bg, bb, 1.0f);
        batch_quad(bx + barW - 1, by, 1.0f, (float)barH, br, bg, bb, 1.0f);

        float frac = energy / 100.0f;
        if (frac > 1.0f) frac = 1.0f;
        if (frac < 0.0f) frac = 0.0f;
        int fillW = (int)(frac * (float)(barW - 2));

        float fr, fg, fb;
        if (energy > 30.0f) {
            fr = 0.0f; fg = 220.0f/255.0f; fb = 190.0f/255.0f;
        } else {
            fr = 200.0f/255.0f; fg = 100.0f/255.0f; fb = 0.0f;
        }
        if (fillW > 0) {
            batch_quad(bx + 1, by + 1,
                       (float)fillW, (float)(barH - 2),
                       fr, fg, fb, 1.0f);
        }

        /* "ENERGY" label left of bar */
        draw_text_ttf(bx - 62.0f, by + 9.0f,
                      "ENERGY",
                      0.0f, 150.0f/255.0f, 130.0f/255.0f, 1.0f, FONT_SM);

        /* ── b) Mode indicator — above energy bar ────────── */
        if (sonarMode == 1) {
            draw_text_ttf(bx, by - 14.0f,
                          "FOCUSED",
                          0.0f, 180.0f/255.0f, 1.0f, 1.0f, FONT_SM);
        } else {
            draw_text_ttf(bx, by - 14.0f,
                          "WIDE",
                          0.0f, 1.0f, 180.0f/255.0f, 1.0f, FONT_SM);
        }
    }

    /* ── c) Crosshair — screen center ────────────────────── */
    {
        float cx = (float)(winW / 2);
        float cy = (float)(winH / 2);
        for (int a = 0; a < 360; a += 15) {
            float rad = (float)a * 3.14159265f / 180.0f;
            float px = cx + 5.0f * cosf(rad);
            float py = cy + 5.0f * sinf(rad);
            batch_quad(px, py, 1.0f, 1.0f,
                       0.0f, 180.0f/255.0f, 160.0f/255.0f, 1.0f);
        }
        batch_quad(cx, cy, 1.0f, 1.0f,
                   0.0f, 1.0f, 220.0f/255.0f, 1.0f);
    }

    /* ── d) Title "SONAR" — top-left ─────────────────────── */
    draw_text_ttf(14.0f, 20.0f + FONT_SIZE_LG,
                  "SONAR",
                  0.0f, 0.78f, 0.70f, 1.0f, FONT_LG);

    /* ── e) Depth "DEPTH -XX M" — top-left ───────────────── */
    {
        int depth = (int)(camPos[2] * 4.0f);
        if (depth < 0) depth = -depth;
        char buf[32];
        snprintf(buf, sizeof(buf), "DEPTH -%d M", depth);
        draw_text_ttf(14.0f, 50.0f + FONT_SIZE_SM,
                      buf,
                      0.0f, 0.50f, 0.44f, 1.0f, FONT_SM);
    }

    /* ── f) FPS counter — top-right ──────────────────────── */
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d FPS", (int)(fps + 0.5f));
        float tw = (float)strlen(buf) * FONT_SIZE_SM * 0.6f;
        draw_text_ttf((float)winW - tw - 14.0f, 14.0f + FONT_SIZE_SM,
                      buf,
                      0.0f, 180.0f/255.0f, 150.0f/255.0f, 0.8f, FONT_SM);
    }

    /* ── g) Minimap — centered square panel, 70% of screen ── */
    if (showMinimap) {
        /* Wave pulse timer — diagonal wave from bottom-left to top-right */
        static float s_mapTime = 0.0f;
        s_mapTime += 1.0f / 60.0f;
        float breath = 0.5f + 0.5f * sinf(s_mapTime * 5.0f); /* border pulse */

        const int *grid = sonar_get_explored_grid();

        /* Square panel: 70% of the smaller screen dimension */
        float panelSz = (winW < winH ? (float)winW : (float)winH) * 0.7f;
        float panelX = ((float)winW - panelSz) * 0.5f;
        float panelY = ((float)winH - panelSz) * 0.5f;

        /* Panel background (dark, semi-transparent) */
        batch_quad(panelX, panelY, panelSz, panelSz,
                   0.0f, 0.01f, 0.03f, 0.88f);

        /* Thick border — deep blue, breathing alpha */
        float bw = 5.0f;
        float bAlpha = 0.5f + 0.4f * breath;
        batch_quad(panelX, panelY, panelSz, bw,
                   0.04f, 0.08f, 0.45f, bAlpha);
        batch_quad(panelX, panelY + panelSz - bw, panelSz, bw,
                   0.04f, 0.08f, 0.45f, bAlpha);
        batch_quad(panelX, panelY, bw, panelSz,
                   0.04f, 0.08f, 0.45f, bAlpha);
        batch_quad(panelX + panelSz - bw, panelY, bw, panelSz,
                   0.04f, 0.08f, 0.45f, bAlpha);

        /* Fit the grid inside the panel with some padding */
        float pad = panelSz * 0.05f;
        float innerW = panelSz - pad * 2.0f;
        float innerH = panelSz - pad * 2.0f;
        float scaleX = innerW / (float)EXPLORE_GRID_W;
        float scaleY = innerH / (float)EXPLORE_GRID_H;
        float cellPx = scaleX < scaleY ? scaleX : scaleY;

        float mapW = (float)EXPLORE_GRID_W * cellPx;
        float mapH = (float)EXPLORE_GRID_H * cellPx;
        float mapOx = panelX + (panelSz - mapW) * 0.5f;
        float mapOy = panelY + (panelSz - mapH) * 0.5f;

        /* Grid cells — diagonal wave: bright band sweeps bottom-left → top-right */
        /* Wave phase = time * speed - spatial offset along diagonal.
         * Diagonal axis: (gx / W + (H-1-gz) / H), so bottom-left=0, top-right=2.
         * Wave wavelength ~0.6 of diagonal, speed ~4.0 rad/s for brisk pulse. */
        float waveSpeed = 4.0f;
        float waveFreq  = 3.5f; /* spatial frequency along diagonal */
        for (int gz = 0; gz < EXPLORE_GRID_H; gz++) {
            for (int gx = 0; gx < EXPLORE_GRID_W; gx++) {
                if (!grid[gz * EXPLORE_GRID_W + gx]) continue;
                /* Diagonal coordinate: 0 at bottom-left, ~2 at top-right */
                float diag = (float)gx / (float)EXPLORE_GRID_W
                           + (float)(EXPLORE_GRID_H - 1 - gz) / (float)EXPLORE_GRID_H;
                float wave = sinf(s_mapTime * waveSpeed - diag * waveFreq);
                float w01 = 0.5f + 0.5f * wave; /* 0..1 */

                /* Blend between dim blue and bright blue based on wave */
                float cr = 0.03f + 0.12f * w01;
                float cg = 0.08f + 0.20f * w01;
                float cb = 0.25f + 0.45f * w01;
                float ca = 0.65f + 0.30f * w01;

                float sx = mapOx + (float)gx * cellPx;
                float sy = mapOy + (float)gz * cellPx;
                batch_quad(sx, sy, cellPx, cellPx, cr, cg, cb, ca);
            }
        }

        /* Player dot — bright blue */
        float px = (camPos[0] - EXPLORE_ORIGIN_X) / EXPLORE_CELL_SIZE;
        float pz = (camPos[2] - EXPLORE_ORIGIN_Z) / EXPLORE_CELL_SIZE;
        float dotX = mapOx + px * cellPx;
        float dotY = mapOy + pz * cellPx;
        float dotSz = cellPx * 0.7f;
        if (dotSz < 3.0f) dotSz = 3.0f;
        batch_quad(dotX - dotSz * 0.5f, dotY - dotSz * 0.5f, dotSz, dotSz,
                   0.3f, 0.5f, 1.0f, 1.0f);

        /* Player direction: line of quads from dot toward facing */
        float lineLen = cellPx * 2.5f;
        if (lineLen < 10.0f) lineLen = 10.0f;
        float dirX = sinf(playerYaw);
        float dirY = cosf(playerYaw);
        int steps = 5;
        float stepLen = lineLen / (float)steps;
        float qSz = cellPx * 0.25f;
        if (qSz < 1.5f) qSz = 1.5f;
        for (int s = 1; s <= steps; s++) {
            float cx = dotX + dirX * stepLen * (float)s;
            float cy = dotY + dirY * stepLen * (float)s;
            float t = 1.0f - (float)s / (float)(steps + 1);
            batch_quad(cx - qSz * 0.5f, cy - qSz * 0.5f, qSz, qSz,
                       0.2f, 0.4f, 1.0f, t);
        }

        /* ── Large energy bar below the panel ──────────────── */
        {
            float eBarW = panelSz * 0.8f;
            float eBarH = 8.0f;
            float ebx = panelX + (panelSz - eBarW) * 0.5f;
            float eby = panelY + panelSz + 12.0f;

            /* Background */
            batch_quad(ebx - 2.0f, eby - 2.0f,
                       eBarW + 4.0f, eBarH + 4.0f,
                       0.0f, 0.0f, 0.0f, 0.7f);

            /* Border — deep blue to match panel */
            batch_quad(ebx, eby, eBarW, 1.0f,
                       0.04f, 0.08f, 0.45f, 0.8f);
            batch_quad(ebx, eby + eBarH - 1.0f, eBarW, 1.0f,
                       0.04f, 0.08f, 0.45f, 0.8f);
            batch_quad(ebx, eby, 1.0f, eBarH,
                       0.04f, 0.08f, 0.45f, 0.8f);
            batch_quad(ebx + eBarW - 1.0f, eby, 1.0f, eBarH,
                       0.04f, 0.08f, 0.45f, 0.8f);

            /* Fill */
            float frac = energy / 100.0f;
            if (frac > 1.0f) frac = 1.0f;
            if (frac < 0.0f) frac = 0.0f;
            float fillW = frac * (eBarW - 2.0f);

            float fr, fg, fb;
            if (energy > 30.0f) {
                fr = 0.1f; fg = 0.3f; fb = 0.9f;
            } else {
                fr = 0.8f; fg = 0.25f; fb = 0.1f;
            }
            if (fillW > 0.0f) {
                batch_quad(ebx + 1.0f, eby + 1.0f,
                           fillW, eBarH - 2.0f,
                           fr, fg, fb, 0.9f);
            }

            /* "ENERGY" label */
            draw_text_ttf(ebx - 62.0f, eby + eBarH - 1.0f,
                          "ENERGY",
                          0.04f, 0.08f, 0.45f, 0.8f, FONT_SM);
        }
    }

    /* ── Flush both batches ───────────────────────────────── */
    batch_flush(winW, winH);
    text_flush(winW, winH);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void hud_shutdown(void)
{
    if (s_textVbo) { glDeleteBuffers(1, &s_textVbo); s_textVbo = 0; }
    if (s_textVao) { glDeleteVertexArrays(1, &s_textVao); s_textVao = 0; }
    if (s_fontTex) { glDeleteTextures(1, &s_fontTex); s_fontTex = 0; }
    if (s_textShader) { shader_destroy(s_textShader); s_textShader = 0; }
    if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_shader) { shader_destroy(s_shader); s_shader = 0; }
}
