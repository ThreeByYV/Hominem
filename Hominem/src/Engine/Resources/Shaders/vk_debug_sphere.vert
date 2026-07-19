#version 450
#extension GL_EXT_buffer_reference : require

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;
};

struct SphereInstance
{
    vec4 positionRadius;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer
{
    SphereInstance instances[];
};

layout(buffer_reference, std430) readonly buffer SceneBuffer
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 lightPos[1];
    vec4 lightColor[1];
    vec4 ambient;
};

layout(push_constant) uniform PushConstants
{
    VertexBuffer   u_Vertices;
    InstanceBuffer u_Instances;
    SceneBuffer    u_Scene;
};

layout(location = 0) out vec3 v_Color;

void main()
{
    Vertex vert = u_Vertices.vertices[gl_VertexIndex];
    SphereInstance inst = u_Instances.instances[gl_InstanceIndex];

    vec3 world = inst.positionRadius.xyz + vec3(vert.px, vert.py, vert.pz) * inst.positionRadius.w;
    v_Color = inst.color.rgb;
    gl_Position = u_Scene.viewProj * vec4(world, 1.0);
}
