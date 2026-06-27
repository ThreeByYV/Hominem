#pragma once

#include "Hominem/Scene/SceneLayer.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Game/Player.h"
#include "Game/WorldConfig.h"
#include "Game/Actors/NarrativeTextActor.h"
#include "Hominem/Assets/AssetLoaders.h"

class SandboxLayer : public Hominem::SceneLayer
{
public:
    SandboxLayer();
    virtual ~SandboxLayer() = default;

    void OnUpdate(Hominem::Timestep ts)              override;
    void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
    void OnImGuiRender()                             override;
    void OnEvent(Hominem::Event& e)                  override;

protected:
    Hominem::SceneDesc Describe()    override;
    void               OnSceneReady()  override;
    void               OnSceneDetach() override;

private:
    Player*                                          m_Player        = nullptr;
    NarrativeTextActor*                              m_NarrativeText = nullptr;
    Hominem::Ref<Hominem::CinematicCameraController> m_CinematicCamera;

    Hominem::AssetHandle<Hominem::Texture2D> m_BgTexHandle;
    float m_OrthoSize = 10.f;
};
