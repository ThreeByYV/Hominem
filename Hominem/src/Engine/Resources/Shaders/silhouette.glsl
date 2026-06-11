#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[]; };
layout(location = 1) in vec2 a_TexCoord;

#include "includes/scene_ubo.glsl"

void main()
{
    gl_Position = u_ViewProjection * u_Model * u_SkinnedPos[gl_VertexID];
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 FragNormal;

void main()
{
    FragColor  = vec4(0.0, 0.0, 0.0, 1.0);
    FragNormal = vec4(0.0, 0.0, 1.0, 0.0);
}
