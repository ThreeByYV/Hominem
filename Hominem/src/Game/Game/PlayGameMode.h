#pragma once

#include "GameMode.h"
#include "Player.h"
#include "Hominem/Scene/Entity.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Game/GameConfig.h"

namespace Hominem {

	class PlayGameMode : public GameMode
	{
	public:
		PlayGameMode();

		void OnEnter(Scene& scene) override;
		void OnUpdate(Timestep ts) override;
		void OnExit() override;
		void OnImGuiRender() override;
		void OnEvent(Event& e) override;

	private:
		void InitWorld(Scene& scene);
		void InitCameraControllers(Scene& scene);

		GameConfig m_Config;

		Scope<Player> m_Player;
		Entity m_CameraEntity;
		Ref<CinematicCameraController> m_CinematicCameraController;

		float m_OrthoSize = 10.0f;
	};

}
