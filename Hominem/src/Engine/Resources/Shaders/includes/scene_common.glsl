#ifndef SCENE_COMMON_GLSL
#define SCENE_COMMON_GLSL

#ifndef SCENE_MAX_LIGHTS
#define SCENE_MAX_LIGHTS 20
#endif

// Layout must match GPUSceneData in VulkanSceneRenderer.cpp exactly
layout(buffer_reference, std430) readonly buffer SceneBuffer
{
    mat4  viewProj;
    vec4  cameraPos;
    ivec4 lightCount;   // x = active lights this frame
    vec4  lightPos[SCENE_MAX_LIGHTS];
    vec4  lightColor[SCENE_MAX_LIGHTS];
    vec4  ambient;
    vec4  ddgiOrigin;
    vec4  ddgiSpacing;
    ivec4 ddgiCounts;   // xyz = counts, w = numRays (0 = DDGI off)
    vec4  ddgiRotation;
    ivec4 ddgiTiles;    // x = irradiance interior, y = distance interior
};

#endif
