#pragma once

#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StorageBuffer.h"
#include "Hominem/Assets/SkinnedMeshData.h"

namespace Hominem {

	class OpenGLSkinnedMesh : public SkinnedMesh
	{
	public:
		OpenGLSkinnedMesh()  = default;
		~OpenGLSkinnedMesh() override;

		[[nodiscard]] std::expected<void, std::string> LoadFromFile(const std::string& filepath) override;
		[[nodiscard]] std::expected<void, std::string> LoadAdditionalAnimation(const std::string& filepath) override;

		void Render(const Ref<Shader>& shader) override;
		void DispatchSkinning(std::span<const glm::mat4> bones) override;

		void GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms,
			bool disableRootMotion = false) override;
		void GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
			uint32_t startAnimIndex, uint32_t endAnimIndex,
			float blendFactor, bool disableRootMotion = false) override;

		void GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
			std::vector<glm::mat4>& transforms, bool disableRootMotion = false) override;

		void        SetShader(const Ref<Shader>& shader) override { m_Shader = shader; }
		Ref<Shader> GetShader() const override                    { return m_Shader; }

		bool     HasSkeleton()       const override { return m_Skeleton.HasBones(); }
		int      GetBoneCount()      const override { return m_Skeleton.GetBoneCount(); }
		uint32_t GetAnimationCount() const override { return m_Skeleton.GetAnimationCount(); }
		uint32_t GetVertexCount()    const override { return static_cast<uint32_t>(m_Positions.size()); }
		uint32_t GetIndexCount()     const override { return static_cast<uint32_t>(m_Indices.size()); }
		uint32_t GetSubmeshCount()   const override { return static_cast<uint32_t>(m_Submeshes.size()); }

		Skeleton&       GetSkeleton()       override { return m_Skeleton; }
		const Skeleton& GetSkeleton() const override { return m_Skeleton; }

		std::optional<glm::mat4> GetBoneWorldTransform(const std::string& name) const override
		{
			return m_Skeleton.GetBoneWorldTransform(name);
		}
		std::vector<std::string> GetBoneNames() const override
		{
			return m_Skeleton.GetBoneNames();
		}

	private:
		enum BufferType { INDEX_BUFFER = 0, POSITION_BUFFER, TEXCOORD_BUFFER, NORMAL_BUFFER, BONE_BUFFER, NUM_BUFFERS };

		void CreateGPUBuffers();
		void ReleaseGPUResources();
		void UploadToGPU();
		void DrawSubmeshes();
		void CreateComputeSSBOs();

		// GL handles
		uint32_t m_VAO                  = 0;
		uint32_t m_Buffers[NUM_BUFFERS] = { 0 };

		// Compute skinning SSBOs
		Ref<StorageBuffer> m_InPosSSBO;
		Ref<StorageBuffer> m_InNormSSBO;
		Ref<StorageBuffer> m_InBoneDataSSBO;
		Ref<StorageBuffer> m_BoneSSBO;
		Ref<StorageBuffer> m_OutPosSSBO;
		Ref<StorageBuffer> m_OutNormSSBO;
		Ref<ComputeShader> m_ComputeShader;

		// CPU geometry kept resident for SSBO creation and submesh draw.
		std::vector<glm::vec3>      m_Positions;
		std::vector<glm::vec3>      m_Normals;
		std::vector<glm::vec2>      m_TexCoords;
		std::vector<uint32_t>       m_Indices;
		std::vector<SkinnedSubmesh> m_Submeshes;
		std::vector<VertexBoneData> m_VertexBoneData;
		std::vector<Ref<Texture2D>> m_Materials;
		std::vector<glm::mat4>      m_BoneCache;

		Skeleton    m_Skeleton;
		Ref<Shader> m_Shader;
	};

}
