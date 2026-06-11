#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

#define MAX_NUM_BONES_PER_VERTEX 4
#define MAX_BONES 100

	/// One animation contributing to a multi-way blend.
	/// Weights do not need to be pre-normalised - the method normalises internally.
	struct AnimBlendSample
	{
		uint32_t animIndex = 0;   // index into loaded animations (0 = base, 1+ = additional)
		float    time      = 0.f; // playback position in seconds (per-clip, independent)
		float    weight    = 1.f; // contribution weight
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
		/// World-space joint transform
		glm::mat4 GlobalTransform{ 1.f };

		BoneInfo(const glm::mat4& offset);
	};

	template<typename T>
	struct AnimKey { float Time; T Value; };

	using Vec3Key = AnimKey<glm::vec3>;
	using QuatKey = AnimKey<glm::quat>;

	/// Per-node keyframe tracks for one animation.
	struct AnimChannel
	{
		std::vector<Vec3Key> Positions;
		std::vector<QuatKey> Rotations;
		std::vector<Vec3Key> Scalings;
	};

	/// One animation clip: timing plus a nodeName -> channel map.
	struct Animation
	{
		float TicksPerSecond = 25.f;
		float Duration       = 0.f;
		std::unordered_map<std::string, AnimChannel> Channels;
	};

	/// One node in the skeleton hierarchy (bind-pose local transform + children indices).
	struct SkeletonNode
	{
		std::string      Name;
		glm::mat4        LocalTransform{ 1.f };
		std::vector<int> Children; // indices into the node array (root = 0)
	};

	class Skeleton
	{
	public:
		Skeleton() = default;

		/// Install the parsed hierarchy + bones. Built by the importer.
		void SetData(std::vector<SkeletonNode> nodes,
		             std::map<std::string, int> boneNameToIndex,
		             std::vector<glm::mat4> boneOffsets,
		             const glm::mat4& globalInverse);

		/// Animation slot 0 (from the main file; may have none).
		void SetMainAnimation(Animation anim);
		/// Append an animation loaded from a separate file (slot 1, 2, ...).
		void AddAnimation(Animation anim);

		void GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms, bool disableRootMotion = false);

		void GetBoneTransformsBlended(float timeInSeconds,
									std::vector<glm::mat4>& blendedTransforms,
									uint32_t startAnimIndex,
									uint32_t endAnimIndex,
									float blendFactor,
									bool disableRootMotion = false);

		/// Blend any number of animations by weight (normalised internally).
		void GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
									   std::vector<glm::mat4>& transforms,
									   bool disableRootMotion = false);

		int GetNumBones() const { return static_cast<int>(m_BoneInfo.size()); }
		int GetBoneCount() const { return static_cast<int>(m_BoneInfo.size()); }
		bool HasBones() const { return !m_BoneInfo.empty(); }

		/// Total reachable animations: main (if any) + additional.
		uint32_t GetAnimationCount() const
		{
			return (m_MainAnim ? 1u : 0u) + static_cast<uint32_t>(m_AdditionalAnims.size());
		}

		std::vector<std::string> GetBoneNames() const
		{
			std::vector<std::string> names;
			names.reserve(m_BoneNameToIndexMap.size());
			for (const auto& [name, idx] : m_BoneNameToIndexMap)
				names.push_back(name);
			return names;
		}

		/// World-space transform of the named bone after the last GetBoneTransforms* call.
		std::optional<glm::mat4> GetBoneWorldTransform(const std::string& name) const;

		/// Extra local-space rotation injected at a named joint during GetBoneTransforms
		/// (anim-0 path only). Applied after the animated local transform so children follow.
		void SetBonePoseOverride(const std::string& boneName, const glm::mat4& localRotation)
		{
			m_BonePoseOverrides[boneName] = localRotation;
		}

		void ClearPoseOverrides() { m_BonePoseOverrides.clear(); }

	private:
		struct LocalTransform
		{
			glm::vec3 Scaling{ 1.f };
			glm::quat Rotation{ 1.f, 0.f, 0.f, 0.f };
			glm::vec3 Translation{ 0.f };
		};

		const Animation* GetAnim(uint32_t animIndex) const;
		float CalcAnimationTimeTicks(float timeInSeconds, uint32_t animationIndex) const;

		void ReadNodeHierarchy(float animationTimeTicks, int nodeIndex,
			const glm::mat4& parentTransform, bool disableRootMotion);

		void ReadNodeHierarchyBlended(float startAnimTimeTicks, float endAnimTimeTicks,
			int nodeIndex, const glm::mat4& parentTransform,
			const Animation& startAnim, const Animation& endAnim,
			float blendFactor, bool disableRootMotion);

		void ReadNodeHierarchyBlendedN(
			const std::vector<std::pair<const Animation*, float>>& animsAndTimes,
			const std::vector<float>& normalisedWeights,
			int nodeIndex, const glm::mat4& parentTransform,
			bool disableRootMotion);

		const AnimChannel* FindChannel(const Animation& anim, const std::string& nodeName) const;
		void CalcLocalTransform(LocalTransform& out, float animTimeTicks, const AnimChannel& channel) const;

	private:
		std::vector<SkeletonNode>  m_Nodes;          // root at index 0
		std::map<std::string, int> m_BoneNameToIndexMap;
		std::vector<BoneInfo>      m_BoneInfo;
		glm::mat4                  m_GlobalInverseTransform{ 1.0f };

		std::optional<Animation>   m_MainAnim;       // animation slot 0
		std::vector<Animation>     m_AdditionalAnims; // slots 1..N

		std::unordered_map<std::string, glm::mat4> m_BonePoseOverrides;
	};

}
