#pragma once

#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StorageBuffer.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>

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
		uint32_t GetAnimationCount() const override;
		uint32_t GetVertexCount()    const override { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Positions.size()); }
		uint32_t GetIndexCount()     const override { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Indices.size()); }
		uint32_t GetSubmeshCount()   const override { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Submeshes.size()); }

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
		struct Submesh
		{
			uint32_t NumIndices    = 0;
			uint32_t BaseVertex    = 0;
			uint32_t BaseIndex     = 0;
			uint32_t MaterialIndex = 0xFFFFFFFF;
		};

		struct MeshGeometry
		{
			std::vector<glm::vec3> Positions;
			std::vector<glm::vec3> Normals;
			std::vector<glm::vec2> TexCoords;
			std::vector<uint32_t>  Indices;
			std::vector<Submesh>   Submeshes;
			std::vector<uint32_t>  SubmeshBaseVertices;

			void Clear()
			{
				Positions.clear(); Normals.clear(); TexCoords.clear();
				Indices.clear();   Submeshes.clear(); SubmeshBaseVertices.clear();
			}

			void Reserve(uint32_t verts, uint32_t indices)
			{
				Positions.reserve(verts); Normals.reserve(verts);
				TexCoords.reserve(verts); Indices.reserve(indices);
			}

			bool IsEmpty() const { return Positions.empty(); }
		};

		enum BufferType { INDEX_BUFFER = 0, POSITION_BUFFER, TEXCOORD_BUFFER, NORMAL_BUFFER, BONE_BUFFER, NUM_BUFFERS };

		// GPU resource management
		void CreateGPUBuffers();
		void ReleaseGPUResources();
		void UploadToGPU();
		void DrawSubmeshes();

		// Compute skinning SSBOs
		void CreateComputeSSBOs();

		// Assimp loading
		bool ParseScene(const aiScene* pScene, const std::string& filepath);
		void ExtractGeometry(const aiScene* pScene);
		void ExtractSubmesh(const aiMesh* pMesh, uint32_t submeshIndex);
		bool LoadMaterials(const aiScene* pScene, const std::string& filepath);
		Ref<Texture2D> LoadTexture(const aiMaterial* mat, aiTextureType type, const std::string& dir);

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

		MeshGeometry                  m_Geometry;
		std::vector<Ref<Texture2D>>   m_Materials;
		std::vector<VertexBoneData>   m_VertexBoneData;
		std::vector<glm::mat4>        m_BoneCache;

		Skeleton   m_Skeleton;
		Ref<Shader> m_Shader;

		Assimp::Importer              m_Importer;
		const aiScene*                m_pScene = nullptr;

		std::vector<Scope<Assimp::Importer>> m_AdditionalImporters;
		std::vector<const aiScene*>          m_AdditionalScenes;
	};

}
