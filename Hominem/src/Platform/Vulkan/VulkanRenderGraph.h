#pragma once

#include "VulkanCore.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Hominem {

/// @brief How a pass touches a resource; maps to a (stage, access) pair in ToStageAccess.
enum class GraphUsage : uint8_t
{
    ComputeRead,       ///< storage read in a compute shader
    ComputeWrite,      ///< storage write in a compute shader
    ComputeReadWrite,  ///< read + write in one dispatch (e.g. blend)
    ComputeSample,     ///< sampled read in a compute shader
    FragmentSample,    ///< sampled read in a fragment shader (external consumer)
    VertexRead,        ///< storage read in a vertex shader (external consumer)
};

/// @brief Per-frame compute render graph: passes declare reads/writes, the graph derives
///        and inserts every barrier between them. It schedules sync, not allocation —
///        resources are caller-owned and referenced by opaque id, all in GENERAL layout.
class VulkanRenderGraph
{
public:
    using ResourceId = uint32_t;

    /// @brief A resource a pass touches, tagged with how.
    struct Use
    {
        ResourceId id;
        GraphUsage usage;
    };

    /// @brief Drop the previous frame's passes and per-resource sync state.
    void Begin();

    /// @brief Register a resource for this frame and return its id.
    ResourceId AddResource(std::string debugName);

    /// @brief Queue a compute pass; @p record may be null for a sync-only handoff step.
    void AddPass(std::string name, std::vector<Use> uses, std::function<void(VkCommandBuffer)> record);

    /// @brief Walk the passes in order, inserting each derived barrier before running it.
    void Execute(VkCommandBuffer cmd);

private:
    struct Pass
    {
        std::string                          name;
        std::vector<Use>                     uses;
        std::function<void(VkCommandBuffer)> record;
    };

    /// @brief Last-access state per resource, used to derive RAW / WAW / WAR hazards.
    struct ResourceState
    {
        VkPipelineStageFlags2 writeStage  = 0;
        VkAccessFlags2        writeAccess = 0;
        VkPipelineStageFlags2 readStages  = 0;
        VkAccessFlags2        readAccess  = 0;
    };

    std::vector<Pass>          m_Passes;
    std::vector<std::string>   m_ResourceNames;
    std::vector<ResourceState> m_State;
};

}
