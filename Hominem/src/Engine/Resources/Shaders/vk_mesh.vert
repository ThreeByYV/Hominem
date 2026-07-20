#version 450
#extension GL_EXT_buffer_reference : require

#include "include/scene_common.glsl"

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants
{
    mat4         u_Model;
    vec4         u_BaseColor;
    VertexBuffer u_Vertices;
    SceneBuffer  u_Scene;
    float        u_Unlit;
};

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;

void main()
{
    Vertex vert = u_Vertices.vertices[gl_VertexIndex];

    vec4 worldPos = u_Model * vec4(vert.px, vert.py, vert.pz, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal   = mat3(u_Model) * vec3(vert.nx, vert.ny, vert.nz);

    gl_Position = u_Scene.viewProj * worldPos;
}
