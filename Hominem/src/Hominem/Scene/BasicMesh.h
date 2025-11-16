#pragma once

#include <future>
#include <algorithm>
#include <execution>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "Hominem/Renderer/Texture.h"
#include "Platform/OpenGL/OpenGLShader.h"

#define INVALID_MATERIAL 0xFFFFFFFF
#define MAX_NUM_BONES_PER_VERTEX 4

namespace Hominem {

	struct VertexBoneData
	{
		int BoneIDs[MAX_NUM_BONES_PER_VERTEX] = { 0 }; //all the bones that affect the current vertex
		float Weights[MAX_NUM_BONES_PER_VERTEX] = { 0 }; //for each bone the equivalent weight
		
		VertexBoneData() {};
	
		void AddBoneData(int boneID, float weight)
		{
			for (uint32_t i = 0; i < ARRAY_SIZE_IN_ELEMENTS(BoneIDs); i++) //searches for a free slot, and place bone id and weight there
			{
				if (Weights[i] == 0.0)
				{
					BoneIDs[i] = boneID;
					Weights[i] = weight;
					HMN_CORE_INFO("Adding data for Bone {} weight {} index {}\n", boneID, weight, i);
					return;
				}
			}
			//should never reach here 
			HMN_CORE_ASSERT(false, "You need to increase the maximum number of bones per vertex!");
		}
	};

	class BasicMesh
	{
	public:
		BasicMesh() = default;

		bool LoadMesh(const std::string& filename);

		void Render();

		void Render(const Ref<Shader>& overrideShader = nullptr);

		int GetNumBones() const { return m_BoneNameToIndexMap.size(); }

		void SetShader(const Ref<Shader>& shader) { m_Shader = shader; }

		Ref<Shader> GetShader() const { return m_Shader; }

		~BasicMesh();

	private:
		void Clear();

		void DrawSubmeshes();
		
		bool InitFromScene(const aiScene* pScene, const std::string& filename);
		
		void CountVerticesAndIndices(const aiScene* pScene, unsigned int& numVertices, unsigned int& numIndices);
		
		void ReserveSpace(unsigned int numVertices, unsigned int numIndices);
		
		void InitAllMeshes(const aiScene* pScene);
		
		void InitSingleMesh(uint32_t meshIndex, const aiMesh* paiMesh);
		
		bool InitMaterials(const aiScene* pScene, const std::string& filename);

		void PopulateBuffers();
		
		void ParseScene(const aiScene* pScene);
		
		void ParseMeshes(const aiScene* pScene);
		
		void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh);
		
		void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone);
		
		int GetBoneId(const aiBone* pBone);
		
		bool LoadFromCache(const std::string& filename);
		
		void SaveToCache(const std::string& filename);

	private:
		enum BUFFER_TYPE
		{
			INDEX_BUFFER = 0,
			POS_VB,
			TEXCOORD_VB,
			NORMAL_VB,
			BONE_VB,
			NUM_BUFFERS
		};

		struct BasicMeshEntry
		{
			BasicMeshEntry()
				: m_NumIndices(0),
				m_BaseVertex(0),
				m_BaseIndex(0),
				m_MaterialIndex(INVALID_MATERIAL)
			{
			}

			uint32_t m_NumIndices;
			uint32_t m_BaseVertex;
			uint32_t m_BaseIndex;
			uint32_t m_MaterialIndex;
		};
	private:
		uint32_t m_VAO = 0;
		uint32_t m_Buffers[NUM_BUFFERS] = { 0 };
		// an aiScene can contain textures / aiMesh as well
		std::vector<BasicMeshEntry> m_Meshes;
		std::vector<Ref<Texture2D>> m_Textures;

		// Temporary space for vertex data right before uploading to GPU
		// i.e. we can do a single call to glBufferData
		std::vector<glm::vec3> m_Positions;
		std::vector<glm::vec3> m_Normals;
		std::vector<glm::vec2> m_TexCoords;
		std::vector<uint32_t> m_Indices;

		std::vector<VertexBoneData> m_Bones;
		std::map<std::string, int> m_BoneNameToIndexMap;
		std::vector<uint32_t> m_MeshBaseVertex;

		Ref<Shader> m_Shader;
	};
}