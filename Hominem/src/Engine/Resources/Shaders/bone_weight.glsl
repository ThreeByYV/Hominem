#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };

struct VertexBoneData { ivec4 BoneIDs; vec4 Weights; };
layout(std430, binding = 3) readonly buffer InBoneData { VertexBoneData u_BoneData[]; };

layout(location = 1) in vec2 a_TexCoord;

#include "includes/scene_ubo.glsl"

flat out ivec4 BoneIDs0;
out      vec4  Weights0;
out      vec2  TexCoord0;
out      vec3  Normal0;

void main()
{
    vec4 worldPos  = u_Model * u_SkinnedPos[gl_VertexID];
    Normal0        = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
    TexCoord0      = a_TexCoord;
    BoneIDs0       = u_BoneData[gl_VertexID].BoneIDs;
    Weights0       = u_BoneData[gl_VertexID].Weights;
    gl_Position    = u_ViewProjection * worldPos;
}



#type fragment
#version 460 core

layout(location = 0) out vec4 color;

flat in ivec4 BoneIDs0;
in      vec4  Weights0;
in      vec2  TexCoord0;
in      vec3  Normal0;

uniform vec4      u_Color;
uniform sampler2D u_Albedo;
uniform int       gDisplayBoneIndex;

void main()
{
    bool found = false;

    for (int i = 0; i < 4; i++)
    {
        if (BoneIDs0[i] == gDisplayBoneIndex)
        {
            if      (Weights0[i] >= 0.7) color = vec4(1.0, 0.0, 0.0, 1.0); // red   — strong
            else if (Weights0[i] >= 0.4) color = vec4(0.0, 1.0, 0.0, 1.0); // green — medium
            else if (Weights0[i] >= 0.1) color = vec4(1.0, 1.0, 0.0, 1.0); // yellow — weak
            else                         color = vec4(0.0, 0.0, 1.0, 1.0); // blue  — trace
            found = true;
            break;
        }
    }

    if (!found)
    {
        vec4  texColor = texture(u_Albedo, TexCoord0);
        vec3  lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff     = max(dot(normalize(Normal0), lightDir), 0.9);
        color = texColor * u_Color * diff;
    }
}
