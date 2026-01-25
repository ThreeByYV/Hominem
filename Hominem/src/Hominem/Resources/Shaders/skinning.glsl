#type vertex
#version 460 core
		
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;
layout(location = 3) in ivec4 BoneIDs;
layout(location = 4) in vec4 Weights;

const int MAX_BONES = 100;

uniform mat4 u_ViewProjection;	
uniform mat4 u_Model;
uniform mat4 g_Bones[MAX_BONES];

out vec2 TexCoord0;
out vec3 Normal0;
out vec3 LocalPos0;
flat out ivec4 BoneIDs0;
out vec4 Weights0;
    
void main()
{
    mat4 BoneTransform = g_Bones[BoneIDs[0]] * Weights[0];
    BoneTransform += g_Bones[BoneIDs[1]] * Weights[1];
    BoneTransform += g_Bones[BoneIDs[2]] * Weights[2];
    BoneTransform += g_Bones[BoneIDs[3]] * Weights[3];
    
    vec4 transformedPos = BoneTransform * vec4(a_Pos, 1.0);
    gl_Position = u_ViewProjection * u_Model * transformedPos;
    
    vec3 transformedNormal = mat3(BoneTransform) * Normal;
    
    TexCoord0 = TexCoord;
    Normal0 = transformedNormal;
    LocalPos0 = a_Pos;
    BoneIDs0 = BoneIDs;
    Weights0 = Weights;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 color;

in vec2 TexCoord0;
in vec3 Normal0;

uniform vec4 u_Color;
uniform sampler2D u_Texture;

void main()
{
    vec4 texColor = texture(u_Texture, TexCoord0);
    
    // Use tan color if no texture
    if (length(texColor.rgb) < 0.1) {
        texColor = vec4(0.8, 0.6, 0.4, 1.0);
    }
    
    // Simple lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(Normal0), lightDir), 0.0);
    float lighting = 0.3 + 0.7 * diff;
    
    color = vec4(texColor.rgb * lighting, 1.0);
}