// Scene-wide uniforms — uploaded once per frame, shared by all shaders.
// std140 layout must match SceneUBOData in Renderer3D.h exactly.
// vec4 used throughout to match glm::vec4 in C++ and avoid vec3 padding quirks.
layout(std140, binding = 0) uniform SceneUBO
{
    mat4  u_ViewProjection;    // offset   0
    vec4  u_CameraWorldPos;    // offset  64  (.xyz used, .w = 0)
    vec4  u_LightDirection;    // offset  80
    vec4  u_LightColor;        // offset  96
    vec4  u_AmbientColor;      // offset 112
    float u_AmbientIntensity;  // offset 128
    float u_DiffuseIntensity;  // offset 132
    float u_EnvMapIntensity;   // offset 136
    float u_ETA;               // offset 140
    float u_FresnelPower;      // offset 144
    uint  u_ScreenWidth;       // offset 148
    int   u_DebugMode;         // offset 152
    int   u_AreaLightsEnabled; // offset 156 → total 160
};

uniform mat4 u_Model; // per-mesh — stays as a regular uniform
