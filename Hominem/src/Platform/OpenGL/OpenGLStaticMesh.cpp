#include "hmnpch.h"
#include "OpenGLStaticMesh.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Renderer/Frustum.h"
#include "Hominem/Assets/StaticMeshImporter.h"
#include "Hominem/Assets/MaterialTextures.h"
#include "Hominem/Core/Profiler.h"

#include <glad/glad.h>

namespace Hominem {

    OpenGLStaticMesh::~OpenGLStaticMesh()
    {
        if (m_VAO)               { glDeleteVertexArrays(1, &m_VAO);           m_VAO               = 0; }
        if (m_VBO)               { glDeleteBuffers(1, &m_VBO);                m_VBO               = 0; }
        if (m_IBO)               { glDeleteBuffers(1, &m_IBO);                m_IBO               = 0; }
        if (m_ModelMatrixSSBO)   { glDeleteBuffers(1, &m_ModelMatrixSSBO);    m_ModelMatrixSSBO   = 0; }
        if (m_DrawCommandBuffer) { glDeleteBuffers(1, &m_DrawCommandBuffer);  m_DrawCommandBuffer = 0; }
    }

    std::expected<void, std::string> OpenGLStaticMesh::LoadFromFile(const std::string& path)
    {
        auto result = ImportStaticMesh(path);
        if (!result) return std::unexpected(result.error());

        MeshData data    = std::move(*result);
        m_DrawGroups     = std::move(data.Groups);
        m_AABBMin        = data.AABBMin;
        m_AABBMax        = data.AABBMax;
        m_PendingVerts   = std::move(data.Vertices);
        m_PendingIndices = std::move(data.Indices);

        RenderThread::QueueUpload([this] {
            Upload(m_PendingVerts, m_PendingIndices);
            m_PendingVerts   = {};
            m_PendingIndices = {};
        });
        return {};
    }

    void OpenGLStaticMesh::Upload(const std::vector<StaticVertex>& verts, const std::vector<uint32_t>& indices)
    {
        if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); }
        if (m_VBO) { glDeleteBuffers(1, &m_VBO); }
        if (m_IBO) { glDeleteBuffers(1, &m_IBO); }

        glCreateVertexArrays(1, &m_VAO);
        glCreateBuffers(1, &m_VBO);
        glCreateBuffers(1, &m_IBO);

        glNamedBufferData(m_VBO, (GLsizeiptr)(verts.size() * sizeof(StaticVertex)), verts.data(), GL_STATIC_DRAW);
        glNamedBufferData(m_IBO, (GLsizeiptr)(indices.size() * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(m_VAO, 0, m_VBO, 0, sizeof(StaticVertex));
        glVertexArrayElementBuffer(m_VAO, m_IBO);

        glEnableVertexArrayAttrib(m_VAO, 0);
        glVertexArrayAttribFormat(m_VAO, 0, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(StaticVertex, Position));
        glVertexArrayAttribBinding(m_VAO, 0, 0);

        glEnableVertexArrayAttrib(m_VAO, 1);
        glVertexArrayAttribFormat(m_VAO, 1, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(StaticVertex, Normal));
        glVertexArrayAttribBinding(m_VAO, 1, 0);

        glEnableVertexArrayAttrib(m_VAO, 2);
        glVertexArrayAttribFormat(m_VAO, 2, 2, GL_FLOAT, GL_FALSE, (GLuint)offsetof(StaticVertex, TexCoord));
        glVertexArrayAttribBinding(m_VAO, 2, 0);

        glEnableVertexArrayAttrib(m_VAO, 3);
        glVertexArrayAttribFormat(m_VAO, 3, 4, GL_FLOAT, GL_FALSE, (GLuint)offsetof(StaticVertex, Tangent));
        glVertexArrayAttribBinding(m_VAO, 3, 0);

        const uint32_t maxGroups = std::max((uint32_t)m_DrawGroups.size(), 1u);
        glCreateBuffers(1, &m_ModelMatrixSSBO);
        glNamedBufferData(m_ModelMatrixSSBO, (GLsizeiptr)(maxGroups * sizeof(glm::mat4)), nullptr, GL_DYNAMIC_DRAW);
        glCreateBuffers(1, &m_DrawCommandBuffer);
        glNamedBufferData(m_DrawCommandBuffer, (GLsizeiptr)(maxGroups * 5 * sizeof(uint32_t)), nullptr, GL_DYNAMIC_DRAW);
    }

    bool OpenGLStaticMesh::HasNormalMap() const
    {
        for (const auto& g : m_DrawGroups)
            if (g.HasRealNormalMap) return true;
        return false;
    }

    bool OpenGLStaticMesh::HasMetalRoughness() const
    {
        for (const auto& g : m_DrawGroups)
            if (g.HasRealMetalRoughness) return true;
        return false;
    }

    std::pair<uint32_t, uint64_t> OpenGLStaticMesh::Draw(
        const Ref<Shader>& shader, const glm::mat4& actorTransform, const Frustum* frustum)
    {
        HMN_PROFILE_FUNCTION();
        if (!m_VAO || m_DrawGroups.empty()) return { 0, 0 };

        // GL indirect draw command — exactly matches DrawElementsIndirectCommand.
        struct DrawCmd {
            uint32_t count;
            uint32_t instanceCount;
            uint32_t firstIndex;   // in indices (not bytes)
            uint32_t baseVertex;
            uint32_t baseInstance; // unused — kept to match DrawElementsIndirectCommand layout
        };

        // Thread-local scratch avoids per-frame heap allocation after first call.
        struct BatchInfo {
            const Texture2D* albedo;
            const Texture2D* mr;
            const Texture2D* normal;
            uint32_t         tris;
        };
        thread_local static std::vector<glm::mat4> tl_matrices;
        thread_local static std::vector<DrawCmd>   tl_cmds;
        thread_local static std::vector<BatchInfo> tl_batches;
        tl_matrices.clear(); tl_matrices.reserve(m_DrawGroups.size());
        tl_cmds.clear();     tl_cmds.reserve(m_DrawGroups.size());
        tl_batches.clear();  tl_batches.reserve(m_DrawGroups.size());

        for (const auto& group : m_DrawGroups)
        {
            const glm::mat4 model = actorTransform * group.NodeTransform;
            if (frustum && !frustum->TestAABBTransformed(group.AABBMin, group.AABBMax, model))
                continue;

            tl_matrices.push_back(model);
            tl_cmds.push_back({
                group.IndexCount,
                1u,
                group.IndexByteOffset / (uint32_t)sizeof(uint32_t),
                (uint32_t)group.BaseVertex,
                0u
            });
            tl_batches.push_back({
                (group.Albedo         ? group.Albedo         : WhiteTexture()).get(),
                (group.MetalRoughness ? group.MetalRoughness : DefaultMetalRoughness()).get(),
                (group.NormalMap      ? group.NormalMap      : FlatNormalMap()).get(),
                group.IndexCount / 3
            });
        }

        if (tl_cmds.empty()) return { 0, 0 };

        // Upload all model matrices to SSBO at binding 5.
        glNamedBufferSubData(m_ModelMatrixSSBO, 0,
            (GLsizeiptr)(tl_matrices.size() * sizeof(glm::mat4)), tl_matrices.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_ModelMatrixSSBO);

        // Upload all draw commands.
        glNamedBufferSubData(m_DrawCommandBuffer, 0,
            (GLsizeiptr)(tl_cmds.size() * sizeof(DrawCmd)), tl_cmds.data());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_DrawCommandBuffer);

        glDisable(GL_CULL_FACE);
        shader->Bind();
        glBindVertexArray(m_VAO);

        uint32_t drawCalls = 0;
        uint64_t triangles = 0;

        // Batch by unique texture triple, bind before each batch.
        shader->SetInt("u_Albedo",         0);
        shader->SetInt("u_MetalRoughness", 1);
        shader->SetInt("u_NormalMap",      2);

        const Texture2D* lastAlbedo = nullptr;
        const Texture2D* lastMR     = nullptr;
        const Texture2D* lastNormal = nullptr;

        uint32_t batchStart = 0;
        const uint32_t total = (uint32_t)tl_batches.size();
        while (batchStart < total)
        {
            uint32_t batchEnd = batchStart + 1;
            while (batchEnd < total &&
                   tl_batches[batchEnd].albedo == tl_batches[batchStart].albedo &&
                   tl_batches[batchEnd].mr     == tl_batches[batchStart].mr     &&
                   tl_batches[batchEnd].normal == tl_batches[batchStart].normal)
                batchEnd++;

            if (tl_batches[batchStart].albedo != lastAlbedo) { tl_batches[batchStart].albedo->Bind(0); lastAlbedo = tl_batches[batchStart].albedo; }
            if (tl_batches[batchStart].mr     != lastMR)     { tl_batches[batchStart].mr->Bind(1);     lastMR     = tl_batches[batchStart].mr;     }
            if (tl_batches[batchStart].normal != lastNormal)  { tl_batches[batchStart].normal->Bind(2); lastNormal = tl_batches[batchStart].normal; }

            shader->SetInt("u_BaseModelIndex", (int)batchStart);

            glMultiDrawElementsIndirect(
                GL_TRIANGLES,
                GL_UNSIGNED_INT,
                reinterpret_cast<const void*>((uintptr_t)(batchStart * sizeof(DrawCmd))),
                (GLsizei)(batchEnd - batchStart),
                (GLsizei)sizeof(DrawCmd));

            for (uint32_t i = batchStart; i < batchEnd; i++)
                triangles += tl_batches[i].tris;
            drawCalls++;
            batchStart = batchEnd;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);

        return { drawCalls, triangles };
    }
}
