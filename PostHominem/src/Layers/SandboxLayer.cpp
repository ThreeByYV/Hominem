#include "hmnpch.h"
#include "SandboxLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Game/PlayLevel.h"
#include "MenuLayer.h"

#include <imgui.h>

using namespace Hominem;

SandboxLayer::SandboxLayer()
	: Layer("Sandbox")
{
}

void SandboxLayer::OnAttach()
{
	m_ActiveScene = CreateRef<Scene>();
	m_GameMode    = CreateScope<PlayLevel>();
	m_GameMode->OnEnter(*m_ActiveScene);
}

void SandboxLayer::OnDetach()
{
	m_GameMode->OnExit();
}

void SandboxLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_1))
	{
		TransitionTo<MenuLayer>();
		return;
	}

	RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	RenderCommand::Clear();
	m_ActiveScene->OnUpdate(ts);  // physics + actors move first
	m_GameMode->OnUpdate(ts);     // camera reads fresh positions
	m_ActiveScene->OnDraw();
}

void SandboxLayer::OnImGuiRender()
{
	ImGui::Begin("Settings");
	m_GameMode->OnImGuiRender();
	ImGui::End();
}

bool SandboxLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (m_ActiveScene)
		m_ActiveScene->OnViewportResize(e.GetWidth(), e.GetHeight());
	return false;
}

void SandboxLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(SandboxLayer::OnWindowResize));
	m_GameMode->OnEvent(e);
}
