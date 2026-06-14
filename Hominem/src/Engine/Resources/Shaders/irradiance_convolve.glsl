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

// Cosine-weighted hemisphere convolution of a source environment cubemap.
// Run once per face of a small (e.g. 32x32) irradiance cubemap. Each fragment's
// world-space direction N is treated as the surface normal of a Lambertian
// surface, and the output is the diffuse irradiance for that normal.

uniform samplerCube u_EnvMap;
uniform mat4        u_InvViewProj; // inverse(proj * lookAt(0, faceTarget, faceUp))

in  vec2 v_NDC;
out vec4 FragColor;

const float PI = 3.14159265359;

void main()
{
    vec4 worldFar = u_InvViewProj * vec4(v_NDC, 1.0, 1.0);
    vec3 N = normalize(worldFar.xyz / worldFar.w);

    vec3 up    = (abs(N.y) < 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    vec3  irradiance  = vec3(0.0);
    float sampleDelta = 0.05;
    float nrSamples   = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // Spherical to tangent-space, then to world space via N's basis.
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += texture(u_EnvMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples  += 1.0;
        }
    }

    irradiance = PI * irradiance / nrSamples;
    FragColor  = vec4(irradiance, 1.0);
}
