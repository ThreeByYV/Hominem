#type vertex
#version 450 core

// Fullscreen triangle — positions generated from gl_VertexID, no VBO.
out vec2 v_NDC;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    v_NDC       = pos[gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 1.0, 1.0);
}

#type fragment
#version 450 core

uniform sampler2D u_Equirect;
uniform mat4      u_InvViewProj; // inverse(viewProjection3D)
uniform vec3      u_CamPos;
uniform float     u_Intensity;

in  vec2 v_NDC;
out vec4 FragColor;

const float PI = 3.14159265359;

// World-space ray direction -> equirectangular UV.
vec2 dirToEquirect(vec3 d)
{
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    // Flip V: EXR scanline 0 (uploaded to texel row 0) is the top of the panorama,
    // which corresponds to looking up (+Y), i.e. v = 0.
    float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / PI;
    return vec2(u, v);
}

void main()
{
    // Reconstruct the world-space ray through this pixel by unprojecting a
    // far-plane point and subtracting the camera position.
    vec4 worldFar = u_InvViewProj * vec4(v_NDC, 1.0, 1.0);
    vec3 dir      = normalize(worldFar.xyz / worldFar.w - u_CamPos);

    vec3 col = texture(u_Equirect, dirToEquirect(dir)).rgb * u_Intensity;
    FragColor = vec4(col, 1.0);
}
