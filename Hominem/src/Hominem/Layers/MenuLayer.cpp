#include "hmnpch.h"
#include <glad/glad.h>

#include "MenuLayer.h"
#include "SandboxLayer.h"
#include "Hominem/Renderer/Font.h"
#include "imgui.h"
#include <chrono>
#include <future>

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
		m_BackgroundTexture = Texture2D::Create("src/Hominem/Resources/Textures/menu2.png");

		// Initialize audio system
		m_AudioSystem.Init();

		// Load audio asynchronously using JobSystem to avoid blocking startup
		HMN_CORE_INFO("MenuLayer: Starting async audio load...");
		m_MusicLoadStarted = true;

		m_MusicLoadFuture = m_JobSystem.SubmitWithResult([this]() -> SoundBufferHandle {
			HMN_CORE_INFO("MenuLayer: Loading audio on worker thread...");
			return m_AudioSystem.LoadSound("src/Hominem/Resources/Sounds/menu_music.mp3");
		});
	}

	void MenuLayer::OnDetach()
	{
		// Stop music if playing
		if (m_MusicPlaying)
		{
			m_AudioSystem.Stop(m_MusicHandle);
		}

		// Wait for async loading to complete before shutdown (if still loading)
		if (m_MusicLoadStarted && !m_MusicLoadComplete)
		{
			HMN_CORE_INFO("MenuLayer: Waiting for audio load to complete...");
			m_MusicBuffer = m_MusicLoadFuture.get(); // Block until done
		}

		// Cleanup
		m_AudioSystem.Shutdown();
	}

	void MenuLayer::OnUpdate(Timestep ts)
	{
		m_CameraController.OnUpdate(ts);

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();

		Renderer2D::BeginScene(m_CameraController.GetCamera());

		// Check if async audio loading completed
		if (m_MusicLoadStarted && !m_MusicLoadComplete)
		{
			// Non-blocking check if future is ready
			if (m_MusicLoadFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
			{
				m_MusicBuffer = m_MusicLoadFuture.get();
				m_MusicLoadComplete = true;

				if (m_MusicBuffer != InvalidSoundBuffer)
				{
					// Auto-play music once loaded
					m_MusicHandle = m_AudioSystem.Play(m_MusicBuffer, 0.9f, true); // volume=0.5, loop=true
					m_MusicPlaying = true;
					HMN_CORE_INFO("MenuLayer: Audio loaded and playing!");
				}
				else
				{
					HMN_CORE_WARN("MenuLayer: Audio file not found");
				}
			}
		}

		// Audio controls - Press M to play/pause music
		if (Input::IsKeyPressed(HMN_KEY_M) && m_MusicLoadComplete)
		{
			if (m_MusicBuffer != InvalidSoundBuffer)
			{
				if (m_MusicPlaying)
				{
					m_AudioSystem.Pause(m_MusicHandle);
					m_MusicPlaying = false;
					HMN_CORE_INFO("Music paused");
				}
				else
				{
					if (m_MusicHandle == InvalidSound)
					{
						// Start playing if never started
						m_MusicHandle = m_AudioSystem.Play(m_MusicBuffer, 0.5f, true);
					}
					else
					{
						// Resume if paused
						m_AudioSystem.Resume(m_MusicHandle);
					}
					m_MusicPlaying = true;
					HMN_CORE_INFO("Music playing");
				}
			}
		}

		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			Renderer2D::EndScene();
			TransitionTo<SandboxLayer>();
			return;
		}

		Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.5f }, { 2.7f, 2.0f }, m_BackgroundTexture);

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0.0f, 0.0f))  // Move left
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

		Renderer2D::DrawString("Hominem", Font::GetDefaultFont(), transform, glm::vec4(1.0f));
		
		Renderer2D::EndScene();
	}

	void MenuLayer::OnImGuiRender()
	{
	}
}