/**
 * shader.c — OpenGL shader program loading and uniform setting implementation
 */

#include "render/shader.h"
#include "core/log.h"

#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

/* Read an entire file into a malloc'd null-terminated string. */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR("Cannot open file: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        LOG_ERROR("malloc failed for file: %s", path);
        fclose(f);
        return NULL;
    }

    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* Compile a single shader stage. Returns GL shader handle, or 0 on error. */
static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        LOG_ERROR("Shader compile error: %s", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

uint32_t shader_load(const char *vertPath, const char *fragPath)
{
    char *vertSrc = read_file(vertPath);
    char *fragSrc = read_file(fragPath);
    if (!vertSrc || !fragSrc) {
        free(vertSrc);
        free(fragSrc);
        return 0;
    }

    GLuint vert = compile_shader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fragSrc);
    free(vertSrc);
    free(fragSrc);

    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    /* Shaders can be detached/deleted after linking */
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        LOG_ERROR("Shader link error: %s", infoLog);
        glDeleteProgram(program);
        return 0;
    }

    return (uint32_t)program;
}

void shader_use(uint32_t program)
{
    glUseProgram(program);
}

void shader_set_mat4(uint32_t program, const char *name, const float *mat)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

void shader_set_vec3(uint32_t program, const char *name, const float *vec)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniform3fv(loc, 1, vec);
}

void shader_set_vec2(uint32_t program, const char *name, const float *vec)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniform2fv(loc, 1, vec);
}

void shader_set_mat3(uint32_t program, const char *name, const float *mat)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniformMatrix3fv(loc, 1, GL_FALSE, mat);
}

void shader_set_float(uint32_t program, const char *name, float value)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniform1f(loc, value);
}

void shader_set_int(uint32_t program, const char *name, int value)
{
    GLint loc = glGetUniformLocation(program, name);
    glUniform1i(loc, value);
}

void shader_destroy(uint32_t program)
{
    if (program) {
        glDeleteProgram(program);
    }
}
