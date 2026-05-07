#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 v_TexCoord;
out vec3 v_Normal;

void main()
{
	vec4 worldPos = u_Model * vec4(a_Position, 1.0);
	v_TexCoord    = a_TexCoord;
	v_Normal      = normalize(mat3(u_Model) * a_Normal);
	gl_Position   = u_ViewProjection * worldPos;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoord;
in vec3 v_Normal;

uniform sampler2D u_Texture;

void main()
{
	vec4 texColor = texture(u_Texture, v_TexCoord);
	if (texColor.a < 0.1) discard;

	vec3  lightDir = normalize(vec3(1.0, 2.0, 1.0));
	float diff     = max(dot(normalize(v_Normal), lightDir), 0.0);
	float light    = 0.35 + diff * 0.65;

	FragColor = vec4(texColor.rgb * light, texColor.a);
}
