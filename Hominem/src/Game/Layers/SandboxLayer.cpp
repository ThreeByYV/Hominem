#include "hmnpch.h"
#include "SandboxLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Game/PlayGameMode.h"
#include "MenuLayer.h"

#include <imgui.h>

namespace Hominem {

	SandboxLayer::SandboxLayer()
		: Layer("Sandbox")
	{
	}

	void SandboxLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();
		m_GameMode    = CreateScope<PlayGameMode>();
		m_GameMode->OnEnter(*m_ActiveScene);
		m_ActiveScene->OnRuntimeStart();
	}

	void SandboxLayer::OnDetach()
	{
		m_GameMode->OnExit();
		m_ActiveScene->OnRuntimeStop();
	}

	void SandboxLayer::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(HMN_KEY_1))
		{
			TransitionTo<MenuLayer>();
			return;
		}

		m_GameMode->OnUpdate(ts);

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();
		m_ActiveScene->OnUpdate(ts);
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

}
