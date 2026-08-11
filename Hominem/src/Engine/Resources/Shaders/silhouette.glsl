#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions     { vec4 u_SkinnedPos[];     };
layout(std430, binding = 6) readonly buffer PrevSkinnedPositions { vec4 u_PrevSkinnedPos[]; };
layout(location = 1) in vec2 a_TexCoord;

#include "includes/scene_ubo.glsl"

out vec4 v_ClipCurr;
out vec4 v_ClipPrev;

void main()
{
    vec4 worldPos     = u_Model * u_SkinnedPos[gl_VertexID];
    vec4 prevWorldPos = u_PrevM * u_PrevSkinnedPos[gl_VertexID];

    gl_Position = u_ViewProjection * worldPos;

    v_ClipCurr = u_ViewProjectionUnjittered * worldPos;
    v_ClipPrev = u_PrevViewProjection       * prevWorldPos;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 FragVelocity;

in vec4 v_ClipCurr;
in vec4 v_ClipPrev;

void main()
{
    FragColor    = vec4(0.0, 0.0, 0.0, 1.0);
    FragVelocity = vec4((v_ClipCurr.xy / v_ClipCurr.w - v_ClipPrev.xy / v_ClipPrev.w) * 0.5,
                        0.0, 0.0);
}
