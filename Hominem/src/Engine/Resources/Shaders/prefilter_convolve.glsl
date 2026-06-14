#type vertex
#version 450 core

// Fullscreen triangle, positions generated from gl_VertexID, no VBO.
out vec2 v_NDC;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    v_NDC       = pos[gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 1.0, 1.0);
}

#type fragment
#version 450 core

// GGX importance-sampled prefilter of a source environment cubemap.
// Run once per face per mip level of a prefiltered specular cubemap. Each
// fragment's world-space direction N is treated as both the surface normal
// and the view direction, and the output is the pre-integrated specular
// radiance for that normal at the mip's roughness.

#include "includes/sampling.glsl"

uniform samplerCube u_EnvMap;
uniform mat4        u_InvViewProj; // inverse(proj * lookAt(0, faceTarget, faceUp))
uniform float       u_Roughness;

in  vec2 v_NDC;
out vec4 FragColor;

const uint SAMPLE_COUNT = 64u;

void main()
{
    vec4 worldFar = u_InvViewProj * vec4(v_NDC, 1.0, 1.0);
    vec3 N = normalize(worldFar.xyz / worldFar.w);
    vec3 V = N;

    vec3  prefiltered = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, N, u_Roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefiltered += texture(u_EnvMap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefiltered = prefiltered / max(totalWeight, 0.0001);
    FragColor   = vec4(prefiltered, 1.0);
}
