#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Scene/CameraController.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include "Hominem/Renderer/Shader.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/Entity.h"
#include "Hominem/Serialization/GameConfig.h"

namespace Hominem {

	class SandboxLayer : public Layer
	{
	public:
		SandboxLayer();
		virtual ~SandboxLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
		void OnEvent(Event& e) override;

	private:
		static long long GetCurrentTimeMillis();

	private:
		// Game configuration loaded from JSON
		GameConfig m_Config;

		// ECS-based camera controller
		EntityCameraController m_CameraController;

		// Cinematic camera controller (for scripted sequences)
		Ref<CinematicCameraController> m_CinematicCameraController;

		Ref<Scene> m_ActiveScene;
		Entity m_MeshEntity;
		Entity m_CameraEntity;
		Entity m_SecondCamera;
		bool m_PrimaryCamera = true;
		bool m_SpaceKeyPressed = false;
		int m_DisplayBoneIndex = -1;
		long long m_StartTimeMillis = 0;

		// Debug controls for mesh
		glm::vec3 m_MeshPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 m_MeshScale = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 m_MeshRotation = glm::vec3(0.0f, 0.0f, 0.0f);
		bool m_UseBasicShader = false;

		// Camera settings
		float m_OrthoSize = 10.0f;
		bool m_UseCinematicCamera = false; // Toggle between ECS camera and cinematic camera

		// Animation state
		enum class CharacterState
		{
			Idle = 0,
			Running = 1
		};
		CharacterState m_CurrentState = CharacterState::Idle;
	};
}
