#pragma once

#include "RendererAPI.h"
#include "Shader.h"
#include "Buffer.h"
#include "StorageBuffer.h"

#include <variant>
#include <vector>
#include <string>
#include <functional>

namespace Hominem {

class RenderCommand;

namespace Cmd {
    struct BindShader       { Ref<Shader> shader; };
    struct SetInt           { Ref<Shader> shader; std::string name; int v; };
    struct SetFloat         { Ref<Shader> shader; std::string name; float v; };
    struct SetFloat3        { Ref<Shader> shader; std::string name; glm::vec3 v; };
    struct SetFloat4        { Ref<Shader> shader; std::string name; glm::vec4 v; };
    struct SetMat4          { Ref<Shader> shader; std::string name; glm::mat4 v; };
    struct BindTexture      { uint32_t slot; uint32_t id; };
    struct SetBufferData    { Ref<VertexBuffer> vbo; std::vector<uint8_t> data; };
    struct DrawIndexed            { Ref<VertexArray> va; };
    struct DrawIndexedLines       { Ref<VertexArray> va; uint32_t indexCount; };
    struct DrawFullscreenTriangle {};
    struct DrawUnitQuad           {};
    struct DrawPoints             { uint32_t count; float pointSize; };
    struct DrawPatches            { uint32_t count; uint32_t patchVertices; };
    struct DrawArraysIndirect     { uint32_t offset; };

    struct SetUniformBufferData  { Ref<UniformBuffer> ubo; std::vector<uint8_t> data; };
    struct SetStorageBufferData  { Ref<StorageBuffer> ssbo; std::vector<uint8_t> data; };
    struct BindStorageBufferBase { Ref<StorageBuffer> ssbo; uint32_t slot; };
    struct ComputeSetUint        { Ref<ComputeShader> shader; std::string name; uint32_t v; };
    struct ComputeSetInt         { Ref<ComputeShader> shader; std::string name; int v; };
    struct ComputeSetMat4        { Ref<ComputeShader> shader; std::string name; glm::mat4 v; };
    struct DispatchCompute       { Ref<ComputeShader> shader; uint32_t groupsX; uint32_t groupsY; uint32_t groupsZ; };

    struct SetPipelineState { PipelineState state; };
    struct SetViewport      { uint32_t x; uint32_t y; uint32_t width; uint32_t height; };
    struct SetClearColor    { glm::vec4 color; };
    struct Clear            {};
    struct BindFramebuffer  { uint32_t id; };

    struct BindVAORaw                 { uint32_t vaoID; };
    struct DrawElementsBaseVertex     { uint32_t indexCount; uint32_t indexByteOffset; int32_t baseVertex; };
    struct UpdateBufferSubData        { uint32_t bufferID; std::vector<uint8_t> data; uint32_t offset; };
    struct BindShaderStorageBufferBase{ uint32_t bufferID; uint32_t slot; };
    struct BindDrawIndirectBuffer     { uint32_t bufferID; };
    struct MultiDrawElementsIndirect  { uint32_t byteOffset; uint32_t drawCount; uint32_t stride; };
    struct SetCullFaceEnabled         { bool enabled; };

    /// For immediate work that has no typed Cmd:: equivalent yet
    /// (ImGui rendering, Renderer2D::Flush, AutoExposure::Compute). Runs in
    /// recorded order at Submit() time.
    struct Invoke { std::function<void()> fn; };
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
    Cmd::DrawArraysIndirect,
    Cmd::SetUniformBufferData,
    Cmd::SetStorageBufferData,
    Cmd::BindStorageBufferBase,
    Cmd::ComputeSetUint,
    Cmd::ComputeSetInt,
    Cmd::ComputeSetMat4,
    Cmd::DispatchCompute,
    Cmd::SetPipelineState,
    Cmd::SetViewport,
    Cmd::SetClearColor,
    Cmd::Clear,
    Cmd::BindFramebuffer,
    Cmd::BindVAORaw,
    Cmd::DrawElementsBaseVertex,
    Cmd::UpdateBufferSubData,
    Cmd::BindShaderStorageBufferBase,
    Cmd::BindDrawIndirectBuffer,
    Cmd::MultiDrawElementsIndirect,
    Cmd::SetCullFaceEnabled,
    Cmd::Invoke
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

    void SetUniformBufferData (const Ref<UniformBuffer>& ubo, const void* data, uint32_t size);
    void SetStorageBufferData (const Ref<StorageBuffer>& ssbo, const void* data, uint32_t size);
    void BindStorageBufferBase(const Ref<StorageBuffer>& ssbo, uint32_t slot);

    void ComputeSetUint (const Ref<ComputeShader>& shader, const std::string& name, uint32_t v);
    void ComputeSetInt  (const Ref<ComputeShader>& shader, const std::string& name, int v);
    void ComputeSetMat4 (const Ref<ComputeShader>& shader, const std::string& name, const glm::mat4& v);
    void DispatchCompute(const Ref<ComputeShader>& shader, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);

    void SetPipelineState(const PipelineState& state);
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void SetClearColor(const glm::vec4& color);
    void Clear();
    void BindFramebuffer(uint32_t id);

    void BindVAORaw(uint32_t vaoID);
    void DrawElementsBaseVertex(uint32_t indexCount, uint32_t indexByteOffset, int32_t baseVertex);
    void UpdateBufferSubData(uint32_t bufferID, const void* data, uint32_t size, uint32_t offset = 0);
    void BindShaderStorageBufferBase(uint32_t bufferID, uint32_t slot);
    void BindDrawIndirectBuffer(uint32_t bufferID);
    void MultiDrawElementsIndirect(uint32_t byteOffset, uint32_t drawCount, uint32_t stride);
    void SetCullFaceEnabled(bool enabled);

    /// Records arbitrary immediate work to run in order at Submit() time.
    void Invoke(std::function<void()> fn);

    /// Replays all recorded commands through the RendererAPI in order, then clears the list.
    void Submit();

private:
    explicit CommandList(RendererAPI* api) : m_API(api) {}

    RendererAPI*            m_API = nullptr;
    std::vector<GpuCommand> m_Cmds;

    friend class RenderCommand;
};

}
