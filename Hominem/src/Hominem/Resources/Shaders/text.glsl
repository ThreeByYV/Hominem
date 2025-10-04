// MSDF text shader

#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;     
layout(location = 2) in vec2 a_TexCoord;   

layout(std140, binding = 0) uniform Camera
{	
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
}; 

layout(location = 0) out VertexOutput Output;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}



#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};  

layout(location = 0) in VertexOutput Input; 

layout(binding = 0) uniform sampler2D u_FontAtlas;

void main()
{
	vec4 texColor = Input.Color * texture(u_FontAtlas, Input.TexCoord);

	if(texColor.a == 0.0) discard;	
		
	o_Color = texColor;
}