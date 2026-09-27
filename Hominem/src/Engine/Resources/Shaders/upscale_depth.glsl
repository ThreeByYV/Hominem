#type vertex
#version 450 core

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}

#type fragment
#version 450 core

// Scene depth (D24S8) to an R32F color target, which can be shared with Vulkan. Rows are i
// flipped to top-down, like the color and velocity copies. Shader bc D24S8 format isoptional some GPUs, like many AMD ones, don't support it)
out float o_Depth;

uniform sampler2D u_Depth;

void main()
{
    const ivec2 p = ivec2(gl_FragCoord.xy);
    o_Depth = texelFetch(u_Depth, ivec2(p.x, textureSize(u_Depth, 0).y - 1 - p.y), 0).r;
}
