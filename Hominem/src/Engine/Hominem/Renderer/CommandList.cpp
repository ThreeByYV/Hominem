#include "hmnpch.h"
#include "CommandList.h"

namespace Hominem {

void CommandList::BindShader(const Ref<Shader>& shader)
{
    m_Cmds.push_back(Cmd::BindShader{ shader });
}

void CommandList::SetInt(const Ref<Shader>& shader, const std::string& name, int v)
{
    m_Cmds.push_back(Cmd::SetInt{ shader, shader->GetUniformLocation(name), v });
}

void CommandList::SetFloat(const Ref<Shader>& shader, const std::string& name, float v)
{
    m_Cmds.push_back(Cmd::SetFloat{ shader, shader->GetUniformLocation(name), v });
}

void CommandList::SetFloat3(const Ref<Shader>& shader, const std::string& name, const glm::vec3& v)
{
    m_Cmds.push_back(Cmd::SetFloat3{ shader, shader->GetUniformLocation(name), v });
}

void CommandList::SetFloat4(const Ref<Shader>& shader, const std::string& name, const glm::vec4& v)
{
    m_Cmds.push_back(Cmd::SetFloat4{ shader, shader->GetUniformLocation(name), v });
}

void CommandList::SetMat4(const Ref<Shader>& shader, const std::string& name, const glm::mat4& v)
{
    m_Cmds.push_back(Cmd::SetMat4{ shader, shader->GetUniformLocation(name), v });
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
                c.shader->SetIntLoc(c.loc, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat>)
                c.shader->SetFloatLoc(c.loc, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat3>)
                c.shader->SetFloat3Loc(c.loc, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetFloat4>)
                c.shader->SetFloat4Loc(c.loc, c.v);
            else if constexpr (std::is_same_v<T, Cmd::SetMat4>)
                c.shader->SetMat4Loc(c.loc, c.v);
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
        }, cmd);
    }
    m_Cmds.clear();
}

}
