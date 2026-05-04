#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in ivec4 a_BoneIDs;
layout(location = 4) in vec4 a_Weights;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_Position;
out vec3 v_Normal;

void main()
{
    v_Position = a_Position;
    v_Normal = a_Normal;
    // Ignore bone data - just render the bind pose
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 color;

in vec3 v_Position;
in vec3 v_Normal;

uniform vec4 u_Color;

void main()
{
    // Simple lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(v_Normal), lightDir), 0.3); // Min 30% ambient

    // Use bright color so it's easy to see
    vec3 baseColor = vec3(0.2, 0.8, 0.2); // Bright green
    color = vec4(baseColor * diff, 1.0);
}
