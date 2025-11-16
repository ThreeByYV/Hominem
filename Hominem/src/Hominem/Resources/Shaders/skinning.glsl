#type vertex
#version 460 core
		
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;
layout(location = 3) in ivec4 BoneIDs;  //ivec4 because IDs are int not floating point
layout(location = 4) in vec4 Weights;

uniform mat4 u_ViewProjection;	
uniform mat4 u_Model;

out vec2 TexCoord0;
out vec3 Normal0;
out vec3 LocalPos0;
flat out ivec4 BoneIDs0; //rasterizer shouldn't interpolate IDs, just copy as-is
out vec4 Weights0;
    
void main()
{
	gl_Position = u_ViewProjection * u_Model * vec4(a_Pos, 1.0);
	TexCoord0 = TexCoord;
	Normal0 = Normal;
	LocalPos0 = a_Pos;
	BoneIDs0 = BoneIDs;
	Weights0 = Weights;
}


#type fragment
#version 460 core

layout(location = 0) out vec4 color;

in vec2 TexCoord0;
in vec3 Normal0;
in vec3 LocalPos0;
flat in ivec4 BoneIDs0;
in vec4 Weights0;

uniform vec4 u_Color;
uniform sampler2D u_Texture;
uniform int gDisplayBoneIndex;
    
void main()
{
    bool found = false;
    
    for (int i = 0; i < 4; i++)
    {
        if (BoneIDs0[i] == gDisplayBoneIndex) 
        {
            if (Weights0[i] >= 0.7)
            {
                color = vec4(1.0, 0.0, 0.0, 1.0); // Red for high weights (fixed alpha)
            }
            else if (Weights0[i] >= 0.4 && Weights0[i] <= 0.6)
            {
                color = vec4(0.0, 1.0, 0.0, 1.0); // Green for medium weights (fixed alpha)
            }
            else if (Weights0[i] >= 0.1)
            {
                color = vec4(1.0, 1.0, 0.0, 1.0); // Yellow for low weights (fixed alpha)
            }
            else if (Weights0[i] > 0.0)
            {
                color = vec4(0.0, 0.0, 1.0, 1.0); // Blue for very low weights
            }
            
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        vec4 texColor = texture(u_Texture, TexCoord0); 
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff = max(dot(normalize(Normal0), lightDir), 0.9);
        color = texColor * u_Color * diff;
    }
}
