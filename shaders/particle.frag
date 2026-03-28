#version 330 core

in vec3 v_color;
in float v_alpha;
out vec4 fragColor;

void main()
{
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0) discard;

    fragColor = vec4(v_color, v_alpha);
}
