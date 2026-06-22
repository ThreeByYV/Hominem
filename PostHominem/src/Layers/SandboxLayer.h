#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Game/Player.h"
#include "Game/WorldConfig.h"
#include "Game/Actors/NarrativeTextActor.h"
#include "Hominem/Assets/AssetLoaders.h"

class SandboxLayer : public Hominem::Layer
{
public:
	SandboxLayer();
	virtual ~SandboxLayer() = default;

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);

	void InitPhysics();
	void InitCamera();
	void InitWorld();

	WorldConfig m_Config;

	Player*                                          m_Player        = nullptr;
	NarrativeTextActor*                              m_NarrativeText = nullptr;
	Hominem::Ref<Hominem::CinematicCameraController> m_CinematicCamera;

	Hominem::AssetHandle<Hominem::Texture2D> m_BgTexHandle;
	float m_OrthoSize = 10.f;
};
