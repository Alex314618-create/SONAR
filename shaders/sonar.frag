#version 330 core

in vec3 v_color;
out vec4 fragColor;

void main()
{
    /* Hard-edge circle — discard corners so 1px feels round, output full color */
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0) discard;

    fragColor = vec4(v_color, 1.0);
}
