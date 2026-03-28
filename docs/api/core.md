# SONAR — Core Module API Reference

> **Version**: 0.1.0
> **Status**: Accepted
> **Last Updated**: 2026-03-06
> **Owner**: CIO Agent
> **Source**: `src/core/`

---

## Table of Contents

1. [Overview](#1-overview)
2. [core/window](#2-corewindow)
3. [core/input](#3-coreinput)
4. [core/timer](#4-coretimer)
5. [core/log](#5-corelog)
6. [Changelog](#6-changelog)

---

## 1. Overview

The `core/` layer is the **foundation** of the SONAR engine. It provides window
management, input handling, frame timing, and logging. All other modules depend on
`core/`; `core/` itself depends on nothing within the project.

**Initialization order** (from `main.c`):
```c
window_init(width, height, title);
renderer_init();    // render/ layer — depends on window being ready
input_init();
timer_init();
```

**Shutdown order** (reverse of init):
```c
timer_shutdown();
input_shutdown();
renderer_shutdown();
window_shutdown();
```

---

## 2. core/window

**File**: `src/core/window.h` / `src/core/window.c`

**Purpose**: Create and manage the SDL2 application window and OpenGL 3.3 Core context.
Owns the swap chain (buffer presentation).

### 2.1 Functions

---

#### `window_init`
```c
int window_init(int width, int height, const char *title);
```
Creates an SDL2 window with an OpenGL 3.3 Core Profile context.

| Parameter | Type | Description |
|-----------|------|-------------|
| `width`   | `int` | Window width in pixels |
| `height`  | `int` | Window height in pixels |
| `title`   | `const char *` | Window title string |

**Returns**: `0` on success, negative on error (SDL not initialized, context creation
failed, etc.).

**Side effects**: Initializes SDL2 video subsystem, creates GL context, loads glad.

---

#### `window_shutdown`
```c
void window_shutdown(void);
```
Destroys the GL context, SDL window, and shuts down the SDL video subsystem.
Call this last in the shutdown sequence.

---

#### `window_swap`
```c
void window_swap(void);
```
Presents the current back buffer to the screen (SDL_GL_SwapWindow).
Called by `renderer_end_frame()` — do not call directly in game code.

---

#### `window_should_close`
```c
int window_should_close(void);
```
**Returns**: Non-zero if the window has been requested to close (e.g. by SDL_QUIT or ESC).

---

#### `window_set_should_close`
```c
void window_set_should_close(int close);
```
Sets the internal close flag. Pass non-zero to request application exit.

| Parameter | Description |
|-----------|-------------|
| `close`   | Non-zero to request close, zero to clear |

---

#### `window_get_size`
```c
void window_get_size(int *width, int *height);
```
Retrieves the current window dimensions.

| Parameter | Description |
|-----------|-------------|
| `width`   | Output: current width in pixels |
| `height`  | Output: current height in pixels |

---

## 3. core/input

**File**: `src/core/input.h` / `src/core/input.c`

**Purpose**: Poll SDL2 events and maintain per-frame keyboard/mouse state with
edge detection (pressed vs. held). Mouse movement is accumulated as a delta.

### 3.1 Functions

---

#### `input_init`
```c
int input_init(void);
```
Initializes internal key state arrays and mouse delta accumulators.

**Returns**: `0` on success, negative on error.

---

#### `input_shutdown`
```c
void input_shutdown(void);
```
Releases input subsystem resources.

---

#### `input_update`
```c
void input_update(void);
```
Polls SDL events and updates all key states and mouse deltas.
**Must be called exactly once per frame**, before any input queries.

**Side effects**: Calls `window_set_should_close(1)` on SDL_QUIT or Escape.

---

#### `input_key_down`
```c
int input_key_down(int scancode);
```
**Returns**: Non-zero if the key identified by `scancode` is currently held down.

| Parameter | Description |
|-----------|-------------|
| `scancode` | SDL_Scancode value (e.g. `SDL_SCANCODE_W`) |

---

#### `input_key_pressed`
```c
int input_key_pressed(int scancode);
```
**Returns**: Non-zero if the key was **just pressed** this frame (edge detection —
true for one frame only).

| Parameter | Description |
|-----------|-------------|
| `scancode` | SDL_Scancode value |

---

#### `input_mouse_delta`
```c
void input_mouse_delta(int *dx, int *dy);
```
Returns accumulated mouse movement since the last `input_update()` call.

| Parameter | Description |
|-----------|-------------|
| `dx` | Output: horizontal pixel delta (positive = right) |
| `dy` | Output: vertical pixel delta (positive = down) |

---

#### `input_set_mouse_captured`
```c
void input_set_mouse_captured(int captured);
```
Enables or disables SDL relative mouse mode (mouse lock to window).

| Parameter | Description |
|-----------|-------------|
| `captured` | Non-zero to capture (hide cursor, use relative motion); zero to release |

**Usage**: Call with `1` at game start for FPS look. Call with `0` when showing menus.

---

## 4. core/timer

**File**: `src/core/timer.h` / `src/core/timer.c`

**Purpose**: Frame timing using SDL high-performance counters. Provides delta time
for physics/movement and a smoothed FPS estimate for diagnostics.

### 4.1 Functions

---

#### `timer_init`
```c
int timer_init(void);
```
Records the initial timestamp using SDL_GetPerformanceCounter.

**Returns**: `0` on success, negative on error.

---

#### `timer_shutdown`
```c
void timer_shutdown(void);
```
Releases timer subsystem resources (no-op in current implementation, present
for symmetry with the module init/shutdown pattern).

---

#### `timer_tick`
```c
void timer_tick(void);
```
Updates the delta time and FPS estimate. **Must be called once per frame**,
typically after `input_update()` and before game logic updates.

---

#### `timer_dt`
```c
float timer_dt(void);
```
**Returns**: Time elapsed since the previous `timer_tick()` call, in seconds.
Clamped to a maximum of `0.05s` (20 FPS floor) to prevent physics tunneling
during frame spikes or debugger pauses.

---

#### `timer_fps`
```c
float timer_fps(void);
```
**Returns**: Smoothed frames-per-second estimate using an exponential moving
average. Suitable for display in a debug HUD.

---

## 5. core/log

**File**: `src/core/log.h`

**Purpose**: Lightweight logging macros that write to `stderr`. This is a
temporary implementation; a structured logging module is planned.

### 5.1 Macros

```c
LOG_INFO(fmt, ...)   // Prints: [INFO]  <message>\n  to stderr
LOG_ERROR(fmt, ...)  // Prints: [ERROR] <message>\n  to stderr
```

**Usage**:
```c
LOG_INFO("Renderer initialized: OpenGL %s", glGetString(GL_VERSION));
LOG_ERROR("Failed to open file: %s", path);
```

**Note**: These macros expand to `fprintf(stderr, ...)`. They are not thread-safe
and do not support log levels or file output. A proper `core/log` module is listed
as a future task.

---

## 6. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-06 | 0.1.0   | Initial API documentation extracted from M1/M2 implementation |
