#version 330 core

uniform sampler2D u_tex;

in vec2 v_uv;
in vec4 v_color;

out vec4 fragColor;

void main()
{
    fragColor = vec4(v_color.rgb, v_color.a * texture(u_tex, v_uv).r);
}
