#include "hmnpch.h"
#include "VulkanRenderGraph.h"
#include "VulkanBarrier.h"

namespace Hominem {

namespace {

struct StageAccess
{
    VkPipelineStageFlags2 stage;
    VkAccessFlags2        access;
    bool                  reads;
    bool                  writes;
};

StageAccess ToStageAccess(GraphUsage usage)
{
    switch (usage)
    {
        case GraphUsage::ComputeRead:
            return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, true, false };
        case GraphUsage::ComputeWrite:
            return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, false, true };
        case GraphUsage::ComputeReadWrite:
            return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, true, true };
        case GraphUsage::ComputeSample:
            return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, true, false };
        case GraphUsage::FragmentSample:
            return { VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, true, false };
        case GraphUsage::VertexRead:
            return { VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, true, false };
    }
    return { 0, 0, false, false };
}

}

void VulkanRenderGraph::Begin()
{
    m_Passes.clear();
    m_ResourceNames.clear();
    m_State.clear();
}

VulkanRenderGraph::ResourceId VulkanRenderGraph::AddResource(std::string debugName)
{
    m_ResourceNames.push_back(std::move(debugName));
    m_State.emplace_back();
    return (ResourceId)(m_ResourceNames.size() - 1);
}

void VulkanRenderGraph::AddPass(std::string name, std::vector<Use> uses,
                                std::function<void(VkCommandBuffer)> record)
{
    m_Passes.push_back({ std::move(name), std::move(uses), std::move(record) });
}

void VulkanRenderGraph::Execute(VkCommandBuffer cmd)
{
    for (const Pass& pass : m_Passes)
    {
        // Compare this pass's uses against each resource's last access to find hazards,
        // accumulating one barrier that covers them all.
        VkPipelineStageFlags2 srcStage = 0, dstStage = 0;
        VkAccessFlags2        srcAccess = 0, dstAccess = 0;

        for (const Use& use : pass.uses)
        {
            const StageAccess sa = ToStageAccess(use.usage);
            ResourceState&    st = m_State[use.id];

            // Read after a prior write: the write's results must be flushed from cache
            // (memory dependency) before this read can see them, so set access masks too.
            if (sa.reads && st.writeStage != 0)
            {
                srcStage  |= st.writeStage;  srcAccess |= st.writeAccess;
                dstStage  |= sa.stage;       dstAccess |= sa.access;
            }
            // Write after a prior write: same story, the earlier write must be made
            // available before this one overwrites it.
            if (sa.writes && st.writeStage != 0)
            {
                srcStage  |= st.writeStage;  srcAccess |= st.writeAccess;
                dstStage  |= sa.stage;       dstAccess |= sa.access;
            }
            // Write after a prior read: a read dirties no cache and produces no new data
            // to flush, so we only need to guarantee the read finished executing before
            // this write clobbers it. Order the stages; leave the access masks empty.
            if (sa.writes && st.readStages != 0)
            {
                srcStage  |= st.readStages;
                dstStage  |= sa.stage;
            }
        }

        if (dstStage != 0)
            PipelineBarrier(cmd, MakeMemoryBarrier(srcStage, srcAccess, dstStage, dstAccess));

        // Advance each touched resource's state before running the pass.
        for (const Use& use : pass.uses)
        {
            const StageAccess sa = ToStageAccess(use.usage);
            ResourceState&    st = m_State[use.id];

            if (sa.writes)
            {
                st.writeStage  = sa.stage;
                st.writeAccess = sa.access;
                st.readStages  = 0;
                st.readAccess  = 0;
            }
            else if (sa.reads)
            {
                st.readStages |= sa.stage;
                st.readAccess |= sa.access;
            }
        }

        if (pass.record)
            pass.record(cmd);
    }
}

}
