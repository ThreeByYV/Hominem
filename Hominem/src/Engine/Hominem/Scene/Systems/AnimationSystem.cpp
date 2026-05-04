 #include "hmnpch.h"
#include "AnimationSystem.h"
#include "Hominem/Scene/Components.h"

namespace Hominem {

	void AnimationSystem::OnUpdate(Timestep ts, entt::registry& registry)
	{
		HMN_PROFILE_FUNCTION();
		auto view = registry.view<SkinnedMeshComponent, AnimationComponent>();
		for (auto entity : view)
		{
			auto&& [meshComp, animComp] = view.get<SkinnedMeshComponent, AnimationComponent>(entity);

			if (!meshComp.Mesh || !meshComp.Mesh->HasSkeleton())
				continue;

			if (animComp.Playing)
				animComp.AnimationTime += ts * animComp.AnimationSpeed;

			if (animComp.UseBlending)
			{
				float target = (animComp.TargetAnimIndex == animComp.EndAnimIndex) ? 1.0f : 0.0f;
				float delta  = animComp.BlendSpeed * ts;
				if (glm::abs(animComp.BlendFactor - target) > 0.01f)
					animComp.BlendFactor = glm::clamp(animComp.BlendFactor + (animComp.BlendFactor < target ? delta : -delta), 0.0f, 1.0f);
				else
					animComp.BlendFactor = target;
			}

			m_BoneCache.clear();

			{
				HMN_PROFILE_SCOPE("GetBoneTransforms");
				if (animComp.UseBlending)
					meshComp.Mesh->GetBoneTransformsBlended(animComp.AnimationTime, m_BoneCache,
						animComp.StartAnimIndex, animComp.EndAnimIndex,
						animComp.BlendFactor, animComp.DisableRootMotion);
				else
					meshComp.Mesh->GetBoneTransforms(animComp.AnimationTime, m_BoneCache, animComp.DisableRootMotion);
			}

			{
				HMN_PROFILE_SCOPE("DispatchSkinning");
				meshComp.Mesh->DispatchSkinning(m_BoneCache);
			}
		}
	}

}
