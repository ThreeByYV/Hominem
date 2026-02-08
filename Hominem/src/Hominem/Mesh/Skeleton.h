#pragma once

#include <map>
#include <vector>
#include <string>

#include <assimp/scene.h>
#include <glm/glm.hpp>

namespace Hominem {

#define MAX_NUM_BONES_PER_VERTEX 4
#define MAX_BONES 100

	struct LocalTransform
	{
		aiVector3D Scaling;
		aiQuaternion Rotation;
		aiVector3D Translation;
	};


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

		void GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms, bool disableRootMotion = false);

		void GetBoneTransformsBlended(float timeInSeconds,
									std::vector<glm::mat4>& blendedTransforms,
									uint32_t startAnimIndex,
									uint32_t endAnimIndex,
									float blendFactor,
									bool disableRootMotion = false);

		int GetNumBones() const { return static_cast<int>(m_BoneInfo.size()); }
		int GetBoneCount() const { return static_cast<int>(m_BoneInfo.size()); }
		bool HasBones() const { return !m_BoneInfo.empty(); }

		void SetScene(const aiScene* pScene) { m_pScene = pScene; }
		void AddAdditionalScene(const aiScene* pScene) { m_AdditionalScenes.push_back(pScene); }

	private:
		void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ReadNodeHierarchyBlended(float startAnimTimeTicks, float endAnimTimeTicks, const aiNode* pNode, const glm::mat4& parentTransform,
									 const aiAnimation& startAnimation, const aiAnimation& endAnimation, float blendFactor, bool disableRootMotion = false);

		int GetBoneId(const aiBone* pBone);

		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);

		void CalcInterpolatedScaling(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedTranslation(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);

		void ReadNodeHierarchy(float animationTimeTicks, const aiNode* pNode, const glm::mat4& parentTransform, bool disableRootMotion = false);

		void CalcLocalTransform(LocalTransform& transform, float animTimeTicks, const aiNodeAnim* pNodeAnim);

		float CalcAnimationTimeTicks(float timeInSeconds, uint32_t animationIndex);

		const aiScene* GetSceneForAnimIndex(uint32_t animIndex) const;
		uint32_t GetAnimIndexInScene(uint32_t globalAnimIndex) const;

	private:
		std::map<std::string, int> m_BoneNameToIndexMap;
		std::vector<BoneInfo> m_BoneInfo;
		glm::mat4 m_GlobalInverseTransform{ 1.0f };
		const aiScene* m_pScene = nullptr;
		std::vector<const aiScene*> m_AdditionalScenes;
	};

}
