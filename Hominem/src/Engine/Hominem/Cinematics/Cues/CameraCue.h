#pragma once

#include "Hominem/Cinematics/Cue.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/SkinnedMesh.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <string>

namespace Hominem {

	/**
	 * @brief Lerps the scene camera's position and orthographic size over the cue's duration.
	 *
	 * Uses a smoothstep ease (cubic Hermite) so zooms feel organic without needing a curve asset.
	 *
	 * Bone sockets: set FromBone / ToBone to a bone name and point ctx.targetMesh at the
	 * SkinnedMesh that owns the skeleton. The positions are resolved once at OnEnter (a
	 * snapshot of the animated pose at the moment the cue fires).
	 * If the mesh has no skeleton or the bone name is not found, the cue silently falls
	 * back to the authored FromPos / ToPos — so a player with no rig just uses those values.
	 */
	class CameraCue : public Cue
	{
	public:
		CameraCue() = default;
		CameraCue(glm::vec3 fromPos, glm::vec3 toPos, float fromOrtho, float toOrtho)
			: FromPos(fromPos), ToPos(toPos), FromOrthoSize(fromOrtho), ToOrthoSize(toOrtho)
		{
		}

		const char* TypeName() const override { return "camera"; }

		void OnEnter(CutsceneContext& ctx) override
		{
			m_ResolvedFrom = FromPos;
			m_ResolvedTo   = ToPos;

			if (ctx.targetMesh)
			{
				if (!FromBone.empty())
					if (auto mat = ctx.targetMesh->GetBoneWorldTransform(FromBone))
						m_ResolvedFrom = glm::vec3((*mat)[3]);

				if (!ToBone.empty())
					if (auto mat = ctx.targetMesh->GetBoneWorldTransform(ToBone))
						m_ResolvedTo = glm::vec3((*mat)[3]);
			}
		}

		void OnTick(CutsceneContext& ctx, float localT) override
		{
			if (!ctx.scene)
				return;

			const float n = Duration > 0.f ? std::clamp(localT / Duration, 0.f, 1.f) : 1.f;
			const float t = n * n * (3.f - 2.f * n); // smoothstep

			ctx.scene->GetCameraPosition() = glm::mix(m_ResolvedFrom, m_ResolvedTo, t);
			ctx.scene->GetCamera().SetOrthographicSize(
				FromOrthoSize + (ToOrthoSize - FromOrthoSize) * t);
		}

		glm::vec3   FromPos      { 0.f };
		glm::vec3   ToPos        { 0.f };
		float       FromOrthoSize = 2.f;
		float       ToOrthoSize   = 2.f;

		/// Bone socket names. When set, the position is sampled from ctx.targetMesh at OnEnter.
		/// Leave empty to use FromPos / ToPos directly (e.g. player with no rig).
		std::string FromBone;
		std::string ToBone;

	private:
		glm::vec3 m_ResolvedFrom{ 0.f };
		glm::vec3 m_ResolvedTo  { 0.f };
	};

}
