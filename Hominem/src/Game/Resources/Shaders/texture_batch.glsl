#type vertex
#version 460 core

struct QuadData
{
    mat4 Transform;
    vec4 Color;
    vec2 UVMin;
    vec2 UVMax;
};

layout(std430, binding = 0) readonly buffer QuadBuffer { QuadData u_Quads[]; };

uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;
out vec4 v_Color;

// Unit-quad corners: BL, BR, TR, TL
const vec2 kCorners[4] = vec2[4](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

// Two CCW triangles per quad
const int kTri[6] = int[6](0, 1, 2, 2, 3, 0);

// UV lerp factors matching corner order
const vec2 kUVFactors[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main()
{
    int qi = gl_VertexID / 6;
    int ci = kTri[gl_VertexID % 6];

    QuadData q = u_Quads[qi];

    gl_Position = u_ViewProjection * q.Transform * vec4(kCorners[ci], 0.0, 1.0);
    v_TexCoord  = mix(q.UVMin, q.UVMax, kUVFactors[ci]);
    v_Color     = q.Color;
}

#type fragment
#version 460 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;

void main()
{
    color = texture(u_Texture, v_TexCoord) * v_Color;
}
