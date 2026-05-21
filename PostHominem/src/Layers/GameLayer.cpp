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

	// Load point lights from config
	WorldConfig cfg;
	if (WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg))
	{
		m_PointLights.clear();
		for (const auto& pl : cfg.PointLights)
		{
			PointLight light;
			light.Position  = pl.Position;
			light.Color     = pl.Color;
			light.Intensity = pl.Intensity;
			light.Radius    = pl.Radius;
			m_PointLights.push_back(light);
		}
	}
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
	frame.clearColor         = { 0.1f, 0.1f, 0.1f, 1.f };
	frame.light              = m_Light;
	frame.pointLights        = m_PointLights;
	frame.debugPointLights   = m_DebugPointLights;
	frame.bloomEnabled       = m_BloomEnabled;
	frame.toneMappingEnabled = m_ToneMappingEnabled;
	frame.bloomStrength      = m_BloomStrength;
	frame.bloomThreshold     = m_BloomThreshold;

	if (m_ActiveScene)
		m_ActiveScene->BuildRenderFrame(frame);
}

void GameLayer::OnImGuiRender()
{
	if (!m_ShowDebugUI) return;
	ImGui::Begin("Settings");
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SliderFloat("Ambient",       &m_Light.AmbientIntensity, 0.f, 1.f);
		ImGui::ColorEdit3("Ambient Color",  &m_Light.AmbientColor.x);
		ImGui::SliderFloat("Diffuse",       &m_Light.DiffuseIntensity, 0.f, 20.f);
		ImGui::ColorEdit3("Light Color",    &m_Light.Color.x);
		ImGui::SliderFloat3("Direction",    &m_Light.Direction.x, -1.f, 1.f);
	}
	if (ImGui::CollapsingHeader("Point Lights"))
	{
		ImGui::Text("%d light(s)", (int)m_PointLights.size());
		ImGui::SameLine();
		if (ImGui::SmallButton("Add"))
		{
			Hominem::PointLight l;
			l.Position = { 0.f, 1.f, 0.f };
			l.Color    = { 1.f, 0.9f, 0.7f };
			m_PointLights.push_back(l);
			m_SelectedLight = (int)m_PointLights.size() - 1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Save"))
		{
			WorldConfig cfg;
			WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg);
			cfg.PointLights.clear();
			for (const auto& pl : m_PointLights)
			{
				PointLightConfig plc;
				plc.Position  = pl.Position;
				plc.Color     = pl.Color;
				plc.Intensity = pl.Intensity;
				plc.Radius    = pl.Radius;
				cfg.PointLights.push_back(plc);
			}
			WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
		}

		for (int i = 0; i < (int)m_PointLights.size(); i++)
		{
			ImGui::PushID(i);
			auto& pl = m_PointLights[i];
			char label[32];
			snprintf(label, sizeof(label), "Light %d", i);
			bool selected = (m_SelectedLight == i);
			if (ImGui::Selectable(label, selected))
				m_SelectedLight = i;
			if (m_SelectedLight == i)
			{
				ImGui::DragFloat3("Position",  &pl.Position.x,  0.05f);
				ImGui::ColorEdit3("Color",     &pl.Color.x);
				ImGui::DragFloat("Intensity",  &pl.Intensity,   0.1f, 0.f, 100.f);
				ImGui::DragFloat("Radius",     &pl.Radius,      0.1f, 0.1f, 50.f);
				if (ImGui::SmallButton("Remove"))
				{
					m_PointLights.erase(m_PointLights.begin() + i);
					m_SelectedLight = -1;
					ImGui::PopID();
					break;
				}
			}
			ImGui::PopID();
		}
	}
	if (ImGui::CollapsingHeader("Post Processing"))
	{
		ImGui::Checkbox("Bloom",         &m_BloomEnabled);
		ImGui::Checkbox("Tone Mapping",  &m_ToneMappingEnabled);
		if (m_BloomEnabled)
		{
			ImGui::SliderFloat("Bloom Strength",   &m_BloomStrength,  0.f, 3.f);
			ImGui::SliderFloat("Bloom Threshold",  &m_BloomThreshold, 0.f, 2.f);
		}
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

	// TAB always toggles debug UI regardless of ImGui focus
	if (e.GetKeyCode() == HMN_KEY_TAB)
	{
		m_ShowDebugUI = !m_ShowDebugUI;
		return false;
	}

	if (e.GetKeyCode() == HMN_KEY_ESCAPE)
	{
		TransitionTo<MenuLayer>();
		return true;
	}

	if (ImGui::GetIO().WantCaptureKeyboard)
		return false;

	if (e.GetKeyCode() == HMN_KEY_N)
		Renderer3D::SetDrawNormals(!Renderer3D::GetDrawNormals());

	if (e.GetKeyCode() == HMN_KEY_B)
		Renderer3D::SetDrawAABB(!Renderer3D::GetDrawAABB());

	if (e.GetKeyCode() == HMN_KEY_L)
		m_DebugPointLights = !m_DebugPointLights;

	return false;
}
