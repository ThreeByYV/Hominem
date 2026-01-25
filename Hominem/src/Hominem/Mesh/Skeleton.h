#pragma once

#include <map>
#include <vector>
#include <string>

#include <assimp/scene.h>
#include <glm/glm.hpp>

namespace Hominem {

#define MAX_NUM_BONES_PER_VERTEX 4
#define MAX_BONES 100

	/**
	 * @brief Per-vertex bone influence data.
	 *
	 * Each vertex can be influenced by up to MAX_NUM_BONES_PER_VERTEX bones.
	 * The shader blends bone transforms using these weights:
	 *
	 *   finalPos = (Bone[ID0] * Weight0 + Bone[ID1] * Weight1 + ...) * vertexPos
	 *
	 * Weights should sum to 1.0 for proper blending.
	 */
	struct VertexBoneData
	{
		int   BoneIDs[MAX_NUM_BONES_PER_VERTEX];  ///< Indices into the bone transform array
		float Weights[MAX_NUM_BONES_PER_VERTEX];  ///< Influence weight for each bone (0.0 - 1.0)

		VertexBoneData();

		/**
		 * @brief Assigns a bone influence to this vertex.
		 *
		 * Finds the first unused slot (weight == 0) and stores the bone ID and weight.
		 * Asserts if more than MAX_NUM_BONES_PER_VERTEX bones try to influence one vertex.
		 */
		void AddBoneData(int boneID, float weight);
	};

	/**
	 * @brief Stores matrices needed for bone transform computation.
	 *
	 * - **OffsetMatrix**: Transforms from mesh space to this bone's local space.
	 *   Baked at export time. Represents "where is this bone relative to mesh origin
	 *   in the bind pose?"
	 *
	 * - **FinalTransformation**: Computed each frame. This is what gets uploaded
	 *   to the shader. Transforms vertices from bind pose to animated pose.
	 */
	struct BoneInfo
	{
		glm::mat4 OffsetMatrix;
		glm::mat4 FinalTransformation;

		BoneInfo(const glm::mat4& offset);
	};

	/**
	 * @brief Manages bone hierarchy and computes animated transforms.
	 *
	 * ## Overview
	 *
	 * The Skeleton class handles all bone-related operations:
	 * 1. **Parsing**: Extracts bone data from Assimp scene during mesh load
	 * 2. **Animation**: Interpolates keyframes and walks bone hierarchy each frame
	 * 3. **Transform Output**: Provides final bone matrices ready for shader upload
	 *
	 * ## The Bone Transform Equation
	 *
	 * The core equation computed in ReadNodeHierarchy():
	 *
	 *   FinalTransform = GlobalInverse * GlobalBoneTransform * OffsetMatrix
	 *
	 * Where:
	 * - **OffsetMatrix**: Transforms vertex from mesh space to bone's local space (bind pose)
	 * - **GlobalBoneTransform**: Bone's animated world transform (includes all parent bones)
	 * - **GlobalInverse**: Inverse of scene root, brings result back to mesh space
	 *
	 * ## Usage
	 *
	 * @code
	 * Skeleton skeleton;
	 * skeleton.ParseFromScene(pScene, meshBaseVertices, vertexBoneData);
	 *
	 * // Each frame:
	 * std::vector<glm::mat4> transforms;
	 * skeleton.GetBoneTransforms(elapsedTime, transforms);
	 * // Upload transforms to shader...
	 * @endcode
	 */
	class Skeleton
	{
	public:
		Skeleton() = default;

		/**
		 * @brief Extracts bone data from an Assimp scene.
		 *
		 * @param pScene The loaded Assimp scene (kept alive by caller)
		 * @param meshBaseVertices Starting vertex index for each submesh
		 * @param[out] vertexBones Per-vertex bone influence data (resized and filled)
		 */
		void ParseFromScene(const aiScene* pScene,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		/**
		 * @brief Computes bone transforms for the current animation frame.
		 *
		 * @param animationTimeSec Time since animation start (seconds)
		 * @param[out] transforms Filled with one mat4 per bone, ready for shader upload
		 */
		void GetBoneTransforms(float animationTimeSec, std::vector<glm::mat4>& transforms);

		int GetNumBones() const { return static_cast<int>(m_BoneInfo.size()); }
		bool HasBones() const { return !m_BoneInfo.empty(); }

		/// @brief Sets the scene pointer (needed for animation data access)
		void SetScene(const aiScene* pScene) { m_pScene = pScene; }

	private:
		// --- Bone Parsing ---
		void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone,
			const std::vector<uint32_t>& meshBaseVertices,
			std::vector<VertexBoneData>& vertexBones);

		int GetBoneId(const aiBone* pBone);

		// --- Animation (each frame) ---
		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);

		void CalcInterpolatedScaling(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedTranslation(aiVector3D& out, float animationTimeTicks, const aiNodeAnim* pNodeAnim);

		/**
		 * @brief Recursively walks bone hierarchy computing transforms.
		 *
		 * For each node in the scene tree:
		 * 1. Get node's local transform (from animation keyframes if animated, else default)
		 * 2. Multiply by parent's global transform to get this node's global transform
		 * 3. If this node is a bone, compute: GlobalInverse * GlobalTransform * Offset
		 * 4. Recurse to children
		 */
		void ReadNodeHierarchy(float animationTimeTicks, const aiNode* pNode, const glm::mat4& parentTransform);

	private:
		/// Maps bone name (from file) to index in m_BoneInfo
		std::map<std::string, int> m_BoneNameToIndexMap;

		/// Offset and final transform for each bone
		std::vector<BoneInfo> m_BoneInfo;

		/// Inverse of scene root transform. Needed to bring animated vertices back to mesh space.
		glm::mat4 m_GlobalInverseTransform{ 1.0f };

		/// Pointer to Assimp scene (not owned, kept alive by SkinnedMesh)
		const aiScene* m_pScene = nullptr;
	};

}
