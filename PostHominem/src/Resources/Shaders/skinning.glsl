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

// Material
uniform vec3  u_MatAmbientColor;
uniform vec3  u_MatDiffuseColor;
uniform vec3  u_MatSpecularColor;
uniform float u_MatSpecIntensity;
uniform float u_MatShininess;

void main()
{
    vec4 texColor = texture(u_Texture, v_TexCoord);
    // Skinned meshes may not have an alpha channel — fall back to a neutral colour
    // rather than discarding, which would make the mesh invisible.
    if (dot(texColor.rgb, texColor.rgb) < 0.001)
        texColor = vec4(0.8, 0.6, 0.4, 1.0);

    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_LightDirection);
    vec3 V = normalize(u_CameraWorldPos - v_WorldPos);

    vec3 ambient  = u_LightColor * u_AmbientIntensity * u_MatAmbientColor;

    float NdotL   = max(dot(N, L), 0.0);
    vec3  diffuse = u_LightColor * u_DiffuseIntensity * NdotL * u_MatDiffuseColor;

    vec3  H       = normalize(L + V);
    float NdotH   = max(dot(N, H), 0.0);
    vec3  specular = u_LightColor * u_MatSpecIntensity * pow(NdotH, u_MatShininess) * u_MatSpecularColor;

    vec3 light = ambient + diffuse + specular;
    FragColor  = vec4(texColor.rgb * light, texColor.a);
}
