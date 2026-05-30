// Shared GPU struct definitions — included by any shader that needs these types.

struct GPUPointLight
{
    vec4 positionAndRadius;  // xyz = world pos, w = radius
    vec4 colorAndIntensity;  // xyz = RGB color, w = intensity
};

struct LightGridEntry
{
    uint offset;
    uint count;
};
