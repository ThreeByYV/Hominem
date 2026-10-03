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

/// Poses the actor's current clip, then looks for, in order: a bone named like "camera",
/// the eye bones' midpoint, the head bone. Facing comes from a left/right eye or shoulder pair,
/// else the hands. Null when the rig has no camera/eye/head bone.
std::optional<ViewModelFit> FitViewModel(SkinnedMeshActor& actor);

}
