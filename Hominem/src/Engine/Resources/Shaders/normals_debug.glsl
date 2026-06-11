#type vertex
#version 460 core

#ifdef SKINNED
layout(std430, binding = 4) readonly buffer SkinnedPositions { vec4 u_SkinnedPos[];  };
layout(std430, binding = 5) readonly buffer SkinnedNormals   { vec4 u_SkinnedNorm[]; };
layout(location = 1) in vec2 a_TexCoord;
#else
layout(std430, binding = 5) readonly buffer ModelMatrices { mat4 b_Models[]; };
uniform int u_BaseModelIndex;
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
#endif

#include "includes/scene_ubo.glsl"

out VS_OUT {
	vec3 WorldPos;
	vec3 WorldNormal;
} vs_out;

void main()
{
#ifdef SKINNED
	vec4 worldPos      = u_Model * u_SkinnedPos[gl_VertexID];
	vs_out.WorldNormal = normalize(mat3(u_Model) * u_SkinnedNorm[gl_VertexID].xyz);
#else
	mat4 model         = b_Models[uint(u_BaseModelIndex) + uint(gl_DrawID)];
	vec4 worldPos      = model * vec4(a_Position, 1.0);
	vs_out.WorldNormal = normalize(mat3(model) * a_Normal);
#endif
	vs_out.WorldPos = worldPos.xyz;
	gl_Position     = worldPos;
}

#type geometry
#version 460 core

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in VS_OUT {
	vec3 WorldPos;
	vec3 WorldNormal;
} gs_in[];

#include "includes/scene_ubo.glsl"

uniform float u_NormalLength;

out vec3 v_Color;

void EmitNormal(int i)
{
	vec3 origin   = gs_in[i].WorldPos;
	vec3 tip      = origin + gs_in[i].WorldNormal * u_NormalLength;
	vec3 toCamera = normalize(u_CameraWorldPos.xyz - origin);
	float facing  = dot(gs_in[i].WorldNormal, toCamera);

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
