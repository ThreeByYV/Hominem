#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };

layout(location = 1) in vec2 a_TexCoord;

#include "includes/scene_ubo.glsl"

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main()
{
    vec4 worldPos = u_Model * u_SkinnedPos[gl_VertexID];
    v_WorldPos    = worldPos.xyz;
    v_Normal      = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
    v_TexCoord    = a_TexCoord;
    gl_Position   = u_ViewProjection * worldPos;
}



#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

#include "includes/scene_ubo.glsl"

// Same uniform array the skinned PBR shader uses
#define MAX_POINT_LIGHTS 16
uniform int   u_PointLightCount;
uniform vec3  u_PointLightPositions[MAX_POINT_LIGHTS];
uniform vec3  u_PointLightColors[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensities[MAX_POINT_LIGHTS];
uniform float u_PointLightRadii[MAX_POINT_LIGHTS];

uniform sampler2D u_Albedo;

float toonBand(float v)
{
    if      (v > 0.5)  return 1.0;
    else if (v > 0.0)  return 0.6;
    else if (v > -0.3) return 0.25;
    else               return 0.05;
}

void main()
{
    vec4 albedo = texture(u_Albedo, v_TexCoord);
    vec3 N      = normalize(v_Normal);
    vec3 V      = normalize(u_CameraWorldPos.xyz - v_WorldPos);
    vec3 L      = normalize(-u_LightDirection.xyz);
    float NdotV = max(dot(N, V), 0.0);

    // ── Directional light ─────────────────────────────────────────────────────
    float NdotL_dir = dot(N, L);
    float dirScale  = clamp(u_DiffuseIntensity * 0.5, 0.5, 1.0);
    vec3  dirLight  = albedo.rgb * u_LightColor.rgb * dirScale * toonBand(NdotL_dir);

    // Specular
    vec3  H_dir    = normalize(L + V);
    float spec_dir = step(0.7, pow(max(dot(N, H_dir), 0.0), 64.0)) * 0.3;

    // Rim from directional light
    float fresnel  = pow(1.0 - NdotV, 3.0);
    float rim_dir  = step(0.4, fresnel * smoothstep(0.0, 0.4, NdotL_dir));
    vec3  rimColor = u_LightColor.rgb * rim_dir * 0.3;

    // ── Point lights (same uniforms as skinned PBR) ───────────────────────────
    vec3 pointLight = vec3(0.0);
    for (int i = 0; i < u_PointLightCount; i++)
    {
        vec3  Lp      = u_PointLightPositions[i] - v_WorldPos;
        float dist    = length(Lp);
        float r       = u_PointLightRadii[i];
        float falloff = clamp(1.0 - (dist / r) * (dist / r), 0.0, 1.0);
        falloff      *= falloff;
        vec3 radiance = u_PointLightColors[i] * u_PointLightIntensities[i]
                      * (falloff / max(dist * dist, 0.0001));

        float NdotL_p  = dot(N, normalize(Lp));
        vec3  scaledRad = clamp(radiance * 0.3, vec3(0.0), vec3(0.8));

        // Toon diffuse band
        pointLight += albedo.rgb * scaledRad * toonBand(NdotL_p);

        // Rim from point light — shows the light's color on silhouette
        float rimP = step(0.4, pow(1.0 - NdotV, 3.0) * smoothstep(0.0, 0.4, NdotL_p));
        pointLight += scaledRad * rimP * 0.5;
    }

    // ── Ambient ───────────────────────────────────────────────────────────────
    vec3 ambient = u_AmbientColor.rgb * u_AmbientIntensity * albedo.rgb;

    vec3 color = dirLight + vec3(spec_dir) + rimColor + pointLight + ambient;
    FragColor  = vec4(color, albedo.a);
}
