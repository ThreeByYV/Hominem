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
		aiProcess_JoinIdenticalVertices |
		aiProcess_GlobalScale; // FBX UnitScaleFactor -> metres; scales verts, bones AND anim keys

	OpenGLSkinnedMesh::~OpenGLSkinnedMesh()
	{
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

	std::expected<void, std::string> OpenGLSkinnedMesh::LoadFromFile(const std::string& filepath)
	{
		m_AdditionalImporters.clear();
		m_AdditionalScenes.clear();

		m_pScene = m_Importer.ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);
		if (!m_pScene)
			return std::unexpected(std::format("SkinnedMesh: failed to load '{}': {}", filepath, m_Importer.GetErrorString()));

		if (m_pScene->mNumMeshes == 0)
			return std::unexpected(std::format("SkinnedMesh: '{}' contains 0 meshes", filepath));

		if (!ParseScene(m_pScene, filepath))
			return std::unexpected(std::format("SkinnedMesh: ParseScene failed for '{}'", filepath));

		HMN_CORE_INFO("SkinnedMesh: '{}' — {}anims {}submesh {}bones {}verts {}idx",
			filepath,
			m_pScene->mNumAnimations, m_Geometry.Submeshes.size(),
			m_Skeleton.GetNumBones(),
			m_Geometry.Positions.size(), m_Geometry.Indices.size());

		RenderThread::QueueUpload([this] {
			CreateGPUBuffers();
			UploadToGPU();
		});
		return {};
	}

	std::expected<void, std::string> OpenGLSkinnedMesh::LoadAdditionalAnimation(const std::string& filepath)
	{
		auto importer = CreateScope<Assimp::Importer>();
		const aiScene* pScene = importer->ReadFile(filepath.c_str(), ASSIMP_LOAD_FLAGS);

		if (!pScene)
			return std::unexpected(std::format("SkinnedMesh: failed to load animation '{}': {}", filepath, importer->GetErrorString()));

		if (pScene->mNumAnimations == 0)
			return std::unexpected(std::format("SkinnedMesh: '{}' contains no animations", filepath));

		m_AdditionalScenes.push_back(pScene);
		m_AdditionalImporters.push_back(std::move(importer));
		m_Skeleton.AddAdditionalScene(pScene);

		HMN_CORE_INFO("SkinnedMesh: anim '{}' loaded — {} total anims", filepath, GetAnimationCount());
		return {};
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
		// Reset existing SSBOs before recreating (safe on reload — Ref<> drops the old GL object).
		m_InPosSSBO = m_InNormSSBO = m_InBoneDataSSBO = m_BoneSSBO = m_OutPosSSBO = m_OutNormSSBO = nullptr;

		uint32_t vertCount = static_cast<uint32_t>(m_Geometry.Positions.size());
		uint32_t boneCount = static_cast<uint32_t>(m_Skeleton.GetNumBones());

		// Pack as vec4 — GPU prefers 16-byte aligned reads.
		std::vector<glm::vec4> pos4(vertCount), norm4(vertCount);
		for (uint32_t i = 0; i < vertCount; i++)
		{
			pos4[i]  = glm::vec4(m_Geometry.Positions[i], 1.f);
			norm4[i] = glm::vec4(m_Geometry.Normals[i],   0.f);
		}

		const uint32_t vertBytes     = vertCount * (uint32_t)sizeof(glm::vec4);
		const uint32_t boneDataBytes = vertCount * (uint32_t)sizeof(VertexBoneData);

		// Rest-pose data — uploaded once at load, never changes.
		m_InPosSSBO = StorageBuffer::Create(vertBytes);
		m_InPosSSBO->SetData(pos4.data(), vertBytes);

		m_InNormSSBO = StorageBuffer::Create(vertBytes);
		m_InNormSSBO->SetData(norm4.data(), vertBytes);

		m_InBoneDataSSBO = StorageBuffer::Create(boneDataBytes);
		m_InBoneDataSSBO->SetData(m_VertexBoneData.data(), boneDataBytes);

		// At least one identity bone so the compute shader never reads OOB on a no-skeleton mesh.
		uint32_t allocBones      = std::max(boneCount, 1u);
		uint32_t boneMatrixBytes = allocBones * (uint32_t)sizeof(glm::mat4);
		std::vector<glm::mat4> identityMats(allocBones, glm::mat4(1.0f));
		m_BoneSSBO = StorageBuffer::Create(boneMatrixBytes);
		m_BoneSSBO->SetData(identityMats.data(), boneMatrixBytes);

		// Initialised with rest-pose so no-animation renders correctly without a dispatch.
		m_OutPosSSBO = StorageBuffer::Create(vertBytes);
		m_OutPosSSBO->SetData(pos4.data(), vertBytes);

		m_OutNormSSBO = StorageBuffer::Create(vertBytes);
		m_OutNormSSBO->SetData(norm4.data(), vertBytes);

		m_ComputeShader = ComputeShader::Create("engine://Shaders/skinning.comp");
		m_BoneCache.reserve(boneCount);
	}

	void OpenGLSkinnedMesh::DispatchSkinning(std::span<const glm::mat4> bones)
	{
		if (!m_VAO || !m_ComputeShader || !m_InBoneDataSSBO) return;

		// Always bind outputs and bone data so vertex + bone-weight shaders can read them.
		m_InBoneDataSSBO->BindBase(3);
		m_OutPosSSBO->BindBase(4);
		m_OutNormSSBO->BindBase(5);

		if (bones.empty()) return;

		m_BoneSSBO->SetData(bones.data(), (uint32_t)(bones.size() * sizeof(glm::mat4)));

		m_BoneSSBO->BindBase(0);
		m_InPosSSBO->BindBase(1);
		m_InNormSSBO->BindBase(2);

		m_ComputeShader->Bind();
		m_ComputeShader->SetUint("u_VertexCount", static_cast<uint32_t>(m_Geometry.Positions.size()));

		uint32_t groups = (static_cast<uint32_t>(m_Geometry.Positions.size()) + 63) / 64;
		m_ComputeShader->Dispatch(groups); // Dispatch() issues GL_SHADER_STORAGE_BARRIER_BIT — vertex shader reads are safe after this.
	}

	void OpenGLSkinnedMesh::GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransforms(timeSeconds, transforms, disableRootMotion);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
		uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlended(timeSeconds, transforms, startAnimIndex, endAnimIndex, blendFactor, disableRootMotion);
	}

	void OpenGLSkinnedMesh::GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
		std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		m_Skeleton.GetBoneTransformsBlendedN(samples, transforms, disableRootMotion);
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

			m_Geometry.Positions.emplace_back(pos.x, pos.y, pos.z); // already metres (aiProcess_GlobalScale)
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

	Ref<Texture2D> OpenGLSkinnedMesh::LoadTexture(const aiMaterial* mat, aiTextureType type, const std::string& dir)
	{
		if (mat->GetTextureCount(type) == 0) return nullptr;

		aiString texPath;
		if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS) return nullptr;

		if (texPath.C_Str()[0] == '*')
		{
			const aiTexture* tex = m_pScene->GetEmbeddedTexture(texPath.C_Str());
			if (!tex) return nullptr;
			if (tex->mHeight == 0)
				return Texture2D::CreateFromMemory(reinterpret_cast<const uint8_t*>(tex->pcData), tex->mWidth);
			Ref<Texture2D> t = Texture2D::Create(tex->mWidth, tex->mHeight, TextureFormat::RGBA8);
			t->SetData(tex->pcData, tex->mWidth * tex->mHeight * sizeof(aiTexel));
			return t;
		}

		return Texture2D::Create(dir + "/" + texPath.C_Str());
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
			m_Materials[i] = LoadTexture(pScene->mMaterials[i], aiTextureType_DIFFUSE, dir);
			if (!m_Materials[i]) allLoaded = false;

			if (i == 0)
			{
				m_Material.NormalMap         = LoadTexture(pScene->mMaterials[i], aiTextureType_NORMALS,   dir);
				m_Material.MetalRoughnessMap = LoadTexture(pScene->mMaterials[i], aiTextureType_METALNESS, dir);
			}
		}

		return allLoaded;
	}

}
