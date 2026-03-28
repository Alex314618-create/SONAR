# SONAR — Render Module API Reference

> **Version**: 0.1.0
> **Status**: Accepted
> **Last Updated**: 2026-03-06
> **Owner**: CIO Agent
> **Source**: `src/render/`

---

## Table of Contents

1. [Overview](#1-overview)
2. [render/renderer](#2-renderrenderer)
3. [render/shader](#3-rendershader)
4. [render/camera](#4-rendercamera)
5. [render/mesh](#5-rendermesh)
6. [render/model](#6-rendermodel)
7. [Changelog](#7-changelog)

---

## 1. Overview

The `render/` layer owns all GPU state and draw calls. It depends on `core/` for the
window and logging, but must not depend on `audio/`, `sonar/`, or `ui/`.

**Frame lifecycle** (called from `main.c`):
```c
renderer_begin_frame();   // clear, set viewport
// ... draw calls ...
renderer_end_frame();     // swap buffers
```

**Vertex format** (universal across all current mesh data):

| Attribute | Location | Type  | Offset | Description |
|-----------|----------|-------|--------|-------------|
| position  | 0        | vec3  | 0      | World-space XYZ |
| normal    | 1        | vec3  | 12     | Surface normal |

Stride: **24 bytes** per vertex (6 floats).

---

## 2. render/renderer

**File**: `src/render/renderer.h` / `src/render/renderer.c`

**Purpose**: Manage global OpenGL state, load the built-in shader, and provide
frame begin/end helpers that wrap buffer clearing and buffer swapping.

### 2.1 Functions

---

#### `renderer_init`
```c
int renderer_init(void);
```
Initializes OpenGL state (depth test, face culling, viewport) and loads the
built-in lighting shader from `shaders/basic.vert` / `shaders/basic.frag`.

**Returns**: `0` on success, negative on error.

**Precondition**: `window_init()` must have been called successfully.

---

#### `renderer_begin_frame`
```c
void renderer_begin_frame(void);
```
Clears the color and depth buffers and sets the viewport to the current window size.
Call at the start of each frame before any draw calls.

---

#### `renderer_end_frame`
```c
void renderer_end_frame(void);
```
Calls `window_swap()` to present the rendered frame.

---

#### `renderer_shutdown`
```c
void renderer_shutdown(void);
```
Destroys the built-in shader program and releases renderer resources.

---

#### `renderer_get_basic_shader`
```c
uint32_t renderer_get_basic_shader(void);
```
**Returns**: The OpenGL program handle for the built-in lighting shader.
Used by `main.c` to set per-frame uniforms (MVP matrices, light direction, etc.).

---

## 3. render/shader

**File**: `src/render/shader.h` / `src/render/shader.c`

**Purpose**: Load GLSL vertex/fragment shaders from disk, compile and link them
into an OpenGL program, and provide typed helpers for setting uniform variables.

### 3.1 Functions

---

#### `shader_load`
```c
uint32_t shader_load(const char *vertPath, const char *fragPath);
```
Reads shader source files, compiles each stage, links the program, and logs
compile/link errors to `stderr`.

| Parameter  | Description |
|------------|-------------|
| `vertPath` | Path to `.vert` GLSL source file |
| `fragPath` | Path to `.frag` GLSL source file |

**Returns**: OpenGL program handle (`uint32_t`) on success; `0` on error.

---

#### `shader_use`
```c
void shader_use(uint32_t program);
```
Binds the shader program (`glUseProgram`). Pass `0` to unbind.

---

#### `shader_set_mat4`
```c
void shader_set_mat4(uint32_t program, const char *name, const float *mat);
```
Sets a `mat4` uniform. `mat` must point to 16 contiguous floats in column-major order
(cglm's native layout).

---

#### `shader_set_mat3`
```c
void shader_set_mat3(uint32_t program, const char *name, const float *mat);
```
Sets a `mat3` uniform. `mat` must point to 9 contiguous floats in column-major order.
Used for the normal matrix (`transpose(inverse(model))`).

---

#### `shader_set_vec3`
```c
void shader_set_vec3(uint32_t program, const char *name, const float *vec);
```
Sets a `vec3` uniform. `vec` must point to 3 contiguous floats.

---

#### `shader_set_float`
```c
void shader_set_float(uint32_t program, const char *name, float value);
```
Sets a `float` uniform.

---

#### `shader_set_int`
```c
void shader_set_int(uint32_t program, const char *name, int value);
```
Sets an `int` uniform. Used for texture sampler bindings.

---

#### `shader_destroy`
```c
void shader_destroy(uint32_t program);
```
Calls `glDeleteProgram`. Safe to call with `0` (no-op).

---

## 4. render/camera

**File**: `src/render/camera.h` / `src/render/camera.c`

**Purpose**: Euler-angle first-person camera. Stores position and orientation,
and computes view and projection matrices for the renderer.

### 4.1 Types

#### `Camera`
```c
typedef struct Camera {
    vec3  position;    // World-space camera position
    vec3  front;       // Normalized forward direction
    vec3  up;          // Normalized up direction (world up after Gram-Schmidt)
    vec3  right;       // Normalized right direction
    float yaw;         // Horizontal rotation, degrees (0 = +X axis, 90 = +Z axis)
    float pitch;       // Vertical rotation, degrees (clamped to [-89, 89])
    float fov;         // Vertical field of view, degrees
    float nearPlane;   // Near clip distance
    float farPlane;    // Far clip distance
} Camera;
```

### 4.2 Functions

---

#### `camera_init`
```c
void camera_init(Camera *cam, float posX, float posY, float posZ,
                 float yaw, float pitch);
```
Initializes the camera at the given world position with the specified orientation.
Sets default FOV (60°), near plane (0.1), and far plane (200.0). Computes initial
`front`, `right`, and `up` vectors from `yaw`/`pitch`.

| Parameter | Description |
|-----------|-------------|
| `cam`     | Camera struct to initialize |
| `posX/Y/Z` | Initial world-space position |
| `yaw`     | Initial yaw in degrees |
| `pitch`   | Initial pitch in degrees |

---

#### `camera_update`
```c
void camera_update(Camera *cam, float dx, float dy);
```
Applies mouse movement to update `yaw` and `pitch`, then recomputes `front`,
`right`, and `up` vectors. Pitch is clamped to [-89°, 89°] to prevent gimbal lock.

| Parameter | Description |
|-----------|-------------|
| `cam`     | Camera to update |
| `dx`      | Horizontal mouse delta (pixels, positive = right) |
| `dy`      | Vertical mouse delta (pixels, positive = down) |

---

#### `camera_view_matrix`
```c
void camera_view_matrix(const Camera *cam, mat4 dest);
```
Computes a look-at view matrix from `position`, `position + front`, and `up`.
Stores the result in `dest` (cglm `mat4`, column-major).

---

#### `camera_proj_matrix`
```c
void camera_proj_matrix(const Camera *cam, float aspect, mat4 dest);
```
Computes a perspective projection matrix.

| Parameter | Description |
|-----------|-------------|
| `cam`     | Provides `fov`, `nearPlane`, `farPlane` |
| `aspect`  | Viewport width / height |
| `dest`    | Output cglm `mat4` |

---

## 5. render/mesh

**File**: `src/render/mesh.h` / `src/render/mesh.c`

**Purpose**: VAO/VBO/EBO wrapper. Uploads interleaved vertex data to the GPU and
provides a single draw call. Vertex format: `[px,py,pz, nx,ny,nz]` (6 floats, 24 bytes).

### 5.1 Types

#### `Mesh`
```c
typedef struct {
    uint32_t vao;          // Vertex Array Object handle
    uint32_t vbo;          // Vertex Buffer Object handle
    uint32_t ebo;          // Element Buffer Object handle (0 if non-indexed)
    int      indexCount;   // Number of indices (0 if non-indexed)
    int      vertexCount;  // Number of vertices
} Mesh;
```

### 5.2 Functions

---

#### `mesh_create`
```c
Mesh mesh_create(const float *verts, int vertCount,
                 const uint32_t *indices, int idxCount);
```
Allocates a VAO + VBO (+ EBO if `indices != NULL`), uploads data with `GL_STATIC_DRAW`,
and configures vertex attribute pointers.

| Parameter   | Description |
|-------------|-------------|
| `verts`     | Interleaved `[px,py,pz, nx,ny,nz]` vertex data |
| `vertCount` | Number of vertices |
| `indices`   | Index array, or `NULL` for non-indexed draw |
| `idxCount`  | Number of indices (ignored if `indices` is `NULL`) |

**Returns**: Initialized `Mesh` with valid GL handles. On failure, handles will be `0`.

---

#### `mesh_draw`
```c
void mesh_draw(const Mesh *m);
```
Binds the VAO and issues `glDrawElements` (indexed) or `glDrawArrays` (non-indexed),
then unbinds the VAO.

---

#### `mesh_destroy`
```c
void mesh_destroy(Mesh *m);
```
Calls `glDeleteVertexArrays`, `glDeleteBuffers` for VBO and EBO, then zeros all
fields in `m`.

---

## 6. render/model

**File**: `src/render/model.h` / `src/render/model.c`

**Purpose**: Load a glTF 2.0 file (`.gltf` or `.glb`) via cgltf, extract position
and normal data from all mesh primitives, and create a `Mesh` array for rendering.

### 6.1 Types

#### `Model`
```c
typedef struct {
    Mesh *meshes;    // Heap-allocated array of Mesh objects
    int   meshCount; // Number of meshes (0 on load failure)
} Model;
```

### 6.2 Functions

---

#### `model_load`
```c
Model model_load(const char *path);
```
Parses the glTF file at `path`, iterates all mesh primitives, de-interleaves
position and normal accessor data, and uploads each primitive as a `Mesh`.

| Parameter | Description |
|-----------|-------------|
| `path`    | Filesystem path to `.gltf` or `.glb` file |

**Returns**: `Model` with `meshCount > 0` on success. On failure, returns a `Model`
with `meshes == NULL` and `meshCount == 0`.

**Note**: Currently extracts `POSITION` and `NORMAL` attributes only. Material data
and textures are ignored (planned for a future milestone).

---

#### `model_draw`
```c
void model_draw(const Model *m, uint32_t shader);
```
Calls `mesh_draw()` for each mesh in the model.

| Parameter | Description |
|-----------|-------------|
| `m`       | Model to render |
| `shader`  | Bound shader program handle (currently unused; reserved for per-mesh materials) |

---

#### `model_destroy`
```c
void model_destroy(Model *m);
```
Calls `mesh_destroy()` on each mesh, frees the `meshes` array, and zeros `m`.

---

## 7. Changelog

| Date       | Version | Changes |
|------------|---------|---------|
| 2026-03-06 | 0.1.0   | Initial API documentation extracted from M1/M2 implementation |
