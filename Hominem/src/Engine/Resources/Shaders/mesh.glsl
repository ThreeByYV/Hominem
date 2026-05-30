#type vertex
#version 460 core

#ifdef SKINNED
    // Positions and normals are written by the skinning compute shader before this
    // draw call. Indexed by gl_VertexID — no VAO vertex fetch for these attributes.
    layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
    layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };
    layout(location = 1) in vec2 a_TexCoord;
#else
    // Static: standard vertex attributes
    layout(location = 0) in vec3 a_Position;
    layout(location = 1) in vec3 a_Normal;
    layout(location = 2) in vec2 a_TexCoord;
    layout(location = 3) in vec4 a_Tangent;   // always in VAO; read only when HAS_NORMAL_MAP
#endif

#include "includes/scene_ubo.glsl"

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    out vec4 v_Tangent;
#endif

void main()
{
#ifdef SKINNED
    vec4 worldPos = u_Model * u_SkinnedPos[gl_VertexID];
    v_Normal      = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
#else
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_Normal      = mat3(u_Model) * a_Normal;
#ifdef HAS_NORMAL_MAP
    v_Tangent     = vec4(mat3(u_Model) * a_Tangent.xyz, a_Tangent.w);
#endif
#endif

    v_WorldPos  = worldPos.xyz;
    v_TexCoord  = a_TexCoord;
    gl_Position = u_ViewProjection * worldPos;
}



#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    in vec4 v_Tangent;
#endif

#include "includes/scene_ubo.glsl"
#include "includes/types.glsl"
#include "includes/pbr.glsl"
#include "includes/env_mapping.glsl"

uniform sampler2D u_Albedo;      // slot 0 — always present
uniform float     u_Roughness;   // scalar fallback when no MR texture
uniform float     u_Metalness;

#ifdef HAS_METALROUGHNESS_TEX
    uniform sampler2D u_MetalRoughness; // slot 1
#endif

#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    uniform sampler2D u_NormalMap;      // slot 2
#endif

#ifdef FORWARD_PLUS
    layout(std430, binding = 1) readonly buffer LightBuffer          { GPUPointLight  lights[];        };
    layout(std430, binding = 2) readonly buffer LightIndexListBuffer { uint            lightIndexList[]; };
    layout(std430, binding = 3) readonly buffer LightGridBuffer      { LightGridEntry  lightGrid[];     };
#else
    #define MAX_POINT_LIGHTS 16
    uniform int   u_PointLightCount;
    uniform vec3  u_PointLightPositions[MAX_POINT_LIGHTS];
    uniform vec3  u_PointLightColors[MAX_POINT_LIGHTS];
    uniform float u_PointLightIntensities[MAX_POINT_LIGHTS];
    uniform float u_PointLightRadii[MAX_POINT_LIGHTS];
#endif


void main()
{
    vec4 albedoSample = texture(u_Albedo, v_TexCoord);
    vec3 albedo       = albedoSample.rgb;

#ifdef SKINNED
    if (dot(albedo, albedo) < 0.001) albedo = vec3(0.8, 0.6, 0.4);
#endif

#ifdef HAS_METALROUGHNESS_TEX
    vec2  mr        = texture(u_MetalRoughness, v_TexCoord).gb;
    float roughness = clamp(mr.x, 0.05, 1.0);
    float metalness = clamp(mr.y, 0.0,  1.0);
#else
    float roughness = clamp(u_Roughness, 0.05, 1.0);
    float metalness = clamp(u_Metalness, 0.0,  1.0);
#endif

#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    vec3 vN = normalize(v_Normal);
    vec3 vT = normalize(v_Tangent.xyz);
    vT      = normalize(vT - dot(vT, vN) * vN);
    vec3 vB = cross(vN, vT) * v_Tangent.w;
    vec3 normalSample = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;
    normalSample.y = -normalSample.y; // DX → OpenGL normal map convention
    vec3 N  = normalize(mat3(vT, vB, vN) * normalSample);
#else
    vec3 N = normalize(v_Normal);
#endif

    vec3 V     = normalize(u_CameraWorldPos.xyz - v_WorldPos);
    vec3 color = u_AmbientColor.xyz * u_AmbientIntensity * albedo;

    // Directional light
    color += evalPBR(N, V, normalize(-u_LightDirection.xyz), albedo, roughness, metalness,
                     u_LightColor.xyz * u_DiffuseIntensity);

    // Point lights
#ifdef FORWARD_PLUS
    uint numTilesX   = (u_ScreenWidth + 15u) / 16u;
    uint tileIdx     = (uint(gl_FragCoord.y) / 16u) * numTilesX + (uint(gl_FragCoord.x) / 16u);
    uint lightOffset = lightGrid[tileIdx].offset;
    uint lightCount  = lightGrid[tileIdx].count;

    for (uint i = 0u; i < lightCount; i++)
    {
        uint  idx     = lightIndexList[lightOffset + i];
        vec3  Lp      = lights[idx].positionAndRadius.xyz - v_WorldPos;
        float dist    = length(Lp);
        float radius  = lights[idx].positionAndRadius.w;
        float falloff = clamp(1.0 - (dist / radius) * (dist / radius), 0.0, 1.0);
        falloff      *= falloff;
        vec3 radiance = lights[idx].colorAndIntensity.xyz
                      * lights[idx].colorAndIntensity.w
                      * (falloff / max(dist * dist, 0.0001));
        color += evalPBR(N, V, normalize(Lp), albedo, roughness, metalness, radiance);
    }
#else
    for (int i = 0; i < u_PointLightCount; i++)
    {
        vec3  toLight     = u_PointLightPositions[i] - v_WorldPos;
        float dist        = length(toLight);
        float r           = u_PointLightRadii[i];
        float falloff     = clamp(1.0 - (dist / r) * (dist / r), 0.0, 1.0);
        falloff          *= falloff;
        vec3  radiance    = u_PointLightColors[i] * u_PointLightIntensities[i]
                          * (falloff / max(dist * dist, 0.0001));
        color += evalPBR(N, V, normalize(toLight), albedo, roughness, metalness, radiance);
    }
#endif

    // Environment map
#ifdef HAS_ENV_MAP
    color = ApplyEnvMapping(color, N, V, roughness, metalness);
#endif

    // Forward+ heatmap debug
#ifdef FORWARD_PLUS
    if (u_DebugMode == 1)
    {
        bool onBorder = (uint(gl_FragCoord.x) % 16u == 0u) || (uint(gl_FragCoord.y) % 16u == 0u);
        if (onBorder) { FragColor = vec4(1.0); return; }
        float t = clamp(float(lightCount) / 8.0, 0.0, 1.0);
        vec3 heat;
        if      (t < 0.25) heat = mix(vec3(0,0,0), vec3(0,0,1), t * 4.0);
        else if (t < 0.50) heat = mix(vec3(0,0,1), vec3(0,1,0), (t-0.25)*4.0);
        else if (t < 0.75) heat = mix(vec3(0,1,0), vec3(1,1,0), (t-0.50)*4.0);
        else               heat = mix(vec3(1,1,0), vec3(1,0,0), (t-0.75)*4.0);
        FragColor = vec4(heat, 1.0);
        return;
    }
#endif

    FragColor = vec4(color, albedoSample.a);
}
