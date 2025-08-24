#pragma once

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <glm/glm.hpp>
#include <glad/glad.h>

#include "Hominem/Renderer/Texture.h"

#define INVALID_MATERIAL 0xFFFFFFFF

namespace Hominem {

	class BasicMesh
	{
	public:
		BasicMesh() = default;

		bool LoadMesh(const std::string& filename);

		void Render();

		~BasicMesh();

	private:
		void Clear();

		bool InitFromScene(const aiScene* pScene, const std::string& filename);

		void CountVerticesAndIndices(const aiScene* pScene, unsigned int& numVertices, unsigned int& numIndices);

		void ReserveSpace(unsigned int numVertices, unsigned int numIndices);

		void InitAllMeshes(const aiScene* pScene);

		void InitSingleMesh(const aiMesh* paiMesh);

		bool InitMaterials(const aiScene* pScene, const std::string& filename);
	
		void PopulateBuffers();

	private:
		enum BUFFER_TYPE
		{
			INDEX_BUFFER = 0,
			POS_VB,
			TEXCOORD_VB,
			NORMAL_VB,
			WVP_MAT_VB,
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
	};

}

