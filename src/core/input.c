/**
 * input.c — Keyboard and mouse input handling implementation
 */

#include "core/input.h"
#include "core/window.h"

#include <SDL.h>
#include <string.h>

static const Uint8 *s_keyState   = NULL;
static Uint8        s_prevKeys[SDL_NUM_SCANCODES];
static int          s_mouseDx    = 0;
static int          s_mouseDy    = 0;
static Uint32       s_mouseState = 0;
static Uint32       s_prevMouse  = 0;

int input_init(void)
{
    s_keyState = SDL_GetKeyboardState(NULL);
    memset(s_prevKeys, 0, sizeof(s_prevKeys));
    s_mouseDx = 0;
    s_mouseDy = 0;
    return 0;
}

void input_shutdown(void)
{
    /* nothing to release */
}

void input_update(void)
{
    /* Save previous key state for edge detection */
    memcpy(s_prevKeys, s_keyState, SDL_NUM_SCANCODES);
    s_prevMouse = s_mouseState;

    /* Reset mouse delta accumulator */
    s_mouseDx = 0;
    s_mouseDy = 0;

    /* Poll all events */
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            window_set_should_close(1);
            break;
        case SDL_MOUSEMOTION:
            s_mouseDx += ev.motion.xrel;
            s_mouseDy += ev.motion.yrel;
            break;
        default:
            break;
        }
    }

    s_mouseState = SDL_GetMouseState(NULL, NULL);
}

int input_key_down(int scancode)
{
    return s_keyState[scancode];
}

int input_key_pressed(int scancode)
{
    return s_keyState[scancode] && !s_prevKeys[scancode];
}

void input_mouse_delta(int *dx, int *dy)
{
    if (dx) *dx = s_mouseDx;
    if (dy) *dy = s_mouseDy;
}

int input_mouse_down(int button)
{
    return (s_mouseState & SDL_BUTTON(button)) != 0;
}

int input_mouse_pressed(int button)
{
    Uint32 mask = SDL_BUTTON(button);
    return (s_mouseState & mask) && !(s_prevMouse & mask);
}

void input_set_mouse_captured(int captured)
{
    SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
}
