#include "hmnpch.h"
#include "ViewModelActor.h"

#include "Hominem/Scene/Scene.h"

#include <algorithm>

namespace Hominem {

namespace {

// What each state is called across rigs, tried in order.
constexpr std::array<std::array<const char*, 4>, (size_t)WeaponAnim::Count> k_ClipAliases =
{{
    { "Equip", "Draw" },
    { "Idle" },
    { "Walk" },
    { "Run", "Sprint" },
    { "Fire", "Shots", "Shoot", "Shot" },
    { "Reload" },
    { "Reload_Empty", "ReloadEmpty", "Reload Empty" },
    { "Inspect" },
    { "Unequip", "Holster" },
}};

// Seconds; Fire is short so shots stay instant.
constexpr float FadeInto(WeaponAnim a) { return a == WeaponAnim::Fire ? 0.05f : 0.15f; }

}

void ViewModelActor::OnCreate()
{
    m_Scene->SetParent(*this, &m_Scene->GetCameraNode());
}

void ViewModelActor::OnUpdate(Timestep ts)
{
    SkinnedMeshActor::OnUpdate(ts);
    if (!Mesh) return;

    if (Paused)
        AnimTime = PoseTime;
    else if (!HasClip(m_State))
        AnimTime = HeldTime();  // no clip for this loop state: hold the last pose
    else if (IsBusy() && IsClipFinished())
        Play(WeaponAnim::Idle);
}

std::optional<ViewModelFit> ViewModelActor::Equip(Ref<SkinnedMesh> mesh)
{
    Mesh  = std::move(mesh);
    Scale = glm::vec3(1.f);
    ResolveClips();

    auto fit = Fit();
    Play(WeaponAnim::Equip, 0.f);
    return fit;
}

std::optional<ViewModelFit> ViewModelActor::Fit()
{
    if (!Mesh) return std::nullopt;
    PoseAtRest();

    auto fit = FitViewModel(*this);
    Position = fit ? fit->Position : glm::vec3(0.f, 0.f, -1.f);
    Rotation = fit ? fit->Rotation : glm::vec3(0.f);
    Position.z += PullIn;
    return fit;
}

bool ViewModelActor::Play(WeaponAnim anim, float fade)
{
    const char* clip = m_Clips[(size_t)anim];
    if (fade < 0.f) fade = FadeInto(anim);
    if (!Mesh || !clip || !PlayAnimation(clip, fade))
    {
        if (IsLooping(anim)) m_State = anim;
        return false;
    }
    m_State  = anim;
    PoseTime = 0.f;
    return true;
}

void ViewModelActor::ResolveClips()
{
    for (size_t i = 0; i < (size_t)WeaponAnim::Count; i++)
    {
        m_Clips[i] = nullptr;
        for (const char* alias : k_ClipAliases[i])
            if (alias && Mesh->FindAnimation(alias)) { m_Clips[i] = alias; break; }
    }
}

void ViewModelActor::PoseAtRest()
{
    // Equip clips start off-screen, so fit on Idle, or Equip's last frame.
    if (!Play(WeaponAnim::Idle, 0.f) && Play(WeaponAnim::Equip, 0.f))
        AnimTime = HeldTime();
}

float ViewModelActor::HeldTime() const
{
    // Just short of the end: sampling at the duration wraps back to frame 0.
    return std::max(0.f, Mesh->GetAnimationDuration(BlendToAnim) - 1e-3f);
}

}
