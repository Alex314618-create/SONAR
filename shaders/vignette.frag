#version 330 core

uniform vec2 u_resolution;
uniform int  u_mode;
uniform float u_pulseRadius;
uniform float u_pulseAlpha;

out vec4 fragColor;

void main()
{
    if (u_mode == 0) {
        /* Scanlines: darken every 3rd row */
        if (int(gl_FragCoord.y) % 3 == 0)
            fragColor = vec4(0.0, 0.0, 0.0, 0.15);
        else
            discard;
    } else if (u_mode == 1) {
        /* Vignette */
        vec2 uv = gl_FragCoord.xy / u_resolution;
        vec2 d = uv - 0.5;
        float v = 1.0 - dot(d, d) * 3.2;
        v = clamp(v, 0.15, 1.0);
        fragColor = vec4(0.0, 0.0, 0.0, 1.0 - v);
    } else {
        /* Pulse ripple: expanding ellipse ring */
        vec2 center = u_resolution * 0.5;
        vec2 diff = gl_FragCoord.xy - center;
        diff.y /= 0.6;
        float dist = length(diff);
        float ring = abs(dist - u_pulseRadius);
        float alpha = smoothstep(3.0, 0.0, ring) * u_pulseAlpha;
        if (alpha < 0.01) discard;
        fragColor = vec4(0.0, 1.0, 0.867, alpha);
    }
}
