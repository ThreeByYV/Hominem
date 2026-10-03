#pragma once

#include "Hominem/Renderer/Geometry/Material.h"

#include "Hominem/Renderer/Geometry/Skeleton.h"
#include "Hominem/Renderer/RHI/Texture.h"

#include <glm/glm.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Hominem {

struct SkinnedSubmesh
{
    uint32_t NumIndices    = 0;
    uint32_t BaseVertex    = 0;
    uint32_t BaseIndex     = 0;
    uint32_t MaterialIndex = 0xFFFFFFFF;
};

/// CPU-side result of importing a skinned mesh. Backend-agnostic; a renderer
/// uploads the geometry and feeds the skeleton block to a Skeleton.
struct SkinnedMeshData
{
    // geometry
    std::vector<glm::vec3>      Positions;
    std::vector<glm::vec3>      Normals;
    std::vector<glm::vec4>      Tangents;   // w: bitangent sign
    std::vector<glm::vec2>      TexCoords;
    std::vector<uint32_t>       Indices;
    std::vector<SkinnedSubmesh> Submeshes;
    std::vector<uint32_t>       SubmeshBaseVertices;
    std::vector<VertexBoneData> VertexBoneData;

    std::vector<Material>       Materials;

    // skeleton (fed to Skeleton::SetData / SetMainAnimation)
    std::vector<SkeletonNode>  Nodes;             // root at index 0
    std::map<std::string, int> BoneNameToIndex;
    std::vector<glm::mat4>     BoneOffsets;        // parallel to bone index
    glm::mat4                  GlobalInverse{ 1.f };
    std::optional<Animation>   MainAnimation;      // animation slot 0 (if the file had one)
    std::vector<Animation>     MoreAnimations;     // the file's others, slots 1..N in file order
};

}
