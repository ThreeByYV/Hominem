#pragma once

#include <optional>
#include <string>
#include <glm/glm.hpp>

namespace Hominem {

class SkinnedMeshActor;

/// Where a first-person rig sits under the camera node so its eyes are on the camera and it
/// faces down the view (-Z). Found from the rig's own bones, so any arms/weapon model works.
struct ViewModelFit
{
    glm::vec3   Position{ 0.f };  // local to the camera node
    glm::vec3   Rotation{ 0.f };  // radians, local to the camera node
    std::string EyeSource;        // which bones the eye point came from, for logging
    std::string FacingSource;     // which bones the facing came from
};

/// Metres the camera sits behind the eyes (head centre). A camera bone overrides it.
inline constexpr float k_ViewModelEyeToCamera = 0.14f;

/// Eye point from a camera bone, else the eye bones, else the head; facing from a left/right
/// pair, else the hands. Null when the rig has none of those.
std::optional<ViewModelFit> FitViewModel(SkinnedMeshActor& actor, float eyeToCamera = k_ViewModelEyeToCamera);

}
