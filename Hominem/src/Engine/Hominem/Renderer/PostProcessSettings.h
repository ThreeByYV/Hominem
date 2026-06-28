#pragma once

namespace Hominem {

struct PostProcessSettings
{
	bool  bloomEnabled       = false;
	bool  toneMappingEnabled = false;
	float bloomStrength      = 1.0f;
	float bloomThreshold     = 0.8f;
	float renderScale        = 1.0f;
	bool  debugLights        = false;
};

} // namespace Hominem
