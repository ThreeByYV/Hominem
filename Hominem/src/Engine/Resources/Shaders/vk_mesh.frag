#version 460
#extension GL_EXT_buffer_reference : require

#include "include/ddgi_common.glsl"
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

layout(set = 0, binding = 0) uniform sampler2D u_IrradianceAtlas;
layout(set = 0, binding = 1) uniform sampler2D u_DistanceAtlas;

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

    vec3 indirect = u_Scene.ambient.rgb;
    if (u_Scene.ddgiCounts.w > 0)
    {
        DDGIVolume v;
        v.origin   = u_Scene.ddgiOrigin;
        v.spacing  = u_Scene.ddgiSpacing;
        v.counts   = u_Scene.ddgiCounts;
        v.rotation = u_Scene.ddgiRotation;
        v.tiles    = u_Scene.ddgiTiles;

        float avgSpacing = (v.spacing.x + v.spacing.y + v.spacing.z) / 3.0;
        vec3  viewDir    = normalize(u_Scene.cameraPos.xyz - v_WorldPos);
        vec3  bias       = N * (0.25 * avgSpacing) + viewDir * (0.1 * avgSpacing);

        indirect = ddgiSampleIrradiance(v_WorldPos, N, bias,
                                        v.tiles.x, v.tiles.y, v,
                                        u_IrradianceAtlas, u_DistanceAtlas);
    }

    vec3 color = indirect * u_BaseColor.rgb;

    for (int i = 0; i < SCENE_LIGHT_COUNT; i++)
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
