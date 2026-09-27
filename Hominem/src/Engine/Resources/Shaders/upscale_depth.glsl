#type vertex
#version 450 core

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}

#type fragment
#version 450 core

// Scene depth (D24S8) to an R32F colour target, which can be shared with Vulkan.
out float o_Depth;

uniform sampler2D u_Depth;

void main()
{
    o_Depth = texelFetch(u_Depth, ivec2(gl_FragCoord.xy), 0).r;
}
