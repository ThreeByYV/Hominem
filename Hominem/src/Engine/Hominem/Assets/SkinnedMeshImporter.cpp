#include "hmnpch.h"
#include "SkinnedMeshImporter.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"
#include "Hominem/Utils/AssimpGlm.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace Hominem {

namespace {

constexpr unsigned int k_LoadFlags =
	aiProcess_Triangulate      |
	aiProcess_GenSmoothNormals |
	aiProcess_FlipUVs          |
	aiProcess_JoinIdenticalVertices |
	aiProcess_GlobalScale; // FBX UnitScaleFactor -> metres; scales verts, bones AND anim keys

// Flatten the aiNode tree into a POD array (pre-order, root at index 0).
int FlattenNode(const aiNode* node, std::vector<SkeletonNode>& out)
{
	int myIndex = static_cast<int>(out.size());
	out.emplace_back(); // reserve our slot before recursing into children

	SkeletonNode sn;
	sn.Name           = node->mName.C_Str();
	sn.LocalTransform = AiToGlm(node->mTransformation);
	sn.Children.reserve(node->mNumChildren);
	for (uint32_t i = 0; i < node->mNumChildren; i++)
		sn.Children.push_back(FlattenNode(node->mChildren[i], out));

	out[myIndex] = std::move(sn);
	return myIndex;
}

// Convert one aiAnimation to the POD Animation (channels keyed by node name).
Animation BuildAnimation(const aiAnimation* a)
{
	Animation anim;
	anim.TicksPerSecond = static_cast<float>(a->mTicksPerSecond != 0 ? a->mTicksPerSecond : 25.0);
	anim.Duration       = static_cast<float>(a->mDuration);

	for (uint32_t c = 0; c < a->mNumChannels; c++)
	{
		const aiNodeAnim* ch = a->mChannels[c];
		AnimChannel out;

		out.Positions.reserve(ch->mNumPositionKeys);
		for (uint32_t k = 0; k < ch->mNumPositionKeys; k++)
			out.Positions.push_back({ static_cast<float>(ch->mPositionKeys[k].mTime), AiToGlm(ch->mPositionKeys[k].mValue) });

		out.Rotations.reserve(ch->mNumRotationKeys);
		for (uint32_t k = 0; k < ch->mNumRotationKeys; k++)
			out.Rotations.push_back({ static_cast<float>(ch->mRotationKeys[k].mTime), AiToGlm(ch->mRotationKeys[k].mValue) });

		out.Scalings.reserve(ch->mNumScalingKeys);
		for (uint32_t k = 0; k < ch->mNumScalingKeys; k++)
			out.Scalings.push_back({ static_cast<float>(ch->mScalingKeys[k].mTime), AiToGlm(ch->mScalingKeys[k].mValue) });

		anim.Channels[ch->mNodeName.C_Str()] = std::move(out);
	}
	return anim;
}

void ExtractGeometry(const aiScene* scene, SkinnedMeshData& data)
{
	uint32_t totalVertices = 0, totalIndices = 0;
	data.Submeshes.resize(scene->mNumMeshes);
	data.SubmeshBaseVertices.resize(scene->mNumMeshes);

	for (uint32_t i = 0; i < scene->mNumMeshes; i++)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		data.Submeshes[i].MaterialIndex = mesh->mMaterialIndex;
		data.Submeshes[i].NumIndices    = mesh->mNumFaces * 3;
		data.Submeshes[i].BaseVertex    = totalVertices;
		data.Submeshes[i].BaseIndex     = totalIndices;
		data.SubmeshBaseVertices[i]     = totalVertices;
		totalVertices += mesh->mNumVertices;
		totalIndices  += data.Submeshes[i].NumIndices;
	}

	data.Positions.reserve(totalVertices);
	data.Normals.reserve(totalVertices);
	data.TexCoords.reserve(totalVertices);
	data.Indices.reserve(totalIndices);
	data.VertexBoneData.resize(totalVertices);

	const aiVector3D zero(0.f);
	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
	{
		const aiMesh* mesh = scene->mMeshes[m];
		if (!mesh->mVertices) { HMN_CORE_ERROR("SkinnedMesh: submesh {} has no vertices", m); continue; }

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			const aiVector3D& p = mesh->mVertices[i];
			const aiVector3D& n = mesh->mNormals ? mesh->mNormals[i] : zero;
			const aiVector3D& u = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : zero;
			data.Positions.emplace_back(p.x, p.y, p.z); // already metres (aiProcess_GlobalScale)
			data.Normals.emplace_back(n.x, n.y, n.z);
			data.TexCoords.emplace_back(u.x, u.y);
		}
		for (uint32_t f = 0; f < mesh->mNumFaces; f++)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue;
			data.Indices.push_back(face.mIndices[0]);
			data.Indices.push_back(face.mIndices[1]);
			data.Indices.push_back(face.mIndices[2]);
		}
	}
}

// Parse bone weights + offset matrices, mirroring the old Skeleton::ParseSingleBone.
void ParseBones(const aiScene* scene, SkinnedMeshData& data)
{
	auto getBoneId = [&](const aiBone* bone) -> int
	{
		std::string name = bone->mName.C_Str();
		auto it = data.BoneNameToIndex.find(name);
		if (it != data.BoneNameToIndex.end()) return it->second;
		int id = static_cast<int>(data.BoneNameToIndex.size());
		data.BoneNameToIndex[name] = id;
		return id;
	};

	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
	{
		const aiMesh* mesh = scene->mMeshes[m];
		if (!mesh->HasBones()) continue;

		for (uint32_t b = 0; b < mesh->mNumBones; b++)
		{
			const aiBone* bone = mesh->mBones[b];
			if (!bone) continue;

			int boneID = getBoneId(bone);
			if (boneID == static_cast<int>(data.BoneOffsets.size()))
				data.BoneOffsets.push_back(AiToGlm(bone->mOffsetMatrix));

			for (uint32_t w = 0; w < bone->mNumWeights; w++)
			{
				const aiVertexWeight& vw = bone->mWeights[w];
				uint32_t globalVertexId = data.SubmeshBaseVertices[m] + vw.mVertexId;
				if (globalVertexId >= data.VertexBoneData.size())
				{
					HMN_CORE_ERROR("SkinnedMesh: vertex ID {} out of range (mesh {} bone {})",
						globalVertexId, m, bone->mName.C_Str());
					continue;
				}
				data.VertexBoneData[globalVertexId].AddBoneData(boneID, vw.mWeight);
			}
		}
	}
}

void LoadMaterials(const aiScene* scene, const std::string& path, SkinnedMeshData& data)
{
	size_t lastSlash = path.find_last_of("/\\");
	std::string dir  = (lastSlash == std::string::npos) ? "."
	                 : (lastSlash == 0)                 ? "/"
	                 : path.substr(0, lastSlash);

	data.MaterialAlbedo.resize(scene->mNumMaterials);
	for (uint32_t i = 0; i < scene->mNumMaterials; i++)
	{
		data.MaterialAlbedo[i] = LoadMaterialTexture(scene, scene->mMaterials[i], aiTextureType_DIFFUSE, dir);
		if (i == 0)
		{
			data.NormalMap         = LoadMaterialTexture(scene, scene->mMaterials[i], aiTextureType_NORMALS,   dir);
			data.MetalRoughnessMap = LoadMaterialTexture(scene, scene->mMaterials[i], aiTextureType_METALNESS, dir);
		}
	}
}

} // namespace

std::expected<SkinnedMeshData, std::string> ImportSkinnedMesh(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path.c_str(), k_LoadFlags);
	if (!scene)
		return std::unexpected(std::format("SkinnedMesh: failed to load '{}': {}", path, importer.GetErrorString()));
	if (scene->mNumMeshes == 0)
		return std::unexpected(std::format("SkinnedMesh: '{}' contains 0 meshes", path));

	SkinnedMeshData data;
	ExtractGeometry(scene, data);
	ParseBones(scene, data);

	data.GlobalInverse = glm::inverse(AiToGlm(scene->mRootNode->mTransformation));
	FlattenNode(scene->mRootNode, data.Nodes);

	if (scene->mNumAnimations > 0)
		data.MainAnimation = BuildAnimation(scene->mAnimations[0]);

	LoadMaterials(scene, path, data);

	HMN_CORE_INFO("SkinnedMesh: '{}' - {}anims {}submesh {}bones {}verts {}idx",
		path, scene->mNumAnimations, data.Submeshes.size(),
		data.BoneOffsets.size(), data.Positions.size(), data.Indices.size());

	return data;
}

std::expected<Animation, std::string> ImportAnimation(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path.c_str(), k_LoadFlags);
	if (!scene)
		return std::unexpected(std::format("SkinnedMesh: failed to load animation '{}': {}", path, importer.GetErrorString()));
	if (scene->mNumAnimations == 0)
		return std::unexpected(std::format("SkinnedMesh: '{}' contains no animations", path));

	return BuildAnimation(scene->mAnimations[0]);
}

}
