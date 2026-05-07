#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;

out VS_OUT {
	vec3 WorldPos;
	vec3 WorldNormal;
} vs_out;

void main()
{
	vec4 worldPos      = u_Model * vec4(a_Position, 1.0);
	vs_out.WorldPos    = worldPos.xyz;
	vs_out.WorldNormal = normalize(mat3(u_Model) * a_Normal);
	gl_Position        = worldPos;
}

#type geometry
#version 460 core

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in VS_OUT {
	vec3 WorldPos;
	vec3 WorldNormal;
} gs_in[];

uniform mat4  u_ViewProjection;
uniform float u_NormalLength;
uniform vec3  u_CameraWorldPos;

out vec3 v_Color;

void EmitNormal(int i)
{
	vec3 origin   = gs_in[i].WorldPos;
	vec3 tip      = origin + gs_in[i].WorldNormal * u_NormalLength;
	vec3 toCamera = normalize(u_CameraWorldPos - origin);
	float facing  = dot(gs_in[i].WorldNormal, toCamera);

	// green = faces camera (correct), red = faces away (flipped)
	vec3 normalColor = facing > 0.0 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

	v_Color     = normalColor;
	gl_Position = u_ViewProjection * vec4(origin, 1.0);
	EmitVertex();

	v_Color     = normalColor;
	gl_Position = u_ViewProjection * vec4(tip, 1.0);
	EmitVertex();

	EndPrimitive();
}

void main()
{
	EmitNormal(0);
	EmitNormal(1);
	EmitNormal(2);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec3 v_Color;

void main()
{
	FragColor = vec4(v_Color, 1.0);
}
