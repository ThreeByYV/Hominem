#include "hmnpch.h"
#include "Skeleton.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>

namespace Hominem {

	// interpolation helpers
	namespace {

		// A shortest-path slerp with a linear fallback for near-parallel quats.
		glm::quat SlerpShortest(const glm::quat& a, const glm::quat& b, float t)
		{
			float cosTheta = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
			glm::quat end = b;
			if (cosTheta < 0.0f)
			{
				cosTheta = -cosTheta;
				end = glm::quat(-b.w, -b.x, -b.y, -b.z); // glm::quat(w, x, y, z)
			}

			glm::quat out;
			if (cosTheta < 0.9999f)
			{
				float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
				float theta    = std::atan2(sinTheta, cosTheta);
				float sf1      = std::sin((1.0f - t) * theta) / sinTheta;
				float sf2      = std::sin(t * theta) / sinTheta;
				out.x = a.x * sf1 + end.x * sf2;
				out.y = a.y * sf1 + end.y * sf2;
				out.z = a.z * sf1 + end.z * sf2;
				out.w = a.w * sf1 + end.w * sf2;
			}
			else
			{
				// Near-parallel: linear blend.
				out.x = a.x * (1.0f - t) + end.x * t;
				out.y = a.y * (1.0f - t) + end.y * t;
				out.z = a.z * (1.0f - t) + end.z * t;
				out.w = a.w * (1.0f - t) + end.w * t;
			}
			return out;
		}

		// Find the keyframe pair surrounding `t` and return the blend factor [0,1].
		// Mirrors the original CalcInterpolatedGeneric key search.
		template<typename T>
		uint32_t KeyIndex(const std::vector<AnimKey<T>>& keys, float t, float& factorOut)
		{
			uint32_t index = 0;
			for (uint32_t i = 0; i < keys.size() - 1; i++)
			{
				if (t < keys[i + 1].Time) { index = i; break; }
			}
			uint32_t nextIndex = index + 1;
			float t1 = keys[index].Time;
			float t2 = keys[nextIndex].Time;
			float deltaTime = t2 - t1;
			// deltaTime 0 makes factor NaN (0/0) which explodes the mesh; guard it.
			factorOut = (deltaTime > 0.0f)
				? glm::clamp((t - t1) / deltaTime, 0.0f, 1.0f)
				: 0.0f;
			return index;
		}

		glm::vec3 InterpVec3(const std::vector<Vec3Key>& keys, float t)
		{
			if (keys.empty()) return glm::vec3(0.f);
			if (keys.size() == 1) return keys[0].Value;
			float f; uint32_t i = KeyIndex(keys, t, f);
			return keys[i].Value + (keys[i + 1].Value - keys[i].Value) * f;
		}

		glm::quat InterpQuat(const std::vector<QuatKey>& keys, float t)
		{
			if (keys.empty()) return glm::quat(1.f, 0.f, 0.f, 0.f);
			if (keys.size() == 1) return keys[0].Value;
			float f; uint32_t i = KeyIndex(keys, t, f);
			return glm::normalize(SlerpShortest(keys[i].Value, keys[i + 1].Value, f));
		}

	} // namespace

	VertexBoneData::VertexBoneData()
	{
		for (int i = 0; i < MAX_NUM_BONES_PER_VERTEX; i++)
		{
			BoneIDs[i] = 0;
			Weights[i] = 0.0f;
		}
	}

	void VertexBoneData::AddBoneData(int boneID, float weight)
	{
		for (int i = 0; i < MAX_NUM_BONES_PER_VERTEX; i++)
		{
			if (Weights[i] == 0.0f)
			{
				BoneIDs[i] = boneID;
				Weights[i] = weight;
				return;
			}
		}

		HMN_CORE_ASSERT(false, "Too many bones influencing one vertex");
	}

	BoneInfo::BoneInfo(const glm::mat4& offset)
		: OffsetMatrix(offset),
		FinalTransformation(1.0f)
	{
	}

	void Skeleton::SetData(std::vector<SkeletonNode> nodes,
		std::map<std::string, int> boneNameToIndex,
		std::vector<glm::mat4> boneOffsets,
		const glm::mat4& globalInverse)
	{
		m_Nodes                  = std::move(nodes);
		m_BoneNameToIndexMap     = std::move(boneNameToIndex);
		m_GlobalInverseTransform = globalInverse;

		m_BoneInfo.clear();
		m_BoneInfo.reserve(boneOffsets.size());
		for (const auto& offset : boneOffsets)
			m_BoneInfo.emplace_back(offset);

		HMN_CORE_INFO("Skeleton: {} bones, {} nodes", m_BoneInfo.size(), m_Nodes.size());
	}

	void Skeleton::SetMainAnimation(Animation anim) { m_MainAnim = std::move(anim); }
	void Skeleton::AddAnimation(Animation anim)     { m_AdditionalAnims.push_back(std::move(anim)); }

	std::optional<glm::mat4> Skeleton::GetBoneWorldTransform(const std::string& name) const
	{
		auto it = m_BoneNameToIndexMap.find(name);
		if (it == m_BoneNameToIndexMap.end())
			return std::nullopt;
		return m_BoneInfo[it->second].GlobalTransform;
	}

	// Animation slot 0 = main; 1.. = additional
	const Animation* Skeleton::GetAnim(uint32_t animIndex) const
	{
		if (animIndex == 0)
			return m_MainAnim ? &*m_MainAnim : nullptr;
		uint32_t additionalIndex = animIndex - 1;
		if (additionalIndex < m_AdditionalAnims.size())
			return &m_AdditionalAnims[additionalIndex];
		HMN_CORE_WARN("Skeleton: Animation index {} not found", animIndex);
		return nullptr;
	}

	float Skeleton::CalcAnimationTimeTicks(float timeInSeconds, uint32_t animationIndex) const
	{
		const Animation* anim = GetAnim(animationIndex);
		if (!anim) { HMN_CORE_WARN("Skeleton: Invalid animation index {}", animationIndex); return 0.0f; }

		float timeInTicks = timeInSeconds * anim->TicksPerSecond;
		return anim->Duration > 0.0f ? std::fmod(timeInTicks, anim->Duration) : 0.0f;
	}

	const AnimChannel* Skeleton::FindChannel(const Animation& anim, const std::string& nodeName) const
	{
		auto it = anim.Channels.find(nodeName);
		return it != anim.Channels.end() ? &it->second : nullptr;
	}

	void Skeleton::CalcLocalTransform(LocalTransform& out, float animTimeTicks, const AnimChannel& channel) const
	{
		out.Scaling     = InterpVec3(channel.Scalings,  animTimeTicks);
		out.Rotation    = InterpQuat(channel.Rotations, animTimeTicks);
		out.Translation = InterpVec3(channel.Positions, animTimeTicks);
	}

	void Skeleton::GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		if (!m_MainAnim || m_Nodes.empty())
		{
			HMN_CORE_WARN("Skeleton: No animations available");
			transforms.resize(m_BoneInfo.size(), glm::mat4(1.0f));
			return;
		}

		float timeInTicks = animationTimeSec * m_MainAnim->TicksPerSecond;
		float animationTimeTicks = m_MainAnim->Duration > 0.0f
			? std::fmod(timeInTicks, m_MainAnim->Duration) : 0.0f;

		ReadNodeHierarchy(animationTimeTicks, 0, glm::mat4(1.0f), disableRootMotion);

		transforms.resize(m_BoneInfo.size());
		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
			transforms[i] = m_BoneInfo[i].FinalTransformation;
	}

	void Skeleton::ReadNodeHierarchy(float animationTimeTicks, int nodeIndex,
		const glm::mat4& parentTransform, bool disableRootMotion)
	{
		const SkeletonNode& node = m_Nodes[nodeIndex];
		const std::string& nodeName = node.Name;

		glm::mat4 nodeTransform = node.LocalTransform;
		bool isRootNode = (parentTransform == glm::mat4(1.0f));

		const AnimChannel* channel = FindChannel(*m_MainAnim, nodeName);
		if (channel)
		{
			glm::vec3 scaling     = InterpVec3(channel->Scalings,  animationTimeTicks);
			glm::quat rotation    = InterpQuat(channel->Rotations, animationTimeTicks);
			glm::vec3 translation = InterpVec3(channel->Positions, animationTimeTicks);

			if (disableRootMotion && isRootNode)
				translation = glm::vec3(0.0f);

			glm::mat4 scale = glm::scale(glm::mat4(1.0f), scaling);
			glm::mat4 rot   = glm::toMat4(rotation);
			glm::mat4 trans = glm::translate(glm::mat4(1.0f), translation);
			nodeTransform = trans * rot * scale; // TRS
		}

		// Optional pose override: extra local rotation at this joint (anim-0 path only).
		if (!m_BonePoseOverrides.empty())
		{
			auto it = m_BonePoseOverrides.find(nodeName);
			if (it != m_BonePoseOverrides.end())
				nodeTransform = nodeTransform * it->second;
		}

		glm::mat4 globalTransform = parentTransform * nodeTransform;

		auto boneIt = m_BoneNameToIndexMap.find(nodeName);
		if (boneIt != m_BoneNameToIndexMap.end())
		{
			int boneIndex = boneIt->second;
			m_BoneInfo[boneIndex].GlobalTransform     = globalTransform;
			m_BoneInfo[boneIndex].FinalTransformation =
				m_GlobalInverseTransform * globalTransform * m_BoneInfo[boneIndex].OffsetMatrix;
		}

		for (int child : node.Children)
			ReadNodeHierarchy(animationTimeTicks, child, globalTransform, disableRootMotion);
	}

	void Skeleton::GetBoneTransformsBlended(float timeInSeconds, std::vector<glm::mat4>& blendedTransforms,
		uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		const Animation* startAnim = GetAnim(startAnimIndex);
		const Animation* endAnim   = GetAnim(endAnimIndex);

		if (!startAnim || !endAnim || m_Nodes.empty())
		{
			HMN_CORE_WARN("Skeleton: Invalid animations for blending");
			blendedTransforms.resize(m_BoneInfo.size(), glm::mat4(1.0f));
			return;
		}

		float startAnimTimeTicks = CalcAnimationTimeTicks(timeInSeconds, startAnimIndex);
		float endAnimTimeTicks   = CalcAnimationTimeTicks(timeInSeconds, endAnimIndex);

		ReadNodeHierarchyBlended(startAnimTimeTicks, endAnimTimeTicks, 0, glm::mat4(1.0f),
			*startAnim, *endAnim, blendFactor, disableRootMotion);

		blendedTransforms.resize(m_BoneInfo.size());
		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
			blendedTransforms[i] = m_BoneInfo[i].FinalTransformation;
	}

	void Skeleton::ReadNodeHierarchyBlended(float startAnimTimeTicks, float endAnimTimeTicks,
		int nodeIndex, const glm::mat4& parentTransform,
		const Animation& startAnim, const Animation& endAnim,
		float blendFactor, bool disableRootMotion)
	{
		const SkeletonNode& node = m_Nodes[nodeIndex];
		const std::string& nodeName = node.Name;

		glm::mat4 nodeTransformation = node.LocalTransform;
		bool isRootNode = (parentTransform == glm::mat4(1.0f));

		const AnimChannel* startChannel = FindChannel(startAnim, nodeName);
		const AnimChannel* endChannel   = FindChannel(endAnim,   nodeName);

		HMN_CORE_ASSERT((startChannel && endChannel) || (!startChannel && !endChannel),
			"Node {} has animation in only one of the blended clips - not supported", nodeName.c_str());

		if (startChannel && endChannel)
		{
			LocalTransform startT{}, endT{};
			CalcLocalTransform(startT, startAnimTimeTicks, *startChannel);
			CalcLocalTransform(endT,   endAnimTimeTicks,   *endChannel);

			glm::vec3 blendedScaling = (1.0f - blendFactor) * startT.Scaling + endT.Scaling * blendFactor;
			glm::mat4 scalingM = glm::scale(glm::mat4(1.0f), blendedScaling);

			// 2-way path does not re-normalise
			glm::quat blendedRot = SlerpShortest(startT.Rotation, endT.Rotation, blendFactor);
			glm::mat4 rotationM = glm::toMat4(blendedRot);

			glm::vec3 blendedTranslation = (1.0f - blendFactor) * startT.Translation + endT.Translation * blendFactor;
			if (disableRootMotion && isRootNode)
				blendedTranslation = glm::vec3(0.0f);

			glm::mat4 translationM = glm::translate(glm::mat4(1.0f), blendedTranslation);
			nodeTransformation = translationM * rotationM * scalingM;
		}

		glm::mat4 globalTransform = parentTransform * nodeTransformation;

		auto boneIt = m_BoneNameToIndexMap.find(nodeName);
		if (boneIt != m_BoneNameToIndexMap.end())
		{
			int boneIndex = boneIt->second;
			m_BoneInfo[boneIndex].GlobalTransform     = globalTransform;
			m_BoneInfo[boneIndex].FinalTransformation =
				m_GlobalInverseTransform * globalTransform * m_BoneInfo[boneIndex].OffsetMatrix;
		}

		for (int child : node.Children)
			ReadNodeHierarchyBlended(startAnimTimeTicks, endAnimTimeTicks, child, globalTransform,
				startAnim, endAnim, blendFactor, disableRootMotion);
	}

	void Skeleton::GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
		std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		if (samples.empty())
		{
			transforms.resize(m_BoneInfo.size(), glm::mat4(1.f));
			return;
		}

		if (samples.size() == 1)
		{
			GetBoneTransformsBlended(samples[0].time, transforms,
				samples[0].animIndex, samples[0].animIndex, 0.f, disableRootMotion);
			return;
		}

		if (samples.size() == 2)
		{
			float total = samples[0].weight + samples[1].weight;
			float factor = total > 0.f ? samples[1].weight / total : 0.f;
			GetBoneTransformsBlended(samples[0].time, transforms,
				samples[0].animIndex, samples[1].animIndex, factor, disableRootMotion);
			return;
		}

		float totalWeight = 0.f;
		for (auto& s : samples) totalWeight += s.weight;
		if (totalWeight <= 0.f || m_Nodes.empty())
		{
			transforms.resize(m_BoneInfo.size(), glm::mat4(1.f));
			return;
		}

		std::vector<std::pair<const Animation*, float>> animsAndTimes;
		std::vector<float> normWeights;
		animsAndTimes.reserve(samples.size());
		normWeights.reserve(samples.size());

		for (auto& s : samples)
		{
			const Animation* anim = GetAnim(s.animIndex);
			if (!anim) continue;
			animsAndTimes.push_back({ anim, CalcAnimationTimeTicks(s.time, s.animIndex) });
			normWeights.push_back(s.weight / totalWeight);
		}

		ReadNodeHierarchyBlendedN(animsAndTimes, normWeights, 0, glm::mat4(1.f), disableRootMotion);

		transforms.resize(m_BoneInfo.size());
		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
			transforms[i] = m_BoneInfo[i].FinalTransformation;
	}

	void Skeleton::ReadNodeHierarchyBlendedN(
		const std::vector<std::pair<const Animation*, float>>& animsAndTimes,
		const std::vector<float>& normWeights,
		int nodeIndex, const glm::mat4& parentTransform, bool disableRootMotion)
	{
		const SkeletonNode& node = m_Nodes[nodeIndex];
		const std::string& nodeName = node.Name;

		glm::mat4 nodeTransformation = node.LocalTransform;
		bool isRootNode = (parentTransform == glm::mat4(1.f));

		glm::vec3 blendedScale{ 1.f };
		glm::quat blendedRot{ 1.f, 0.f, 0.f, 0.f };
		glm::vec3 blendedTranslation{ 0.f };
		float     accumWeight = 0.f;

		for (size_t i = 0; i < animsAndTimes.size(); i++)
		{
			const AnimChannel* channel = FindChannel(*animsAndTimes[i].first, nodeName);
			if (!channel) continue;

			LocalTransform t;
			CalcLocalTransform(t, animsAndTimes[i].second, *channel);

			float w = normWeights[i];
			if (accumWeight == 0.f)
			{
				blendedScale       = t.Scaling;
				blendedRot         = t.Rotation;
				blendedTranslation = t.Translation;
				accumWeight        = w;
			}
			else
			{
				float f = w / (accumWeight + w);
				blendedScale       = (1.f - f) * blendedScale       + f * t.Scaling;
				blendedTranslation = (1.f - f) * blendedTranslation + f * t.Translation;
				blendedRot         = glm::normalize(SlerpShortest(blendedRot, t.Rotation, f));
				accumWeight       += w;
			}
		}

		if (accumWeight > 0.f)
		{
			if (disableRootMotion && isRootNode)
				blendedTranslation = glm::vec3(0.f);

			glm::mat4 S = glm::scale(glm::mat4(1.f), blendedScale);
			glm::mat4 R = glm::toMat4(blendedRot);
			glm::mat4 T = glm::translate(glm::mat4(1.f), blendedTranslation);
			nodeTransformation = T * R * S;
		}

		glm::mat4 globalTransform = parentTransform * nodeTransformation;

		auto boneIt = m_BoneNameToIndexMap.find(nodeName);
		if (boneIt != m_BoneNameToIndexMap.end())
		{
			int boneIndex = boneIt->second;
			m_BoneInfo[boneIndex].GlobalTransform     = globalTransform;
			m_BoneInfo[boneIndex].FinalTransformation =
				m_GlobalInverseTransform * globalTransform * m_BoneInfo[boneIndex].OffsetMatrix;
		}

		for (int child : node.Children)
			ReadNodeHierarchyBlendedN(animsAndTimes, normWeights, child, globalTransform, disableRootMotion);
	}

}
