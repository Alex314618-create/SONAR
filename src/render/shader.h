/**
 * shader.h — OpenGL shader program loading and uniform setting
 *
 * Loads GLSL vertex/fragment shaders from files, compiles and links them,
 * and provides helpers for setting common uniform types.
 */
#pragma once

#include <stdint.h>

/**
 * Load and link a shader program from vertex and fragment shader files.
 *
 * @param vertPath  Path to the vertex shader source file
 * @param fragPath  Path to the fragment shader source file
 * @return GL program handle on success, 0 on error
 */
uint32_t shader_load(const char *vertPath, const char *fragPath);

/** Bind a shader program for use. */
void shader_use(uint32_t program);

/** Set a mat4 uniform. */
void shader_set_mat4(uint32_t program, const char *name, const float *mat);

/** Set a vec3 uniform. */
void shader_set_vec3(uint32_t program, const char *name, const float *vec);

/** Set a float uniform. */
void shader_set_float(uint32_t program, const char *name, float value);

/** Set a vec2 uniform. */
void shader_set_vec2(uint32_t program, const char *name, const float *vec);

/** Set a mat3 uniform. */
void shader_set_mat3(uint32_t program, const char *name, const float *mat);

/** Set an int uniform. */
void shader_set_int(uint32_t program, const char *name, int value);

/** Delete a shader program and free GPU resources. */
void shader_destroy(uint32_t program);
