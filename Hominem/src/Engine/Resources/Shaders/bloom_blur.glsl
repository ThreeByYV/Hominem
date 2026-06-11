#type vertex
#version 450 core

out vec2 v_UV;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    const vec2 uv [3] = vec2[3](vec2( 0.0,  0.0), vec2(2.0,  0.0), vec2( 0.0, 2.0));
    v_UV        = uv[gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}

#type fragment
#version 450 core

uniform sampler2D u_Src;
uniform int       u_Horizontal;

// 9-tap separable Gaussian
const float k[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

in  vec2 v_UV;
out vec4 FragColor;

void main()
{
    vec2 step   = 1.0 / vec2(textureSize(u_Src, 0));
    vec3 result = texture(u_Src, v_UV).rgb * k[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = u_Horizontal == 1 ? vec2(step.x * i, 0.0)
                                        : vec2(0.0, step.y * i);
        result += texture(u_Src, v_UV + offset).rgb * k[i];
        result += texture(u_Src, v_UV - offset).rgb * k[i];
    }

    FragColor = vec4(result, 1.0);
}
