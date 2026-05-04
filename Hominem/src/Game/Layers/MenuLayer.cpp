#include "hmnpch.h"
#include "MenuLayer.h"
#include "SandboxLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Scene/Systems/AudioSourceSystem.h"
#include "Hominem/Scene/Systems/RenderSystem.h"

namespace Hominem {

	MenuLayer::MenuLayer()
		: Layer("Menu")
	{
	}

	void MenuLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->RegisterSystem<AudioSourceSystem>();
		m_ActiveScene->RegisterSystem<RenderSystem>();

		auto& window = Application::Get().GetWindow();

		// Camera must exist before OnViewportResize so it gets a valid aspect ratio
		auto camera = m_ActiveScene->CreateEntity("Menu Camera");
		auto& camComp = camera.AddComponent<CameraComponent>();
		camComp.Camera.SetOrthographicSize(2.0f);

		m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

		// Background
		auto background = m_ActiveScene->CreateEntity("Background");
		auto& bgT = background.GetComponent<TransformComponent>();
		bgT.Translation = { 0.0f, 0.0f, -0.5f };
		bgT.Scale       = { 2.7f, 2.0f, 1.0f };
		background.AddComponent<SpriteRendererComponent>(Texture2D::Create("Resources/Textures/menu2.png"));

		// Title
		auto title = m_ActiveScene->CreateEntity("Title");
		auto& titleT = title.GetComponent<TransformComponent>();
		titleT.Translation = { -1.2f, 0.0f, 0.0f };
		titleT.Scale       = { 0.25f, 0.25f, 0.25f };
		title.AddComponent<TextRendererComponent>("Hominem", Font::GetDefaultFont(), glm::vec4(1.0f));

		// Music
		auto& audio = Application::Get().GetAudioSystem();
		m_MusicEntity = m_ActiveScene->CreateEntity("Menu Music");
		auto& src  = m_MusicEntity.AddComponent<AudioSourceComponent>();
		src.Buffer     = audio.LoadSound("Resources/Sounds/menu_music.mp3");
		src.Volume     = 0.9f;
		src.Loop       = true;
		src.ShouldPlay = true;

		m_ActiveScene->OnRuntimeStart();
	}

	void MenuLayer::OnDetach()
	{
		m_ActiveScene->OnRuntimeStop();
	}

	void MenuLayer::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			TransitionTo<SandboxLayer>();
			return;
		}

		if (Input::IsKeyPressed(HMN_KEY_M) && m_MusicEntity.HasComponent<AudioSourceComponent>())
			m_MusicEntity.GetComponent<AudioSourceComponent>().ShouldPlay ^= true;

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
		m_ActiveScene->OnUpdate(ts);
	}

	bool MenuLayer::OnWindowResize(WindowResizeEvent& e)
	{
		if (m_ActiveScene)
			m_ActiveScene->OnViewportResize(e.GetWidth(), e.GetHeight());
		return false;
	}

	void MenuLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(MenuLayer::OnWindowResize));
	}

}
