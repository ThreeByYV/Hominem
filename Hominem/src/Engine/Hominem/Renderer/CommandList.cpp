#include "hmnpch.h"
#include "CommandList.h"

namespace Hominem {

void CommandList::BindShader(const Ref<Shader>& shader)
{
    m_Cmds.push_back(Cmd::BindShader{ shader });
}

void CommandList::SetInt(const Ref<Shader>& shader, const std::string& name, int v)
{
    m_Cmds.push_back(Cmd::SetInt{ shader, name, v });
}

void CommandList::SetFloat(const Ref<Shader>& shader, const std::string& name, float v)
{
    m_Cmds.push_back(Cmd::SetFloat{ shader, name, v });
}

void CommandList::SetFloat3(const Ref<Shader>& shader, const std::string& name, const glm::vec3& v)
{
    m_Cmds.push_back(Cmd::SetFloat3{ shader, name, v });
}

void CommandList::SetFloat4(const Ref<Shader>& shader, const std::string& name, const glm::vec4& v)
{
    m_Cmds.push_back(Cmd::SetFloat4{ shader, name, v });
}

void CommandList::SetMat4(const Ref<Shader>& shader, const std::string& name, const glm::mat4& v)
{
    m_Cmds.push_back(Cmd::SetMat4{ shader, name, v });
}

void CommandList::BindTexture(uint32_t slot, uint32_t id)
{
    m_Cmds.push_back(Cmd::BindTexture{ slot, id });
}

void CommandList::SetBufferData(const Ref<VertexBuffer>& vbo, const void* data, uint32_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_Cmds.push_back(Cmd::SetBufferData{ vbo, std::vector<uint8_t>(bytes, bytes + size) });
}

void CommandList::DrawIndexed(const Ref<VertexArray>& va)
{
    m_Cmds.push_back(Cmd::DrawIndexed{ va });
}

void CommandList::DrawIndexedLines(const Ref<VertexArray>& va, uint32_t indexCount)
{
    m_Cmds.push_back(Cmd::DrawIndexedLines{ va, indexCount });
}

void CommandList::DrawFullscreenTriangle()
{
    m_Cmds.push_back(Cmd::DrawFullscreenTriangle{});
}

void CommandList::DrawUnitQuad()
{
    m_Cmds.push_back(Cmd::DrawUnitQuad{});
}

void CommandList::DrawPoints(uint32_t count, float pointSize)
{
    m_Cmds.push_back(Cmd::DrawPoints{ count, pointSize });
}

void CommandList::DrawPatches(uint32_t count, uint32_t patchVertices)
{
    m_Cmds.push_back(Cmd::DrawPatches{ count, patchVertices });
}

void CommandList::DrawArraysIndirect(uint32_t offset)
{
    m_Cmds.push_back(Cmd::DrawArraysIndirect{ offset });
}

void CommandList::SetUniformBufferData(const Ref<UniformBuffer>& ubo, const void* data, uint32_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_Cmds.push_back(Cmd::SetUniformBufferData{ ubo, std::vector<uint8_t>(bytes, bytes + size) });
}

void CommandList::SetStorageBufferData(const Ref<StorageBuffer>& ssbo, const void* data, uint32_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_Cmds.push_back(Cmd::SetStorageBufferData{ ssbo, std::vector<uint8_t>(bytes, bytes + size) });
}

void CommandList::BindStorageBufferBase(const Ref<StorageBuffer>& ssbo, uint32_t slot)
{
    m_Cmds.push_back(Cmd::BindStorageBufferBase{ ssbo, slot });
}

void CommandList::ComputeSetUint(const Ref<ComputeShader>& shader, const std::string& name, uint32_t v)
{
    m_Cmds.push_back(Cmd::ComputeSetUint{ shader, name, v });
}

void CommandList::ComputeSetInt(const Ref<ComputeShader>& shader, const std::string& name, int v)
{
    m_Cmds.push_back(Cmd::ComputeSetInt{ shader, name, v });
}

void CommandList::ComputeSetMat4(const Ref<ComputeShader>& shader, const std::string& name, const glm::mat4& v)
{
    m_Cmds.push_back(Cmd::ComputeSetMat4{ shader, name, v });
}

void CommandList::DispatchCompute(const Ref<ComputeShader>& shader, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    m_Cmds.push_back(Cmd::DispatchCompute{ shader, groupsX, groupsY, groupsZ });
}

void CommandList::SetPipelineState(const PipelineState& state)
{
    m_Cmds.push_back(Cmd::SetPipelineState{ state });
}

void CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    m_Cmds.push_back(Cmd::SetViewport{ x, y, width, height });
}

void CommandList::SetClearColor(const glm::vec4& color)
{
    m_Cmds.push_back(Cmd::SetClearColor{ color });
}

void CommandList::Clear()
{
    m_Cmds.push_back(Cmd::Clear{});
}

void CommandList::BindFramebuffer(uint32_t id)
{
    m_Cmds.push_back(Cmd::BindFramebuffer{ id });
}

void CommandList::BindVAORaw(uint32_t vaoID)
{
    m_Cmds.push_back(Cmd::BindVAORaw{ vaoID });
}

void CommandList::DrawElementsBaseVertex(uint32_t indexCount, uint32_t indexByteOffset, int32_t baseVertex)
{
    m_Cmds.push_back(Cmd::DrawElementsBaseVertex{ indexCount, indexByteOffset, baseVertex });
}

void CommandList::UpdateBufferSubData(uint32_t bufferID, const void* data, uint32_t size, uint32_t offset)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_Cmds.push_back(Cmd::UpdateBufferSubData{ bufferID, std::vector<uint8_t>(bytes, bytes + size), offset });
}

void CommandList::BindShaderStorageBufferBase(uint32_t bufferID, uint32_t slot)
{
    m_Cmds.push_back(Cmd::BindShaderStorageBufferBase{ bufferID, slot });
}

void CommandList::BindDrawIndirectBuffer(uint32_t bufferID)
{
    m_Cmds.push_back(Cmd::BindDrawIndirectBuffer{ bufferID });
}

void CommandList::MultiDrawElementsIndirect(uint32_t byteOffset, uint32_t drawCount, uint32_t stride)
{
    m_Cmds.push_back(Cmd::MultiDrawElementsIndirect{ byteOffset, drawCount, stride });
}

void CommandList::SetCullFaceEnabled(bool enabled)
{
    m_Cmds.push_back(Cmd::SetCullFaceEnabled{ enabled });
}

void CommandList::Invoke(std::function<void()> fn)
{
    m_Cmds.push_back(Cmd::Invoke{ std::move(fn) });
}

void CommandList::Submit()
{
    for (const auto& cmd : m_Cmds)
    {
        std::visit([&]<typename T0>(const T0& c)
        {
            using T = std::decay_t<T0>;
            if      constexpr (std::is_same_v<T, Cmd::BindShader>)
                c.shader->Bind();
            else if constexpr (std::is_same_v<T, Cmd::SetInt>)
                c.shader->SetInt(c.name, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat>)
                c.shader->SetFloat(c.name, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat3>)
                c.shader->SetFloat3(c.name, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat4>)
                c.shader->SetFloat4(c.name, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetMat4>)
                c.shader->SetMat4(c.name, c.v);
            else if constexpr (std::is_same_v<T, Cmd::BindTexture>)
                m_API->BindTexture(c.slot, c.id);
            else if constexpr (std::is_same_v<T, Cmd::SetBufferData>)
                c.vbo->SetData(c.data.data(), static_cast<uint32_t>(c.data.size()));
            else if constexpr (std::is_same_v<T, Cmd::DrawIndexed>)
                m_API->DrawIndexed(c.va);
            else if constexpr (std::is_same_v<T, Cmd::DrawIndexedLines>)
                m_API->DrawIndexedLines(c.va, c.indexCount);
            else if constexpr (std::is_same_v<T, Cmd::DrawFullscreenTriangle>)
                m_API->DrawFullscreenTriangle();
            else if constexpr (std::is_same_v<T, Cmd::DrawUnitQuad>)
                m_API->DrawUnitQuad();
            else if constexpr (std::is_same_v<T, Cmd::DrawPoints>)
                m_API->DrawPoints(c.count, c.pointSize);
            else if constexpr (std::is_same_v<T, Cmd::DrawPatches>)
                m_API->DrawPatches(c.count, c.patchVertices);
            else if constexpr (std::is_same_v<T, Cmd::DrawArraysIndirect>)
                m_API->DrawArraysIndirect(c.offset);
            else if constexpr (std::is_same_v<T, Cmd::SetUniformBufferData>)
                c.ubo->SetData(c.data.data(), static_cast<uint32_t>(c.data.size()));
            else if constexpr (std::is_same_v<T, Cmd::SetStorageBufferData>)
                c.ssbo->SetData(c.data.data(), static_cast<uint32_t>(c.data.size()));
            else if constexpr (std::is_same_v<T, Cmd::BindStorageBufferBase>)
                c.ssbo->BindBase(c.slot);
            else if constexpr (std::is_same_v<T, Cmd::ComputeSetUint>)
            {
                c.shader->Bind();
                c.shader->SetUint(c.name, c.v);
            }
            else if constexpr (std::is_same_v<T, Cmd::ComputeSetInt>)
            {
                c.shader->Bind();
                c.shader->SetInt(c.name, c.v);
            }
            else if constexpr (std::is_same_v<T, Cmd::ComputeSetMat4>)
            {
                c.shader->Bind();
                c.shader->SetMat4(c.name, c.v);
            }
            else if constexpr (std::is_same_v<T, Cmd::DispatchCompute>)
            {
                c.shader->Bind();
                c.shader->Dispatch(c.groupsX, c.groupsY, c.groupsZ);
            }
            else if constexpr (std::is_same_v<T, Cmd::SetPipelineState>)
                m_API->ApplyState(c.state);
            else if constexpr (std::is_same_v<T, Cmd::SetViewport>)
                m_API->SetViewport(c.x, c.y, c.width, c.height);
            else if constexpr (std::is_same_v<T, Cmd::SetClearColor>)
                m_API->SetClearColor(c.color);
            else if constexpr (std::is_same_v<T, Cmd::Clear>)
                m_API->Clear();
            else if constexpr (std::is_same_v<T, Cmd::BindFramebuffer>)
                m_API->BindFramebuffer(c.id);
            else if constexpr (std::is_same_v<T, Cmd::BindVAORaw>)
                m_API->BindVertexArray(c.vaoID);
            else if constexpr (std::is_same_v<T, Cmd::DrawElementsBaseVertex>)
                m_API->DrawElementsBaseVertex(c.indexCount, c.indexByteOffset, c.baseVertex);
            else if constexpr (std::is_same_v<T, Cmd::UpdateBufferSubData>)
                m_API->UpdateBufferSubData(c.bufferID, c.data.data(), static_cast<uint32_t>(c.data.size()), c.offset);
            else if constexpr (std::is_same_v<T, Cmd::BindShaderStorageBufferBase>)
                m_API->BindShaderStorageBufferBase(c.bufferID, c.slot);
            else if constexpr (std::is_same_v<T, Cmd::BindDrawIndirectBuffer>)
                m_API->BindDrawIndirectBuffer(c.bufferID);
            else if constexpr (std::is_same_v<T, Cmd::MultiDrawElementsIndirect>)
                m_API->MultiDrawElementsIndirect(c.byteOffset, c.drawCount, c.stride);
            else if constexpr (std::is_same_v<T, Cmd::SetCullFaceEnabled>)
                m_API->SetCullFaceEnabled(c.enabled);
            else if constexpr (std::is_same_v<T, Cmd::Invoke>)
                c.fn();
        }, cmd);
    }
    m_Cmds.clear();
}

}
