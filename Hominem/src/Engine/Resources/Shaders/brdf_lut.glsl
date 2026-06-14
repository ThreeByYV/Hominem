#type vertex
#version 450 core

out vec2 v_NDC;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    v_NDC       = pos[gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 1.0, 1.0);
}

#type fragment
#version 450 core

// Split-sum BRDF integration. X = NdotV, Y = roughness, output = (scale, bias)
// applied to F0 for the specular IBL term: specular = prefiltered * (F0 * scale + bias).
// Baked once at startup into a 2D LUT.

#include "includes/sampling.glsl"

in  vec2 v_NDC;
out vec4 FragColor;

const uint SAMPLE_COUNT = 1024u;

// IBL uses a different k than direct lighting (roughness^2 / 2, vs (roughness+1)^2 / 8).
float geomSchlickGGX_IBL(float NdotX, float roughness)
{
    float k = (roughness * roughness) / 2.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float geomSmith_IBL(float NdotV, float NdotL, float roughness)
{
    return geomSchlickGGX_IBL(NdotV, roughness) * geomSchlickGGX_IBL(NdotL, roughness);
}

vec2 integrateBRDF(float NdotV, float roughness)
{
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G     = geomSmith_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc    = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return vec2(A, B) / float(SAMPLE_COUNT);
}

void main()
{
    vec2 uv     = v_NDC * 0.5 + 0.5;
    float NdotV = max(uv.x, 0.001);
    float roughness = uv.y;

    FragColor = vec4(integrateBRDF(NdotV, roughness), 0.0, 1.0);
}
