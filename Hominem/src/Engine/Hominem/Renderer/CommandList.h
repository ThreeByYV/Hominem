#pragma once

#include "RendererAPI.h"
#include "Shader.h"
#include "Buffer.h"

#include <variant>
#include <vector>
#include <string>

namespace Hominem {

class RenderCommand;

namespace Cmd {
    struct BindShader       { Ref<Shader> shader; };
    struct SetInt           { Ref<Shader> shader; int loc; int v; };
    struct SetFloat         { Ref<Shader> shader; int loc; float v; };
    struct SetFloat3        { Ref<Shader> shader; int loc; glm::vec3 v; };
    struct SetFloat4        { Ref<Shader> shader; int loc; glm::vec4 v; };
    struct SetMat4          { Ref<Shader> shader; int loc; glm::mat4 v; };
    struct BindTexture      { uint32_t slot; uint32_t id; };
    struct SetBufferData    { Ref<VertexBuffer> vbo; std::vector<uint8_t> data; };
    struct DrawIndexed            { Ref<VertexArray> va; };
    struct DrawIndexedLines       { Ref<VertexArray> va; uint32_t indexCount; };
    struct DrawFullscreenTriangle {};
    struct DrawUnitQuad           {};
    struct DrawPoints             { uint32_t count; float pointSize; };
    struct DrawPatches            { uint32_t count; uint32_t patchVertices; };
    struct DrawArraysIndirect     { uint32_t offset; };
}

using GpuCommand = std::variant<
    Cmd::BindShader,
    Cmd::SetInt,
    Cmd::SetFloat,
    Cmd::SetFloat3,
    Cmd::SetFloat4,
    Cmd::SetMat4,
    Cmd::BindTexture,
    Cmd::SetBufferData,
    Cmd::DrawIndexed,
    Cmd::DrawIndexedLines,
    Cmd::DrawFullscreenTriangle,
    Cmd::DrawUnitQuad,
    Cmd::DrawPoints,
    Cmd::DrawPatches,
    Cmd::DrawArraysIndirect
>;

class CommandList
{
public:
    CommandList(const CommandList&)            = default;
    CommandList& operator=(const CommandList&) = default;
    CommandList(CommandList&&)                 = default;
    CommandList& operator=(CommandList&&)      = default;

    void BindShader (const Ref<Shader>& shader);
    void SetInt     (const Ref<Shader>& shader, const std::string& name, int v);
    void SetFloat   (const Ref<Shader>& shader, const std::string& name, float v);
    void SetFloat3  (const Ref<Shader>& shader, const std::string& name, const glm::vec3& v);
    void SetFloat4  (const Ref<Shader>& shader, const std::string& name, const glm::vec4& v);
    void SetMat4    (const Ref<Shader>& shader, const std::string& name, const glm::mat4& v);
    void BindTexture   (uint32_t slot, uint32_t id);
    void SetBufferData (const Ref<VertexBuffer>& vbo, const void* data, uint32_t size);

    void DrawIndexed          (const Ref<VertexArray>& va);
    void DrawIndexedLines     (const Ref<VertexArray>& va, uint32_t indexCount);
    void DrawFullscreenTriangle();
    void DrawUnitQuad         ();
    void DrawPoints           (uint32_t count, float pointSize = 1.f);
    void DrawPatches          (uint32_t count, uint32_t patchVertices = 1);
    void DrawArraysIndirect   (uint32_t offset);

    /// Replays all recorded commands through the RendererAPI in order, then clears the list.
    void Submit();

private:
    explicit CommandList(RendererAPI* api) : m_API(api) {}

    RendererAPI*            m_API = nullptr;
    std::vector<GpuCommand> m_Cmds;

    friend class RenderCommand;
};

}
