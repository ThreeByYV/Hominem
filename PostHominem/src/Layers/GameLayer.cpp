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

	auto& window = Application::Get().GetWindow();
	m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

	m_GameMode->OnEnter(*m_ActiveScene);
}

void GameLayer::OnDetach()
{
	m_GameMode->OnExit();
	m_ActiveScene.reset();
}

void GameLayer::OnUpdate(Timestep ts)
{
	if (!ImGui::GetIO().WantCaptureKeyboard && Input::IsKeyPressed(HMN_KEY_1))
	{
		TransitionTo<MenuLayer>();
		return;
	}

	m_ActiveScene->OnUpdate(ts);
	m_GameMode->OnUpdate(ts);
}

void GameLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	frame.clearColor = { 0.1f, 0.1f, 0.1f, 1.f };
	frame.light      = m_Light;
	if (m_ActiveScene)
		m_ActiveScene->BuildRenderFrame(frame);
}

void GameLayer::OnImGuiRender()
{
	if (!m_ShowDebugUI) return;
	ImGui::Begin("Settings");
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SliderFloat("Ambient",  &m_Light.AmbientIntensity, 0.f, 1.f);
		ImGui::SliderFloat("Diffuse",  &m_Light.DiffuseIntensity, 0.f, 2.f);
		ImGui::ColorEdit3("Color",     &m_Light.Color.x);
		ImGui::SliderFloat3("Direction", &m_Light.Direction.x, -1.f, 1.f);
	}
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

	if (e.GetKeyCode() == HMN_KEY_TAB)
		m_ShowDebugUI = !m_ShowDebugUI;

	if (e.GetKeyCode() == HMN_KEY_N)
		Renderer3D::SetDrawNormals(!Renderer3D::GetDrawNormals());

	if (e.GetKeyCode() == HMN_KEY_B)
		Renderer3D::SetDrawAABB(!Renderer3D::GetDrawAABB());

	return false;
}
