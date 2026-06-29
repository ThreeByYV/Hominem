#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <optional>
#include <any>

#include "Hominem/Renderer/Framebuffer.h"
#include "Hominem/Renderer/CommandList.h"

namespace Hominem {

struct RenderFrame;

// Named texture slot indices — use instead of raw integers in PassBuilder::Read.
enum class Slot : uint32_t {
    Color0  = 0,  // primary color input
    Color1  = 1,  // secondary color / blend target
    Color2  = 2,
    Color3  = 3,
    Normal  = 4,  // view-space normal map
    Depth   = 5,  // shadow / scene depth
    Env     = 6,  // environment / IBL cubemap
};

// ── PassBuilder ───────────────────────────────────────────────────────────────
// Fluent declaration of what a pass reads and writes.
// The graph binds all declared resources before calling the pass fn and unbinds
// them after — the fn never touches BindTexture / BindFramebuffer for its IO.
//
// Convention: .Read(...) before .WriteFBO(...) — inputs then output.
struct ResourceBinding { std::string name; uint32_t slot; };

struct PassBuilder
{
    std::vector<ResourceBinding> reads;
    std::optional<std::string>   writeFBO;

    PassBuilder& Read(std::string_view resource, Slot slot)
    {
        reads.push_back({ std::string(resource), static_cast<uint32_t>(slot) });
        return *this;
    }

    PassBuilder& WriteFBO(std::string_view fboName)
    {
        writeFBO = std::string(fboName);
        return *this;
    }
};

class RenderGraph
{
public:
    using PassFn = std::function<void(RenderGraph&, const RenderFrame&, CommandList&)>;

    // Appends a named pass. The graph applies state, IO bindings, and cleanup
    // around the fn each frame — fn only needs to issue draw commands.
    void AddPass(std::string name, PipelineState state, PassBuilder io, PassFn fn);

    // Registers a named render target. scale is relative to the viewport.
    void AddFBO(std::string name, FramebufferFormat format, float scale = 1.0f, uint32_t numColorAttachments = 1);

    // Returns a previously declared FBO. May be null before the first valid resize.
    Ref<Framebuffer> GetFBO(const std::string& name);

    // Records all passes into CommandLists. Resets the blackboard at the start
    // of each frame so stale entries never bleed across frames.
    std::vector<CommandList> Record(const RenderFrame& frame);

    void  Resize(uint32_t w, uint32_t h);
    void  SetRenderScale(float scale);
    float GetRenderScale() const { return m_RenderScale; }

    // ── Typed blackboard — inter-pass CPU-side data (matrices, scalars, etc.) ──
    template<typename T>
    void SetBlackboard(T value)
    {
        m_Blackboard[typeid(T).hash_code()] = std::make_any<T>(std::move(value));
    }

    template<typename T>
    const T& GetBlackboard() const
    {
        auto it = m_Blackboard.find(typeid(T).hash_code());
        HMN_CORE_ASSERT(it != m_Blackboard.end(), "RenderGraph::GetBlackboard — type not registered");
        return std::any_cast<const T&>(it->second);
    }

    template<typename T>
    bool HasBlackboard() const
    {
        return m_Blackboard.count(typeid(T).hash_code()) > 0;
    }

    void ResetBlackboard() { m_Blackboard.clear(); }

private:
    void     OnResize(uint32_t w, uint32_t h);
    uint32_t ResolveResource(const std::string& name);

    struct Pass     { std::string name; PipelineState state; PassBuilder io; PassFn fn; };
    struct FBOEntry { Ref<Framebuffer> fbo; FramebufferFormat format; float scale = 1.0f; uint32_t numColorAttachments = 1; };

    std::vector<Pass>                         m_Passes;
    std::unordered_map<std::string, FBOEntry> m_FBOs;
    std::unordered_map<size_t, std::any>      m_Blackboard;
    uint32_t m_Width       = 0;
    uint32_t m_Height      = 0;
    float    m_RenderScale = 1.0f;
};

}
