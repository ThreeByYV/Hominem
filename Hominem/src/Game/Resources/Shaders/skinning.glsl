#type vertex
#version 460 core

layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };

layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 TexCoord0;
out vec3 Normal0;

void main()
{
    gl_Position = u_ViewProjection * u_Model * u_SkinnedPos[gl_VertexID];
    TexCoord0   = a_TexCoord;
    Normal0     = mat3(transpose(inverse(u_Model))) * u_SkinnedNorm[gl_VertexID].xyz;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 color;

in vec2 TexCoord0;
in vec3 Normal0;

uniform sampler2D u_Texture;

void main()
{
    vec4 texColor = texture(u_Texture, TexCoord0);
    if (length(texColor.rgb) < 0.1)
        texColor = vec4(0.8, 0.6, 0.4, 1.0);

    vec3 lightDir  = normalize(vec3(0.5, 1.0, 0.3));
    float lighting = 0.3 + 0.7 * max(dot(normalize(Normal0), lightDir), 0.0);

    color = vec4(texColor.rgb * lighting, 1.0);
}
