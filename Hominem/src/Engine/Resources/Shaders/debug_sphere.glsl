#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

out vec3 v_Center;

void main()
{
    v_Center    = a_Position;
    gl_Position = vec4(a_Position, 1.0); // passed through — TCS/TES handle projection
}

#type tcs
#version 460 core

layout(vertices = 1) out;

in  vec3 v_Center[];
out vec3 tc_Center[];

uniform float u_TessLevel; // controls sphere smoothness

void main()
{
    tc_Center[gl_InvocationID] = v_Center[gl_InvocationID];

    gl_TessLevelInner[0] = u_TessLevel;
    gl_TessLevelInner[1] = u_TessLevel;
    gl_TessLevelOuter[0] = u_TessLevel;
    gl_TessLevelOuter[1] = u_TessLevel;
    gl_TessLevelOuter[2] = u_TessLevel;
    gl_TessLevelOuter[3] = u_TessLevel;
}

#type tes
#version 460 core

layout(quads, equal_spacing, ccw) in;

in  vec3 tc_Center[];
out vec3 te_Normal;

#include "includes/scene_ubo.glsl"
uniform float u_Radius;

const float PI = 3.14159265359;

void main()
{
    // Map [0,1]x[0,1] tess coords to spherical coordinates
    float u = gl_TessCoord.x * 2.0 * PI;
    float v = gl_TessCoord.y * PI - PI * 0.5;

    vec3 dir = vec3(cos(v) * cos(u), sin(v), cos(v) * sin(u));
    te_Normal   = dir;
    gl_Position = u_ViewProjection * vec4(tc_Center[0] + dir * u_Radius, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in  vec3 te_Normal;

uniform vec4 u_Color;

void main()
{
    // Simple normal-based shading so the sphere reads as 3D, not flat
    float light = dot(normalize(te_Normal), normalize(vec3(1, 1, 1))) * 0.4 + 0.6;
    FragColor   = vec4(u_Color.rgb * light, u_Color.a);
}
