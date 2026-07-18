#pragma once

#include "Hominem/Cinematics/Cutscene.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Game/SideScrollerCamera.h"

#include <functional>
#include <glm/glm.hpp>

class Player;

namespace Hominem { class Scene; }

class IntroSequence
{
public:
    IntroSequence(Player& player, SideScrollerCamera& camera);

    void Start(Hominem::Scene& scene, bool skip);
    void OnUpdate(Hominem::Timestep ts);
    void OnBuildRenderFrame(Hominem::RenderFrame& frame);
    bool IsPlaying() const;

    void SetOnFinished(std::function<void()> cb) { m_OnFinished = std::move(cb); }

    static inline glm::vec3 s_EyeTarget = { 0.084f, 0.8f, 40.0f };

private:
    void StartZoomIn();
    glm::vec3 ResolveEyeTarget(const glm::vec3& fallback) const;

    static constexpr glm::vec3 k_EyeTarget = { 0.084f, 0.8f, 29.0f };
    static constexpr float     k_EyeZoom   = 0.08f;
    static constexpr float     k_WaitDur   = 2.5f;
    static constexpr float     k_ZoomDur   = 0.7f;
    static constexpr float     k_FlashDur  = 0.15f;

    Player&              m_Player;
    SideScrollerCamera&  m_Camera;
    Hominem::Scene*      m_Scene = nullptr;

    float                    m_Timer = 0.f;
    Hominem::Cutscene        m_Cutscene;
    Hominem::CutsceneContext m_Ctx;
    std::function<void()>   m_OnFinished;
};
