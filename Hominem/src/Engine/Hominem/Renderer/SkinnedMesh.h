#pragma once

#include <vector>
#include <span>
#include <string>

#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Shader.h"
#include "Skeleton.h"

namespace Hominem {

	class SkinnedMesh
	{
	public:
		virtual ~SkinnedMesh() = default;

		virtual bool LoadFromFile(const std::string& filepath) = 0;
		virtual bool LoadAdditionalAnimation(const std::string& filepath) = 0;

		virtual void Render(const Ref<Shader>& shader) = 0;

		/// Upload bone matrices and dispatch GPU skinning compute shader.
		virtual void DispatchSkinning(std::span<const glm::mat4> bones) = 0;

		virtual void GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms,
			bool disableRootMotion = false) = 0;

		virtual void GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
			uint32_t startAnimIndex, uint32_t endAnimIndex,
			float blendFactor, bool disableRootMotion = false) = 0;

		/// Blend any number of loaded animations by weight.
		/// Each sample carries its own playback time so clips can run independently.
		virtual void GetBoneTransformsBlendedN(const std::vector<AnimBlendSample>& samples,
			std::vector<glm::mat4>& transforms, bool disableRootMotion = false) = 0;

		virtual void         SetShader(const Ref<Shader>& shader) = 0;
		virtual Ref<Shader>  GetShader() const = 0;

		virtual bool     HasSkeleton()       const = 0;
		virtual int      GetBoneCount()      const = 0;
		virtual uint32_t GetAnimationCount() const = 0;
		virtual uint32_t GetVertexCount()    const = 0;
		virtual uint32_t GetIndexCount()     const = 0;
		virtual uint32_t GetSubmeshCount()   const = 0;

		virtual Skeleton&       GetSkeleton()       = 0;
		virtual const Skeleton& GetSkeleton() const = 0;

		static Ref<SkinnedMesh> Create();
	};

}
