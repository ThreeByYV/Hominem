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
			auto mesh = Hominem::StaticMesh::Create();
			if (auto res = mesh->LoadFromFile(meshPath); !res)
			{
				HMN_CORE_ERROR("{}", res.error());
				return nullptr;
			}
			HMN_CORE_INFO("CutscenePreload: '{}' ready", meshPath);
			return mesh;
		});
	}

	Hominem::Ref<Hominem::StaticMesh> TryGet() { return s_Load.TryGet(); }
}
