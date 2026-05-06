#pragma once

#include "Level.h"
#include "Player.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Game/Actors/NarrativeTextActor.h"
#include "Game/WorldConfig.h"

class PlayLevel : public Level
{
public:
	PlayLevel();

	void OnEnter(Hominem::Scene& scene) override;
	void OnUpdate(Hominem::Timestep ts)  override;
	void OnExit()                        override;
	void OnImGuiRender()                 override;
	void OnEvent(Hominem::Event& e)      override;

private:
	void InitPhysics(Hominem::Scene& scene);
	void InitCamera(Hominem::Scene& scene);
	void InitWorld(Hominem::Scene& scene);

	WorldConfig m_Config;

	Player*             m_Player        = nullptr; // non-owning; owned by Scene
	NarrativeTextActor* m_NarrativeText = nullptr; // non-owning; owned by Scene

	Hominem::Ref<Hominem::CinematicCameraController> m_CinematicCamera;

	float m_OrthoSize = 10.f;
};
