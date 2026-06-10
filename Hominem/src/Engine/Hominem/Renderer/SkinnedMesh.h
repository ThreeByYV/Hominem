#pragma once

#include <vector>
#include <span>
#include <string>
#include <optional>

#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Shader.h"
#include "Hominem/Renderer/Material.h"
#include "Skeleton.h"

namespace Hominem {

	class SkinnedMesh : public RefCounted
	{
	public:
		~SkinnedMesh() override = default;

		[[nodiscard]] virtual std::expected<void, std::string> LoadFromFile(const std::string& filepath) = 0;
		[[nodiscard]] virtual std::expected<void, std::string> LoadAdditionalAnimation(const std::string& filepath) = 0;

		virtual void Render(const Ref<Shader>& shader) = 0;

		//todo: we should dispatch in constructor so we cant forget this call and remove then this method
		/// Upload bone matrices and dispatch GPU skinning compute shader.
		virtual void DispatchSkinning(std::span<const glm::mat4> bones) = 0;

		virtual void GetBoneTransforms(float timeSeconds, std::vector<glm::mat4>& transforms,
			bool disableRootMotion = false) = 0;

		virtual void GetBoneTransformsBlended(float timeSeconds, std::vector<glm::mat4>& transforms,
			uint32_t startAnimIndex, uint32_t endAnimIndex,
			float blendFactor, bool disableRootMotion = false) = 0;

		/// Blend any number of loaded animations by weight.
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

		/// @brief World-space transform of the named bone joint after the last animation update.
		/// Returns nullopt when the mesh has no skeleton or the bone name is not found —
		/// callers should fall back to a hardcoded position in that case.
		virtual std::optional<glm::mat4>  GetBoneWorldTransform(const std::string& name) const = 0;
		virtual std::vector<std::string>  GetBoneNames() const = 0;

		const Material& GetMaterial() const              { return m_Material; }
		void            SetMaterial(const Material& mat) { m_Material = mat; }

		static Ref<SkinnedMesh> Create();

	protected:
		Material m_Material;
	};

}
