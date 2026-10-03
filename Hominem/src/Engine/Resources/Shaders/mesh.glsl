#type vertex
#version 460 core

#ifdef SKINNED
   // Positions and normals are written by the skinning compute shader before this
   // draw call. Indexed by gl_VertexID — no VAO vertex fetch for these attributes.
    layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
    layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };
    // Last frame's skinned output, kept so velocity picks up limb motion and not just the
    // actor's transform. Written by the same compute pass, one frame behind.
    layout(std430, binding = 6) readonly buffer PrevSkinnedPositions { vec4 u_PrevSkinnedPos[]; };
    layout(location = 1) in vec2 a_TexCoord;
#else
    // Model matrices uploaded once per draw call batch; gl_DrawID indexes within the batch,
    // u_BaseModelIndex offsets to the correct position in the global SSBO.
    layout(std430, binding = 5) readonly buffer ModelMatrices     { mat4 b_Models[];     };
    layout(std430, binding = 6) readonly buffer PrevModelMatrices { mat4 b_PrevModels[]; };
    uniform int u_BaseModelIndex;

    layout(location = 0) in vec3 a_Position;
    layout(location = 1) in vec3 a_Normal;
    layout(location = 2) in vec2 a_TexCoord;
    layout(location = 3) in vec4 a_Tangent;
#endif

#include "includes/scene_ubo.glsl"

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_ClipCurr;
out vec4 v_ClipPrev;

#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    out vec4 v_Tangent;
#endif

void main()
{
#ifdef SKINNED
    vec4 worldPos     = u_Model * u_SkinnedPos[gl_VertexID];
    vec4 prevWorldPos = u_PrevM * u_PrevSkinnedPos[gl_VertexID];
    v_Normal          = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
#else
    uint modelIdx     = uint(u_BaseModelIndex) + uint(gl_DrawID);
    mat4 model        = b_Models[modelIdx];
    vec4 worldPos     = model * vec4(a_Position, 1.0);
    vec4 prevWorldPos = b_PrevModels[modelIdx] * vec4(a_Position, 1.0);
    v_Normal          = mat3(model) * a_Normal;
#ifdef HAS_NORMAL_MAP
    v_Tangent     = vec4(mat3(model) * a_Tangent.xyz, a_Tangent.w);
#endif
#endif

    v_WorldPos  = worldPos.xyz;
    v_TexCoord  = a_TexCoord;
    gl_Position = u_ViewProjection * worldPos;

    v_ClipCurr = u_ViewProjectionUnjittered * worldPos;
    v_ClipPrev = u_PrevViewProjection       * prevWorldPos;
}



#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 FragVelocity;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_ClipCurr;
in vec4 v_ClipPrev;
#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    in vec4 v_Tangent;
#endif

#include "includes/scene_ubo.glsl"
#include "includes/types.glsl"
#include "includes/pbr.glsl"
#include "includes/env_mapping.glsl"
#ifdef HAS_ENV_MAP
    #include "includes/irradiance.glsl"
#endif
#include "includes/ddgi_common.glsl"

    // Ray-traced irradiance field, written by Vulkan into memory this context imports.
    // u_DDGICounts.w == 0 means no volume this frame; the atlases are then unbound.
    uniform vec4      u_DDGIOrigin;
    uniform vec4      u_DDGISpacing;
    uniform vec4      u_DDGICounts;  // xyz = probe counts, w = rays per probe
    uniform vec4      u_DDGITiles;   // x = irradiance interior, y = distance interior
    uniform int       u_DDGIDebug;   // 1 = shade with the raw irradiance, nothing else
    uniform float     u_DDGIIntensity;
    uniform sampler2D u_DDGIIrradiance; // slot 7
    uniform sampler2D u_DDGIDistance;   // slot 8

    vec3 SampleDDGI(vec3 worldPos, vec3 N, vec3 V)
    {
        DDGIVolume v;
        v.origin   = u_DDGIOrigin;
        v.spacing  = u_DDGISpacing;
        v.counts   = ivec4(u_DDGICounts);
        v.rotation = vec4(0.0, 0.0, 0.0, 1.0); // only the trace needs the ray rotation
        v.tiles    = ivec4(u_DDGITiles);

        float avgSpacing = (v.spacing.x + v.spacing.y + v.spacing.z) / 3.0;
        vec3  bias       = N * (0.25 * avgSpacing) + V * (0.1 * avgSpacing);

        return ddgiSampleIrradiance(worldPos, N, bias, v.tiles.x, v.tiles.y, v,
                                    u_DDGIIrradiance, u_DDGIDistance);
    }

    uniform sampler2D u_Albedo;      // slot 0
    uniform float     u_Roughness;   // scalar fallback when no MR texture
    uniform float     u_Metalness;
#ifdef HAS_METALROUGHNESS_TEX
    uniform sampler2D u_MetalRoughness; // slot 1
#endif
#if defined(HAS_NORMAL_MAP) && !defined(SKINNED)
    uniform sampler2D u_NormalMap;      // slot 2
#endif

#ifdef FORWARD_PLUS
    layout(std430, binding = 1) readonly buffer LightBuffer          { GPULight       lights[];         };
    layout(std430, binding = 2) readonly buffer LightIndexListBuffer { uint           lightIndexList[];  };
    layout(std430, binding = 3) readonly buffer LightGridBuffer      { LightGridEntry lightGrid[];       };
#else
    #define MAX_POINT_LIGHTS 16
    uniform int   u_PointLightCount;
    uniform vec3  u_PointLightPositions[MAX_POINT_LIGHTS];
    uniform vec3  u_PointLightColors[MAX_POINT_LIGHTS];
    uniform float u_PointLightIntensities[MAX_POINT_LIGHTS];
    uniform float u_PointLightRadii[MAX_POINT_LIGHTS];
    uniform float u_PointLightSourceRadii[MAX_POINT_LIGHTS];
#endif

void main()
{
    // Written first because main() has several early returns, and an MRT attachment left
    // unwritten holds undefined values. The divides are guarded because a zero w on a
    // vertex behind the eye produces NaN, which then poisons the history it reprojects.
    vec2 ndcCurr = v_ClipCurr.xy / max(abs(v_ClipCurr.w), 1e-6) * sign(v_ClipCurr.w);
    vec2 ndcPrev = v_ClipPrev.xy / max(abs(v_ClipPrev.w), 1e-6) * sign(v_ClipPrev.w);
    FragVelocity = vec4((ndcCurr - ndcPrev) * 0.5, 0.0, 0.0);

    vec4 albedoSample = texture(u_Albedo, v_TexCoord, u_TextureLodBias);
    vec3 albedo = albedoSample.rgb;

#ifdef HAS_METALROUGHNESS_TEX
    vec2  mr        = texture(u_MetalRoughness, v_TexCoord, u_TextureLodBias).gb;
#ifdef SKINNED
    mr *= vec2(u_Roughness, u_Metalness); // glTF: the map is scaled by the material's factors
#endif
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
    vec3 normalSample = texture(u_NormalMap, v_TexCoord, u_TextureLodBias).xyz * 2.0 - 1.0;
    normalSample.y = -normalSample.y; // DX -> OpenGL normal map convention
    vec3 N  = normalize(mat3(vT, vB, vN) * normalSample);
#else
    vec3 N = normalize(v_Normal);
#endif

    vec3 V = normalize(u_CameraWorldPos.xyz - v_WorldPos);

#ifdef HAS_TOON
    // Shared toon helper: quantize NdotL into 4 hard bands
    #define TOON_BAND(ndotl) ((ndotl) > 0.5 ? 1.0 : (ndotl) > 0.0 ? 0.6 : (ndotl) > -0.3 ? 0.25 : 0.05)

    vec3 L_dir     = normalize(-u_LightDirection.xyz);
    float NdotL_d  = dot(N, L_dir);
    float dirScale = clamp(u_DiffuseIntensity * 0.5, 0.5, 1.0);
    vec3 color     = albedo * u_LightColor.xyz * dirScale * TOON_BAND(NdotL_d);

    // Ambient
    color += u_AmbientColor.xyz * u_AmbientIntensity * albedo;

    // Specular highlight — single hard dot
    vec3  H_dir   = normalize(L_dir + V);
    float spec    = step(0.7, pow(max(dot(N, H_dir), 0.0), 64.0)) * 0.3;
    color += vec3(spec);

    // Rim light — Fresnel on lit side, colored by directional light
    float NdotV   = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.0);
    float rim     = step(0.4, fresnel * smoothstep(0.0, 0.4, NdotL_d));
    color += u_LightColor.xyz * rim * 0.3;

    // Point lights for forward plus
    #ifdef FORWARD_PLUS
    uint numTilesX_t  = (u_ScreenWidth + 15u) / 16u;
    uint tileIdx_t    = (uint(gl_FragCoord.y) / 16u) * numTilesX_t + (uint(gl_FragCoord.x) / 16u);
    uint lightOffset_t = lightGrid[tileIdx_t].offset;
    // Clamped to what the culler guarantees. An out-of-range count walks off the index
    // list and shades with whatever memory follows, which is near impossible to read back
    // from the resulting image.
    uint lightCount_t  = min(lightGrid[tileIdx_t].count, 128u);
    for (uint i = 0u; i < lightCount_t; i++)
    {
        uint  idx_t    = lightIndexList[lightOffset_t + i];
        vec3  Lp_t     = lights[idx_t].positionAndRadius.xyz - v_WorldPos;
        float dist_t   = length(Lp_t);
        float radius_t = lights[idx_t].positionAndRadius.w;
        float fo_t     = clamp(1.0 - (dist_t/radius_t)*(dist_t/radius_t), 0.0, 1.0); fo_t *= fo_t;
        vec3  rad_t    = lights[idx_t].colorAndIntensity.xyz * lights[idx_t].colorAndIntensity.w
                       * (fo_t / max(dist_t*dist_t, 0.0001));
        float NdotL_t  = dot(N, normalize(Lp_t));
        vec3  srad_t   = clamp(rad_t * 0.3, vec3(0.0), vec3(0.8));
        color += albedo * srad_t * TOON_BAND(NdotL_t);
        float rimP_t   = step(0.4, pow(1.0 - NdotV, 3.0) * smoothstep(0.0, 0.4, NdotL_t));
        color += srad_t * rimP_t * 0.5;
    }
    #else
    for (int i = 0; i < u_PointLightCount; i++)
    {
        vec3  Lp_t   = u_PointLightPositions[i] - v_WorldPos;
        float dist_t = length(Lp_t);
        float r_t    = u_PointLightRadii[i];
        float fo_t   = clamp(1.0 - (dist_t/r_t)*(dist_t/r_t), 0.0, 1.0); fo_t *= fo_t;
        vec3  rad_t  = u_PointLightColors[i] * u_PointLightIntensities[i]
                     * (fo_t / max(dist_t*dist_t, 0.0001));
        float NdotL_t = dot(N, normalize(Lp_t));
        vec3  srad_t  = clamp(rad_t * 0.3, vec3(0.0), vec3(0.8));
        color += albedo * srad_t * TOON_BAND(NdotL_t);
        float rimP_t  = step(0.4, pow(1.0 - NdotV, 3.0) * smoothstep(0.0, 0.4, NdotL_t));
        color += srad_t * rimP_t * 0.5;
    }
    #endif

#else
    // ── Standard PBR path ─────────────────────────────────────────────────────
#ifdef HAS_ENV_MAP
    vec3 color = ApplyIrradiance(N, V, albedo, roughness, metalness);
#else
    vec3 color = u_AmbientColor.xyz * u_AmbientIntensity * albedo;
#endif

    // Ray-traced GI adds the local bounce that ambient and distant IBL can't carry.
    // Purely additive so u_DDGIIntensity 0 is exactly the look without a volume — the
    // existing indirect stays put rather than being swapped out. Diffuse only.
    if (u_DDGICounts.w > 0.0)
    {
        vec3 ddgi = SampleDDGI(v_WorldPos, N, V) * u_DDGIIntensity;
        if (u_DDGIDebug != 0)
        {
            FragColor = vec4(ddgi, 1.0);
            return;
        }
        color += ddgi * albedo * (1.0 - metalness);
    }
    else if (u_DDGIDebug != 0)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

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
        vec3  radiance = lights[idx].colorAndIntensity.xyz
                       * lights[idx].colorAndIntensity.w
                       * (falloff / max(dist * dist, 0.0001));

        if (lights[idx].directionAndType.w > 0.5)
        {
            vec3  lightDir   = normalize(lights[idx].directionAndType.xyz);
            float cosAngle   = dot(-normalize(Lp), lightDir);
            float cosInner   = lights[idx].coneAngles.x;
            float cosOuter   = lights[idx].coneAngles.y;
            float coneFactor = smoothstep(cosOuter, cosInner, cosAngle);
            radiance *= coneFactor;
        }

        float sourceRadius = (u_AreaLightsEnabled != 0) ? lights[idx].coneAngles.z : 0.0;
        color += evalPBRSphere(N, V, Lp, dist, sourceRadius, albedo, roughness, metalness, radiance);
    }
    #else
    for (int i = 0; i < u_PointLightCount; i++)
    {
        vec3  toLight  = u_PointLightPositions[i] - v_WorldPos;
        float dist     = length(toLight);
        float r        = u_PointLightRadii[i];
        float falloff  = clamp(1.0 - (dist / r) * (dist / r), 0.0, 1.0);
        falloff       *= falloff;
        vec3  radiance = u_PointLightColors[i] * u_PointLightIntensities[i]
                       * (falloff / max(dist * dist, 0.0001));
        float sourceRadius = (u_AreaLightsEnabled != 0) ? u_PointLightSourceRadii[i] : 0.0;
        color += evalPBRSphere(N, V, toLight, dist, sourceRadius, albedo, roughness, metalness, radiance);
    }
    #endif

    // Environment map
    #ifdef HAS_ENV_MAP
    color = ApplyEnvMapping(color, N, V, albedo, roughness, metalness);
    #endif
#endif

    // Forward+ heatmap debug (PBR only)
#if defined(FORWARD_PLUS) && !defined(HAS_TOON)
    if (u_DebugMode == 1)
    {
        bool onBorder = (uint(gl_FragCoord.x) % 16u == 0u) || (uint(gl_FragCoord.y) % 16u == 0u);
        if (onBorder) { FragColor = vec4(1.0); return; }
        uint numTilesX2 = (u_ScreenWidth + 15u) / 16u;
        uint tileIdx2   = (uint(gl_FragCoord.y) / 16u) * numTilesX2 + (uint(gl_FragCoord.x) / 16u);
        float t = clamp(float(lightGrid[tileIdx2].count) / 8.0, 0.0, 1.0);
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