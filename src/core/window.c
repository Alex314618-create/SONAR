/**
 * window.c — SDL2/OpenGL window management implementation
 */

#include "core/window.h"
#include "core/log.h"

#include <glad/gl.h>
#include <SDL.h>

static SDL_Window   *s_window  = NULL;
static SDL_GLContext  s_glCtx  = NULL;
static int            s_shouldClose = 0;

int window_init(int width, int height, const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    if (!s_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return -2;
    }

    s_glCtx = SDL_GL_CreateContext(s_window);
    if (!s_glCtx) {
        LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return -3;
    }

    /* Enable vsync */
    SDL_GL_SetSwapInterval(1);

    /* Load OpenGL function pointers via glad */
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        LOG_ERROR("gladLoadGL failed");
        SDL_GL_DeleteContext(s_glCtx);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return -4;
    }

    LOG_INFO("OpenGL %s, GLSL %s",
             glGetString(GL_VERSION),
             glGetString(GL_SHADING_LANGUAGE_VERSION));

    s_shouldClose = 0;
    return 0;
}

void window_shutdown(void)
{
    if (s_glCtx) {
        SDL_GL_DeleteContext(s_glCtx);
        s_glCtx = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    SDL_Quit();
}

void window_swap(void)
{
    SDL_GL_SwapWindow(s_window);
}

int window_should_close(void)
{
    return s_shouldClose;
}

void window_set_should_close(int close)
{
    s_shouldClose = close;
}

void window_get_size(int *width, int *height)
{
    SDL_GetWindowSize(s_window, width, height);
}
