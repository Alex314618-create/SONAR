/**
 * timer.c — Frame timing and FPS measurement implementation
 */

#include "core/timer.h"

#include <SDL.h>

static Uint64 s_freq     = 0;
static Uint64 s_lastTick = 0;
static float  s_dt       = 0.0f;
static float  s_fps      = 0.0f;

int timer_init(void)
{
    s_freq     = SDL_GetPerformanceFrequency();
    s_lastTick = SDL_GetPerformanceCounter();
    s_dt       = 0.0f;
    s_fps      = 0.0f;
    return 0;
}

void timer_shutdown(void)
{
    /* nothing to release */
}

void timer_tick(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    s_dt = (float)(now - s_lastTick) / (float)s_freq;
    s_lastTick = now;

    /* Clamp to avoid spiral-of-death on breakpoints / hitches */
    if (s_dt > 0.05f) {
        s_dt = 0.05f;
    }

    /* Exponential moving average FPS */
    float instantFps = (s_dt > 0.0f) ? (1.0f / s_dt) : 0.0f;
    s_fps = s_fps * 0.9f + instantFps * 0.1f;
}

float timer_dt(void)
{
    return s_dt;
}

float timer_fps(void)
{
    return s_fps;
}
