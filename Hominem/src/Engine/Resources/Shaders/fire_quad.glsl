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

uniform vec3  u_ColorCore;
uniform vec3  u_ColorMid;
uniform vec3  u_ColorEdge;
uniform float u_Intensity;
uniform float u_ScrollSpeed;
uniform float u_Time;
uniform float u_Seed;

void main()
{
    vec2 uv = v_UV;

    // Flames rise: scroll noise upward over time, with finer detail stacked via FBM.
    vec2 noiseUV = uv * vec2(3.0, 4.0) + vec2(u_Seed, -u_Time * u_ScrollSpeed);
    float n = fbm(noiseUV, 4);

    // Taper the flame shape toward the top of the quad and fade the rectangle edges
    // so it blends into the surrounding scene instead of showing a hard border.
    // smoothstep() requires edge0 < edge1, so the "fade toward 1" masks are written
    // as 1.0 - smoothstep(...) rather than passing decreasing edges.
    float taper = 1.0 - smoothstep(0.0, 1.0, uv.y);
    float edge  = smoothstep(0.0, 0.25, uv.x) * (1.0 - smoothstep(0.75, 1.0, uv.x))
                * smoothstep(0.0, 0.15, uv.y);

    float flame = n * taper * edge;

    vec3 color = mix(u_ColorEdge, u_ColorMid,  smoothstep(0.2, 0.6, flame));
    color      = mix(color,       u_ColorCore, smoothstep(0.6, 0.9, flame));

    float alpha = clamp(flame * u_Intensity, 0.0, 1.0);
    FragColor = vec4(color * u_Intensity, alpha);
}
