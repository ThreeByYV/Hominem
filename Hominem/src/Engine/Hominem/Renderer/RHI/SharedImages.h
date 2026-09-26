#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

/// Names are the whole contract between a Vulkan technique that publishes a result and
/// the GL pass that samples it. Add one per new ray-traced output.
namespace SharedImageName {

inline constexpr const char* DDGIIrradiance = "ddgi.irradiance";
inline constexpr const char* DDGIDistance   = "ddgi.distance";

}

/// A Vulkan image to bind as a GL texture over the same device memory.
struct SharedImageDesc
{
    HANDLE   memHandle = nullptr;  // opaque Win32 handle; the importer takes its own reference
    uint64_t memSize   = 0;
    uint32_t width     = 0;
    uint32_t height    = 0;

    /// True when Vulkan leaves the image in VK_IMAGE_LAYOUT_GENERAL (a compute storage
    /// image) rather than shader-read-only. Must match, or the semaphore wait describes
    /// a transition that never happened.
    bool     generalLayout = false;
};

struct SharedImageExport
{
    std::string     name;
    SharedImageDesc desc;
};

/// GL texture ids for the images Vulkan published, by name. Immutable once built; the
/// render thread swaps in a new one when the Vulkan side republishes.
class SharedImageTable
{
public:
    void Set(std::string name, uint32_t texID) { m_Entries.push_back({ std::move(name), texID }); }

    uint32_t Get(std::string_view name) const
    {
        for (const auto& e : m_Entries)
            if (e.name == name) return e.texture;
        return 0;
    }

    const std::vector<uint32_t>& Textures() const { return m_Textures; }

    void Finalize()
    {
        m_Textures.clear();
        m_Textures.reserve(m_Entries.size());
        for (const auto& e : m_Entries) m_Textures.push_back(e.texture);
    }

private:
    struct Entry { std::string name; uint32_t texture; };

    std::vector<Entry>    m_Entries;
    std::vector<uint32_t> m_Textures;
};

}
