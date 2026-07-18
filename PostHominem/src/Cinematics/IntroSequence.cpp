#include "hmnpch.h"
#include "IntroSequence.h"

#include "Game/Player.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Cinematics/Cues/CameraCue.h"
#include "Hominem/Cinematics/Cues/FadeCue.h"
#include "Hominem/Cinematics/Cues/EventCue.h"

using namespace Hominem;

IntroSequence::IntroSequence(Player& player, SideScrollerCamera& camera)
    : m_Player(player), m_Camera(camera)
{
}

void IntroSequence::Start(Scene& scene, bool skip)
{
    m_Scene    = &scene;
    m_Ctx.scene = &scene;
    m_Timer    = 0.f;

    if (skip)
    {
        m_Camera.Snap();

        s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
        s_EyeTarget.z = k_EyeTarget.z;

        const auto [position, orthoSize] = m_Scene->GetCameraSnapshot();
        m_Scene->ApplyCameraSnapshot({ s_EyeTarget, k_EyeZoom });

        m_Cutscene.Add<CameraCue>(s_EyeTarget, position, k_EyeZoom, orthoSize).For(k_ZoomDur);
        m_Cutscene.Play();
    }
}

void IntroSequence::OnUpdate(Timestep ts)
{
    if (m_Cutscene.IsPlaying())
    {
        m_Cutscene.OnUpdate(ts, m_Ctx);
        return;
    }

    if (!m_Cutscene.IsFinished())
    {
        m_Timer += ts;
        if (m_Timer >= k_WaitDur)
            StartZoomIn();
    }
}

void IntroSequence::StartZoomIn()
{
    s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
    s_EyeTarget.z = k_EyeTarget.z;

    const auto from = m_Scene->GetCameraSnapshot();
    m_Cutscene.Add<CameraCue>(from.position, s_EyeTarget, from.orthoSize, k_EyeZoom).For(k_ZoomDur);

    // Hard cut to white (From == To == 1), not a fade.
    m_Cutscene.Add<FadeCue>(glm::vec3(1.f), 1.f, 1.f).At(k_ZoomDur).For(k_FlashDur);
    m_Cutscene.Add<EventCue>([this] { if (m_OnFinished) m_OnFinished(); })
        .At(k_ZoomDur + k_FlashDur);

    m_Cutscene.Play();
}

void IntroSequence::OnBuildRenderFrame(RenderFrame& frame)
{
    m_Cutscene.BuildRenderFrame(frame);
}

bool IntroSequence::IsPlaying() const
{
    return m_Cutscene.IsPlaying();
}

glm::vec3 IntroSequence::ResolveEyeTarget(const glm::vec3& fallback) const
{
    auto* mesh = m_Player.GetMesh();
    if (!mesh) return fallback;
    auto mat = mesh->GetBoneWorldTransform("mixamorig:Head");
    if (!mat)  return fallback;
    return glm::vec3(m_Player.GetTransform() * glm::vec4(glm::vec3((*mat)[3]), 1.0f));
}
