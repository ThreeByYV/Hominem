#type vertex
#version 460 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos; // world-space position written into the position GBuffer slot

void main()
{
    vec4 worldPos  = u_Model * vec4(a_Pos, 1.0);
    v_FragPos      = worldPos.xyz;
    v_Normal       = normalize(mat3(u_Model) * a_Normal);
    v_TexCoord     = a_TexCoord;

    gl_Position = u_ViewProjection * worldPos;
}

// The FS is responsible for pulling from Framebuffer for MRT (Multiple Render Targets).
// Each output layout location maps to the corresponding GL_COLOR_ATTACHMENTn
// set up in GBuffer::Init() via glDrawBuffers, matching GBUFFER_TEXTURE_TYPE_*.

#type fragment
#version 460 core

// Must match GBUFFER_TEXTURE_TYPE_* enum order in GBuffer.h
layout(location = 0) out vec3 gPosition; // GBUFFER_TEXTURE_TYPE_POSITION
layout(location = 1) out vec3 gDiffuse;  // GBUFFER_TEXTURE_TYPE_DIFFUSE
layout(location = 2) out vec3 gNormal;   // GBUFFER_TEXTURE_TYPE_NORMAL
layout(location = 3) out vec3 gTexCoord; // GBUFFER_TEXTURE_TYPE_TEXCOORD

in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;

uniform sampler2D gColorMap;

void main()
{
    gPosition = v_FragPos;
    gDiffuse  = texture(gColorMap, v_TexCoord).rgb;
    gNormal   = normalize(v_Normal);
    gTexCoord = vec3(v_TexCoord, 0.0);
}
