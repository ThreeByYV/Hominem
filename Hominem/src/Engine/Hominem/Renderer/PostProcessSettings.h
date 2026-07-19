#pragma once
#include "RenderSettings.h"

namespace Hominem {

struct PostProcessSettings
{
	bool  bloomEnabled       = false;
	bool  toneMappingEnabled = false;
	float bloomStrength      = 1.0f;
	float bloomThreshold     = 0.8f;
	float renderScale        = RenderSettings::RecommendedRenderScale;;
	bool  debugLights        = false;
	float bottomBarFraction  = 0.f;
};

} // namespace Hominem
