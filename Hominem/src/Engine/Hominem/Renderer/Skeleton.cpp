#include "hmnpch.h"
#include "Skeleton.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Hominem/Utils/Renderer.h"

namespace Hominem {

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

	void Skeleton::ParseFromScene(const aiScene* pScene,
		const std::vector<uint32_t>& meshBaseVertices,
		std::vector<VertexBoneData>& vertexBones)
	{
		m_pScene = pScene;

		// Store the global inverse transform for animation math
		m_GlobalInverseTransform = glm::inverse(AiToGlm(pScene->mRootNode->mTransformation));


		// Parse bones from each mesh
		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];
			if (pMesh->HasBones())
				ParseMeshBones(i, pMesh, meshBaseVertices, vertexBones);
		}

		HMN_CORE_INFO("Skeleton: {} bones parsed", m_BoneInfo.size());
	}

	void Skeleton::ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh,
		const std::vector<uint32_t>& meshBaseVertices,
		std::vector<VertexBoneData>& vertexBones)
	{
		for (uint32_t i = 0; i < pMesh->mNumBones; ++i)
			ParseSingleBone(meshIndex, pMesh->mBones[i], meshBaseVertices, vertexBones);
	}

	/**
	 * Processes one bone from the mesh file:
	 * 1. Assigns a unique ID to this bone (or retrieves existing ID)
	 * 2. Stores the bone's offset matrix (mesh space -> bone local space)
	 * 3. Records which vertices this bone influences and by how much
	 */
	void Skeleton::ParseSingleBone(uint32_t meshIdx, const aiBone* pBone,
		const std::vector<uint32_t>& meshBaseVertices,
		std::vector<VertexBoneData>& vertexBones)
	{
		if (!pBone) return;

		int boneID = GetBoneId(pBone);

		// First time seeing this bone - store its offset matrix
		if (boneID == static_cast<int>(m_BoneInfo.size()))
		{
			// IMPORTANT: Assimp uses row-major matrices, GLM uses column-major.
			// AiToGlm handles the transpose during conversion.
			// See Renderer.h for detailed explanation.
			glm::mat4 offsetMatrix = AiToGlm(pBone->mOffsetMatrix);
			m_BoneInfo.push_back(BoneInfo(offsetMatrix));
		}

		// Record vertex weights for this bone
		for (uint32_t i = 0; i < pBone->mNumWeights; ++i)
		{
			const aiVertexWeight& vw = pBone->mWeights[i];

			// Vertex IDs in aiBone are local to the submesh.
			// Add base vertex offset to get global vertex ID.
			uint32_t globalVertexId = meshBaseVertices[meshIdx] + vw.mVertexId;

			if (globalVertexId >= vertexBones.size())
			{
				HMN_CORE_ERROR("Vertex ID {} out of range (vertexBones.size={} meshIdx={} bone={})",
					globalVertexId, vertexBones.size(), meshIdx, pBone->mName.C_Str());
				continue;
			}

			vertexBones[globalVertexId].AddBoneData(boneID, vw.mWeight);
		}
	}

	/**
	 * Returns a unique ID for this bone, creating a new entry if first encounter.
	 * Bone names come from the 3D modeling software (e.g., "Spine", "LeftArm").
	 */
	int Skeleton::GetBoneId(const aiBone* pBone)
	{
		int boneID = 0;
		std::string boneName{ pBone->mName.C_Str() };

		if (m_BoneNameToIndexMap.find(boneName) == m_BoneNameToIndexMap.end())
		{
			boneID = static_cast<int>(m_BoneNameToIndexMap.size());
			m_BoneNameToIndexMap[boneName] = boneID;
		}
		else
		{
			boneID = m_BoneNameToIndexMap[boneName];
		}

		return boneID;
	}

	/**
	 * Animation update pipeline:
	 * 1. Convert time from seconds to animation ticks
	 * 2. Loop animation using fmod
	 * 3. Recursively walk scene hierarchy computing bone transforms
	 * 4. Copy final transforms to output vector
	 */
	void Skeleton::GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms, bool disableRootMotion)
	{
		if (!m_pScene || !m_pScene->mAnimations || m_pScene->mNumAnimations == 0)
		{
			HMN_CORE_WARN("Skeleton: No animations available");
			transforms.resize(m_BoneInfo.size(), glm::mat4(1.0f));
			return;
		}

		glm::mat4 identity = glm::mat4(1.0f);

		// Animations use "ticks" as time units, not seconds.
		// ticksPerSecond tells us how to convert.
		float ticksPerSecond = static_cast<float>(m_pScene->mAnimations[0]->mTicksPerSecond != 0
			? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f);
		float timeInTicks = animationTimeSec * ticksPerSecond;

		// Loop the animation
		float animationTimeTicks = fmod(timeInTicks, static_cast<float>(m_pScene->mAnimations[0]->mDuration));

		// This is where the magic happens - recursively compute all bone transforms
		ReadNodeHierarchy(animationTimeTicks, m_pScene->mRootNode, identity, disableRootMotion);

		// Copy results for shader upload
		transforms.resize(m_BoneInfo.size());

		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
			transforms[i] = m_BoneInfo[i].FinalTransformation;
	}

	const aiScene* Skeleton::GetSceneForAnimIndex(uint32_t animIndex) const
	{
		// Animation 0 is always in the main scene
		if (animIndex == 0 && m_pScene)
			return m_pScene;

		// Additional animations (1, 2, 3...) are in additional scenes
		// Each additional scene contains 1 animation at index 0
		uint32_t additionalIndex = animIndex - 1;
		if (additionalIndex < m_AdditionalScenes.size())
			return m_AdditionalScenes[additionalIndex];

		HMN_CORE_WARN("Skeleton: Animation index {} not found", animIndex);
		return nullptr;
	}

	uint32_t Skeleton::GetAnimIndexInScene(uint32_t globalAnimIndex) const
	{
		// Animation 0 is at index 0 in main scene
		if (globalAnimIndex == 0)
			return 0;

		// Additional animations are always at index 0 in their respective scenes
		return 0;
	}

	float Skeleton::CalcAnimationTimeTicks(float timeInSeconds, uint32_t animationIndex)
	{
		const aiScene* pScene = GetSceneForAnimIndex(animationIndex);
		if (!pScene || !pScene->mAnimations || pScene->mNumAnimations == 0)
		{
			HMN_CORE_WARN("Skeleton: Invalid animation index {}", animationIndex);
			return 0.0f;
		}

		uint32_t sceneAnimIndex = GetAnimIndexInScene(animationIndex);
		const aiAnimation* pAnimation = pScene->mAnimations[sceneAnimIndex];
		float ticksPerSecond = static_cast<float>(pAnimation->mTicksPerSecond != 0
			? pAnimation->mTicksPerSecond : 25.0f);
		float timeInTicks = timeInSeconds * ticksPerSecond;
		float animationTimeTicks = fmod(timeInTicks, static_cast<float>(pAnimation->mDuration));

		return animationTimeTicks;
	}

	void Skeleton::GetBoneTransformsBlended(float timeInSeconds, std::vector<glm::mat4>& blendedTransforms, uint32_t startAnimIndex, uint32_t endAnimIndex, float blendFactor, bool disableRootMotion)
	{
		const aiScene* startScene = GetSceneForAnimIndex(startAnimIndex);
		const aiScene* endScene = GetSceneForAnimIndex(endAnimIndex);

		if (!startScene || !endScene)
		{
			HMN_CORE_WARN("Skeleton: Invalid scenes for animation blending");
			blendedTransforms.resize(m_BoneInfo.size(), glm::mat4(1.0f));
			return;
		}

		float startAnimTimeTicks = CalcAnimationTimeTicks(timeInSeconds, startAnimIndex);
		float endAnimTimeTicks = CalcAnimationTimeTicks(timeInSeconds, endAnimIndex);

		uint32_t startSceneAnimIndex = GetAnimIndexInScene(startAnimIndex);
		uint32_t endSceneAnimIndex = GetAnimIndexInScene(endAnimIndex);

		if (startSceneAnimIndex >= startScene->mNumAnimations || endSceneAnimIndex >= endScene->mNumAnimations)
		{
			HMN_CORE_WARN("Skeleton: Animation indices out of range");
			blendedTransforms.resize(m_BoneInfo.size(), glm::mat4(1.0f));
			return;
		}

		const aiAnimation* startAnimation = startScene->mAnimations[startSceneAnimIndex];
		const aiAnimation* endAnimation = endScene->mAnimations[endSceneAnimIndex];

		// Use the main scene's root node for hierarchy (both animations should have same skeleton structure)
		ReadNodeHierarchyBlended(startAnimTimeTicks, endAnimTimeTicks, m_pScene->mRootNode, glm::mat4(1.0f), *startAnimation, *endAnimation, blendFactor, disableRootMotion);

		blendedTransforms.resize(m_BoneInfo.size());

		for (uint32_t i = 0; i < m_BoneInfo.size(); i++)
		{
			blendedTransforms[i] = m_BoneInfo[i].FinalTransformation;
		}
	}

	/**
	 * Core animation function. Recursively processes the scene tree.
	 *
	 * For each node:
	 * 1. Start with node's default transform (from bind pose)
	 * 2. If this node is animated, replace with interpolated keyframe values
	 * 3. Multiply by parent's global transform to get this node's global transform
	 * 4. If this node is a bone, compute the final shader matrix:
	 *
	 *    FinalTransform = GlobalInverse * GlobalTransform * OffsetMatrix
	 *
	 *    - OffsetMatrix: Moves vertex from mesh space to bone's local space
	 *    - GlobalTransform: Bone's animated position in world space
	 *    - GlobalInverse: Brings result back to mesh space
	 *
	 * 5. Recurse to children, passing our global transform as their parent
	 */
	void Skeleton::ReadNodeHierarchy(float animationTimeTicks, const aiNode* pNode,
		const glm::mat4& parentTransform, bool disableRootMotion)
	{
		std::string nodeName{ pNode->mName.C_Str() };

		const aiAnimation* pAnimation = m_pScene->mAnimations[0];

		// Default: use the node's static transform from the bind pose
		glm::mat4 nodeTransform = AiToGlm(pNode->mTransformation);

		// Check if this is the root node (parent transform is identity)
		bool isRootNode = (parentTransform == glm::mat4(1.0f));

		// Check if this node has animation data
		const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, nodeName);

		if (pNodeAnim)
		{
			// This node is animated - interpolate keyframes to get current transform

			// Scale
			aiVector3D scaling;
			CalcInterpolatedScaling(scaling, animationTimeTicks, pNodeAnim);
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), AiToGlm(scaling));

			// Rotation (quaternion for smooth interpolation)
			aiQuaternion rotation;
			CalcInterpolatedRotation(rotation, animationTimeTicks, pNodeAnim);
			glm::mat4 rot = glm::toMat4(AiToGlm(rotation));

			// Translation
			aiVector3D translation;
			CalcInterpolatedTranslation(translation, animationTimeTicks, pNodeAnim);

			// If root motion is disabled and this is the root node, zero out translation
			if (disableRootMotion && isRootNode)
			{
				translation = aiVector3D(0.0f, 0.0f, 0.0f);
			}

			glm::mat4 trans = glm::translate(glm::mat4(1.0f), AiToGlm(translation));

			// Combine: Translation * Rotation * Scale (TRS order)
			nodeTransform = trans * rot * scale;
		}

		// Accumulate parent transforms to get global transform
		glm::mat4 globalTransform = parentTransform * nodeTransform;

		// If this node corresponds to a bone, compute final shader matrix
		if (m_BoneNameToIndexMap.contains(nodeName))
		{
			uint32_t boneIndex = m_BoneNameToIndexMap[nodeName];

			// THE CORE EQUATION:
			// FinalTransform = GlobalInverse * GlobalTransform * OffsetMatrix
			//
			// Reading right-to-left (how matrix multiplication works on vectors):
			// 1. OffsetMatrix: Transform vertex from mesh space to bone's local space
			// 2. GlobalTransform: Apply bone's animated world transform
			// 3. GlobalInverse: Transform back from world space to mesh space
			m_BoneInfo[boneIndex].FinalTransformation =
				m_GlobalInverseTransform * globalTransform * m_BoneInfo[boneIndex].OffsetMatrix;
		}

		// Recurse to children
		for (uint32_t i = 0; i < pNode->mNumChildren; i++)
			ReadNodeHierarchy(animationTimeTicks, pNode->mChildren[i], globalTransform, disableRootMotion);
	}

	void Skeleton::ReadNodeHierarchyBlended(float startAnimTimeTicks, float endAnimTimeTicks, const aiNode* pNode, const glm::mat4& parentTransform,
											const aiAnimation& startAnimation, const aiAnimation& endAnimation, float blendFactor, bool disableRootMotion)
	{
		std::string nodeName{ pNode->mName.C_Str() };
		glm::mat4 nodeTransformation{ AiToGlm(pNode->mTransformation) };

		// Check if this is the root node
		bool isRootNode = (parentTransform == glm::mat4(1.0f));

		const aiNodeAnim* pStartNodeAnim = FindNodeAnim(&startAnimation, nodeName);

		LocalTransform startTransform{};

		if (pStartNodeAnim)
		{
			CalcLocalTransform(startTransform, startAnimTimeTicks, pStartNodeAnim);
		}

		LocalTransform endTransform{};

		const aiNodeAnim* pEndNodeAnim = FindNodeAnim(&endAnimation, nodeName);

		HMN_CORE_ASSERT((pStartNodeAnim && pEndNodeAnim) || (!pStartNodeAnim && !pEndNodeAnim),
			"On the node {} there is an animation node for only one of the start/end animations. This is not supported!", nodeName.c_str());

		if (pEndNodeAnim)
		{
			CalcLocalTransform(endTransform, endAnimTimeTicks, pEndNodeAnim);
		}

		if (pStartNodeAnim && pEndNodeAnim)
		{
			// Interpolate scaling
			const auto& scale0 = startTransform.Scaling;
			const auto& scale1 = endTransform.Scaling;
			aiVector3D blendedScaling = (1.0f - blendFactor) * scale0 + scale1 * blendFactor;
			glm::mat4 scalingM = glm::scale(glm::mat4(1.0f), AiToGlm(blendedScaling));

			// Interpolate rotation
			const auto& rot0 = startTransform.Rotation;
			const auto& rot1 = endTransform.Rotation;
			aiQuaternion blendedRot{};
			aiQuaternion::Interpolate(blendedRot, rot0, rot1, blendFactor);
			glm::mat4 rotationM = glm::toMat4(AiToGlm(blendedRot));

			// Interpolate translation
			const auto& pos0 = startTransform.Translation;
			const auto& pos1 = endTransform.Translation;
			aiVector3D blendedTranslation = (1.0f - blendFactor) * pos0 + pos1 * blendFactor;

			// If root motion is disabled and this is the root node, zero out translation
			if (disableRootMotion && isRootNode)
			{
				blendedTranslation = aiVector3D(0.0f, 0.0f, 0.0f);
			}

			glm::mat4 translationM = glm::translate(glm::mat4(1.0f), AiToGlm(blendedTranslation));

			// Combine all (TRS order: Translation * Rotation * Scale)
			nodeTransformation = translationM * rotationM * scalingM;
		}

		glm::mat4 globalTransform = parentTransform * nodeTransformation;

		if (m_BoneNameToIndexMap.contains(nodeName))
		{
			uint32_t boneIndex = m_BoneNameToIndexMap[nodeName];
			m_BoneInfo[boneIndex].FinalTransformation = m_GlobalInverseTransform * globalTransform * m_BoneInfo[boneIndex].OffsetMatrix;
		}

		// Recurse to all children
		for (size_t i = 0; i < pNode->mNumChildren; i++)
		{
			ReadNodeHierarchyBlended(startAnimTimeTicks, endAnimTimeTicks, pNode->mChildren[i], globalTransform,
				startAnimation, endAnimation, blendFactor, disableRootMotion);
		}
	}

	void Skeleton::CalcLocalTransform(LocalTransform& localTransform, float animTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		CalcInterpolatedScaling(localTransform.Scaling, animTimeTicks, pNodeAnim);
		CalcInterpolatedRotation(localTransform.Rotation, animTimeTicks, pNodeAnim);
		CalcInterpolatedTranslation(localTransform.Translation, animTimeTicks, pNodeAnim);
	}

	/**
	 * Searches animation channels for one matching the given node name.
	 * Returns nullptr if the node is not animated.
	 */
	const aiNodeAnim* Skeleton::FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName)
	{
		for (uint32_t i = 0; i < pAnimation->mNumChannels; i++)
		{
			const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
			if (std::string{ pNodeAnim->mNodeName.C_Str() } == nodeName)
				return pNodeAnim;
		}
		return nullptr;
	}

	/**
	 * @brief Interpolates between animation keyframes to find the value at a specific time.
	 *
	 * Animation data is stored as discrete keyframes (e.g., position at t=0, t=0.5, t=1.0).
	 * This function finds which two keyframes surround the current time and linearly
	 * interpolates between them.
	 */
	// Works directly on Assimp's raw key arrays — no vector copy, no heap allocation.
	template<typename T, typename KeyType, typename Interpolator>
	static void CalcInterpolatedGeneric(
		T& out,
		float animationTimeTicks,
		const KeyType* keys,
		uint32_t numKeys,
		Interpolator interpolateFunc)
	{
		if (numKeys == 1)
		{
			out = keys[0].mValue;
			return;
		}

		uint32_t index = 0;
		for (uint32_t i = 0; i < numKeys - 1; i++)
		{
			if (animationTimeTicks < static_cast<float>(keys[i + 1].mTime))
			{
				index = i;
				break;
			}
		}

		uint32_t nextIndex = index + 1;
		float t1 = static_cast<float>(keys[index].mTime);
		float t2 = static_cast<float>(keys[nextIndex].mTime);
		float deltaTime = t2 - t1;
		float factor = glm::clamp((animationTimeTicks - t1) / deltaTime, 0.0f, 1.0f);

		interpolateFunc(out, keys[index].mValue, keys[nextIndex].mValue, factor);
	}

	void Skeleton::CalcInterpolatedScaling(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		CalcInterpolatedGeneric<aiVector3D>(
			out, animationTimeTicks,
			pNodeAnim->mScalingKeys, pNodeAnim->mNumScalingKeys,
			[](aiVector3D& result, const aiVector3D& start, const aiVector3D& end, float factor)
			{
				result = start + (end - start) * factor;
			}
		);
	}

	void Skeleton::CalcInterpolatedRotation(aiQuaternion& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		CalcInterpolatedGeneric<aiQuaternion>(
			out, animationTimeTicks,
			pNodeAnim->mRotationKeys, pNodeAnim->mNumRotationKeys,
			[](aiQuaternion& result, const aiQuaternion& start, const aiQuaternion& end, float factor)
			{
				aiQuaternion::Interpolate(result, start, end, factor);
				result.Normalize();
			}
		);
	}

	void Skeleton::CalcInterpolatedTranslation(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim)
	{
		CalcInterpolatedGeneric<aiVector3D>(
			out, animationTimeTicks,
			pNodeAnim->mPositionKeys, pNodeAnim->mNumPositionKeys,
			[](aiVector3D& result, const aiVector3D& start, const aiVector3D& end, float factor)
			{
				result = start + (end - start) * factor;
			}
		);
	}

}
