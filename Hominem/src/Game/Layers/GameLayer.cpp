#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Scene/Systems/RenderSystem.h"

namespace Hominem {

	GameLayer::GameLayer()
		: Layer("Game")
	{
	}

	void GameLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->RegisterSystem<RenderSystem>();

		auto& window = Application::Get().GetWindow();
		m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

		// Camera
		m_CameraEntity = m_ActiveScene->CreateEntity("Camera");
		m_CameraEntity.AddComponent<CameraComponent>().Primary = true;
		m_CameraController.SetEntity(m_CameraEntity);

		// Background
		auto bg = m_ActiveScene->CreateEntity("Background");
		auto& bgT = bg.GetComponent<TransformComponent>();
		bgT.Translation = { 0.0f, 0.0f, -0.5f };
		bgT.Scale       = { 2.7f, 2.0f, 1.0f };
		bg.AddComponent<SpriteRendererComponent>(Texture2D::Create("Resources/Textures/gamebg.png"));

		// Character
		auto character = m_ActiveScene->CreateEntity("Character");
		auto& charT = character.GetComponent<TransformComponent>();
		charT.Translation = { 0.0f, -0.5f, 0.0f };
		charT.Scale       = { 0.6f, 0.6f, 1.0f };
		character.AddComponent<SpriteRendererComponent>(Texture2D::Create("Resources/Textures/mujun.png"));

		// Spritesheet
		auto sheet = m_ActiveScene->CreateEntity("Spritesheet");
		sheet.GetComponent<TransformComponent>().Scale = { 1.0f, 1.0f, 1.0f };
		sheet.AddComponent<SpriteRendererComponent>(Texture2D::Create("Resources/Textures/spritesheet.png"));

		m_ActiveScene->OnRuntimeStart();
	}

	void GameLayer::OnDetach()
	{
		m_ActiveScene->OnRuntimeStop();
	}

	void GameLayer::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(HMN_KEY_1))
		{
			TransitionTo<MenuLayer>();
			return;
		}

		m_CameraController.OnUpdate(ts);

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
		m_ActiveScene->OnUpdate(ts);
	}

	bool GameLayer::OnWindowResize(WindowResizeEvent& e)
	{
		if (m_ActiveScene)
			m_ActiveScene->OnViewportResize(e.GetWidth(), e.GetHeight());
		return false;
	}

	void GameLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(GameLayer::OnWindowResize));
		m_CameraController.OnEvent(e);
	}

}
