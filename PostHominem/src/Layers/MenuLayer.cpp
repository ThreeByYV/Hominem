#include "hmnpch.h"
#include "MenuLayer.h"
#include "SandboxLayer.h"
#include "GameLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Scene/Actors/SpriteActor.h"
#include "Hominem/Scene/Actors/TextActor.h"

using namespace Hominem;

MenuLayer::MenuLayer()
	: Layer("Menu")
{
}

void MenuLayer::OnAttach()
{
	m_ActiveScene = CreateRef<Scene>();

	auto& window = Application::Get().GetWindow();

	m_ActiveScene->GetCamera().SetOrthographic(k_OrthoSize, -1.f, 1.f);
	m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

	float aspect = (float)window.GetWidth() / (float)window.GetHeight();
	m_Background = &m_ActiveScene->SpawnActor<SpriteActor>(
		glm::vec3{ 0.f, 0.f, -0.5f },
		glm::vec3{ k_OrthoSize * aspect, k_OrthoSize, 1.f },
		Texture2D::Create("Resources/Textures/menu2.png"));

	m_ActiveScene->SpawnActor<TextActor>(
		glm::vec3{ -1.2f, 0.f, 0.f },
		glm::vec3{ 0.25f, 0.25f, 0.25f },
		"Hominem");

	auto& audio = Application::Get().GetAudioSystem();
	audio.LoadMusicAsync("Resources/Sounds/menu_music.mp3", /*autoPlay=*/true, 0.9f, /*loop=*/true);
}

void MenuLayer::OnDetach()
{
	Application::Get().GetAudioSystem().StopMusic();
}

void MenuLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_2))
	{
		TransitionTo<GameLayer>();
		return;
	}

	if (Input::IsKeyPressed(HMN_KEY_M))
		Application::Get().GetAudioSystem().ToggleMusic();

	Application::Get().GetAudioSystem().UpdateMusic();

	RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.f });
	RenderCommand::Clear();
	m_ActiveScene->OnUpdate(ts);
	m_ActiveScene->OnDraw();
}

bool MenuLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (m_ActiveScene)
		m_ActiveScene->OnViewportResize(e.GetWidth(), e.GetHeight());

	if (m_Background)
	{
		float aspect = (float)e.GetWidth() / (float)e.GetHeight();
		m_Background->Scale = glm::vec3{ k_OrthoSize * aspect, k_OrthoSize, 1.f };
	}

	return false;
}

void MenuLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(MenuLayer::OnWindowResize));
}
