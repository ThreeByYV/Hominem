#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

#include <assimp/scene.h>
#include <glm/glm.hpp>

namespace Hominem {

#define MAX_NUM_BONES_PER_VERTEX 4
#define MAX_BONES 100

	/// One animation contributing to a multi-way blend.
	/// Caller fills an array with these and passes to GetBoneTransformsBlendedN.
	/// Weights do not need to be pre-normalised — the method normalises internally.
	struct AnimBlendSample
	{
		uint32_t animIndex = 0;   // index into loaded animations (0 = base, 1+ = additional)
		float    time      = 0.f; // playback position in seconds (per-clip, independent)
		float    weight    = 1.f; // contribution weight
	};

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
		/// World-space joint transform, updated each call to GetBoneTransforms*.
		/// Translation column gives the joint's 3D position in model/world space.
		glm::mat4 GlobalTransform{ 1.f };

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

		/// Blend any number of animations by weight.
		/// Weights are normalised internally so they don't need to sum to 1.
		/// Uses per-clip local-space slerp — same quality as the 2-way blend path.
		void GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
									   std::vector<glm::mat4>& transforms,
									   bool disableRootMotion = false);

		int GetNumBones() const { return static_cast<int>(m_BoneInfo.size()); }
		int GetBoneCount() const { return static_cast<int>(m_BoneInfo.size()); }
		bool HasBones() const { return !m_BoneInfo.empty(); }

		std::vector<std::string> GetBoneNames() const
		{
			std::vector<std::string> names;
			names.reserve(m_BoneNameToIndexMap.size());
			for (const auto& [name, idx] : m_BoneNameToIndexMap)
				names.push_back(name);
			return names;
		}

		/// @brief Returns the world-space transform of the named bone joint after
		///        the last GetBoneTransforms* call. Returns nullopt if the bone
		///        doesn't exist (e.g. mesh has no skeleton, or wrong name).
		std::optional<glm::mat4> GetBoneWorldTransform(const std::string& name) const;

		/// Extra local-space rotation injected at a named joint during GetBoneTransforms
		/// (anim-0 path only). Multiplied AFTER the animated local transform, so child
		/// bones follow, e.g. rotate "mixamorig:RightArm" to make the whole arm reach.
		/// Set per-actor before each GetBoneTransforms call (cheap; transient).
		void SetBonePoseOverride(const std::string& boneName, const glm::mat4& localRotation)
		{
			m_BonePoseOverrides[boneName] = localRotation;
		}
		void ClearPoseOverrides() { m_BonePoseOverrides.clear(); }

		void SetScene(const aiScene* pScene) { m_pScene = pScene; }
		void AddAdditionalScene(const aiScene* pScene)
		{
			m_AdditionalScenes.push_back(pScene);
			if (pScene->mNumAnimations > 0)
				m_AdditionalChannelMaps.push_back(BuildChannelMap(pScene->mAnimations[0]));
			else
				m_AdditionalChannelMaps.emplace_back();
		}

	private:
		using ChannelMap = std::unordered_map<std::string, const aiNodeAnim*>;

		void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ReadNodeHierarchy(float animationTimeTicks, const aiNode* pNode,
			const glm::mat4& parentTransform, bool disableRootMotion = false);

		void ReadNodeHierarchyBlended(float startAnimTimeTicks, float endAnimTimeTicks,
			const aiNode* pNode, const glm::mat4& parentTransform,
			const aiAnimation& startAnimation, const aiAnimation& endAnimation,
			float blendFactor, bool disableRootMotion = false);

		void ReadNodeHierarchyBlendedN(
			const std::vector<std::pair<const aiAnimation*, float>>& animsAndTimes,
			const std::vector<float>& normalisedWeights,
			const aiNode* pNode, const glm::mat4& parentTransform,
			bool disableRootMotion);

		const ChannelMap& GetChannelMapForAnim(const aiAnimation* anim) const;

		int GetBoneId(const aiBone* pBone);

		const aiNodeAnim* FindNodeAnim(const ChannelMap& map, const std::string& nodeName) const;
		ChannelMap BuildChannelMap(const aiAnimation* pAnimation) const;

		void CalcInterpolatedScaling(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedTranslation(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);

		void CalcLocalTransform(LocalTransform& transform, float animTimeTicks, const aiNodeAnim* pNodeAnim);

		float CalcAnimationTimeTicks(float timeInSeconds, uint32_t animationIndex);

		const aiScene* GetSceneForAnimIndex(uint32_t animIndex) const;
		uint32_t GetAnimIndexInScene(uint32_t globalAnimIndex) const;

	private:
		std::map<std::string, int> m_BoneNameToIndexMap;
		std::vector<BoneInfo>      m_BoneInfo;
		glm::mat4                  m_GlobalInverseTransform{ 1.0f };
		const aiScene*             m_pScene = nullptr;
		std::vector<const aiScene*> m_AdditionalScenes;

		ChannelMap              m_MainChannelMap;
		std::vector<ChannelMap> m_AdditionalChannelMaps;

		std::unordered_map<std::string, glm::mat4> m_BonePoseOverrides;
	};

}
