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

	vec3 N = normalize(v_Normal);

	// Hemisphere ambient — sky above, warm ground bounce below
	vec3 sky    = vec3(0.45, 0.55, 0.75);
	vec3 ground = vec3(0.25, 0.18, 0.12);
	vec3 ambient = mix(ground, sky, N.y * 0.5 + 0.5) * 0.55;

	// Main sun (warm directional)
	vec3  sunDir   = normalize(vec3(0.5, 1.0, 0.4));
	float NdotS    = max(dot(N, sunDir), 0.0);
	vec3  sun      = vec3(1.0, 0.93, 0.78) * NdotS * 0.85;

	// Cool fill from opposite side — stops backs going pure black
	vec3  fillDir  = normalize(vec3(-0.6, 0.1, -0.4));
	float NdotF    = max(dot(N, fillDir), 0.0);
	vec3  fill     = vec3(0.25, 0.35, 0.55) * NdotF * 0.25;

	vec3 light = ambient + sun + fill;

	// Reinhard tone map + gamma
	light = light / (light + vec3(1.0));
	light = pow(light, vec3(1.0 / 2.2));

	FragColor = vec4(texColor.rgb * light, texColor.a);
}
