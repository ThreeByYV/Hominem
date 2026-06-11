#pragma once

#include "MeshData.h"

#include <expected>
#include <string>

namespace Hominem {

/// Import a static mesh (FBX/OBJ/glTF) into CPU-side MeshData; non-glTF formats cache to a sibling ".bin".
std::expected<MeshData, std::string> ImportStaticMesh(const std::string& path);

}
