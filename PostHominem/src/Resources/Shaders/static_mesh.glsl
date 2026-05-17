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
    v_Normal      = normalize(mat3(u_Model) * a_Normal);
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

// Light
uniform vec3  u_LightDirection;
uniform vec3  u_LightColor;
uniform float u_AmbientIntensity;
uniform float u_DiffuseIntensity;

// Material
uniform vec3  u_MatAmbientColor;
uniform vec3  u_MatDiffuseColor;
uniform vec3  u_MatSpecularColor;
uniform float u_MatSpecIntensity;
uniform float u_MatShininess;

uniform vec3  u_CameraWorldPos;

void main()
{
    vec4 texColor = texture(u_Texture, v_TexCoord);
    if (texColor.a < 0.1) discard;

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
