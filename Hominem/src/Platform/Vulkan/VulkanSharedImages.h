#pragma once

#include "Hominem/Renderer/SharedImages.h"
#include "VulkanRenderTarget.h"

#include <string>
#include <vector>

namespace Hominem {

/**
 * Ray-traced results Vulkan hands to the GL passes, registered by name.
 *
 * A technique calls Publish every time it (re)creates an exportable render target; the
 * generation bump tells the GL side to re-import. Nothing else in the interop chain has
 * to know what the image is for.
 */
class VulkanSharedImages
{
public:
    void Publish(std::string name, const VulkanRenderTarget& rt, bool generalLayout = true)
    {
        HMN_CORE_ASSERT(rt.IsShared(), "VulkanSharedImages: '{0}' is not exportable — "
                                       "create it with VulkanRenderTarget::CreateShared", name);

        for (auto& e : m_Entries)
        {
            if (e.name != name) continue;
            e.target        = &rt;
            e.generalLayout = generalLayout;
            m_Generation++;
            return;
        }
        m_Entries.push_back({ std::move(name), &rt, generalLayout });
        m_Generation++;
    }

    void Unpublish(std::string_view name)
    {
        for (auto it = m_Entries.begin(); it != m_Entries.end(); ++it)
        {
            if (it->name != name) continue;
            m_Entries.erase(it);
            m_Generation++;
            return;
        }
    }

    /// 0 while nothing is published. Poll this before Collect — that mints Win32 handles.
    uint32_t GetGeneration() const { return m_Entries.empty() ? 0 : m_Generation; }

    /// Win32 handles for every published image. The caller owns them and must CloseHandle.
    std::vector<SharedImageExport> Collect(VkDevice device) const
    {
        std::vector<SharedImageExport> out;
        out.reserve(m_Entries.size());
        for (const auto& e : m_Entries)
        {
            const VkExtent2D extent = e.target->GetExtent();
            out.push_back({ e.name, { e.target->GetWin32Handle(device), e.target->GetMemorySize(),
                                      extent.width, extent.height, e.generalLayout } });
        }
        return out;
    }

private:
    // Points at the technique's render target, which owns the memory and outlives the entry
    // only if it republishes on recreate — which is exactly what the generation tracks.
    struct Entry
    {
        std::string               name;
        const VulkanRenderTarget* target;
        bool                      generalLayout;
    };

    std::vector<Entry> m_Entries;
    uint32_t           m_Generation = 0;
};

}
