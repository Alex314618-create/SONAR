/**
 * renderer.c — Core OpenGL renderer implementation
 */

#include "render/renderer.h"
#include "render/shader.h"
#include "core/window.h"
#include "core/log.h"

#include <glad/gl.h>

static uint32_t s_basicShader = 0;

int renderer_init(void)
{
    glEnable(GL_DEPTH_TEST);

    s_basicShader = shader_load("shaders/basic.vert", "shaders/basic.frag");
    if (!s_basicShader) {
        LOG_ERROR("Failed to load basic shader");
        return -1;
    }

    LOG_INFO("Renderer initialized");
    return 0;
}

void renderer_begin_frame(void)
{
    int w, h;
    window_get_size(&w, &h);
    glViewport(0, 0, w, h);

    /* Near-black clear color */
    glClearColor(0.008f, 0.008f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void renderer_end_frame(void)
{
    window_swap();
}

void renderer_shutdown(void)
{
    shader_destroy(s_basicShader);
    s_basicShader = 0;
}

uint32_t renderer_get_basic_shader(void)
{
    return s_basicShader;
}
