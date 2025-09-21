#include "hmnpch.h"
#include "BasicMesh.h"

#define POSITION_LOCATION  0
#define TEX_COORD_LOCATION 1
#define NORMAL_LOCATION    2


#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate)
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
		if (m_VAO != 0)
		{
			glDeleteVertexArrays(1, &m_VAO);
			m_VAO = 0;
		}

		if (m_Buffers[0] != 0) 
		{
			glDeleteBuffers(ARRAY_SIZE_IN_ELEMENTS(m_Buffers), m_Buffers);
			memset(m_Buffers, 0, sizeof(m_Buffers));
		}

		m_Positions.clear();
		m_Normals.clear();
		m_TexCoords.clear();
		m_Indices.clear();
		m_Meshes.clear();
		m_Textures.clear();

		HMN_CORE_INFO("BasicMesh resources cleared");
	}

	bool BasicMesh::LoadMesh(const std::string& filename)
	{
		HMN_CORE_INFO("Starting to load mesh: {}", filename);

		Clear();

		bool success = false;

		if (LoadFromCache(filename))
		{
			success = true;
			HMN_CORE_INFO("Mesh was loaded from cache");
		} 
		else
		{
			bool Ret = false;
			const aiScene* pScene = nullptr;
			Assimp::Importer Importer; //class allows access the features in assimp


			auto fileLoadingTask = std::async(std::launch::async, [&]() {

				HMN_CORE_INFO("Reading file with Assimp on background thread...");

				//aiScene is the top level data structure from assimp's representation of a model
				pScene = Importer.ReadFile(filename.c_str(), ASSIMP_LOAD_FLAGS);

				return pScene != nullptr;
			});

			bool isFileLoaded = fileLoadingTask.get();

			if (isFileLoaded && pScene)
			{
				HMN_CORE_INFO("File loaded successfully. Meshes: {}, Materials: {}",
					pScene->mNumMeshes, pScene->mNumMaterials);
				Ret = InitFromScene(pScene, filename);
			}
			else
			{
				HMN_CORE_ERROR("Error parsing {}: {}", filename, Importer.GetErrorString());
			}

			glBindVertexArray(0);

			if (Ret)
			{
				success = true;
				HMN_CORE_INFO("Mesh loading completed successfully");
				SaveToCache(filename);
			}
			else
			{
				HMN_CORE_ERROR("Mesh loading failed");
			}
		}

		// ONLY create OpenGL objects if we have valid data
		if (success)
		{
			glGenVertexArrays(1, &m_VAO);
			glBindVertexArray(m_VAO);
			HMN_CORE_INFO("Generated VAO: {}", m_VAO);

			//Creating the buffers for each of the vertex attributes
			glGenBuffers(ARRAY_SIZE_IN_ELEMENTS(m_Buffers), m_Buffers);
			HMN_CORE_INFO("Generated {} buffers", ARRAY_SIZE_IN_ELEMENTS(m_Buffers));

			PopulateBuffers();
			glBindVertexArray(0);

			HMN_CORE_INFO("Mesh setup completed, VAO: {}", m_VAO);
		}

		return success;
	}

	bool BasicMesh::InitFromScene(const aiScene* pScene, const std::string& filename)
	{
		HMN_CORE_INFO("Initializing scene with {} meshes and {} materials",
			pScene->mNumMeshes, pScene->mNumMaterials);

		m_Meshes.resize(pScene->mNumMeshes);  //assimp makes aiMesh seperate struct for each component, like in modelers
		m_Textures.resize(pScene->mNumMaterials); //multiple textures are inside a "material" in assimp

		uint32_t numVertices = 0;
		uint32_t numIndices = 0;

		CountVerticesAndIndices(pScene, numVertices, numIndices);
		HMN_CORE_INFO("Total vertices: {}, Total indices: {}", numVertices, numIndices);

		ReserveSpace(numVertices, numIndices);
		InitAllMeshes(pScene);

		HMN_CORE_INFO("Vertices populated: {}, Normals: {}, TexCoords: {}, Indices: {}",
			m_Positions.size(), m_Normals.size(), m_TexCoords.size(), m_Indices.size());

		if (!InitMaterials(pScene, filename)) 
		{
			HMN_CORE_ERROR("Failed to initialize materials");
			return false;
		}

		return true;
	}

	void BasicMesh::CountVerticesAndIndices(const aiScene* pScene, unsigned int& numVertices, unsigned int& numIndices)
	{
		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			m_Meshes[i].m_MaterialIndex = pMesh->mMaterialIndex;
			m_Meshes[i].m_NumIndices = pMesh->mNumFaces * 3; //after triangulation all polygons in the mesh are now triangles

			m_Meshes[i].m_BaseVertex = numVertices; //this is the first vertex of the current mesh in the global buffers object
			m_Meshes[i].m_BaseIndex = numIndices;

			//addition needed to add the offset when there is aiMesh with 100, and another with 200 vertices 
			numVertices += pMesh->mNumVertices;
			numIndices += m_Meshes[i].m_NumIndices;

			HMN_CORE_INFO("Mesh {}: vertices={}, faces={}, indices={}, materialIndex={}, baseVertex={}, baseIndex={}",
				i, pMesh->mNumVertices, pMesh->mNumFaces, m_Meshes[i].m_NumIndices,
				m_Meshes[i].m_MaterialIndex, m_Meshes[i].m_BaseVertex, m_Meshes[i].m_BaseIndex);
		}
	}

	//allocates space in the four vectors before sending to the GPU
	void BasicMesh::ReserveSpace(unsigned int numVertices, unsigned int numIndices)
	{
		m_Positions.reserve(numVertices);
		m_Normals.reserve(numVertices);
		m_TexCoords.reserve(numVertices);
		m_Indices.reserve(numIndices);

		HMN_CORE_INFO("Reserved space for {} vertices and {} indices", numVertices, numIndices);
	}

	void BasicMesh::InitAllMeshes(const aiScene* pScene)
	{
		HMN_CORE_INFO("Initializing {} meshes", m_Meshes.size());

		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			const aiMesh* paiMesh = pScene->mMeshes[i];
			HMN_CORE_INFO("Processing mesh {} with {} vertices and {} faces",
				i, paiMesh->mNumVertices, paiMesh->mNumFaces);
			InitSingleMesh(paiMesh);
		}
	}

	//this is what populates the all vectors that will be sent to the GPU
	void BasicMesh::InitSingleMesh(const aiMesh* paiMesh)
	{
		const aiVector3D Zero3D(0.0f);

		if (!paiMesh->mVertices) 
		{
			HMN_CORE_ERROR("Mesh has no vertices!");
			return;
		}

		if (!paiMesh->mNormals) 
		{
			HMN_CORE_WARN("Mesh has no normals - using zero normals");
		}

		// Populate the respective vertex attributes vectors
		for (uint32_t i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D& pPos = paiMesh->mVertices[i];
			const aiVector3D& pNormal = paiMesh->mNormals ? paiMesh->mNormals[i] : Zero3D;
			const aiVector3D& pTexCoord = paiMesh->HasTextureCoords(0) ? paiMesh->mTextureCoords[0][i] : Zero3D;

			m_Positions.push_back(glm::vec3(pPos.x, pPos.y, pPos.z));
			m_Normals.push_back(glm::vec3(pNormal.x, pNormal.y, pNormal.z));
			m_TexCoords.push_back(glm::vec2(pTexCoord.x, pTexCoord.y));

		}


		// Populate the index buffer
		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& face = paiMesh->mFaces[i];
			if (face.mNumIndices != 3) 
			{
				HMN_CORE_ERROR("Face {} has {} indices instead of 3! Triangulation may have failed.",i, face.mNumIndices);
				continue;
			}

			m_Indices.push_back(face.mIndices[0]);
			m_Indices.push_back(face.mIndices[1]);
			m_Indices.push_back(face.mIndices[2]);
		}

		HMN_CORE_INFO("Processed mesh: {} vertices, {} faces, {} indices total so far",
			paiMesh->mNumVertices, paiMesh->mNumFaces, m_Indices.size());
	}

	// this allocates GLTextures for the images inside of the model
	bool BasicMesh::InitMaterials(const aiScene* pScene, const std::string& filename)
	{
		// Extract directory from filename
		std::string::size_type slashIndex = filename.find_last_of("/\\");
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
		}

		HMN_CORE_INFO("Model directory: {}", dir);

		bool Ret = true;

		//Initialize the materials
		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = pScene->mMaterials[i];
			m_Textures[i] = nullptr;

			HMN_CORE_INFO("Processing material {}: {} diffuse textures",
				i, pMaterial->GetTextureCount(aiTextureType_DIFFUSE));

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString path;

				if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
				{
					std::string texPath(path.data);
					HMN_CORE_INFO("Found texture path: {}", texPath);

					// the path of the texture must be relative to location of the model file
					if (texPath.substr(0, 2) == ".\\")
					{
						texPath = texPath.substr(2);
					}

					std::string fullPath = dir + "/" + texPath;
					HMN_CORE_INFO("Attempting to load texture: {}", fullPath);

					try {
						m_Textures[i] = Texture2D::Create(fullPath);
						
						if (m_Textures[i])
						{
							HMN_CORE_INFO("Successfully loaded texture: {}", fullPath);
						}
						else 
						{
							HMN_CORE_WARN("Failed to load texture: {}", fullPath);
						}
					}
					catch (const std::exception& e)
					{
						HMN_CORE_ERROR("Exception loading texture {}: {}", fullPath, e.what());
					}
				}
				else 
				{
					HMN_CORE_WARN("Failed to get texture path for material {}", i);
				}
			}
			else 
			{
				HMN_CORE_INFO("Material {} has no diffuse texture", i);
			}
		}

		return Ret;
	}

	void BasicMesh::PopulateBuffers()
	{
		HMN_CORE_INFO("Populating buffers...");

		if (m_Positions.empty()) 
		{
			HMN_CORE_ERROR("No positions to populate!");
			return;
		}

		// Position buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[POS_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Positions[0]) * m_Positions.size(), &m_Positions[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(POSITION_LOCATION);
		glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);
		HMN_CORE_INFO("Position buffer populated: {} vertices", m_Positions.size());

		// Texture coordinate buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[TEXCOORD_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_TexCoords[0]) * m_TexCoords.size(), &m_TexCoords[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(TEX_COORD_LOCATION);
		glVertexAttribPointer(TEX_COORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, 0);
		HMN_CORE_INFO("TexCoord buffer populated: {} coordinates", m_TexCoords.size());

		// Normal buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[NORMAL_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Normals[0]) * m_Normals.size(), &m_Normals[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(NORMAL_LOCATION);
		glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);
		HMN_CORE_INFO("Normal buffer populated: {} normals", m_Normals.size());

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Buffers[INDEX_BUFFER]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_Indices[0]) * m_Indices.size(), &m_Indices[0], GL_STATIC_DRAW);
		HMN_CORE_INFO("Index buffer populated: {} indices", m_Indices.size());

		GLenum error = glGetError();

		if (error != GL_NO_ERROR) 
		{
			HMN_CORE_ERROR("OpenGL error during buffer population: {}", error);
		}
	}

	bool BasicMesh::LoadFromCache(const std::string& filename)
	{
		std::string cacheFile = filename + ".cache";
		std::ifstream file(cacheFile, std::ios::binary);

		if (!file.is_open()) return false;

		HMN_CORE_INFO("Loading from cache: {}", cacheFile);

		try {
			// Read vector sizes
			size_t posSize, normalSize, texSize, indexSize, meshSize;

			file.read((char*)&posSize, sizeof(posSize));
			file.read((char*)&normalSize, sizeof(normalSize));
			file.read((char*)&texSize, sizeof(texSize));
			file.read((char*)&indexSize, sizeof(indexSize));
			file.read((char*)&meshSize, sizeof(meshSize));

			// Resize vectors
			m_Positions.resize(posSize);
			m_Normals.resize(normalSize);
			m_TexCoords.resize(texSize);
			m_Indices.resize(indexSize);
			m_Meshes.resize(meshSize);

			// Read vertex/index data
			file.read((char*)m_Positions.data(), posSize * sizeof(glm::vec3));
			file.read((char*)m_Normals.data(), normalSize * sizeof(glm::vec3));
			file.read((char*)m_TexCoords.data(), texSize * sizeof(glm::vec2));
			file.read((char*)m_Indices.data(), indexSize * sizeof(uint32_t));

			// Read mesh metadata 
			file.read((char*)m_Meshes.data(), meshSize * sizeof(BasicMeshEntry));

			if (file.good()) {
				uint32_t maxMaterialIndex = 0;

				m_Textures.clear();

				// Find the highest material index to size the texture array
				for (const auto& mesh : m_Meshes)
				{
					if (mesh.m_MaterialIndex != INVALID_MATERIAL)
					{
						maxMaterialIndex = std::max(maxMaterialIndex, mesh.m_MaterialIndex);
					}
				}

				m_Textures.resize(maxMaterialIndex + 1, nullptr);

				HMN_CORE_INFO("Cache loaded: {} vertices, {} meshes, {} texture slots",
					posSize, meshSize, m_Textures.size());
				return true;
			}
		}
		catch (...) {
			HMN_CORE_WARN("Cache file corrupted: {}", cacheFile);
		}

		return false;
	}

	void BasicMesh::SaveToCache(const std::string& filename)
	{
		std::string cacheFile = filename + ".cache";
		std::ofstream file(cacheFile, std::ios::binary);

		if (!file.is_open()) return;

		HMN_CORE_INFO("Saving to cache: {}", cacheFile);

		// Write vector sizes
		size_t posSize = m_Positions.size();
		size_t normalSize = m_Normals.size();
		size_t texSize = m_TexCoords.size();
		size_t indexSize = m_Indices.size();
		size_t meshSize = m_Meshes.size();

		file.write((char*)&posSize, sizeof(posSize));
		file.write((char*)&normalSize, sizeof(normalSize));
		file.write((char*)&texSize, sizeof(texSize));
		file.write((char*)&indexSize, sizeof(indexSize));
		file.write((char*)&meshSize, sizeof(meshSize));

		// Must write data in SAME ORDER as LoadFromCache reads
		file.write((char*)m_Positions.data(), posSize * sizeof(glm::vec3));
		file.write((char*)m_Normals.data(), normalSize * sizeof(glm::vec3));
		file.write((char*)m_TexCoords.data(), texSize * sizeof(glm::vec2));
		file.write((char*)m_Indices.data(), indexSize * sizeof(uint32_t));
		file.write((char*)m_Meshes.data(), meshSize * sizeof(BasicMeshEntry));

		HMN_CORE_INFO("Cache saved: {} vertices", posSize);
	}

	void BasicMesh::Render()
	{
		if (m_VAO == 0) 
		{
			HMN_CORE_ERROR("Cannot render: VAO is 0");
			return;
		}

		if (m_Meshes.empty()) 
		{
			HMN_CORE_ERROR("Cannot render: No meshes loaded");
			return;
		}

		glBindVertexArray(m_VAO);

		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			uint32_t materialIndex = m_Meshes[i].m_MaterialIndex;

			if (materialIndex >= m_Textures.size())
			{
				HMN_CORE_ERROR("Material index {} out of range (max: {})", materialIndex, m_Textures.size() - 1);
				materialIndex = 0;  // Use first texture or no texture
				continue;
			}

			//in your meshes check if you have a correspsonding element in the texture array, maybe material w/o diffuse texture
			if (m_Textures[materialIndex])
			{
				m_Textures[materialIndex]->Bind(GL_TEXTURE0);
			}

			// Validate mesh data
			if (m_Meshes[i].m_NumIndices == 0)
			{
				HMN_CORE_WARN("Mesh {} has 0 indices, skipping", i);
				continue;
			}

			HMN_CORE_TRACE("Rendering mesh {}: {} indices, base vertex {}, base index {}",
				i, m_Meshes[i].m_NumIndices, m_Meshes[i].m_BaseVertex, m_Meshes[i].m_BaseIndex);

			//this allows us to draw from sub-regions of the vertex and index buffers
			//i.e. needed for different offsets in the vertex buffer since we are using SoA instead of AoS
			glDrawElementsBaseVertex(GL_TRIANGLES,
				m_Meshes[i].m_NumIndices,
				GL_UNSIGNED_INT,
				(void*)(sizeof(uint32_t) * m_Meshes[i].m_BaseIndex), //offset to the first index of the current mesh in the IBO
				m_Meshes[i].m_BaseVertex); //this will be added to each index of the vertice, so it will match any offset in our buffer

			// Check for OpenGL errors after each draw call
			GLenum error = glGetError();

			if (error != GL_NO_ERROR) 
			{
				HMN_CORE_ERROR("OpenGL error during render of mesh {}: {}", i, error);
			}
		}

		glBindVertexArray(0);
	}
}