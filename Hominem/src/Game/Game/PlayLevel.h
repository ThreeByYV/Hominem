#pragma once

#include "Level.h"
#include "Player.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Hominem/Scene/Actors/NarrativeTextActor.h"
#include "Game/WorldConfig.h"

namespace Hominem {


	class PlayLevel : public Level
	{
	public:
		PlayLevel();

		void OnEnter(Scene& scene) override;
		void OnUpdate(Timestep ts)  override;
		void OnExit()               override;
		void OnImGuiRender()        override;
		void OnEvent(Event& e)      override;

	private:
		void InitPhysics(Scene& scene);
		void InitCamera(Scene& scene);
		void InitWorld(Scene& scene);

		WorldConfig m_Config;

		Player*             m_Player        = nullptr; // non-owning; owned by Scene
		NarrativeTextActor* m_NarrativeText = nullptr; // non-owning; owned by Scene

		Ref<CinematicCameraController> m_CinematicCamera;

		float m_OrthoSize = 10.f;
	};

}
