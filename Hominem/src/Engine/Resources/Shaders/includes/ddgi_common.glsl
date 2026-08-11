#ifndef DDGI_COMMON_GLSL
#define DDGI_COMMON_GLSL

const float DDGI_PI = 3.14159265359;

// Volume parameters shared by every DDGI shader. Mirrors VulkanDDGIParams / DDGIVolumeDesc.
struct DDGIVolume
{
    vec4  origin;       // xyz = volume centre
    vec4  spacing;      // xyz = probe spacing
    ivec4 counts;       // xyz = probe counts, w = rays per probe
    vec4  rotation;     // per-frame random rotation quaternion (x,y,z,w)
    ivec4 tiles;        // x = irradiance interior size, y = distance interior size
};

int ddgiNumProbes(DDGIVolume v) { return v.counts.x * v.counts.y * v.counts.z; }

// RTXGI probe index <-> grid coordinate (XZ-major, Y slowest).
ivec3 ddgiProbeCoords(int probeIndex, DDGIVolume v)
{
    return ivec3(probeIndex % v.counts.x,
                 probeIndex / (v.counts.x * v.counts.z),
                 (probeIndex / v.counts.x) % v.counts.z);
}

vec3 ddgiProbeWorldPosition(int probeIndex, DDGIVolume v)
{
    ivec3 c = ddgiProbeCoords(probeIndex, v);
    vec3 local = vec3(c) * v.spacing.xyz - vec3(v.counts.xyz - 1) * 0.5 * v.spacing.xyz;
    return v.origin.xyz + local;
}

// RTXGI spherical fibonacci + quaternion rotate, verbatim.
vec3 ddgiSphericalFibonacci(float i, float n)
{
    const float PHI = sqrt(5.0) * 0.5 + 0.5;
    float phi      = 2.0 * DDGI_PI * fract(i * (PHI - 1.0));
    float cosTheta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}
vec3 ddgiQuatRotate(vec3 v, vec4 q)
{
    vec3 b = q.xyz; float b2 = dot(b, b);
    return v * (q.w * q.w - b2) + b * (dot(v, b) * 2.0) + cross(b, v) * (q.w * 2.0);
}
vec3 ddgiProbeRayDirection(int rayIndex, DDGIVolume v)
{
    vec3 dir = ddgiSphericalFibonacci(float(rayIndex), float(v.counts.w));
    return normalize(ddgiQuatRotate(dir, vec4(-v.rotation.xyz, v.rotation.w)));
}

// Octahedral encode/decode. encode: unit dir -> [-1,1]^2. decode: [-1,1]^2 -> unit dir.
vec2 ddgiOctEncode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 o = (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    return o;
}
vec3 ddgiOctDecode(vec2 f)
{
    vec3 n = vec3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// Atlas layout: tiles laid out (countX*countZ) wide by countY tall; each tile is
// (interior + 2) texels with a 1-texel border for bilinear wrap.
ivec2 ddgiProbeTile(int probeIndex, DDGIVolume v)
{
    int perSlice = v.counts.x * v.counts.z;
    return ivec2(probeIndex % perSlice, probeIndex / perSlice);
}

// Top-left interior texel of a probe's tile in the atlas.
ivec2 ddgiTileInteriorOrigin(int probeIndex, int interior, DDGIVolume v)
{
    ivec2 tile = ddgiProbeTile(probeIndex, v);
    return tile * (interior + 2) + ivec2(1);
}

vec2 ddgiAtlasSize(int interior, DDGIVolume v)
{
    return vec2(v.counts.x * v.counts.z, v.counts.y) * float(interior + 2);
}

vec3 ddgiProbeWorldPositionFromCoord(ivec3 c, DDGIVolume v)
{
    vec3 local = vec3(c) * v.spacing.xyz - vec3(v.counts.xyz - 1) * 0.5 * v.spacing.xyz;
    return v.origin.xyz + local;
}

int ddgiProbeIndexFromCoord(ivec3 c, DDGIVolume v)
{
    return c.x + v.counts.x * (c.z + v.counts.z * c.y);
}

// Normalized atlas UV for sampling a probe's octahedral tile along a world direction.
// The +1 border offset makes hardware bilinear pick up the wrap texels at tile edges.
vec2 ddgiProbeAtlasUV(int probeIndex, vec3 dir, int interior, DDGIVolume v)
{
    vec2  octUV     = ddgiOctEncode(normalize(dir)) * 0.5 + 0.5;
    ivec2 tile      = ddgiProbeTile(probeIndex, v);
    float full      = float(interior + 2);
    vec2  atlasSize = vec2(v.counts.x * v.counts.z, v.counts.y) * full;
    vec2  texel     = vec2(tile) * full + vec2(1.0) + octUV * float(interior);
    return texel / atlasSize;
}

// Chebyshev's inequality gives an upper bound on the probability the shaded point is
// visible to the probe, from the mean and mean-squared hit distance. This is the
// anti-leak term: a probe behind a wall records a short mean distance, so a point in
// front of the wall (further than that mean) gets a low weight.
float ddgiChebyshev(float distToProbe, vec2 meanMeanSq)
{
    float mean = meanMeanSq.x;
    if (distToProbe <= mean) return 1.0;
    float variance = abs(mean * mean - meanMeanSq.y);
    float d        = distToProbe - mean;
    float cheb     = variance / (variance + d * d);
    return max(cheb * cheb * cheb, 0.0);
}

// 8-probe trilinear irradiance gather with Chebyshev visibility. `normal` is the
// surface normal (irradiance is sampled along it); the distance atlas is sampled toward
// the shaded point. `bias` offsets the sample position off the surface (normal/view bias).
vec3 ddgiSampleIrradiance(vec3 worldPos, vec3 normal, vec3 bias,
                          int irrInterior, int distInterior, DDGIVolume v,
                          sampler2D irrAtlas, sampler2D distAtlas)
{
    vec3  biasedPos = worldPos + bias;
    vec3  gridMin   = v.origin.xyz - vec3(v.counts.xyz - 1) * 0.5 * v.spacing.xyz;
    vec3  rel       = (biasedPos - gridMin) / v.spacing.xyz;
    ivec3 base      = ivec3(floor(rel));
    vec3  frac      = clamp(rel - vec3(base), 0.0, 1.0);

    vec3  sumIrradiance = vec3(0.0);
    float sumWeight     = 0.0;
    for (int i = 0; i < 8; i++)
    {
        ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        ivec3 coord  = clamp(base + offset, ivec3(0), v.counts.xyz - 1);

        vec3  triline = mix(1.0 - frac, frac, vec3(offset));
        float weight  = triline.x * triline.y * triline.z;

        int  probeIndex = ddgiProbeIndexFromCoord(coord, v);
        vec3 probePos   = ddgiProbeWorldPositionFromCoord(coord, v);

        vec3  toProbe   = normalize(probePos - worldPos);
        float dirWeight = max(0.0, (dot(toProbe, normal) + 1.0) * 0.5);
        weight *= dirWeight * dirWeight + 0.2;

        vec3  probeToPoint = biasedPos - probePos;
        float distToProbe  = length(probeToPoint);
        vec2  distUV       = ddgiProbeAtlasUV(probeIndex, normalize(probeToPoint), distInterior, v);
        weight *= max(ddgiChebyshev(distToProbe, texture(distAtlas, distUV).rg), 0.05);

        vec2 irrUV     = ddgiProbeAtlasUV(probeIndex, normal, irrInterior, v);
        sumIrradiance += weight * texture(irrAtlas, irrUV).rgb;
        sumWeight     += weight;
    }
    return sumIrradiance / max(sumWeight, 0.0001);
}

#endif
