#pragma once

#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Core/KeyCodes.h"

#include <algorithm>

namespace Hominem {

inline bool HandleDebugKey(int keyCode, Scene& scene, bool& showPerfPanel)
{
    PostProcessSettings& pp = scene.GetPostProcess();

    switch (keyCode)
    {
        case HMN_KEY_N: RenderSettings::DrawNormals     = !RenderSettings::DrawNormals;     return true;
        case HMN_KEY_B: RenderSettings::DrawAABB        = !RenderSettings::DrawAABB;        return true;
        case HMN_KEY_L: pp.debugLights                  = !pp.debugLights;                 return true;
        case HMN_KEY_H: RenderSettings::DebugHeatmap    = !RenderSettings::DebugHeatmap;    return true;
        case HMN_KEY_O: RenderSettings::DrawBoneWeights = !RenderSettings::DrawBoneWeights; return true;

        // Swapping the shading model repaints every pixel, so both things derived from
        // the old look have to go: the accumulated TAA history, and the baked cubemap,
        // which was captured through whichever mesh shaders were active at bake time.
        case HMN_KEY_T:
            RenderSettings::ToonShading = !RenderSettings::ToonShading;
            RenderSettings::RequestTAAHistoryReset();
            scene.RebakeEnvironment();
            return true;

        case HMN_KEY_LEFT_BRACKET:
            if (RenderSettings::DrawBoneWeights)
                RenderSettings::DisplayBoneIndex = std::max(0, RenderSettings::DisplayBoneIndex - 1);
            return true;

        case HMN_KEY_RIGHT_BRACKET:
            if (RenderSettings::DrawBoneWeights)
                RenderSettings::DisplayBoneIndex++;
            return true;

        case HMN_KEY_P:
            showPerfPanel = !showPerfPanel;
            return true;

        default:
            return false;
    }
}

}
