#type vertex
#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord  = a_TexCoord;
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
}


#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec4      u_Color;
uniform vec2      u_TexelSize;   // (1/width, 1/height)
uniform float     u_BlurRadius;  // multiplier for sample spread (world units -> texels)
uniform bool      u_Horizontal;  // true = horizontal pass, false = vertical pass

void main()
{
    // 5-tap separable Gaussian kernel
    const float offsets[5] = float[5](-2.0, -1.0, 0.0, 1.0, 2.0);
    const float weights[5] = float[5](0.0625, 0.25, 0.375, 0.25, 0.0625);

    vec2 dir = u_Horizontal ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

    vec4 result = vec4(0.0);
    for (int i = 0; i < 5; i++)
    {
        vec2 offset = dir * offsets[i] * u_BlurRadius * u_TexelSize;
        result += texture(u_Texture, v_TexCoord + offset) * weights[i];
    }

    color = result * u_Color;
}
