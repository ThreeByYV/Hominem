#version 450
#extension GL_EXT_buffer_reference : require

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

#define LIGHT_COUNT 1

layout(buffer_reference, std430) readonly buffer SceneBuffer
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 lightPos[LIGHT_COUNT];
    vec4 lightColor[LIGHT_COUNT];
    vec4 ambient;
};

layout(push_constant) uniform PushConstants
{
    mat4         u_Model;
    vec4         u_BaseColor;
    VertexBuffer u_Vertices;
    SceneBuffer  u_Scene;
    float        u_Unlit;
};

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;

layout(location = 0) out vec4 FragColor;

void main()
{
    if (u_Unlit > 0.5)
    {
        FragColor = vec4(u_BaseColor.rgb, 1.0);
        return;
    }

    vec3 N = normalize(v_Normal);

    vec3 color = u_Scene.ambient.rgb * u_BaseColor.rgb;

    for (int i = 0; i < LIGHT_COUNT; i++)
    {
        vec3  toLight = u_Scene.lightPos[i].xyz - v_WorldPos;
        float dist    = length(toLight);
        vec3  L       = toLight / max(dist, 0.0001);

        float radius = u_Scene.lightPos[i].w;
        float window = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
        float atten  = (window * window) / (dist * dist + 1.0);

        float NdotL = max(dot(N, L), 0.0);
        color += u_BaseColor.rgb * u_Scene.lightColor[i].rgb
               * u_Scene.lightColor[i].w * NdotL * atten;
    }

    FragColor = vec4(color, 1.0);
}
