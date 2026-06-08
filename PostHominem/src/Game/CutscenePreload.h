#pragma once

#include "Hominem/Renderer/StaticMesh.h"
#include <string>

namespace CutscenePreload
{
	/// Idempotent - only the first call starts the load.
	void Begin(const std::string& meshPath);

	/// Non-blocking. Returns the loaded mesh once the background load completes, cached
	Hominem::Ref<Hominem::StaticMesh> TryGet();
}
