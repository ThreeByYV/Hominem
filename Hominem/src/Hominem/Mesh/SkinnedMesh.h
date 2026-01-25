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

	/**
	 * @brief Loads and renders 3D meshes with skeletal animation support.
	 *
	 * ## Architecture
	 *
	 * SkinnedMesh is a complete mesh system that composes:
	 * - **Geometry Data**: Vertex positions, normals, UVs, indices (private)
	 * - **Skeleton**: Bone hierarchy and animation transforms
	 * - **Materials**: Textures and material properties
	 * - **GPU Resources**: OpenGL VAO/VBOs for rendering
	 *
	 * The internal geometry representation (MeshGeometry) is intentionally
	 * hidden as an implementation detail. Users interact only through the
	 * SkinnedMesh public interface.
	 *
	 * ## Usage
	 *
	 * @code
	 * SkinnedMesh mesh;
	 * mesh.LoadFromFile("character.fbx");
	 * mesh.InitBoneUniforms(skinningShader);
	 *
	 * // Each frame:
	 * std::vector<glm::mat4> boneTransforms;
	 * mesh.GetBoneTransforms(elapsedTime, boneTransforms);
	 * for (size_t i = 0; i < boneTransforms.size(); i++)
	 *     mesh.UploadBoneTransform(i, boneTransforms[i]);
	 * mesh.Render();
	 * @endcode
	 */
	class SkinnedMesh
	{
	public:
		SkinnedMesh() = default;
		~SkinnedMesh();

		/**
		 * @brief Loads mesh and animation data from file.
		 *
		 * Supports any format Assimp can read (FBX, GLTF, OBJ, DAE, etc.).
		 * After loading, call InitBoneUniforms() with your shader before animating.
		 *
		 * @param filepath Path to mesh file
		 * @return true if load succeeded
		 */
		bool LoadFromFile(const std::string& filepath);

		/// @brief Renders using the mesh's assigned shader.
		void Render();

		/// @brief Renders using a specific shader (for shadow passes, etc.).
		void Render(const Ref<Shader>& overrideShader);

		// --- Shader Management ---
		void SetShader(const Ref<Shader>& shader) { m_Shader = shader; }
		Ref<Shader> GetShader() const { return m_Shader; }

		// --- Skeleton Access ---
		int GetBoneCount() const { return m_Skeleton.GetNumBones(); }
		bool HasSkeleton() const { return m_Skeleton.HasBones(); }
		Skeleton& GetSkeleton() { return m_Skeleton; }
		const Skeleton& GetSkeleton() const { return m_Skeleton; }

		/**
		 * @brief Computes bone transforms for the current animation frame.
		 * @param timeSeconds Time since animation start (seconds)
		 * @param[out] transforms Filled with one mat4 per bone
		 */
		void GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms);

		/**
		 * @brief Uploads a single bone transform to the shader.
		 * @param boneIndex Index of the bone (0 to GetBoneCount()-1)
		 * @param transform The bone's final transformation matrix
		 */
		void UploadBoneTransform(uint32_t boneIndex, const glm::mat4& transform);

		/**
		 * @brief Caches uniform locations for g_Bones[] array.
		 * Must be called once after loading, before rendering with animation.
		 */
		void InitBoneUniforms(const Ref<Shader>& shader);

		// --- Geometry Queries (Read-Only) ---
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Geometry.Positions.size()); }
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Geometry.Indices.size()); }
		uint32_t GetSubmeshCount() const { return static_cast<uint32_t>(m_Geometry.Submeshes.size()); }
		uint32_t GetVAO() const { return m_VAO; }

	private:
		/**
		 * @brief Describes one submesh within the loaded model.
		 *
		 * Internal use only - not exposed in public API.
		 */
		struct Submesh
		{
			uint32_t NumIndices = 0;
			uint32_t BaseVertex = 0;
			uint32_t BaseIndex = 0;
			uint32_t MaterialIndex = INVALID_MATERIAL;
		};

		/**
		 * @brief Internal geometry storage.
		 *
		 * This is kept private to enforce that geometry is only manipulated
		 * through SkinnedMesh's controlled interface (LoadFromFile, etc.).
		 * Users should never create or modify MeshGeometry directly.
		 */
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
		// --- GPU Resources ---
		uint32_t m_VAO = 0;
		uint32_t m_Buffers[NUM_BUFFERS] = { 0 };

		// --- Data ---
		MeshGeometry m_Geometry;  // Private implementation detail
		std::vector<Ref<Texture2D>> m_Materials;
		std::vector<VertexBoneData> m_VertexBoneData;

		// --- Skeleton ---
		Skeleton m_Skeleton;
		uint32_t m_BoneUniformLocations[MAX_BONES] = { 0 };

		// --- Rendering ---
		Ref<Shader> m_Shader;

		// --- Assimp (kept alive for animation data) ---
		Assimp::Importer m_Importer;
		const aiScene* m_pScene = nullptr;
	};

}
