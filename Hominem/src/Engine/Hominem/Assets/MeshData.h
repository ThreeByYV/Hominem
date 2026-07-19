#pragma once

#include "Hominem/Renderer/Texture.h"

#include <glm/glm.hpp>
#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

namespace Hominem {

struct StaticVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec4 Tangent; ///< xyz = tangent, w = handedness
};

/// One material's worth of geometry within a mesh: an index range plus its textures.
struct MeshDrawGroup
{
    Ref<Texture2D> Albedo;
    Ref<Texture2D> MetalRoughness;
    Ref<Texture2D> NormalMap;
    bool      HasRealMetalRoughness = false;
    bool      HasRealNormalMap      = false;
    uint32_t  IndexByteOffset = 0;
    uint32_t  IndexCount      = 0;
    int32_t   BaseVertex      = 0;
    glm::vec3 AABBMin {  FLT_MAX };
    glm::vec3 AABBMax { -FLT_MAX };
    glm::mat4 NodeTransform { 1.f };
};

/// CPU-side result of importing a static mesh. Backend-agnostic; a renderer uploads it.
struct MeshData
{
    std::vector<StaticVertex>  Vertices;
    std::vector<uint32_t>      Indices;
    std::vector<MeshDrawGroup> Groups;
    glm::vec3 AABBMin { 0.f };
    glm::vec3 AABBMax { 0.f };
};

class StaticMeshData : public RefCounted
{
public:
    explicit StaticMeshData(MeshData meshData) : data(std::move(meshData)) {}

    MeshData data;
};

}
