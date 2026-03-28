/**
 * timer.h — Frame timing and FPS measurement
 *
 * Uses SDL high-performance counters for delta-time calculation
 * and exponential-moving-average FPS.
 */
#pragma once

/** Initialize the timer subsystem. @return 0 on success, negative on error */
int timer_init(void);

/** Shut down the timer subsystem. */
void timer_shutdown(void);

/** Tick the timer. Call once per frame, after input_update. */
void timer_tick(void);

/** @return delta time in seconds since the last tick (clamped to 0.05s max). */
float timer_dt(void);

/** @return smoothed frames-per-second estimate. */
float timer_fps(void);
