#include "hmnpch.h"
#include "CutscenePreload.h"

#include "Hominem/Core/AsyncLoad.h"

namespace CutscenePreload
{
	namespace { Hominem::AsyncLoad<Hominem::StaticMesh> s_Load; }

	void Begin(const std::string& meshPath)
	{
		HMN_CORE_INFO("CutscenePreload: loading '{}' on a background thread", meshPath);
		s_Load.Begin([meshPath]() -> Hominem::Ref<Hominem::StaticMesh>
		{
			auto mesh = Hominem::CreateRef<Hominem::StaticMesh>();
			if (!mesh->LoadFromFile(meshPath))
			{
				HMN_CORE_ERROR("CutscenePreload: failed to load '{}'", meshPath);
				return nullptr;
			}
			HMN_CORE_INFO("CutscenePreload: '{}' ready", meshPath);
			return mesh;
		});
	}

	Hominem::Ref<Hominem::StaticMesh> TryGet() { return s_Load.TryGet(); }
}
