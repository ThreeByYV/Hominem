#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };

layout(location = 1) in vec2 a_TexCoord;

#include "includes/scene_ubo.glsl"

out vec3 v_WorldPos;
out vec3 v_Normal;

void main()
{
    v_WorldPos = (u_Model * u_SkinnedPos[gl_VertexID]).xyz;
    v_Normal   = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
    // Pass clip position — geometry shader will offset
    gl_Position = u_ViewProjection * vec4(v_WorldPos, 1.0);
}



#type geometry
#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

#include "includes/scene_ubo.glsl"

uniform float u_OutlineThickness;

in vec3 v_WorldPos[];
in vec3 v_Normal[];

void main()
{
    for (int i = 0; i < 3; i++)
    {
        // Extrude vertex along its normal in clip space
        vec4 clipPos  = u_ViewProjection * vec4(v_WorldPos[i], 1.0);
        vec4 clipNorm = u_ViewProjection * vec4(v_Normal[i], 0.0);

        // Offset in clip space along the normal direction (screen-space outline)
        vec2 offset   = normalize(clipNorm.xy) * u_OutlineThickness * clipPos.w;
        clipPos.xy   += offset;

        gl_Position = clipPos;
        EmitVertex();
    }
    EndPrimitive();
}



#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

uniform vec4 u_OutlineColor;

void main()
{
    FragColor = u_OutlineColor;
}
