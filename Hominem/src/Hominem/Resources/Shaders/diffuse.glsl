#type vertex
#version 330 core
		
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;


uniform mat4 u_ViewProjection;	
uniform mat4 u_Transform;

out vec2 v_TexCoord;
out vec3 v_Normal;

    
void main()
{
    v_Normal = a_Normal;
	v_TexCoord = a_TexCoord;

	gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
}


#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;
in vec3 v_Normal;

uniform vec4 u_Color;
uniform sampler2D u_Texture;

    
void main()
{
	vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(v_Normal), lightDir), 0.3); // 0.3 = ambient
    color = u_Color * diff;
}