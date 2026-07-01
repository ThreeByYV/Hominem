#pragma once

#include "CommandList.h"

namespace Hominem {

class RenderCommand
{
public:
    inline static void Init()     { s_RendererAPI->Init(); }
    inline static void Shutdown() { s_RendererAPI->Shutdown(); }

    /// Sets all rasterizer/blend/depth state and returns a CommandList token to issue draws.
    /// Every draw call must go through the returned CommandList, you cannot draw without one.
    inline static CommandList SetPipelineState(const PipelineState& s)
    {
        s_RendererAPI->ApplyState(s);
        return CommandList{ s_RendererAPI };
    }

    /// Returns a CommandList with no immediate state change. Use cmd.SetPipelineState(s)
    /// to record the state change as the first recorded command instead.
    inline static CommandList GetCommandList()
    {
        return CommandList{ s_RendererAPI };
    }

    inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        s_RendererAPI->SetViewport(x, y, width, height);
    }

    inline static void SetClearColor(const glm::vec4& color)
    {
        s_RendererAPI->SetClearColor(color);
    }

    inline static void Clear()
    {
        s_RendererAPI->Clear();
    }

    inline static void SetDepthTestEnabled(bool enabled)
    {
        s_RendererAPI->SetDepthTestEnabled(enabled);
    }

    inline static void SetDepthWriteEnabled(bool enabled)
    {
        s_RendererAPI->SetDepthWriteEnabled(enabled);
    }

    inline static void SetBlendMode(BlendMode mode)
    {
        s_RendererAPI->SetBlendMode(mode);
    }

    inline static void SetCullFaceEnabled(bool enabled)
    {
        s_RendererAPI->SetCullFaceEnabled(enabled);
    }

    inline static void SetScissorEnabled(bool enabled)
    {
        s_RendererAPI->SetScissorEnabled(enabled);
    }

    inline static void SetWireframe(bool enabled)
    {
        s_RendererAPI->SetWireframe(enabled);
    }

    inline static void BindEmptyVAO()
    {
        s_RendererAPI->BindEmptyVAO();
    }

    inline static void UnbindVAO()
    {
        s_RendererAPI->UnbindVAO();
    }

    inline static void BindTexture(uint32_t slot, uint32_t id)
    {
        s_RendererAPI->BindTexture(slot, id);
    }

    inline static const char* GetGPUVendor()   { return s_RendererAPI->GetGPUVendor();   }
    inline static const char* GetGPURenderer() { return s_RendererAPI->GetGPURenderer(); }

    inline static uint32_t GenFramebuffer()                                    { return s_RendererAPI->GenFramebuffer(); }
    inline static void     BindFramebuffer(uint32_t id)                        { s_RendererAPI->BindFramebuffer(id); }
    inline static void     UnbindFramebuffer()                                 { s_RendererAPI->UnbindFramebuffer(); }
    inline static void     DeleteFramebuffer(uint32_t id)                      { s_RendererAPI->DeleteFramebuffer(id); }
    inline static void     AttachCubeFace(uint32_t cubeID, int face, int mip)  { s_RendererAPI->AttachCubeFace(cubeID, face, mip); }
    inline static void     Attach2DTexture(uint32_t texID)                     { s_RendererAPI->Attach2DTexture(texID); }

    inline static uint32_t GenRenderbuffer()                                   { return s_RendererAPI->GenRenderbuffer(); }
    inline static void     BindRenderbuffer(uint32_t id)                       { s_RendererAPI->BindRenderbuffer(id); }
    inline static void     RenderbufferDepth(uint32_t width, uint32_t height)  { s_RendererAPI->RenderbufferDepth(width, height); }
    inline static void     AttachRenderbuffer(uint32_t rboID)                  { s_RendererAPI->AttachRenderbuffer(rboID); }
    inline static void     DeleteRenderbuffer(uint32_t id)                     { s_RendererAPI->DeleteRenderbuffer(id); }

private:
    static RendererAPI* s_RendererAPI;
    friend class CommandList;
};

}
