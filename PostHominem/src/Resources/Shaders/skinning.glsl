#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };

layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

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

uniform sampler2D u_Texture;

uniform vec3  u_LightDirection;
uniform vec3  u_LightColor;
uniform float u_AmbientIntensity;
uniform float u_DiffuseIntensity;
uniform vec3  u_CameraWorldPos;

// PBR material — uniform per mesh (no per-pixel maps for skinned meshes yet)
uniform float u_Roughness;
uniform float u_Metalness;

const float PI = 3.14159265359;

float ggxDistribution(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

float geomSchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
}

float geomSmith(float NdotV, float NdotL, float roughness)
{
    return geomSchlickGGX(NdotV, roughness) * geomSchlickGGX(NdotL, roughness);
}

vec3 schlickFresnel(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec4 texSample = texture(u_Texture, v_TexCoord);
    vec3 albedo    = texSample.rgb;
    if (dot(albedo, albedo) < 0.001)
        albedo = vec3(0.8, 0.6, 0.4);

    float roughness = clamp(u_Roughness, 0.05, 1.0);
    float metalness = clamp(u_Metalness, 0.0,  1.0);

    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraWorldPos - v_WorldPos);
    vec3 L = normalize(-u_LightDirection);
    vec3 H = normalize(V + L);

    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    float D = ggxDistribution(NdotH, roughness);
    float G = geomSmith(NdotV, NdotL, roughness);
    vec3  F = schlickFresnel(VdotH, F0);

    vec3 specBRDF    = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD          = (1.0 - F) * (1.0 - metalness);
    vec3 diffuseBRDF = kD * albedo / PI;

    vec3 lightIntensity = u_LightColor * u_DiffuseIntensity;
    vec3 ambient        = u_LightColor * u_AmbientIntensity * albedo;

    vec3 color = ambient + (diffuseBRDF + specBRDF) * lightIntensity * NdotL;

    FragColor = vec4(color, texSample.a);
}
