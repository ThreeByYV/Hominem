#include "hmnpch.h"
#include "BasicMesh.h"


#define INVALID_UNIFORM_LOCATION -1
#define POSITION_LOCATION         0
#define TEX_COORD_LOCATION        1
#define NORMAL_LOCATION           2
#define BONE_ID_LOCATION          3
#define BONE_WEIGHT_LOCATION      4


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
		m_Bones.clear();
		m_BoneNameToIndexMap.clear();
		m_MeshBaseVertex.clear();

		HMN_CORE_INFO("BasicMesh resources cleared");
	}

	void BasicMesh::DrawSubmeshes()
	{
		if (m_VAO == 0) {
			HMN_CORE_ERROR("Cannot render: VAO is 0");
			return;
		}
		if (m_Meshes.empty()) {
			HMN_CORE_ERROR("Cannot render: No meshes loaded");
			return;
		}

		for (uint32_t i = 0; i < m_Meshes.size(); i++) {
			uint32_t materialIndex = m_Meshes[i].m_MaterialIndex;

			if (materialIndex >= m_Textures.size()) {
				HMN_CORE_ERROR("Material index {} out of range (max: {})",
					materialIndex, m_Textures.size() ? m_Textures.size() - 1 : 0);
				continue;
			}

			if (m_Textures[materialIndex])
				m_Textures[materialIndex]->Bind(GL_TEXTURE0);

			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				m_Meshes[i].m_NumIndices,
				GL_UNSIGNED_INT,
				(void*)(sizeof(uint32_t) * m_Meshes[i].m_BaseIndex),
				m_Meshes[i].m_BaseVertex
			);
		}
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
			Assimp::Importer Importer;

			auto fileLoadingTask = std::async(std::launch::async, [&]() {
				HMN_CORE_INFO("Reading file with Assimp on background thread...");
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

		m_Meshes.resize(pScene->mNumMeshes);
		m_Textures.resize(pScene->mNumMaterials);

		uint32_t numVertices = 0;
		uint32_t numIndices = 0;

		CountVerticesAndIndices(pScene, numVertices, numIndices);
		HMN_CORE_INFO("Total vertices: {}, Total indices: {}", numVertices, numIndices);

		ReserveSpace(numVertices, numIndices);

		InitAllMeshes(pScene);

		ParseScene(pScene);

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
			m_Meshes[i].m_NumIndices = pMesh->mNumFaces * 3;

			m_Meshes[i].m_BaseVertex = numVertices;
			m_Meshes[i].m_BaseIndex = numIndices;

			numVertices += pMesh->mNumVertices;
			numIndices += m_Meshes[i].m_NumIndices;

			HMN_CORE_INFO("Mesh {}: vertices={}, faces={}, indices={}, materialIndex={}, baseVertex={}, baseIndex={}",
				i, pMesh->mNumVertices, pMesh->mNumFaces, m_Meshes[i].m_NumIndices,
				m_Meshes[i].m_MaterialIndex, m_Meshes[i].m_BaseVertex, m_Meshes[i].m_BaseIndex);
		}
	}

	void BasicMesh::ReserveSpace(unsigned int numVertices, unsigned int numIndices)
	{
		m_Positions.reserve(numVertices);
		m_Normals.reserve(numVertices);
		m_TexCoords.reserve(numVertices);
		m_Indices.reserve(numIndices);
		m_Bones.reserve(numVertices);

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
			InitSingleMesh(i, paiMesh);
		}
	}

	void BasicMesh::InitSingleMesh(uint32_t meshIndex, const aiMesh* paiMesh)
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
			m_Bones.push_back(VertexBoneData());
		}

		// Populate the index buffer
		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& face = paiMesh->mFaces[i];
			if (face.mNumIndices != 3)
			{
				HMN_CORE_ERROR("Face {} has {} indices instead of 3! Triangulation may have failed.", i, face.mNumIndices);
				continue;
			}

			m_Indices.push_back(face.mIndices[0]);
			m_Indices.push_back(face.mIndices[1]);
			m_Indices.push_back(face.mIndices[2]);
		}

		HMN_CORE_INFO("Processed mesh: {} vertices, {} faces, {} indices total so far",
			paiMesh->mNumVertices, paiMesh->mNumFaces, m_Indices.size());
	}

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

		bool Ret = true;

		// Initialize the materials
		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = pScene->mMaterials[i];

			m_Textures[i] = nullptr;

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString Path;

				if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
				{
					std::string fullPath = dir + "/" + Path.data;
					HMN_CORE_INFO("Loading texture: {}", fullPath);

					try
					{
						m_Textures[i] = Texture2D::Create(fullPath);

						if (m_Textures[i])
						{
							HMN_CORE_INFO("Loaded texture successfully: {}", fullPath);
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

		//Bones buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[BONE_VB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_Bones[0]) * m_Bones.size(), &m_Bones[0], GL_STATIC_DRAW);
		
		glEnableVertexAttribArray(BONE_ID_LOCATION);
		glVertexAttribIPointer(BONE_ID_LOCATION, MAX_NUM_BONES_PER_VERTEX, GL_INT,
			sizeof(VertexBoneData), (const GLvoid*)0);

		glEnableVertexAttribArray(BONE_WEIGHT_LOCATION);
		glVertexAttribPointer(BONE_WEIGHT_LOCATION, MAX_NUM_BONES_PER_VERTEX, GL_FLOAT, GL_FALSE,
			sizeof(VertexBoneData),
			(const GLvoid*)(MAX_NUM_BONES_PER_VERTEX * sizeof(int)));

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

	void BasicMesh::ParseSingleBone(uint32_t meshIdx, const aiBone* pBone)
	{
		if (!pBone)
		{
			HMN_CORE_WARN("Bone is null for mesh {}", meshIdx);
			return;
		}

		HMN_CORE_INFO("Bone '{}': {} vertices affected by this bone", pBone->mName.C_Str(), pBone->mNumWeights);

		int boneID = GetBoneId(pBone);
		HMN_CORE_INFO("Bone ID: {}", boneID);

		for (uint32_t i = 0; i < pBone->mNumWeights; ++i)
		{
			const aiVertexWeight& vw = pBone->mWeights[i];

			uint32_t globalVertexId = m_MeshBaseVertex[meshIdx] + vw.mVertexId;

			HMN_CORE_INFO("{}: vertex id {}  weight {}", i, vw.mVertexId, vw.mWeight);

			HMN_CORE_ASSERT(
				globalVertexId < m_Bones.size(),
				"Out-of-range vertex-to-bone access in ParseSingleBone(): "
				"globalVertexId={}  m_Bones.size={}  meshIdx={}  localVertex={}  bone={}",
				globalVertexId, m_Bones.size(), meshIdx, vw.mVertexId, pBone->mName.C_Str()
			);

			m_Bones[globalVertexId].AddBoneData(boneID, vw.mWeight);
		}
	}

	void BasicMesh::ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh)
	{
		for (uint32_t i = 0; i < pMesh->mNumBones; ++i)
		{
			ParseSingleBone(meshIndex, pMesh->mBones[i]);
		}
	}

	void BasicMesh::ParseMeshes(const aiScene* pScene)
	{
		HMN_CORE_INFO("Parsing {} meshes for bone data", pScene->mNumMeshes);

		uint32_t totalBones = 0;
		m_MeshBaseVertex.resize(pScene->mNumMeshes);

		// Calculate base vertices for each mesh (needed for global vertex indexing)
		uint32_t vertexOffset = 0;
		for (uint32_t i = 0; i < pScene->mNumMeshes; i++) 
		{
			const aiMesh* pMesh = pScene->mMeshes[i];
			m_MeshBaseVertex[i] = vertexOffset;
			vertexOffset += pMesh->mNumVertices;

			HMN_CORE_INFO("Mesh {} '{}': {} bones, base vertex: {}",
				i, pMesh->mName.C_Str(), pMesh->mNumBones, m_MeshBaseVertex[i]);
			totalBones += pMesh->mNumBones;
		}

		// Now process bone weights for each mesh
		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			if (pMesh->HasBones()) 
			{
				ParseMeshBones(i, pMesh);
			}
		}

		HMN_CORE_INFO("Total bones processed: {}", totalBones);
		HMN_CORE_INFO("Final bone map size: {}", m_BoneNameToIndexMap.size());
	}

	void BasicMesh::ParseScene(const aiScene* pScene)
	{
		ParseMeshes(pScene);
	}

	int  BasicMesh::GetBoneId(const aiBone* pBone)
	{
		int boneID = 0;
		std::string boneName{ pBone->mName.C_Str() };

		if (m_BoneNameToIndexMap.find(boneName) == m_BoneNameToIndexMap.end())
		{
			//allocate an index for a new bone here
			boneID = m_BoneNameToIndexMap.size();
			m_BoneNameToIndexMap[boneName] = boneID;
			HMN_CORE_INFO("Added new bone '{}' with ID {}", boneName, boneID);
		}
		else
		{
			boneID = m_BoneNameToIndexMap[boneName];
		}

		return boneID;
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
			m_Bones.resize(posSize);

			// Read vertex/index data
			file.read((char*)m_Positions.data(), posSize * sizeof(glm::vec3));
			file.read((char*)m_Normals.data(), normalSize * sizeof(glm::vec3));
			file.read((char*)m_TexCoords.data(), texSize * sizeof(glm::vec2));
			file.read((char*)m_Indices.data(), indexSize * sizeof(uint32_t));

			// Read mesh metadata 
			file.read((char*)m_Meshes.data(), meshSize * sizeof(BasicMeshEntry));

			// Read bone name mapping
			size_t boneMapSize;
			file.read((char*)&boneMapSize, sizeof(boneMapSize));

			for (size_t i = 0; i < boneMapSize; i++) {
				size_t nameLength;
				file.read((char*)&nameLength, sizeof(nameLength));

				std::string boneName(nameLength, '\0');
				file.read(&boneName[0], nameLength);

				uint32_t boneIndex;
				file.read((char*)&boneIndex, sizeof(boneIndex));

				m_BoneNameToIndexMap[boneName] = boneIndex;
			}

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

				HMN_CORE_INFO("Cache loaded: {} vertices, {} meshes, {} texture slots, {} bone mappings",
					posSize, meshSize, m_Textures.size(), m_BoneNameToIndexMap.size());
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

		// Save bone name mapping
		size_t boneMapSize = m_BoneNameToIndexMap.size();
		file.write((char*)&boneMapSize, sizeof(boneMapSize));

		for (const auto& [boneName, boneIndex] : m_BoneNameToIndexMap) {
			size_t nameLength = boneName.length();
			file.write((char*)&nameLength, sizeof(nameLength));
			file.write(boneName.c_str(), nameLength);
			file.write((char*)&boneIndex, sizeof(boneIndex));
		}

		HMN_CORE_INFO("Cache saved: {} vertices, {} bone mappings", posSize, boneMapSize);
	}

	void BasicMesh::Render(const Ref<Shader>& overrideShader)
	{
		Ref<Shader> shader = overrideShader ? overrideShader : m_Shader;
		HMN_CORE_ASSERT(shader, "BasicMesh::Render called without any shader");

		// Bind the shader here to guarantee it’s active
		shader->Bind();

		// Bind VAO once
		glBindVertexArray(m_VAO);

		// If you need a sampler uniform:
		// shader->SetInt("u_Texture", 0);

		DrawSubmeshes();

		glBindVertexArray(0);
	}

	void BasicMesh::Render()
	{
		// Delegate to the main overload using the stored per-object shader
		Render(m_Shader);
	}

}
