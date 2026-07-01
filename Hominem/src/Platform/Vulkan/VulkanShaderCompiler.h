#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace Hominem {

enum class VulkanShaderStage
{
    Vertex,
    Fragment,
    Compute,
    Geometry,
};

class VulkanShaderCompiler
{
public:
    static std::vector<uint32_t> Compile(const std::string& source,
                                         VulkanShaderStage stage,
                                         const std::string& debugName = "shader");
};

}
