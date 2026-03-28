#version 330 core

in vec3 v_worldNormal;
in vec3 v_worldPos;

uniform vec3 u_lightDir;
uniform vec3 u_lightColor;
uniform vec3 u_ambientColor;
uniform vec3 u_objectColor;

out vec4 fragColor;

void main()
{
    vec3 norm = normalize(v_worldNormal);
    float diff = max(dot(norm, u_lightDir), 0.0);
    vec3 result = (u_ambientColor + diff * u_lightColor) * u_objectColor;
    fragColor = vec4(result, 1.0);
}
