#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec3 a_color;

uniform mat4 u_ortho;

out vec3 v_color;

void main()
{
    gl_Position = u_ortho * vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
}
