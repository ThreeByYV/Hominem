#pragma once

#include "SkinnedMeshData.h"
#include "Hominem/Renderer/Skeleton.h"

#include <expected>
#include <string>

namespace Hominem {

/// Import a skinned mesh (FBX/glTF) into CPU-side SkinnedMeshData: geometry,
/// bone weights, materials, node hierarchy and the main animation (if any).
std::expected<SkinnedMeshData, std::string> ImportSkinnedMesh(const std::string& path);

/// Import a single animation clip from a separate file (channels keyed by node
/// name, to be layered onto an already-loaded skeleton).
std::expected<Animation, std::string> ImportAnimation(const std::string& path);

}
