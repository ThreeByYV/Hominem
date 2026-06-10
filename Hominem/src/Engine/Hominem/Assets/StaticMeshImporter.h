#pragma once

#include "MeshData.h"

#include <expected>
#include <string>

namespace Hominem {

/// Import a static mesh (FBX/OBJ/glTF) into CPU-side MeshData. Geometry is parsed,
/// meshopt-optimised, and AABBs computed; textures are created (GPU upload queued).
/// Non-glTF formats are cached to a sibling ".bin" for fast subsequent loads.
std::expected<MeshData, std::string> ImportStaticMesh(const std::string& path);

}
