#include "hmnpch.h"
#include "ViewModel.h"

#include "Hominem/Scene/Actors/SkinnedMeshActor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

namespace {

std::string Lower(std::string s)
{
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool Contains(const std::string& name, std::string_view part) { return name.find(part) != std::string::npos; }

// Which side a lowercase bone name is on: 'l', 'r' or 0. Covers left/right, .l/.r, _l/_r.
char Side(const std::string& n)
{
    if (Contains(n, "left"))  return 'l';
    if (Contains(n, "right")) return 'r';
    for (size_t i = 1; i < n.size(); i++)
    {
        const bool separator = n[i - 1] == '.' || n[i - 1] == '_' || n[i - 1] == '-';
        const bool ends      = i + 1 == n.size() || !std::isalpha((unsigned char)n[i + 1]);
        if (separator && ends && (n[i] == 'l' || n[i] == 'r')) return n[i];
    }
    return 0;
}

// Average mesh-space position of the bones whose lowercase name passes `match`.
std::optional<glm::vec3> Average(const SkinnedMesh& mesh, const std::vector<std::string>& names,
                                 const auto& match)
{
    glm::vec3 sum{ 0.f };
    int       count = 0;
    for (const auto& name : names)
    {
        if (!match(Lower(name))) continue;
        if (auto m = mesh.GetBoneWorldTransform(name))
        {
            sum += glm::vec3((*m)[3]);
            count++;
        }
    }
    if (count == 0) return std::nullopt;
    return sum / (float)count;
}

}

std::optional<ViewModelFit> FitViewModel(SkinnedMeshActor& actor)
{
    if (!actor.Mesh || !actor.Mesh->HasSkeleton()) return std::nullopt;
    SkinnedMesh& mesh = *actor.Mesh;

    // Bone positions are only known for a posed skeleton.
    std::vector<glm::mat4> pose;
    actor.SamplePose(pose);
    const auto names = mesh.GetBoneNames();

    // Rigs carry many eye-area bones (lids, brows, lashes); the eyeballs themselves are enough.
    auto isEye = [](const std::string& n)
    {
        return Contains(n, "eye") && !Contains(n, "lid") && !Contains(n, "brow") && !Contains(n, "lash");
    };

    ViewModelFit fit;
    std::optional<glm::vec3> eye = Average(mesh, names, [](const std::string& n) { return Contains(n, "camera"); });
    fit.EyeSource = "camera bone";
    if (!eye)
    {
        eye           = Average(mesh, names, isEye);
        fit.EyeSource = "eye bones";
    }
    if (!eye)
    {
        eye = Average(mesh, names, [](const std::string& n) { return Contains(n, "head"); });
        fit.EyeSource = "head bone";
    }
    if (!eye) return std::nullopt;

    // Facing, kept flat so pitch stays the camera's. Best from a left/right pair, whose line
    // is the rig's sideways axis (.L is the rig's own left); otherwise toward the hands.
    auto flat    = [](glm::vec3 v) { v.y = 0.f; return v; };
    auto isHand  = [](const std::string& n) { return Contains(n, "hand") && !Contains(n, "handle"); };
    auto fromPair = [&](const auto& isPart) -> std::optional<glm::vec3>
    {
        const auto left  = Average(mesh, names, [&](const std::string& n) { return isPart(n) && Side(n) == 'l'; });
        const auto right = Average(mesh, names, [&](const std::string& n) { return isPart(n) && Side(n) == 'r'; });
        if (!left || !right) return std::nullopt;
        const glm::vec3 side = flat(*right - *left);
        if (glm::dot(side, side) < 1e-8f) return std::nullopt;
        return glm::normalize(glm::cross(glm::vec3(0.f, 1.f, 0.f), side));
    };

    glm::vec3 forward{ 0.f, 0.f, -1.f };
    fit.FacingSource = "default -Z";
    if (auto f = fromPair(isEye))
        forward = *f, fit.FacingSource = "eye pair";
    else if (auto f2 = fromPair([](const std::string& n) { return Contains(n, "shoulder"); }))
        forward = *f2, fit.FacingSource = "shoulder pair";
    else if (auto hands = Average(mesh, names, isHand); hands && glm::dot(flat(*hands - *eye), flat(*hands - *eye)) > 1e-6f)
        forward = glm::normalize(flat(*hands - *eye)), fit.FacingSource = "hands";

    // Yaw that turns `forward` onto -Z, then the offset that puts the eye on the origin.
    const float     yaw = std::atan2(forward.x, -forward.z);
    const glm::quat rot = glm::quat(glm::vec3(0.f, yaw, 0.f));
    fit.Rotation = { 0.f, yaw, 0.f };
    fit.Position = -(rot * *eye);
    return fit;
}

}
