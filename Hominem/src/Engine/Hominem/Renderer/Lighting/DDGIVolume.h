#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Hominem/Core/Log.h"

#include <cmath>
#include <cstdint>
#include <random>

namespace Hominem {

struct DDGIVolumeDesc
{
    glm::vec3  origin       { 0.f };          // volume center in world space
    glm::vec3  probeSpacing { 1.f };
    glm::ivec3 probeCounts  { 8, 4, 8 };
    int        probeNumRays = 64;
    glm::quat  probeRayRotation { 1.f, 0.f, 0.f, 0.f }; // per-frame random rotation

    int NumProbes() const { return probeCounts.x * probeCounts.y * probeCounts.z; }

    glm::ivec3 ProbeCoords(int probeIndex) const
    {
        return {
            probeIndex % probeCounts.x,
            probeIndex / (probeCounts.x * probeCounts.z),
            (probeIndex / probeCounts.x) % probeCounts.z
        };
    }

    glm::vec3 ProbeWorldPosition(glm::ivec3 probeCoords) const
    {
        glm::vec3 local = glm::vec3(probeCoords) * probeSpacing;
        local -= glm::vec3(probeCounts - 1) * 0.5f * probeSpacing;
        return origin + local;
    }

    /*
     * The volume extends past the  geometry by `paddingProbes` on every axis so at least one layer of probes sits
     * outside each wall/floor/ceiling, the visibility term needs probes on both sides
     * of a surface to reject light leaks. Density is chosen by world-space spacing, not
     * by a fixed probe count; counts derive from the padded bounds.
     */
    static DDGIVolumeDesc FitToBounds(glm::vec3 aabbMin, glm::vec3 aabbMax,
                                      float targetSpacing, int paddingProbes = 1,
                                      int maxProbes = 8192)
    {
        // Counts follow from spacing, so a large volume at a fine spacing has no upper
        // bound - and every probe traces probeNumRays rays through the whole TLAS every
        // frame. Fitting a 40x30x55 m building at 1.25 m gives 45k probes and ~2.9M rays
        // per frame, which hangs the GPU outright rather than merely running slowly.
        // Coarsen until the total fits: worse GI density beats not rendering.
        float spacing       = targetSpacing;
        int   requestedCount = 0;

        DDGIVolumeDesc desc;
        for (;;)
        {
            const glm::vec3 pad       = glm::vec3(spacing * (float)paddingProbes);
            const glm::vec3 paddedMin = aabbMin - pad;
            const glm::vec3 paddedMax = aabbMax + pad;
            const glm::vec3 extent    = paddedMax - paddedMin;

            desc.probeCounts  = glm::max(glm::ivec3(glm::ceil(extent / spacing)) + 1, glm::ivec3(2));
            desc.origin       = (paddedMin + paddedMax) * 0.5f;
            desc.probeSpacing = extent / glm::vec3(desc.probeCounts - 1);

            if (requestedCount == 0) requestedCount = desc.NumProbes();

            if (desc.NumProbes() <= maxProbes || desc.probeCounts == glm::ivec3(2))
                break;
            spacing *= 1.25f;
        }

        if (spacing > targetSpacing)
            HMN_CORE_WARN("DDGI: {0} probes at {1:.2f} m exceeds the {2} budget - coarsened to {3} probes at {4:.2f} m",
                          requestedCount, targetSpacing, maxProbes, desc.NumProbes(), spacing);
        return desc;
    }
};

inline glm::vec3 SphericalFibonacci(float sampleIndex, float numSamples)
{
    constexpr float kTwoPi = 6.2831853071795864769f;
    const float b        = (std::sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    const float phi      = kTwoPi * glm::fract(sampleIndex * b);
    const float cosTheta = 1.f - (2.f * sampleIndex + 1.f) * (1.f / numSamples);
    const float sinTheta = std::sqrt(glm::clamp(1.f - cosTheta * cosTheta, 0.f, 1.f));
    return { std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta };
}

inline glm::vec3 QuaternionRotate(glm::vec3 v, glm::quat q)
{
    const glm::vec3 b  = { q.x, q.y, q.z };
    const float     b2 = glm::dot(b, b);
    return v * (q.w * q.w - b2) + b * (glm::dot(v, b) * 2.f) + glm::cross(b, v) * (q.w * 2.f);
}

inline glm::quat QuaternionConjugate(glm::quat q)
{
    return { q.w, -q.x, -q.y, -q.z };
}

// This is for the base case (no relocation/classification):
// spherical-Fibonacci direction rotated by the volume's per-frame random rotation.
inline glm::vec3 ProbeRayDirection(int rayIndex, const DDGIVolumeDesc& volume)
{
    const glm::vec3 direction = SphericalFibonacci((float)rayIndex, (float)volume.probeNumRays);
    return glm::normalize(QuaternionRotate(direction, QuaternionConjugate(volume.probeRayRotation)));
}

// Uniformly-distributed random rotation (Shoemake) for probeRayRotation each frame.
inline glm::quat RandomRotationQuaternion()
{
    static thread_local std::mt19937 rng { std::random_device{}() };
    std::uniform_real_distribution<float> dist(0.f, 1.f);

    const float u1 = dist(rng), u2 = dist(rng), u3 = dist(rng);
    constexpr float kTwoPi = 6.2831853071795864769f;
    const float s1 = std::sqrt(1.f - u1), s2 = std::sqrt(u1);
    return {
        s2 * std::cos(kTwoPi * u3), // w
        s1 * std::sin(kTwoPi * u2), // x
        s1 * std::cos(kTwoPi * u2), // y
        s2 * std::sin(kTwoPi * u3)  // z
    };
}

}
