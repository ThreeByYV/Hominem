// Shared GPU struct definitions — included by any shader that needs these types.

struct GPULight
{
    vec4 positionAndRadius; // xyz = world pos, w = radius
    vec4 colorAndIntensity; // xyz = RGB color, w = intensity
    vec4 directionAndType;  // xyz = direction (spot), w = type (0=point 1=spot)
    vec4 coneAngles;        // x = cos(innerAngle), y = cos(outerAngle)
};

struct LightGridEntry
{
    uint offset;
    uint count;
};
