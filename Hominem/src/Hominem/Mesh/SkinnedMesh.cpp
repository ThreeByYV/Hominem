#include "hmnpch.h"
#include "SkinnedMesh.h"
#include <glad/glad.h>

#include "glm/gtc/type_ptr.hpp"
#include "Hominem/Utils/Renderer.h"
#include "Hominem/Core/Core.h"

namespace Hominem {

	// Vertex attribute locations (must match shader layout qualifiers)
	namespace VertexAttrib {

		constexpr int Position = 0;
		constexpr int TexCoord = 1;
		constexpr int Normal = 2;
		constexpr int BoneIDs = 3;
		constexpr int BoneWeights = 4;
	}

	// Assimp post-processing flags
	constexpr unsigned int ASSIMP_LOAD_FLAGS =
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices;

	SkinnedMesh::~SkinnedMesh()
	{
		ReleaseGPUResources();
	}

	void SkinnedMesh::ReleaseGPUResources()
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

		m_Geometry.Clear();
		m_Materials.clear();
		m_VertexBoneData.clear();

		HMN_CORE_INFO("SkinnedMesh: GPU resources released");
	}

	bool SkinnedMesh::LoadFromFile(const std::string& filepath)
	{
		HMN_CORE_INFO("SkinnedMesh::LoadFromFile - Starting load of '{}'", filepath);

		ReleaseGPUResources();
		CreateGPUBuffers();

		// Clear any previous additional animations
		m_AdditionalImporters.clear();
		m_AdditionalScenes.clear();

		HMN_CORE_INFO("SkinnedMesh::LoadFromFile - Calling Assimp ReadFile...");
		m_pScene = m_Importer.ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!m_pScene)
		{
			HMN_CORE_ERROR("SkinnedMesh: Failed to load '{}': {}", filepath, m_Importer.GetErrorString());
			glBindVertexArray(0);
			return false;
		}

		HMN_CORE_INFO("SkinnedMesh: Loaded '{}' - {} animations, {} meshes",
			filepath, m_pScene->mNumAnimations, m_pScene->mNumMeshes);

		if (m_pScene->mNumMeshes == 0)
		{
			HMN_CORE_ERROR("SkinnedMesh: File loaded but contains 0 meshes!");
			glBindVertexArray(0);
			return false;
		}

		bool success = ParseScene(m_pScene, filepath);

		if (!success)
		{
			HMN_CORE_ERROR("SkinnedMesh: ParseScene failed!");
			glBindVertexArray(0);
			return false;
		}

		HMN_CORE_INFO("SkinnedMesh: ParseScene succeeded, uploading to GPU...");

		UploadToGPU();

		HMN_CORE_INFO("SkinnedMesh: LoadFromFile complete - Submeshes: {}", m_Geometry.Submeshes.size());

		glBindVertexArray(0);
		return success;
	}

	bool SkinnedMesh::LoadAdditionalAnimation(const std::string& filepath)
	{
		HMN_CORE_INFO("SkinnedMesh::LoadAdditionalAnimation - Loading animation from '{}'", filepath);

		// Create a new importer for this animation file
		auto importer = CreateScope<Assimp::Importer>();
		const aiScene* pScene = importer->ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!pScene)
		{
			HMN_CORE_ERROR("SkinnedMesh: Failed to load animation from '{0}': {1}", filepath, std::string(importer->GetErrorString()));
			return false;
		}

		if (pScene->mNumAnimations == 0)
		{
			HMN_CORE_WARN("SkinnedMesh: File '{}' contains no animations!", filepath);
			return false;
		}

		HMN_CORE_INFO("SkinnedMesh: Loaded additional animation from '{}' - {} animations",
			filepath, pScene->mNumAnimations);

		// Store the importer and scene
		m_AdditionalScenes.push_back(pScene);
		m_AdditionalImporters.push_back(std::move(importer));

		// Add the scene to the skeleton
		m_Skeleton.AddAdditionalScene(pScene);

		HMN_CORE_INFO("SkinnedMesh: Total animations now: {}", GetAnimationCount());

		return true;
	}

	void SkinnedMesh::CreateGPUBuffers()
	{
		glGenVertexArrays(1, &m_VAO);
		glBindVertexArray(m_VAO);
		glGenBuffers(NUM_BUFFERS, m_Buffers);
	}

	bool SkinnedMesh::ParseScene(const aiScene* pScene, const std::string& filepath)
	{
		HMN_CORE_INFO("SkinnedMesh: Parsing {} submeshes, {} materials",
			pScene->mNumMeshes, pScene->mNumMaterials);

		// Count total vertices and indices
		uint32_t totalVertices = 0;
		uint32_t totalIndices = 0;

		m_Geometry.Submeshes.resize(pScene->mNumMeshes);
		m_Geometry.SubmeshBaseVertices.resize(pScene->mNumMeshes);

		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			m_Geometry.Submeshes[i].MaterialIndex = pMesh->mMaterialIndex;
			m_Geometry.Submeshes[i].NumIndices = pMesh->mNumFaces * 3;
			m_Geometry.Submeshes[i].BaseVertex = totalVertices;
			m_Geometry.Submeshes[i].BaseIndex = totalIndices;
			m_Geometry.SubmeshBaseVertices[i] = totalVertices;

			totalVertices += pMesh->mNumVertices;
			totalIndices += m_Geometry.Submeshes[i].NumIndices;

			HMN_CORE_INFO("  Submesh {}: {} verts, {} indices, material {}",
				i, pMesh->mNumVertices, m_Geometry.Submeshes[i].NumIndices, pMesh->mMaterialIndex);
		}

		HMN_CORE_INFO("SkinnedMesh: Total {} vertices, {} indices", totalVertices, totalIndices);

		// Allocate space
		m_Geometry.Reserve(totalVertices, totalIndices);
		m_VertexBoneData.resize(totalVertices);

		// Extract geometry from each submesh
		ExtractGeometry(pScene);

		// Parse skeleton and bone weights
		m_Skeleton.ParseFromScene(pScene, m_Geometry.SubmeshBaseVertices, m_VertexBoneData);

		HMN_CORE_INFO("SkinnedMesh: Extracted {} bones", m_Skeleton.GetNumBones());

		// Load materials/textures
		m_Materials.resize(pScene->mNumMaterials);
		if (!LoadMaterials(pScene, filepath))
		{
			HMN_CORE_WARN("SkinnedMesh: Some materials failed to load");
		}

		return true;
	}

	void SkinnedMesh::ExtractGeometry(const aiScene* pScene)
	{
		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			ExtractSubmesh(pScene->mMeshes[i], i);
		}
	}

	void SkinnedMesh::ExtractSubmesh(const aiMesh* pMesh, uint32_t submeshIndex)
	{
		const aiVector3D Zero3D(0.0f);

		if (!pMesh->mVertices)
		{
			HMN_CORE_ERROR("SkinnedMesh: Submesh {} has no vertices!", submeshIndex);
			return;
		}

		if (!pMesh->mNormals)
			HMN_CORE_WARN("SkinnedMesh: Submesh {} has no normals", submeshIndex);

		// Extract vertex attributes
		for (uint32_t i = 0; i < pMesh->mNumVertices; i++)
		{
			const aiVector3D& pos = pMesh->mVertices[i];
			const aiVector3D& normal = pMesh->mNormals ? pMesh->mNormals[i] : Zero3D;
			const aiVector3D& uv = pMesh->HasTextureCoords(0) ? pMesh->mTextureCoords[0][i] : Zero3D;

			m_Geometry.Positions.emplace_back(pos.x, pos.y, pos.z);
			m_Geometry.Normals.emplace_back(normal.x, normal.y, normal.z);
			m_Geometry.TexCoords.emplace_back(uv.x, uv.y);
		}

		// Extract triangle indices
		for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
		{
			const aiFace& face = pMesh->mFaces[i];
			if (face.mNumIndices != 3)
			{
				HMN_CORE_ERROR("SkinnedMesh: Face {} has {} indices (expected 3)", i, face.mNumIndices);
				continue;
			}

			m_Geometry.Indices.push_back(face.mIndices[0]);
			m_Geometry.Indices.push_back(face.mIndices[1]);
			m_Geometry.Indices.push_back(face.mIndices[2]);
		}
	}

	bool SkinnedMesh::LoadMaterials(const aiScene* pScene, const std::string& filepath)
	{
		// Extract directory from filepath
		std::string directory;
		size_t lastSlash = filepath.find_last_of("/\\");

		if (lastSlash == std::string::npos)
			directory = ".";
		else if (lastSlash == 0)
			directory = "/";
		else
			directory = filepath.substr(0, lastSlash);

		bool allLoaded = true;

		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = pScene->mMaterials[i];
			m_Materials[i] = nullptr;

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) == 0)
			{
				HMN_CORE_INFO("SkinnedMesh: Material {} has no diffuse texture", i);
				continue;
			}

			aiString texturePath;
			if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS)
			{
				HMN_CORE_WARN("SkinnedMesh: Failed to get texture path for material {}", i);
				allLoaded = false;
				continue;
			}

			std::string fullPath = directory + "/" + texturePath.data;
			HMN_CORE_INFO("SkinnedMesh: Loading texture '{}'", fullPath);

			try
			{
				m_Materials[i] = Texture2D::Create(fullPath);
				if (m_Materials[i])
					HMN_CORE_INFO("SkinnedMesh: Texture loaded successfully");
				else
				{
					HMN_CORE_WARN("SkinnedMesh: Texture creation returned null");
					allLoaded = false;
				}
			}
			catch (const std::exception& e)
			{
				HMN_CORE_ERROR("SkinnedMesh: Exception loading texture: {}", e.what());
				allLoaded = false;
			}
		}

		return allLoaded;
	}

	void SkinnedMesh::UploadToGPU()
	{
		HMN_CORE_INFO("SkinnedMesh: Uploading to GPU...");

		if (m_Geometry.IsEmpty())
		{
			HMN_CORE_ERROR("SkinnedMesh: No geometry to upload!");
			return;
		}

		// Make sure VAO is bound before setting up vertex attributes
		glBindVertexArray(m_VAO);
		HMN_CORE_INFO("SkinnedMesh: Binding VAO {} for upload", m_VAO);

		// Position buffer (vec3)
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[POSITION_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(glm::vec3) * m_Geometry.Positions.size(),
			m_Geometry.Positions.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(VertexAttrib::Position);
		glVertexAttribPointer(VertexAttrib::Position, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Texture coordinate buffer (vec2)
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[TEXCOORD_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(glm::vec2) * m_Geometry.TexCoords.size(),
			m_Geometry.TexCoords.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(VertexAttrib::TexCoord);
		glVertexAttribPointer(VertexAttrib::TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Normal buffer (vec3)
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[NORMAL_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(glm::vec3) * m_Geometry.Normals.size(),
			m_Geometry.Normals.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(VertexAttrib::Normal);
		glVertexAttribPointer(VertexAttrib::Normal, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Bone data buffer (interleaved int4 IDs + float4 weights)
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[BONE_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(VertexBoneData) * m_VertexBoneData.size(),
			m_VertexBoneData.data(), GL_STATIC_DRAW);

		// Bone IDs (integers - use glVertexAttribIPointer)
		glEnableVertexAttribArray(VertexAttrib::BoneIDs);
		glVertexAttribIPointer(VertexAttrib::BoneIDs, MAX_NUM_BONES_PER_VERTEX, GL_INT,
			sizeof(VertexBoneData), (const GLvoid*)0);

		// Bone weights (floats)
		glEnableVertexAttribArray(VertexAttrib::BoneWeights);
		glVertexAttribPointer(VertexAttrib::BoneWeights, MAX_NUM_BONES_PER_VERTEX, GL_FLOAT, GL_FALSE,
			sizeof(VertexBoneData),
			(const GLvoid*)(MAX_NUM_BONES_PER_VERTEX * sizeof(int)));

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Buffers[INDEX_BUFFER]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			sizeof(uint32_t) * m_Geometry.Indices.size(),
			m_Geometry.Indices.data(), GL_STATIC_DRAW);

		// Debug output
		HMN_CORE_INFO("SkinnedMesh: Uploaded {} vertices, {} indices",
			m_Geometry.Positions.size(), m_Geometry.Indices.size());

		// Log first few vertices to verify data
		for (uint32_t i = 0; i < std::min(3u, (uint32_t)m_Geometry.Positions.size()); i++)
		{
			HMN_CORE_INFO("  Vertex[{}] pos: ({:.2f}, {:.2f}, {:.2f})", i,
				m_Geometry.Positions[i].x, m_Geometry.Positions[i].y, m_Geometry.Positions[i].z);
		}

		// Log first few indices
		HMN_CORE_INFO("  First indices: {} {} {} {} {} {}",
			m_Geometry.Indices.size() > 0 ? m_Geometry.Indices[0] : -1,
			m_Geometry.Indices.size() > 1 ? m_Geometry.Indices[1] : -1,
			m_Geometry.Indices.size() > 2 ? m_Geometry.Indices[2] : -1,
			m_Geometry.Indices.size() > 3 ? m_Geometry.Indices[3] : -1,
			m_Geometry.Indices.size() > 4 ? m_Geometry.Indices[4] : -1,
			m_Geometry.Indices.size() > 5 ? m_Geometry.Indices[5] : -1);

		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
			HMN_CORE_ERROR("SkinnedMesh: OpenGL error during upload: {}", error);
	}

	void SkinnedMesh::Render()
	{
		Render(m_Shader);
	}

	void SkinnedMesh::Render(const Ref<Shader>& overrideShader)
	{
		Ref<Shader> shader = overrideShader ? overrideShader : m_Shader;
		HMN_CORE_ASSERT(shader, "SkinnedMesh::Render called without a shader");

		shader->Bind();

		glBindVertexArray(m_VAO);
		DrawSubmeshes();
		glBindVertexArray(0);
	}

	void SkinnedMesh::DrawSubmeshes()
	{
		if (m_VAO == 0)
		{
			HMN_CORE_ERROR("SkinnedMesh: Cannot render - VAO is 0");
			return;
		}

		if (m_Geometry.Submeshes.empty())
		{
			HMN_CORE_ERROR("SkinnedMesh: Cannot render - no submeshes");
			return;
		}

		for (size_t i = 0; i < m_Geometry.Submeshes.size(); i++)
		{
			const auto& submesh = m_Geometry.Submeshes[i];

			// Clear any previous errors
			glGetError();

			// Bind material texture if available
			if (submesh.MaterialIndex < m_Materials.size() && m_Materials[submesh.MaterialIndex])
			{
				m_Materials[submesh.MaterialIndex]->Bind(GL_TEXTURE0);
			}
			else
			{
				// No material - unbind texture to use shader's default color
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			GLenum error = glGetError();
			if (error != GL_NO_ERROR)
			{
				HMN_CORE_ERROR("SkinnedMesh: OpenGL error BEFORE draw call: {}", error);
			}

			// Draw with base vertex offset (allows shared index buffer)
			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				submesh.NumIndices,
				GL_UNSIGNED_INT,
				(void*)(sizeof(uint32_t) * submesh.BaseIndex),
				submesh.BaseVertex
			);

			error = glGetError();
			if (error != GL_NO_ERROR)
			{
				HMN_CORE_ERROR("SkinnedMesh: OpenGL error AFTER draw call for submesh {}: {} (0x{:X})", i, error, error);
				HMN_CORE_ERROR("NumIndices: {}, BaseVertex: {}, BaseIndex: {}",
					submesh.NumIndices, submesh.BaseVertex, submesh.BaseIndex);
			}
		}
	}

	void SkinnedMesh::GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransforms(timeSeconds, transforms, disableRootMotion);
	}

	void SkinnedMesh::GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
		uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlended(timeSeconds, transforms, startAnimIndex, endAnimIndex, blendFactor, disableRootMotion);
	}

	void SkinnedMesh::UploadBoneTransform(uint32_t boneIndex, const glm::mat4& transform)
	{
		HMN_CORE_ASSERT(boneIndex < MAX_BONES, "Bone index out of range");

		if (m_BoneUniformLocations[boneIndex] == static_cast<uint32_t>(-1))
		{
			static bool warned = false;
			if (!warned)
			{
				HMN_CORE_WARN("SkinnedMesh: Bone uniform {} not found in shader", boneIndex);
				warned = true;
			}
			return;
		}

		glUniformMatrix4fv(m_BoneUniformLocations[boneIndex], 1, GL_FALSE, glm::value_ptr(transform));
	}

	void SkinnedMesh::InitBoneUniforms(const Ref<Shader>& shader)
	{
		shader->Bind();

		int foundCount = 0;
		for (uint32_t i = 0; i < MAX_BONES; i++)
		{
			std::string uniformName = "g_Bones[" + std::to_string(i) + "]";
			m_BoneUniformLocations[i] = shader->GetUniformLocation(uniformName);

			if (m_BoneUniformLocations[i] != static_cast<uint32_t>(-1) && i < 5)
			{
				HMN_CORE_INFO("SkinnedMesh: Found uniform '{}' at location {}",
					uniformName, m_BoneUniformLocations[i]);
				foundCount++;
			}
		}

		HMN_CORE_INFO("SkinnedMesh: Initialized {} bone uniforms", foundCount);
	}

}
