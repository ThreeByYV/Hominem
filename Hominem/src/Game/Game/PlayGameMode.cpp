#include "hmnpch.h"
#include "PlayGameMode.h"

#include "Hominem/Core/Application.h"
#include "Game/GameConfig.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Scene/Systems/AnimationSystem.h"
#include "Hominem/Scene/Systems/PhysicsSystem.h"
#include "Hominem/Scene/Systems/AudioSourceSystem.h"
#include "Hominem/Scene/Systems/RenderSystem.h"

#include <imgui.h>

namespace Hominem {

	PlayGameMode::PlayGameMode()
	{
		if (!GameConfig::LoadFromFile("Resources/Config/game_config.json", m_Config))
			HMN_CORE_WARN("PlayGameMode: using default game config");
	}

	void PlayGameMode::OnEnter(Scene& scene)
	{
		scene.RegisterSystem<AnimationSystem>();
		scene.RegisterSystem<PhysicsSystem>();
		scene.RegisterSystem<AudioSourceSystem>();
		scene.RegisterSystem<RenderSystem>();

		InitCameraControllers(scene);
		InitWorld(scene);

		m_CinematicCameraController->RegisterOnComplete("vista_reveal", []() {
			HMN_CORE_INFO("Vista 1 complete");
		});
		m_CinematicCameraController->RegisterOnComplete("vista_reveal_2", []() {
			HMN_CORE_INFO("Vista 2 complete");
		});

		m_Player = CreateScope<Player>(scene, m_Config.Player);
	}

	void PlayGameMode::OnExit()
	{
		m_Player.reset();
	}

	void PlayGameMode::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			Renderer2D::GetShaderLibrary()->ReloadAll();
			Renderer3D::GetShaderLibrary()->ReloadAll();
			if (m_Player) m_Player->ReloadShader();
		}

glm::vec2 playerPos = m_Player ? glm::vec2(m_Player->GetPosition()) : glm::vec2(0.0f);
		m_CinematicCameraController->UpdateCameraForPlayer(playerPos);
		m_CinematicCameraController->OnUpdate(ts);

		if (m_Player) m_Player->OnUpdate(ts);
	}

	void PlayGameMode::OnImGuiRender()
	{
		if (m_Player) m_Player->OnImGuiRender();

		ImGui::Separator();
		ImGui::Text("Camera");

		if (ImGui::DragFloat("Ortho Size", &m_OrthoSize, 0.5f, 1.0f, 50.0f))
		{
			if (m_CameraEntity && m_CameraEntity.HasComponent<CameraComponent>())
				m_CameraEntity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(m_OrthoSize);
		}

		if (m_CinematicCameraController)
		{
			ImGui::Text("In Cinematic: %s", m_CinematicCameraController->IsInCinematic() ? "YES" : "NO");

			float smoothing = m_CinematicCameraController->GetSmoothingFactor();
			if (ImGui::SliderFloat("Smoothing", &smoothing, 0.01f, 1.0f))
				m_CinematicCameraController->SetSmoothingFactor(smoothing);

			float zoom = m_CinematicCameraController->GetZoomLevel();
			if (ImGui::SliderFloat("Zoom", &zoom, 1.0f, 30.0f))
				m_CinematicCameraController->SetZoomLevel(zoom);

			if (ImGui::Button("Trigger Vista"))
				m_CinematicCameraController->TriggerCinematic("vista_reveal");
			ImGui::SameLine();
			if (ImGui::Button("End Cinematic"))
				m_CinematicCameraController->EndCinematic();
		}
	}

	void PlayGameMode::OnEvent(Event& e)
	{
		if (m_CinematicCameraController)
			m_CinematicCameraController->OnEvent(e);
	}

	void PlayGameMode::InitWorld(Scene& scene)
	{
		auto bg = scene.CreateEntity("Background");
		auto& bgT = bg.GetComponent<TransformComponent>();
		bgT.Translation = { 0.0f, 0.0f, -5.0f };
		bgT.Scale       = { 20.0f, 12.0f, 1.0f };
		bg.AddComponent<SpriteRendererComponent>(Texture2D::Create("Resources/Textures/gamebg.png"));

		const auto& fc = m_Config.Physics.Floor;
		auto floor = scene.CreateEntity("Floor");
		auto& floorT = floor.GetComponent<TransformComponent>();
		floorT.Translation = fc.Position;
		floorT.Scale       = fc.Scale;
		floor.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.3f, 0.2f, 0.1f, 1.0f });

		auto& floorRb = floor.AddComponent<RigidbodyComponent>();
		floorRb.Type           = RigidbodyComponent::BodyType::Static;
		floorRb.GravityEnabled = false;

		auto& floorCol = floor.AddComponent<BoxColliderComponent>();
		floorCol.HalfExtents     = { fc.Scale.x * 0.5f, fc.Scale.y * 0.5f };
		floorCol.StaticFriction  = fc.StaticFriction;
		floorCol.DynamicFriction = fc.DynamicFriction;
	}

	void PlayGameMode::InitCameraControllers(Scene& scene)
	{
		auto& window = Application::Get().GetWindow();
		uint32_t w = window.GetWidth();
		uint32_t h = window.GetHeight();

		m_CameraEntity = scene.CreateEntity("Main Camera");
		auto& camComp = m_CameraEntity.AddComponent<CameraComponent>();
		camComp.Camera.SetOrthographic(
			m_Config.Camera.OrthoSize,
			m_Config.Camera.OrthoNear,
			m_Config.Camera.OrthoFar
		);

		scene.OnViewportResize(w, h);

		float aspect = (float)w / (float)h;
		m_CinematicCameraController = CreateRef<CinematicCameraController>(aspect);
		m_CinematicCameraController->SetCameraEntity(m_CameraEntity);

		if (!m_CinematicCameraController->LoadFromFile("Resources/Config/camera_config.json", "camera_sequences.level_01_vista"))
			HMN_CORE_WARN("PlayGameMode: camera_config.json not found, using defaults");

	}

}
