#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main()
{
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos    = worldPos.xyz;
    v_Normal      = mat3(u_Model) * a_Normal;
    v_TexCoord    = a_TexCoord;
    gl_Position   = u_ViewProjection * worldPos;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;


uniform sampler2D u_Albedo;         // slot 0
uniform sampler2D u_MetalRoughness; // slot 1 — G = roughness, B = metalness

uniform vec3  u_LightDirection;
uniform vec3  u_LightColor;
uniform float u_AmbientIntensity;
uniform vec3  u_AmbientColor;
uniform float u_DiffuseIntensity;
uniform vec3  u_CameraWorldPos;
uniform float u_Roughness;
uniform float u_Metalness;

// Viewport width for tile index lookup
uniform uint u_ScreenWidth;

// 0 = normal, 1 = tile light-count heat map
uniform int u_DebugMode;

struct GPUPointLight 
{
    vec4 positionAndRadius; // xyz = world pos,  w = radius
    vec4 colorAndIntensity; // xyz = RGB colour,  w = intensity
};

struct LightGridEntry 
{
    uint offset;
    uint count;
};

layout(std430, binding = 1) readonly buffer LightBuffer 
{
    GPUPointLight lights[];
};

layout(std430, binding = 2) readonly buffer LightIndexListBuffer
{
    uint lightIndexList[];
};

layout(std430, binding = 3) readonly buffer LightGridBuffer {
    LightGridEntry lightGrid[];
};

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

vec3 evalPBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metalness, vec3 radiance)
{
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3  F0 = mix(vec3(0.04), albedo, metalness);
    float D  = ggxDistribution(NdotH, roughness);
    float G  = geomSmith(NdotV, NdotL, roughness);
    vec3  F  = schlickFresnel(VdotH, F0);

    vec3 specBRDF    = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD          = (1.0 - F) * (1.0 - metalness);
    vec3 diffuseBRDF = kD * albedo / PI;

    return (diffuseBRDF + specBRDF) * radiance * NdotL;
}

void main()
{
    vec4  albedoSample = texture(u_Albedo, v_TexCoord);
    vec3  albedo       = albedoSample.rgb;
    vec2  mr           = texture(u_MetalRoughness, v_TexCoord).gb;
    float roughness    = clamp(mr.x, 0.05, 1.0);
    float metalness    = clamp(mr.y, 0.0,  1.0);

    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraWorldPos - v_WorldPos);

    vec3 color = u_AmbientColor * u_AmbientIntensity * albedo;

    // Directional light 
    vec3 L = normalize(-u_LightDirection);
    color += evalPBR(N, V, L, albedo, roughness, metalness, u_LightColor * u_DiffuseIntensity);

    // The forward+ tile lookup below
    // Derive tile coordinates from this fragment's screen position.
    uint numTilesX = (u_ScreenWidth + 15u) / 16u;
    uint tileX     = uint(gl_FragCoord.x) / 16u;
    uint tileY     = uint(gl_FragCoord.y) / 16u;
    uint tileIdx   = tileY * numTilesX + tileX;

    uint lightOffset = lightGrid[tileIdx].offset;
    uint lightCount  = lightGrid[tileIdx].count;

    for (uint i = 0u; i < lightCount; i++)
    {
        uint      idx      = lightIndexList[lightOffset + i];
        vec3      lightPos = lights[idx].positionAndRadius.xyz;
        float     radius   = lights[idx].positionAndRadius.w;
        vec3      Lp       = lightPos - v_WorldPos;
        float     dist     = length(Lp);

        // falloff reaches exactly 0 at radius
        float falloff    = clamp(1.0 - (dist / radius) * (dist / radius), 0.0, 1.0);
        falloff         *= falloff;
        float attenuation = falloff / max(dist * dist, 0.0001);

        vec3 radiance = lights[idx].colorAndIntensity.xyz
                      * lights[idx].colorAndIntensity.w
                      * attenuation;

        color += evalPBR(N, V, normalize(Lp), albedo, roughness, metalness, radiance);
    }

    if (u_DebugMode == 1)
    {
        // White border on every tile edge so the grid is visible
        bool onBorder = (uint(gl_FragCoord.x) % 16u == 0u) || (uint(gl_FragCoord.y) % 16u == 0u);
        if (onBorder) { FragColor = vec4(1.0); return; }

        // Intermediate counts interpolate i.e. 1=dark blue, 3=cyan, 5=lime, 7=orange
        float t = clamp(float(lightCount) / 8.0, 0.0, 1.0);
        vec3 heat;
        if      (t < 0.25) heat = mix(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), t * 4.0);       // 0–2  lights: black  → blue
        else if (t < 0.50) heat = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0); // 2–4  lights: blue   → green
        else if (t < 0.75) heat = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.50) * 4.0); // 4–6  lights: green  → yellow
        else               heat = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0); // 6–8+ lights: yellow → red

        // Dim tones on geometry so zero-light tiles look clearly black.
        FragColor = vec4(heat, 1.0);
        return;
    }

    FragColor = vec4(color, albedoSample.a);
}
