#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

namespace Hominem {

class RenderGraph;
class CommandList;

enum class UpscalerBackend : uint8_t { TAA, DLSS, FSR };

struct UpscalerCaps
{
    bool  temporal       = true;  // wants jitter + velocity
    float minRenderScale = 0.25f;
    float maxRenderScale = 1.0f;
    float mipBiasOffset  = 0.f;   // DLSS wants -1
};

// Textures are bound by the upscale pass: Color0 = scene color, Color2 = velocity, Depth = depth.
struct UpscalerInputs
{
    glm::uvec2 renderSize {};
    glm::vec2  jitterUV   {};
    glm::mat4  viewProjUnjittered { 1.f };
    glm::mat4  prevViewProj       { 1.f };
    bool       reset = true;
};

class Upscaler
{
public:
    static constexpr const char* OutputTarget = "upscaled";

    virtual ~Upscaler() = default;

    virtual const char*  GetName() const = 0;
    virtual bool         IsSupported() const = 0;
    virtual UpscalerCaps GetCaps() const = 0;

    /// `requested` is the scene's render-scale setting; DLSS overrides this with its quality mode.
    virtual float GetRenderScale(float requested, glm::uvec2 outputSize) const
    {
        const UpscalerCaps caps = GetCaps();
        return std::clamp(requested, caps.minRenderScale, caps.maxRenderScale);
    }

    virtual void Init() = 0;
    /// Declares the upscaler's targets and aliases OutputTarget to its output.
    virtual void DeclareResources(RenderGraph& graph) = 0;
    virtual void BeginFrame(RenderGraph& graph) = 0;
    virtual void Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd) = 0;
};

}
