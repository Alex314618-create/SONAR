#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;

uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_camPos;

out vec3 v_color;

void main()
{
    vec4 viewPos = u_view * vec4(a_position, 1.0);
    gl_Position = u_proj * viewPos;

    /* Tiny sharp point — 1 to 3 px, barely grows at close range */
    float dist = length(a_position - u_camPos);
    float size = clamp(4.0 / (dist + 0.5), 1.0, 3.0);
    gl_PointSize = size;

    v_color = a_color;
}
