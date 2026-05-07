#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Game/FactoryLevel.h"

#include <imgui.h>

using namespace Hominem;

GameLayer::GameLayer()
	: Layer("Game")
{
}

void GameLayer::OnAttach()
{
	m_ActiveScene = CreateRef<Scene>();
	m_GameMode    = CreateScope<FactoryLevel>();
	m_GameMode->OnEnter(*m_ActiveScene);
}

void GameLayer::OnDetach()
{
	m_GameMode->OnExit();
}

void GameLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_1))
	{
		TransitionTo<MenuLayer>();
		return;
	}

	RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	RenderCommand::Clear();
	m_ActiveScene->OnUpdate(ts);
	m_GameMode->OnUpdate(ts);
	m_ActiveScene->OnDraw();
}

void GameLayer::OnImGuiRender()
{
	ImGui::Begin("Settings");
	m_GameMode->OnImGuiRender();
	ImGui::End();
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
	dispatcher.Dispatch<KeyPressedEvent>(HMN_BIND_EVENT_FN(GameLayer::OnKeyPressed));
	m_GameMode->OnEvent(e);
}

bool GameLayer::OnKeyPressed(KeyPressedEvent& e)
{
	if (e.GetRepeatCount() > 0)
		return false;

	if (e.GetKeyCode() == HMN_KEY_N)
		Renderer3D::SetDrawNormals(!Renderer3D::GetDrawNormals());

	return false;
}
