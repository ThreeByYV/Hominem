#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Scene/Actors/SpriteActor.h"

using namespace Hominem;

GameLayer::GameLayer()
	: Layer("Game")
{
}

void GameLayer::OnAttach()
{
	m_ActiveScene = CreateRef<Scene>();

	auto& window = Application::Get().GetWindow();
	m_ActiveScene->GetCamera().SetOrthographic(2.0f, -1.f, 1.f);
	m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

	m_ActiveScene->SpawnActor<SpriteActor>(
		glm::vec3{ 0.f, 0.f, -0.5f },
		glm::vec3{ 2.7f, 2.f, 1.f },
		Texture2D::Create("Resources/Textures/gamebg.png"));

	m_ActiveScene->SpawnActor<SpriteActor>(
		glm::vec3{ 0.f, -0.5f, 0.f },
		glm::vec3{ 0.6f, 0.6f, 1.f },
		Texture2D::Create("Resources/Textures/mujun.png"));
}

void GameLayer::OnDetach()
{
}

void GameLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_1))
	{
		TransitionTo<MenuLayer>();
		return;
	}

	RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.f });
	RenderCommand::Clear();
	m_ActiveScene->OnUpdate(ts);
	m_ActiveScene->OnDraw();
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
}
