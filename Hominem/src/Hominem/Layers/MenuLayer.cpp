#include "hmnpch.h"
#include <glad/glad.h>

#include "MenuLayer.h"
#include "SandboxLayer.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Core/Application.h"
#include "imgui.h"

namespace Hominem {

	static Ref<Font> s_Font;

	MenuLayer::MenuLayer()
		: Layer("MenuLayer"), m_CameraController(800.0f / 600.0f)
	{
		HMN_CORE_INFO("Created new MenuLayer!");
	}

	void MenuLayer::OnAttach()
	{
		Renderer2D::Init();
		s_Font = Font::GetDefaultFont();

		m_Scene = CreateRef<Scene>();

		auto& audioSystem = Application::Get().GetAudioSystem();

		// Load menu music
		SoundBufferHandle musicBuffer = audioSystem.LoadSound("src/Hominem/Resources/Sounds/menu_music.mp3");

		// Create music entity
		m_MusicEntity = m_Scene->CreateEntity("Menu Music");
		auto& audioSource = m_MusicEntity.AddComponent<AudioSourceComponent>();
		audioSource.Buffer = musicBuffer;
		audioSource.Volume = 0.9f;
		audioSource.Loop = true;
		audioSource.ShouldPlay = true;

		// Create camera entity BEFORE calling OnViewportResize
		m_CameraEntity = m_Scene->CreateEntity("Menu Camera");
		auto& cameraComp = m_CameraEntity.AddComponent<CameraComponent>();

		// Set orthographic size to 2.0 so sprites fill the screen properly
		cameraComp.Camera.SetOrthographicSize(2.0f);

		// Initialize scene camera with window dimensions (AFTER camera entity is created)
		auto& window = Application::Get().GetWindow();
		uint32_t width = window.GetWidth();
		uint32_t height = window.GetHeight();
		HMN_CORE_INFO("MenuLayer::OnAttach - Window size: {}x{}", width, height);
		m_Scene->OnViewportResize(width, height);

		// Create background sprite entity
		auto backgroundEntity = m_Scene->CreateEntity("Background");
		auto& bgTransform = backgroundEntity.GetComponent<TransformComponent>();
		bgTransform.Transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(2.7f, 2.0f, 1.0f));

		auto backgroundTexture = Texture2D::Create("src/Hominem/Resources/Textures/menu2.png");
		backgroundEntity.AddComponent<SpriteRendererComponent>(backgroundTexture);

		// Create title text entity
		auto titleEntity = m_Scene->CreateEntity("Title");
		auto& titleTransform = titleEntity.GetComponent<TransformComponent>();
		titleTransform.Transform = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

		titleEntity.AddComponent<TextRendererComponent>("Hominem", Font::GetDefaultFont(), glm::vec4(1.0f));
	}

	void MenuLayer::OnDetach()
	{
		// Scene destructor will auto-stop all audio
	}

	void MenuLayer::OnUpdate(Timestep ts)
	{
		m_CameraController.OnUpdate(ts);

		// Sync camera controller to camera entity
		if (m_CameraEntity.HasComponent<TransformComponent>())
		{
			auto& transform = m_CameraEntity.GetComponent<TransformComponent>();
			glm::mat4 viewMatrix = m_CameraController.GetCamera().GetViewMatrix();
			transform.Transform = glm::inverse(viewMatrix);

		}
		else
		{
			HMN_CORE_WARN("MenuLayer: Camera entity has no TransformComponent!");
		}

		// Toggle music with M key
		if (Input::IsKeyPressed(HMN_KEY_M))
		{
			if (m_MusicEntity.HasComponent<AudioSourceComponent>())
			{
				auto& audioSource = m_MusicEntity.GetComponent<AudioSourceComponent>();
				audioSource.ShouldPlay = !audioSource.ShouldPlay;
			}
		}

		// Transition to sandbox
		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			TransitionTo<SandboxLayer>();
			return;
		}

		// Clear screen
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();

		// Scene handles all rendering via RenderSystem2D and AudioECSSystem
		m_Scene->OnUpdate(ts);
	}

	void MenuLayer::OnImGuiRender()
	{
	}
}