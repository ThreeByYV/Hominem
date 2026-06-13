#type vertex
#version 450 core

#include "includes/scene_ubo.glsl"

out vec2 v_UV;

void main()
{
    // Unit quad (-0.5..0.5) in the XY plane
    const vec2 corners[6] = vec2[6](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
        vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
    );

    vec2 local = corners[gl_VertexID];
    v_UV = local + 0.5;
    gl_Position = u_ViewProjection * u_Model * vec4(local, 0.0, 1.0);
}

#type fragment
#version 450 core

#include "includes/noise.glsl"

in  vec2 v_UV;
out vec4 FragColor;

uniform vec3  u_ColorDark;  // cold smoke, top of the plume
uniform vec3  u_ColorLit;   // fire-lit underside
uniform float u_Opacity;
uniform float u_ScrollSpeed;
uniform float u_Time;
uniform float u_Seed;

void main()
{
    vec2 uv = v_UV;

    // Smoke drifts slowly up and sideways. Large noise scale + few octaves gives
    // big soft blobs instead of the fine flicker used for fire.
    vec2 noiseUV = uv * vec2(1.5, 1.5)
                 + vec2(u_Seed + u_Time * u_ScrollSpeed * 0.3, -u_Time * u_ScrollSpeed);
    float n = fbm(noiseUV, 3);

    // Soft round-edged blob: fade the whole quad toward its border so there's no
    // hard rectangle outline, then let the noise itself carve the silhouette.
    vec2  c    = uv - 0.5;
    float vign = 1.0 - smoothstep(0.25, 0.5, length(c));
    float blob = smoothstep(0.15, 0.9, n * vign);

    // Lit from below by the fire: warm tint fades out toward the top of the plume.
    float litAmount = (1.0 - smoothstep(0.0, 0.6, uv.y)) * n;
    vec3  color     = mix(u_ColorDark, u_ColorLit, litAmount);

    float alpha = clamp(blob * u_Opacity, 0.0, 1.0);
    FragColor = vec4(color, alpha);
}
