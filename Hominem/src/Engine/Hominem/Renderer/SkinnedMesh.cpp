#include "hmnpch.h"
#include "SkinnedMesh.h"
#include <glad/glad.h>

#include "glm/gtc/type_ptr.hpp"
#include "Hominem/Utils/Renderer.h"
#include "Hominem/Utils/FileUtils.h"
#include "Hominem/Core/Core.h"
#include "Hominem/Renderer/Shader.h"

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
		ReleaseComputeSSBOs();
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
	}

	bool SkinnedMesh::LoadFromFile(const std::string& filepath)
	{
		ReleaseGPUResources();
		CreateGPUBuffers();

		m_AdditionalImporters.clear();
		m_AdditionalScenes.clear();

		m_pScene = m_Importer.ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!m_pScene)
		{
			HMN_CORE_ERROR("SkinnedMesh: failed to load '{}': {}", filepath, m_Importer.GetErrorString());
			glBindVertexArray(0);
			return false;
		}

		if (m_pScene->mNumMeshes == 0)
		{
			HMN_CORE_ERROR("SkinnedMesh: '{}' contains 0 meshes", filepath);
			glBindVertexArray(0);
			return false;
		}

		bool success = ParseScene(m_pScene, filepath);

		if (!success)
		{
			HMN_CORE_ERROR("SkinnedMesh: ParseScene failed for '{}'", filepath);
			glBindVertexArray(0);
			return false;
		}

		UploadToGPU();

		HMN_CORE_INFO("SkinnedMesh: '{}' — {}anims {}submesh {}bones {}verts {}idx",
			filepath,
			m_pScene->mNumAnimations, m_Geometry.Submeshes.size(),
			m_Skeleton.GetNumBones(),
			m_Geometry.Positions.size(), m_Geometry.Indices.size());

		glBindVertexArray(0);
		return success;
	}

	bool SkinnedMesh::LoadAdditionalAnimation(const std::string& filepath)
	{
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

		m_AdditionalScenes.push_back(pScene);
		m_AdditionalImporters.push_back(std::move(importer));
		m_Skeleton.AddAdditionalScene(pScene);

		HMN_CORE_INFO("SkinnedMesh: anim '{}' loaded — {} total anims", filepath, GetAnimationCount());

		return true;
	}

	void SkinnedMesh::CreateGPUBuffers()
	{
		glCreateVertexArrays(1, &m_VAO);
		glCreateBuffers(NUM_BUFFERS, m_Buffers);
	}

	bool SkinnedMesh::ParseScene(const aiScene* pScene, const std::string& filepath)
	{
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
		}

		// Allocate space
		m_Geometry.Reserve(totalVertices, totalIndices);
		m_VertexBoneData.resize(totalVertices);

		// Extract geometry from each submesh
		ExtractGeometry(pScene);

		// Parse skeleton and bone weights
		m_Skeleton.ParseFromScene(pScene, m_Geometry.SubmeshBaseVertices, m_VertexBoneData);

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
		if (m_Geometry.IsEmpty())
		{
			HMN_CORE_ERROR("SkinnedMesh: No geometry to upload!");
			return;
		}

		glNamedBufferData(m_Buffers[TEXCOORD_BUFFER],
			sizeof(glm::vec2) * m_Geometry.TexCoords.size(),
			m_Geometry.TexCoords.data(), GL_STATIC_DRAW);

		glNamedBufferData(m_Buffers[INDEX_BUFFER],
			sizeof(uint32_t) * m_Geometry.Indices.size(),
			m_Geometry.Indices.data(), GL_STATIC_DRAW);

		constexpr GLuint kTexCoordBinding = 0;
		glEnableVertexArrayAttrib(m_VAO, VertexAttrib::TexCoord);
		glVertexArrayAttribFormat(m_VAO, VertexAttrib::TexCoord, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(m_VAO, VertexAttrib::TexCoord, kTexCoordBinding);
		glVertexArrayVertexBuffer(m_VAO, kTexCoordBinding, m_Buffers[TEXCOORD_BUFFER], 0, sizeof(glm::vec2));
		glVertexArrayElementBuffer(m_VAO, m_Buffers[INDEX_BUFFER]);

		CreateComputeSSBOs();
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

	void SkinnedMesh::CreateComputeSSBOs()
	{
		ReleaseComputeSSBOs();

		uint32_t vertCount = static_cast<uint32_t>(m_Geometry.Positions.size());
		uint32_t boneCount = static_cast<uint32_t>(m_Skeleton.GetNumBones());

		// Pack positions and normals as vec4 (GPU prefers 16-byte aligned reads).
		std::vector<glm::vec4> pos4(vertCount), norm4(vertCount);
		for (uint32_t i = 0; i < vertCount; i++)
		{
			pos4[i]  = glm::vec4(m_Geometry.Positions[i], 1.0f);
			norm4[i] = glm::vec4(m_Geometry.Normals[i],   0.0f);
		}

		// Uploaded once at load — rest-pose data never changes
		glCreateBuffers(1, &m_InPosSSBO);
		glNamedBufferData(m_InPosSSBO, sizeof(glm::vec4) * vertCount, pos4.data(), GL_STATIC_DRAW);

		glCreateBuffers(1, &m_InNormSSBO);
		glNamedBufferData(m_InNormSSBO, sizeof(glm::vec4) * vertCount, norm4.data(), GL_STATIC_DRAW);

		glCreateBuffers(1, &m_InBoneDataSSBO);
		glNamedBufferData(m_InBoneDataSSBO, sizeof(VertexBoneData) * vertCount, m_VertexBoneData.data(), GL_STATIC_DRAW);

		// Updated per frame
		glCreateBuffers(1, &m_BoneSSBO);
		glNamedBufferData(m_BoneSSBO, sizeof(glm::mat4) * boneCount, nullptr, GL_DYNAMIC_DRAW);

		// GPU-to-GPU only — compute writes, VS reads, CPU never touches
		glCreateBuffers(1, &m_OutPosSSBO);
		glNamedBufferData(m_OutPosSSBO, sizeof(glm::vec4) * vertCount, nullptr, GL_DYNAMIC_COPY);

		glCreateBuffers(1, &m_OutNormSSBO);
		glNamedBufferData(m_OutNormSSBO, sizeof(glm::vec4) * vertCount, nullptr, GL_DYNAMIC_COPY);

		m_ComputeShader = ComputeShader::Create("Resources/Shaders/skinning.comp");
		m_BoneCache.reserve(boneCount);
	}

	void SkinnedMesh::ReleaseComputeSSBOs()
	{
		GLuint bufs[] = { m_InPosSSBO, m_InNormSSBO, m_InBoneDataSSBO, m_BoneSSBO, m_OutPosSSBO, m_OutNormSSBO };
		glDeleteBuffers(6, bufs);
		m_InPosSSBO = m_InNormSSBO = m_InBoneDataSSBO = m_BoneSSBO = m_OutPosSSBO = m_OutNormSSBO = 0;
		m_ComputeShader.reset();
	}

	void SkinnedMesh::DispatchSkinning(const std::vector<glm::mat4>& bones)
	{
		if (!m_ComputeShader || bones.empty()) return;

		glNamedBufferSubData(m_BoneSSBO, 0, sizeof(glm::mat4) * bones.size(), bones.data());

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_BoneSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_InPosSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_InNormSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_InBoneDataSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_OutPosSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_OutNormSSBO);

		m_ComputeShader->Bind();
		m_ComputeShader->SetUint("u_VertexCount", static_cast<uint32_t>(m_Geometry.Positions.size()));

		// Dispatch one thread per vertex. local_size_x=64 so we need ceil(verts/64) groups.
		uint32_t groups = (static_cast<uint32_t>(m_Geometry.Positions.size()) + 63) / 64;
		m_ComputeShader->Dispatch(groups);
		// Dispatch() already issues GL_SHADER_STORAGE_BARRIER_BIT — VS reads are safe after this.
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


}
