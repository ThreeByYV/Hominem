#pragma once

#include <vector>
#include <string>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Shader.h"
#include "Skeleton.h"

#define INVALID_MATERIAL 0xFFFFFFFF

namespace Hominem {
	
	// Forward declare to avoid include
	struct VertexBoneData;
	class Skeleton;
	

	class SkinnedMesh
	{
	public:
		SkinnedMesh() = default;
		~SkinnedMesh();

		bool LoadFromFile(const std::string& filepath);
		bool LoadAdditionalAnimation(const std::string& filepath);

		void Render();

		void Render(const Ref<Shader>& overrideShader);

		// --- Shader Management ---
		void SetShader(const Ref<Shader>& shader) { m_Shader = shader; }
		Ref<Shader> GetShader() const { return m_Shader; }

		// --- Skeleton Access ---
		int GetBoneCount() const { return m_Skeleton.GetBoneCount(); }
		bool HasSkeleton() const { return m_Skeleton.HasBones(); }
		Skeleton& GetSkeleton() { return m_Skeleton; }
		const Skeleton& GetSkeleton() const { return m_Skeleton; }

		void GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms, bool disableRootMotion = false);
		void GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
			uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion = false);

		// Uploads the current bone matrices and dispatches the compute shader to skin
		// all vertices on the GPU. Must be called once per frame before Render().
		// The output SSBOs (skinned positions + normals) are then read directly by the
		// vertex shader using gl_VertexID — no per-bone uniform calls needed.
		void DispatchSkinning(const std::vector<glm::mat4>& bones);

		// --- Animation Access ---
		uint32_t GetAnimationCount() const
		{
			uint32_t count = m_pScene ? m_pScene->mNumAnimations : 0;
			// Each additional scene adds its animations
			for (const auto* scene : m_AdditionalScenes)
			{
				if (scene)
					count += scene->mNumAnimations;
			}
			return count;
		}

		// --- Geometry Queries (Read-Only) ---
		uint32_t GetVertexCount() const { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Positions.size()); }
		uint32_t GetIndexCount() const { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Indices.size()); }
		uint32_t GetSubmeshCount() const { return m_Geometry.IsEmpty() ? 0 : static_cast<uint32_t>(m_Geometry.Submeshes.size()); }
		uint32_t GetVAO() const { return m_VAO; }

	private:
		struct Submesh
		{
			uint32_t NumIndices = 0;
			uint32_t BaseVertex = 0;
			uint32_t BaseIndex = 0;
			uint32_t MaterialIndex = INVALID_MATERIAL;
		};

		struct MeshGeometry
		{
			std::vector<glm::vec3> Positions;
			std::vector<glm::vec3> Normals;
			std::vector<glm::vec2> TexCoords;
			std::vector<uint32_t> Indices;
			std::vector<Submesh> Submeshes;
			std::vector<uint32_t> SubmeshBaseVertices;

			void Clear()
			{
				Positions.clear();
				Normals.clear();
				TexCoords.clear();
				Indices.clear();
				Submeshes.clear();
				SubmeshBaseVertices.clear();
			}

			void Reserve(uint32_t numVertices, uint32_t numIndices)
			{
				Positions.reserve(numVertices);
				Normals.reserve(numVertices);
				TexCoords.reserve(numVertices);
				Indices.reserve(numIndices);
			}

			bool IsEmpty() const { return Positions.empty(); }
		};

		enum BufferType
		{
			INDEX_BUFFER = 0,
			POSITION_BUFFER,
			TEXCOORD_BUFFER,
			NORMAL_BUFFER,
			BONE_BUFFER,
			NUM_BUFFERS
		};

		void ReleaseGPUResources();
		void CreateGPUBuffers();
		void UploadToGPU();
		void DrawSubmeshes();

		// --- Loading Pipeline ---
		bool ParseScene(const aiScene* pScene, const std::string& filepath);
		void ExtractGeometry(const aiScene* pScene);
		void ExtractSubmesh(const aiMesh* pMesh, uint32_t submeshIndex);
		bool LoadMaterials(const aiScene* pScene, const std::string& filepath);

	private:
		void CreateComputeSSBOs();
		void ReleaseComputeSSBOs();

	private:
		// --- GPU Resources ---
		uint32_t m_VAO = 0;
		uint32_t m_Buffers[NUM_BUFFERS] = { 0 };

		// --- Compute Skinning ---
		uint32_t m_InPosSSBO      = 0;
		uint32_t m_InNormSSBO     = 0;
		uint32_t m_InBoneDataSSBO = 0;
		uint32_t m_BoneSSBO       = 0;
		uint32_t m_OutPosSSBO     = 0;
		uint32_t m_OutNormSSBO    = 0;

		Ref<ComputeShader> m_ComputeShader;

		// --- Data ---
		MeshGeometry m_Geometry;
		std::vector<Ref<Texture2D>> m_Materials;
		std::vector<VertexBoneData> m_VertexBoneData;

		// --- Skeleton ---
		Skeleton m_Skeleton;
		std::vector<glm::mat4> m_BoneCache; // reused every frame — avoids heap alloc per-frame

		// --- Rendering ---
		Ref<Shader> m_Shader;

		// --- Assimp ---
		Assimp::Importer m_Importer;
		const aiScene* m_pScene = nullptr;

		// Additional animation files (for multi-file animation support)
		std::vector<Scope<Assimp::Importer>> m_AdditionalImporters;
		std::vector<const aiScene*> m_AdditionalScenes;
	};

}
