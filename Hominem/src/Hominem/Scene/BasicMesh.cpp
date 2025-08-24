#include "hmnpch.h"
#include "BasicMesh.h"

#define POSITION_LOCATION  0
#define TEX_COORD_LOCATION 1
#define NORMAL_LOCATION    2


#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices)
// modelers allow polygons, but we only can use triangles, tell assimp to split polygons into triangles
// lighting
// flips the texture coords on the V or Y axis, again modelers can control this
// adjust the index buffer accordingly and saves memory


namespace Hominem {

	BasicMesh::~BasicMesh()
	{
		Clear();
	}

	void BasicMesh::Clear()
	{
		//todo impl this so you can reuse the same class for multiple meshes

	}

	

	bool BasicMesh::LoadMesh(const std::string& filename)
	{
		Clear();

		glGenVertexArrays(1, &m_VAO);
		glBindVertexArray(m_VAO);

		//Creating the buffers for each of the vertex attributes
		glGenBuffers(ARRAY_SIZE_IN_ELEMENTS(m_Buffers), m_Buffers);

		bool Ret = false;
		Assimp::Importer Importer; //class allows access the features in assimp

		//aiScene is the top level data structure from assimp's representation of a model
		const aiScene* pScene = Importer.ReadFile(filename.c_str(), ASSIMP_LOAD_FLAGS);
		
		if (pScene)
		{
			Ret = InitFromScene(pScene, filename);
		}
		else
		{
			HMN_CORE_ERROR("Error parsing {0}: {1} \n", filename, Importer.GetErrorString());
		}

		glBindVertexArray(0);

		return Ret;
	}

	bool BasicMesh::InitFromScene(const aiScene* pScene, const std::string& filename)
	{
		m_Meshes.resize(pScene->mNumMeshes);  //assimp makes aiMesh seperate struct for each component, like in modelers
		m_Textures.resize(pScene->mNumMaterials); //multiple textures are inside a "material" in assimp

		uint32_t numVertices = 0;
		uint32_t numIndices = 0;

		CountVerticesAndIndices(pScene, numVertices, numIndices);
		ReserveSpace(numVertices, numIndices);
		InitAllMeshes(pScene);

		if (!InitMaterials(pScene, filename))
			return false;

		PopulateBuffers();

		return true;
	}

	void BasicMesh::CountVerticesAndIndices(const aiScene* pScene, unsigned int& numVertices, unsigned int& numIndices)
	{
		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			m_Meshes[i].m_MaterialIndex = pScene->mMeshes[i]->mMaterialIndex;
			m_Meshes[i].m_NumIndices = pScene->mMeshes[i]->mNumFaces * 3; //after triangulation all polygons in the mesh are now triangles
			
			m_Meshes[i].m_BaseVertex = numVertices; //this is the first vertex of the current mesh in the global buffers object
			m_Meshes[i].m_BaseIndex = numIndices;

			//addition needed to add the offset when there is aiMesh with 100, and another with 200 vertices 
			numVertices += pScene->mMeshes[i]->mNumVertices;
			numIndices += m_Meshes[i].m_NumIndices;
		}
	}

	//allocates space in the four vectors before sending to the GPU
	void BasicMesh::ReserveSpace(unsigned int numVertices, unsigned int numIndices)
	{
		m_Positions.reserve(numVertices);
		m_Normals.reserve(numVertices);
		m_TexCoords.reserve(numVertices);
		m_Indices.reserve(numVertices);
	}

	void BasicMesh::InitAllMeshes(const aiScene* pScene)
	{
		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			const aiMesh* paiMesh = pScene->mMeshes[i];
			InitSingleMesh(paiMesh);
		}
	}

	//this is what populates the all vectors that will be sent to the GPU
	void BasicMesh::InitSingleMesh(const aiMesh* paiMesh)
	{
		const aiVector3D Zero3D(0.0f);

		// Populate the respective vertex attributes vectors
		for (uint32_t i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D& pPos = paiMesh->mVertices[i];
			const aiVector3D& pNormal = paiMesh->mNormals[i];
			const aiVector3D& pTexCoord = paiMesh->HasTextureCoords(0) ? paiMesh->mTextureCoords[0][i] : Zero3D;

			m_Positions.push_back(glm::vec3(pPos.x, pPos.y, pPos.z));
			m_Normals.push_back(glm::vec3(pNormal.x, pNormal.y, pNormal.z));
			m_TexCoords.push_back(glm::vec2(pTexCoord.x, pTexCoord.y));

		}


		// Populate the index buffer
		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& face = paiMesh->mFaces[i];
			HMN_CORE_ASSERT(face.mNumIndices == 3, "Your one or more of your model faces does not have exactly 3 indices!");
			
			m_Indices.push_back(face.mIndices[0]);
			m_Indices.push_back(face.mIndices[0]);
			m_Indices.push_back(face.mIndices[0]);
		}
	}

	// this allocates GLTextures for the images inside of the model
	bool BasicMesh::InitMaterials(const aiScene* pScene, const std::string& filename)
	{
	/*	std::string::size_type slashIndex = filename.find_last_of("/");
		std::string dir;

		if (slashIndex == std::string::npos)
		{
			dir = ".";
		}
		else if (slashIndex == 0)
		{
			dir = "/";
		}
		else
		{
			dir = filename.substr(0, slashIndex);
		}*/

		bool Ret = true;

		//Initialize the materials
		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = pScene->mMaterials[i];

			m_Textures[i] = nullptr;

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE))
			{
				aiString path;

				if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
				{
					std::string p(path.data);

					// the path of the texture must be relative to location of the model file
					/*if (p.substr(0, 2) == ".\\")
					{
						p = p.substr(2, p.size() - 2);
					}

					std::string fullPath = dir + "/" + p;*/

					m_Textures[i] = Texture2D::Create(filename);
				}
			}
		}

		return true;
	}

	void BasicMesh::PopulateBuffers()
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[POS_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Positions[0]) * m_Positions.size(), &m_Positions[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(POSITION_LOCATION);
		glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0); //for last two params we no longer need sizeof each vertex or offset, since SoA and data is packed together in one array 

		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[TEXCOORD_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_TexCoords[0]) * m_TexCoords.size(), &m_TexCoords[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(TEX_COORD_LOCATION);
		glVertexAttribPointer(TEX_COORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[NORMAL_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Normals[0]) * m_Normals.size(), &m_Normals[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(NORMAL_LOCATION);
		glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Buffers[INDEX_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Indices[0]) * m_Indices.size(), &m_Indices[0], GL_STATIC_DRAW);
	}

	void BasicMesh::Render()
	{
		glBindVertexArray(m_VAO);

		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			uint32_t materialIndex = m_Meshes[i].m_MaterialIndex;

			HMN_CORE_ASSERT(materialIndex < m_Textures.size(), "You have more textures than indices in your model!");
		
			//in your meshes check if you have a correspsonding element in the texture array, maybe material w/o diffuse texture
			if (m_Textures[materialIndex])
			{
				m_Textures[materialIndex]->Bind(GL_TEXTURE0);
			}

			//this allows us to draw from sub-regions of the vertex and index buffers
			//i.e. needed for different offsets in the vertex buffer since we are using SoA instead of AoS
			glDrawElementsBaseVertex(GL_TRIANGLES,
				m_Meshes[i].m_NumIndices,
				GL_UNSIGNED_INT,
				(void*)(sizeof(uint32_t) * m_Meshes[i].m_BaseIndex), //offset to the first index of the current mesh in the IBO
				m_Meshes[i].m_BaseVertex); //this will be added to each index of the vertice, so it will match any offset in our buffer
		}

		glBindVertexArray(0);
	}

}

