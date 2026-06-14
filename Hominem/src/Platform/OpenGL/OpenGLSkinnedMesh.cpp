#include "hmnpch.h"
#include "OpenGLSkinnedMesh.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Assets/SkinnedMeshImporter.h"

#include <glad/glad.h>

namespace Hominem {

	namespace VertexAttrib {
		constexpr int TexCoord = 1;
	}

	OpenGLSkinnedMesh::~OpenGLSkinnedMesh()
	{
		ReleaseGPUResources();
	}

	void OpenGLSkinnedMesh::ReleaseGPUResources()
	{
		if (m_VAO != 0)
		{
			glDeleteVertexArrays(1, &m_VAO);
			m_VAO = 0;
		}

		if (m_Buffers[0] != 0)
		{
			glDeleteBuffers(NUM_BUFFERS, m_Buffers);
			memset(m_Buffers, 0, sizeof(m_Buffers));
		}

		m_Positions.clear(); m_Normals.clear(); m_TexCoords.clear();
		m_Indices.clear();   m_Submeshes.clear();
		m_Materials.clear(); m_VertexBoneData.clear();
	}

	std::expected<void, std::string> OpenGLSkinnedMesh::LoadFromFile(const std::string& filepath)
	{
		auto result = ImportSkinnedMesh(filepath);
		if (!result) return std::unexpected(result.error());

		SkinnedMeshData data = std::move(*result);

		m_Positions      = std::move(data.Positions);
		m_Normals        = std::move(data.Normals);
		m_TexCoords      = std::move(data.TexCoords);
		m_Indices        = std::move(data.Indices);
		m_Submeshes      = std::move(data.Submeshes);
		m_VertexBoneData = std::move(data.VertexBoneData);
		m_Materials      = std::move(data.MaterialAlbedo);

		m_Material.NormalMap         = data.NormalMap;
		m_Material.MetalRoughnessMap = data.MetalRoughnessMap;

		m_Skeleton.SetData(std::move(data.Nodes), std::move(data.BoneNameToIndex),
			std::move(data.BoneOffsets), data.GlobalInverse);
		if (data.MainAnimation)
			m_Skeleton.SetMainAnimation(std::move(*data.MainAnimation));

		RenderThread::QueueUpload([this] {
			CreateGPUBuffers();
			UploadToGPU();
		});
		return {};
	}

	std::expected<void, std::string> OpenGLSkinnedMesh::LoadAdditionalAnimation(const std::string& filepath)
	{
		auto result = ImportAnimation(filepath);
		if (!result) return std::unexpected(result.error());

		m_Skeleton.AddAnimation(std::move(*result));
		HMN_CORE_INFO("SkinnedMesh: anim '{}' loaded - {} total anims", filepath, GetAnimationCount());
		return {};
	}

	void OpenGLSkinnedMesh::CreateGPUBuffers()
	{
		glCreateVertexArrays(1, &m_VAO);
		glCreateBuffers(NUM_BUFFERS, m_Buffers);
	}

	void OpenGLSkinnedMesh::UploadToGPU()
	{
		if (m_Positions.empty())
		{
			HMN_CORE_ERROR("SkinnedMesh: No geometry to upload");
			return;
		}

		glNamedBufferData(m_Buffers[TEXCOORD_BUFFER],
			sizeof(glm::vec2) * m_TexCoords.size(),
			m_TexCoords.data(), GL_STATIC_DRAW);

		glNamedBufferData(m_Buffers[INDEX_BUFFER],
			sizeof(uint32_t) * m_Indices.size(),
			m_Indices.data(), GL_STATIC_DRAW);

		constexpr GLuint kTexCoordBinding = 0;
		glEnableVertexArrayAttrib(m_VAO, VertexAttrib::TexCoord);
		glVertexArrayAttribFormat(m_VAO, VertexAttrib::TexCoord, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(m_VAO, VertexAttrib::TexCoord, kTexCoordBinding);
		glVertexArrayVertexBuffer(m_VAO, kTexCoordBinding, m_Buffers[TEXCOORD_BUFFER], 0, sizeof(glm::vec2));
		glVertexArrayElementBuffer(m_VAO, m_Buffers[INDEX_BUFFER]);

		CreateComputeSSBOs();
	}

	void OpenGLSkinnedMesh::Render(const Ref<Shader>& shader, CommandList& cmd)
	{
		if (!m_VAO) return;
		Ref<Shader> active = shader ? shader : m_Shader;
		HMN_CORE_ASSERT(active, "SkinnedMesh::Render called without a shader");

		cmd.BindShader(active);
		cmd.BindVAORaw(m_VAO);
		DrawSubmeshes(cmd);
		cmd.BindVAORaw(0);
	}

	void OpenGLSkinnedMesh::DrawSubmeshes(CommandList& cmd)
	{
		if (m_VAO == 0 || m_Submeshes.empty())
		{
			HMN_CORE_ERROR("SkinnedMesh: Cannot draw - VAO or submeshes missing");
			return;
		}

		for (const auto& submesh : m_Submeshes)
		{
			if (submesh.MaterialIndex < m_Materials.size() && m_Materials[submesh.MaterialIndex])
				cmd.BindTexture(0, m_Materials[submesh.MaterialIndex]->GetRendererID());
			else
				cmd.BindTexture(0, 0);

			cmd.DrawElementsBaseVertex(
				submesh.NumIndices,
				(uint32_t)(sizeof(uint32_t) * submesh.BaseIndex),
				submesh.BaseVertex);
		}
	}

	void OpenGLSkinnedMesh::CreateComputeSSBOs()
	{
		// Reset existing SSBOs before recreating (safe on reload - Ref<> drops the old GL object).
		m_InPosSSBO = m_InNormSSBO = m_InBoneDataSSBO = m_BoneSSBO = m_OutPosSSBO = m_OutNormSSBO = nullptr;

		uint32_t vertCount = static_cast<uint32_t>(m_Positions.size());
		uint32_t boneCount = static_cast<uint32_t>(m_Skeleton.GetNumBones());

		// Pack as vec4 - GPU prefers 16-byte aligned reads.
		std::vector<glm::vec4> pos4(vertCount), norm4(vertCount);
		for (uint32_t i = 0; i < vertCount; i++)
		{
			pos4[i]  = glm::vec4(m_Positions[i], 1.f);
			norm4[i] = glm::vec4(m_Normals[i],   0.f);
		}

		const uint32_t vertBytes     = vertCount * (uint32_t)sizeof(glm::vec4);
		const uint32_t boneDataBytes = vertCount * (uint32_t)sizeof(VertexBoneData);

		// Rest-pose data - uploaded once at load, never changes.
		m_InPosSSBO = StorageBuffer::Create(vertBytes);
		m_InPosSSBO->SetData(pos4.data(), vertBytes);

		m_InNormSSBO = StorageBuffer::Create(vertBytes);
		m_InNormSSBO->SetData(norm4.data(), vertBytes);

		m_InBoneDataSSBO = StorageBuffer::Create(boneDataBytes);
		m_InBoneDataSSBO->SetData(m_VertexBoneData.data(), boneDataBytes);

		// At least one identity bone so the compute shader never reads OOB on a no-skeleton mesh.
		uint32_t allocBones      = std::max(boneCount, 1u);
		uint32_t boneMatrixBytes = allocBones * (uint32_t)sizeof(glm::mat4);
		std::vector<glm::mat4> identityMats(allocBones, glm::mat4(1.0f));
		m_BoneSSBO = StorageBuffer::Create(boneMatrixBytes);
		m_BoneSSBO->SetData(identityMats.data(), boneMatrixBytes);

		// Initialised with rest-pose so no-animation renders correctly without a dispatch.
		m_OutPosSSBO = StorageBuffer::Create(vertBytes);
		m_OutPosSSBO->SetData(pos4.data(), vertBytes);

		m_OutNormSSBO = StorageBuffer::Create(vertBytes);
		m_OutNormSSBO->SetData(norm4.data(), vertBytes);

		m_ComputeShader = ComputeShader::Create("engine://Shaders/skinning.comp");
		m_BoneCache.reserve(boneCount);
	}

	void OpenGLSkinnedMesh::DispatchSkinning(std::span<const glm::mat4> bones, CommandList& cmd)
	{
		if (!m_VAO || !m_ComputeShader || !m_InBoneDataSSBO) return;

		// Always bind outputs and bone data so vertex + bone-weight shaders can read them.
		cmd.BindStorageBufferBase(m_InBoneDataSSBO, 3);
		cmd.BindStorageBufferBase(m_OutPosSSBO,     4);
		cmd.BindStorageBufferBase(m_OutNormSSBO,    5);

		if (bones.empty()) return;

		cmd.SetStorageBufferData(m_BoneSSBO, bones.data(), (uint32_t)(bones.size() * sizeof(glm::mat4)));

		cmd.BindStorageBufferBase(m_BoneSSBO,   0);
		cmd.BindStorageBufferBase(m_InPosSSBO,  1);
		cmd.BindStorageBufferBase(m_InNormSSBO, 2);

		cmd.ComputeSetUint(m_ComputeShader, "u_VertexCount", static_cast<uint32_t>(m_Positions.size()));

		uint32_t groups = (static_cast<uint32_t>(m_Positions.size()) + 63) / 64;
		cmd.DispatchCompute(m_ComputeShader, groups); // Dispatch() issues GL_SHADER_STORAGE_BARRIER_BIT.
	}

	void OpenGLSkinnedMesh::GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransforms(timeSeconds, transforms, disableRootMotion);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
		uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlended(timeSeconds, transforms, startAnimIndex, endAnimIndex, blendFactor, disableRootMotion);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
		std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlendedN(samples, transforms, disableRootMotion);
	}

}
