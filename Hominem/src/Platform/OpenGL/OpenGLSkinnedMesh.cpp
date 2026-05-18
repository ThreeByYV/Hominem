#include "hmnpch.h"
#include "OpenGLSkinnedMesh.h"
#include "Hominem/Renderer/RenderThread.h"

#include <glad/glad.h>
#include <assimp/postprocess.h>
#include <glm/gtc/type_ptr.hpp>

#include "Hominem/Utils/Renderer.h"
#include "Hominem/Utils/FileUtils.h"

namespace Hominem {

	namespace VertexAttrib {
		constexpr int TexCoord = 1;
	}

	constexpr unsigned int ASSIMP_LOAD_FLAGS =
		aiProcess_Triangulate      |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipUVs          |
		aiProcess_JoinIdenticalVertices;

	OpenGLSkinnedMesh::~OpenGLSkinnedMesh()
	{
		ReleaseComputeSSBOs();
		ReleaseGPUResources();
	}

	void OpenGLSkinnedMesh::ReleaseGPUResources()
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

	bool OpenGLSkinnedMesh::LoadFromFile(const std::string& filepath)
	{
		// CPU-only — no GL calls. GPU init queued to run on the render thread.
		m_AdditionalImporters.clear();
		m_AdditionalScenes.clear();

		m_pScene = m_Importer.ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!m_pScene)
		{
			HMN_CORE_ERROR("SkinnedMesh: failed to load '{}': {}", filepath, m_Importer.GetErrorString());
			return false;
		}

		if (m_pScene->mNumMeshes == 0)
		{
			HMN_CORE_ERROR("SkinnedMesh: '{}' contains 0 meshes", filepath);
			return false;
		}

		// Detect unit scale from FBX metadata (same logic as StaticMesh).
		m_ScaleToMetres = 1.0f;
		if (m_pScene->mMetaData)
		{
			double unitScale = 1.0;
			if (m_pScene->mMetaData->Get("UnitScaleFactor", unitScale))
				m_ScaleToMetres = static_cast<float>(unitScale) * 0.01f;
		}
		HMN_CORE_INFO("SkinnedMesh: unit scale {:.4f} (1 file unit = {:.4f} m)",
			m_ScaleToMetres, m_ScaleToMetres);

		bool success = ParseScene(m_pScene, filepath);

		if (!success)
		{
			HMN_CORE_ERROR("SkinnedMesh: ParseScene failed for '{}'", filepath);
			return false;
		}

		HMN_CORE_INFO("SkinnedMesh: '{}' — {}anims {}submesh {}bones {}verts {}idx",
			filepath,
			m_pScene->mNumAnimations, m_Geometry.Submeshes.size(),
			m_Skeleton.GetNumBones(),
			m_Geometry.Positions.size(), m_Geometry.Indices.size());

		RenderThread::QueueUpload([this] {
			CreateGPUBuffers();
			UploadToGPU();
		});
		return success;
	}

	bool OpenGLSkinnedMesh::LoadAdditionalAnimation(const std::string& filepath)
	{
		auto importer = CreateScope<Assimp::Importer>();
		const aiScene* pScene = importer->ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!pScene)
		{
			HMN_CORE_ERROR("SkinnedMesh: Failed to load animation '{}': {}", filepath, importer->GetErrorString());
			return false;
		}

		if (pScene->mNumAnimations == 0)
		{
			HMN_CORE_WARN("SkinnedMesh: '{}' contains no animations", filepath);
			return false;
		}

		m_AdditionalScenes.push_back(pScene);
		m_AdditionalImporters.push_back(std::move(importer));
		m_Skeleton.AddAdditionalScene(pScene);

		HMN_CORE_INFO("SkinnedMesh: anim '{}' loaded — {} total anims", filepath, GetAnimationCount());
		return true;
	}

	uint32_t OpenGLSkinnedMesh::GetAnimationCount() const
	{
		uint32_t count = m_pScene ? m_pScene->mNumAnimations : 0;
		for (const auto* scene : m_AdditionalScenes)
			if (scene) count += scene->mNumAnimations;
		return count;
	}

	void OpenGLSkinnedMesh::CreateGPUBuffers()
	{
		glCreateVertexArrays(1, &m_VAO);
		glCreateBuffers(NUM_BUFFERS, m_Buffers);
	}

	void OpenGLSkinnedMesh::UploadToGPU()
	{
		if (m_Geometry.IsEmpty())
		{
			HMN_CORE_ERROR("SkinnedMesh: No geometry to upload");
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

	void OpenGLSkinnedMesh::Render(const Ref<Shader>& shader)
	{
		if (!m_VAO) return;
		Ref<Shader> active = shader ? shader : m_Shader;
		HMN_CORE_ASSERT(active, "SkinnedMesh::Render called without a shader");

		active->Bind();
		glBindVertexArray(m_VAO);
		DrawSubmeshes();
		glBindVertexArray(0);
	}

	void OpenGLSkinnedMesh::DrawSubmeshes()
	{
		if (m_VAO == 0 || m_Geometry.Submeshes.empty())
		{
			HMN_CORE_ERROR("SkinnedMesh: Cannot draw — VAO or submeshes missing");
			return;
		}

		for (size_t i = 0; i < m_Geometry.Submeshes.size(); i++)
		{
			const auto& submesh = m_Geometry.Submeshes[i];

			if (submesh.MaterialIndex < m_Materials.size() && m_Materials[submesh.MaterialIndex])
				m_Materials[submesh.MaterialIndex]->Bind(0);
			else
				glBindTexture(GL_TEXTURE_2D, 0);

			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				submesh.NumIndices,
				GL_UNSIGNED_INT,
				(void*)(sizeof(uint32_t) * submesh.BaseIndex),
				submesh.BaseVertex);
		}
	}

	void OpenGLSkinnedMesh::CreateComputeSSBOs()
	{
		ReleaseComputeSSBOs();

		uint32_t vertCount = static_cast<uint32_t>(m_Geometry.Positions.size());
		uint32_t boneCount = static_cast<uint32_t>(m_Skeleton.GetNumBones());

		// Pack as vec4 — GPU prefers 16-byte aligned reads.
		std::vector<glm::vec4> pos4(vertCount), norm4(vertCount);
		for (uint32_t i = 0; i < vertCount; i++)
		{
			pos4[i]  = glm::vec4(m_Geometry.Positions[i], 1.f);
			norm4[i] = glm::vec4(m_Geometry.Normals[i],   0.f);
		}

		// Rest-pose data — uploaded once at load, never changes.
		glCreateBuffers(1, &m_InPosSSBO);
		glNamedBufferData(m_InPosSSBO, sizeof(glm::vec4) * vertCount, pos4.data(), GL_STATIC_DRAW);

		glCreateBuffers(1, &m_InNormSSBO);
		glNamedBufferData(m_InNormSSBO, sizeof(glm::vec4) * vertCount, norm4.data(), GL_STATIC_DRAW);

		glCreateBuffers(1, &m_InBoneDataSSBO);
		glNamedBufferData(m_InBoneDataSSBO, sizeof(VertexBoneData) * vertCount, m_VertexBoneData.data(), GL_STATIC_DRAW);

		// Updated per frame.
		glCreateBuffers(1, &m_BoneSSBO);
		glNamedBufferData(m_BoneSSBO, sizeof(glm::mat4) * boneCount, nullptr, GL_DYNAMIC_DRAW);

		// GPU-to-GPU only — compute writes, vertex shader reads, CPU never touches.
		glCreateBuffers(1, &m_OutPosSSBO);
		glNamedBufferData(m_OutPosSSBO, sizeof(glm::vec4) * vertCount, nullptr, GL_DYNAMIC_COPY);

		glCreateBuffers(1, &m_OutNormSSBO);
		glNamedBufferData(m_OutNormSSBO, sizeof(glm::vec4) * vertCount, nullptr, GL_DYNAMIC_COPY);

		m_ComputeShader = ComputeShader::Create("Resources/Shaders/skinning.comp");
		m_BoneCache.reserve(boneCount);
	}

	void OpenGLSkinnedMesh::ReleaseComputeSSBOs()
	{
		GLuint bufs[] = { m_InPosSSBO, m_InNormSSBO, m_InBoneDataSSBO, m_BoneSSBO, m_OutPosSSBO, m_OutNormSSBO };
		glDeleteBuffers(6, bufs);
		m_InPosSSBO = m_InNormSSBO = m_InBoneDataSSBO = m_BoneSSBO = m_OutPosSSBO = m_OutNormSSBO = 0;
		m_ComputeShader.reset();
	}

	void OpenGLSkinnedMesh::DispatchSkinning(std::span<const glm::mat4> bones)
	{
		if (!m_VAO || !m_ComputeShader || bones.empty()) return;

		glNamedBufferSubData(m_BoneSSBO, 0, sizeof(glm::mat4) * bones.size(), bones.data());

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_BoneSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_InPosSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_InNormSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_InBoneDataSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_OutPosSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_OutNormSSBO);

		m_ComputeShader->Bind();
		m_ComputeShader->SetUint("u_VertexCount", static_cast<uint32_t>(m_Geometry.Positions.size()));

		uint32_t groups = (static_cast<uint32_t>(m_Geometry.Positions.size()) + 63) / 64;
		m_ComputeShader->Dispatch(groups); // Dispatch() issues GL_SHADER_STORAGE_BARRIER_BIT — vertex shader reads are safe after this.
	}

	// Bone matrices are computed in the file's unit space (e.g. cm for Blender FBX).
	// Vertex positions were already scaled to metres in ExtractSubmesh, so the translation
	// column of each bone matrix must match. Rotation/scale are unitless — only column 3
	// (the translation) needs to change: S * M * S_inv scales just the translation by s.
	void OpenGLSkinnedMesh::ApplyUnitScale(std::vector<glm::mat4>& transforms) const
	{
		if (m_ScaleToMetres == 1.0f) return;
		for (auto& m : transforms)
		{
			m[3][0] *= m_ScaleToMetres;
			m[3][1] *= m_ScaleToMetres;
			m[3][2] *= m_ScaleToMetres;
		}
	}

	void OpenGLSkinnedMesh::GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransforms(timeSeconds, transforms, disableRootMotion);
		ApplyUnitScale(transforms);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
		uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlended(timeSeconds, transforms, startAnimIndex, endAnimIndex, blendFactor, disableRootMotion);
		ApplyUnitScale(transforms);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
		std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlendedN(samples, transforms, disableRootMotion);
		ApplyUnitScale(transforms);
	}

	bool OpenGLSkinnedMesh::ParseScene(const aiScene* pScene, const std::string& filepath)
	{
		uint32_t totalVertices = 0, totalIndices = 0;

		m_Geometry.Submeshes.resize(pScene->mNumMeshes);
		m_Geometry.SubmeshBaseVertices.resize(pScene->mNumMeshes);

		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];
			m_Geometry.Submeshes[i].MaterialIndex = pMesh->mMaterialIndex;
			m_Geometry.Submeshes[i].NumIndices    = pMesh->mNumFaces * 3;
			m_Geometry.Submeshes[i].BaseVertex    = totalVertices;
			m_Geometry.Submeshes[i].BaseIndex     = totalIndices;
			m_Geometry.SubmeshBaseVertices[i]     = totalVertices;

			totalVertices += pMesh->mNumVertices;
			totalIndices  += m_Geometry.Submeshes[i].NumIndices;
		}

		m_Geometry.Reserve(totalVertices, totalIndices);
		m_VertexBoneData.resize(totalVertices);

		ExtractGeometry(pScene);
		m_Skeleton.ParseFromScene(pScene, m_Geometry.SubmeshBaseVertices, m_VertexBoneData);

		m_Materials.resize(pScene->mNumMaterials);
		if (!LoadMaterials(pScene, filepath))
			HMN_CORE_WARN("SkinnedMesh: Some materials failed to load");

		return true;
	}

	void OpenGLSkinnedMesh::ExtractGeometry(const aiScene* pScene)
	{
		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
			ExtractSubmesh(pScene->mMeshes[i], i);
	}

	void OpenGLSkinnedMesh::ExtractSubmesh(const aiMesh* pMesh, uint32_t submeshIndex)
	{
		const aiVector3D zero(0.f);

		if (!pMesh->mVertices)
		{
			HMN_CORE_ERROR("SkinnedMesh: Submesh {} has no vertices", submeshIndex);
			return;
		}

		for (uint32_t i = 0; i < pMesh->mNumVertices; i++)
		{
			const aiVector3D& pos    = pMesh->mVertices[i];
			const aiVector3D& normal = pMesh->mNormals ? pMesh->mNormals[i] : zero;
			const aiVector3D& uv     = pMesh->HasTextureCoords(0) ? pMesh->mTextureCoords[0][i] : zero;

			m_Geometry.Positions.emplace_back(pos.x * m_ScaleToMetres, pos.y * m_ScaleToMetres, pos.z * m_ScaleToMetres);
			m_Geometry.Normals.emplace_back(normal.x, normal.y, normal.z);
			m_Geometry.TexCoords.emplace_back(uv.x, uv.y);
		}

		for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
		{
			const aiFace& face = pMesh->mFaces[i];
			if (face.mNumIndices != 3) continue;
			m_Geometry.Indices.push_back(face.mIndices[0]);
			m_Geometry.Indices.push_back(face.mIndices[1]);
			m_Geometry.Indices.push_back(face.mIndices[2]);
		}
	}

	bool OpenGLSkinnedMesh::LoadMaterials(const aiScene* pScene, const std::string& filepath)
	{
		size_t lastSlash = filepath.find_last_of("/\\");
		std::string dir  = (lastSlash == std::string::npos) ? "."
		                 : (lastSlash == 0)                 ? "/"
		                 : filepath.substr(0, lastSlash);

		bool allLoaded = true;

		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			m_Materials[i] = nullptr;

			const aiMaterial* mat = pScene->mMaterials[i];
			if (mat->GetTextureCount(aiTextureType_DIFFUSE) == 0) continue;

			aiString texPath;
			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) != AI_SUCCESS)
			{
				allLoaded = false;
				continue;
			}

			m_Materials[i] = Texture2D::Create(dir + "/" + texPath.data);
			if (!m_Materials[i]) allLoaded = false;
		}

		return allLoaded;
	}

}
