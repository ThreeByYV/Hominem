#pragma once

#include "Hominem/Assets/MeshData.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hominem {

// A unit-radius UV sphere in StaticVertex layout, for instanced debug rendering.
struct UvSphereMesh
{
    std::vector<StaticVertex> vertices;
    std::vector<uint32_t>     indices;
};

inline UvSphereMesh GenerateUvSphere(uint32_t rings = 8, uint32_t sectors = 16)
{
    constexpr float kPi = 3.14159265358979323846f;
    UvSphereMesh mesh;

    for (uint32_t r = 0; r <= rings; r++)
    {
        const float phi = kPi * (float)r / (float)rings; // 0..pi
        const float y   = std::cos(phi);
        const float rad = std::sin(phi);

        for (uint32_t s = 0; s <= sectors; s++)
        {
            const float theta = 2.f * kPi * (float)s / (float)sectors;
            const glm::vec3 pos { rad * std::cos(theta), y, rad * std::sin(theta) };
            mesh.vertices.push_back({
                pos, pos,
                { (float)s / sectors, (float)r / rings },
                { 1.f, 0.f, 0.f, 1.f }
            });
        }
    }

    const uint32_t stride = sectors + 1;
    for (uint32_t r = 0; r < rings; r++)
    {
        for (uint32_t s = 0; s < sectors; s++)
        {
            const uint32_t a = r * stride + s;
            const uint32_t b = a + stride;
            mesh.indices.insert(mesh.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
        }
    }
    return mesh;
}

}
