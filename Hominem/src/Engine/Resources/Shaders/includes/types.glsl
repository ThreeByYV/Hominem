// Shared GPU struct definitions — included by any shader that needs these types.

struct GPULight
{
    vec4 positionAndRadius; // xyz = world pos, w = radius
    vec4 colorAndIntensity; // xyz = RGB color, w = intensity
    vec4 directionAndType;  // xyz = direction (spot), w = type (0=point 1=spot)
    vec4 coneAngles;        // x = cos(innerAngle), y = cos(outerAngle)
};

// Must match ClusteredRenderer::GRID_SIZE_* and MAX_LIGHTS_PER_CLUSTER
const uvec3 CLUSTER_GRID_SIZE = uvec3(12u, 12u, 24u);
#define MAX_LIGHTS_PER_CLUSTER 100u

struct Cluster
{
    vec4 minPoint;
    vec4 maxPoint;
    uint count;
    uint lightIndices[100];
};

// Step 3: cluster creation run backwards. z from the exponential formula, xy from pixel / tileSize.
uint ClusterIndex(vec2 fragCoord, float viewZ, vec2 screenDimensions, float zNear, float zFar)
{
    uvec3 gridSize = CLUSTER_GRID_SIZE;

    // clamped because depth outside [zNear, zFar] would give a negative or out-of-range slice
    float slice = (log(abs(viewZ) / zNear) * float(gridSize.z)) / log(zFar / zNear);
    uint  zTile = uint(clamp(slice, 0.0, float(gridSize.z - 1u)));

    vec2  tileSize = screenDimensions / vec2(gridSize.xy);
    uvec3 tile     = uvec3(min(uvec2(fragCoord / tileSize), gridSize.xy - 1u), zTile);

    return tile.x + tile.y * gridSize.x + tile.z * gridSize.x * gridSize.y;
}

// Inverse square times a window that hits 0 at the radius, so culled lights contribute nothing
float ClusteredAttenuation(float d, float radius)
{
    float window = clamp(1.0 - pow(d / radius, 4.0), 0.0, 1.0);
    window *= window;
    return window / (d * d + 1.0);
}
