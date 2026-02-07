#pragma once

#include <map>
#include <vector>
#include <string>

#include <assimp/scene.h>
#include <glm/glm.hpp>

namespace Hominem {

#define MAX_NUM_BONES_PER_VERTEX 4
#define MAX_BONES 100


	struct VertexBoneData
	{
		int   BoneIDs[MAX_NUM_BONES_PER_VERTEX];
		float Weights[MAX_NUM_BONES_PER_VERTEX];

		VertexBoneData();

		void AddBoneData(int boneID, float weight);
	};

	struct BoneInfo
	{
		glm::mat4 OffsetMatrix;
		glm::mat4 FinalTransformation;

		BoneInfo(const glm::mat4& offset);
	};

	class Skeleton
	{
	public:
		Skeleton() = default;

		void ParseFromScene(const aiScene* pScene,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms);

		int GetNumBones() const { return static_cast<int>(m_BoneInfo.size()); }
		int GetBoneCount() const { return static_cast<int>(m_BoneInfo.size()); }
		bool HasBones() const { return !m_BoneInfo.empty(); }

		void SetScene(const aiScene* pScene) { m_pScene = pScene; }

	private:
		void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		int GetBoneId(const aiBone* pBone);

		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);

		void CalcInterpolatedScaling(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedTranslation(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);

		void ReadNodeHierarchy(float animationTimeTicks, const aiNode* pNode, const glm::mat4& parentTransform);

	private:
		std::map<std::string, int> m_BoneNameToIndexMap;

		std::vector<BoneInfo> m_BoneInfo;

		glm::mat4 m_GlobalInverseTransform{ 1.0f };

		const aiScene* m_pScene = nullptr;
	};

}
