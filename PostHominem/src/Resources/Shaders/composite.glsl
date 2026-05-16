#type vertex
#version 450 core

out vec2 v_UV;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    const vec2 uv [3] = vec2[3](vec2( 0.0,  0.0), vec2(2.0,  0.0), vec2( 0.0, 2.0));
    v_UV        = uv [gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}

#type fragment
#version 450 core

uniform sampler2D u_HDR;

in  vec2 v_UV;
out vec4 FragColor;

void main()
{
    FragColor = texture(u_HDR, v_UV);
}
