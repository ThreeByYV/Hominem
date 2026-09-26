#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Hominem {

class RenderGraph;
class CommandList;

// Temporal upscaler port (TAA, DLSS, FSR). Vendor SDK headers stay in each backend's folder.
// The engine owns render resolution, jitter, velocity and mip bias; the upscaler owns its history.

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

    virtual void Init() = 0;
    /// Declares the upscaler's targets and aliases OutputTarget to its output.
    virtual void DeclareResources(RenderGraph& graph) = 0;
    virtual void BeginFrame(RenderGraph& graph) = 0;
    virtual void Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd) = 0;
};

}
