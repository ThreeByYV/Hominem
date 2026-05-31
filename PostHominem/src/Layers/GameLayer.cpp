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

	WorldConfig cfg;
	if (WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg))
	{
		m_Lights.clear();
		for (const auto& lc : cfg.Lights)
		{
			Light l;
			l.Position   = lc.Position;
			l.Color      = lc.Color;
			l.Direction  = lc.Direction;
			l.Intensity  = lc.Intensity;
			l.Radius     = lc.Radius;
			l.InnerAngle = lc.InnerAngle;
			l.OuterAngle = lc.OuterAngle;
			l.Type       = static_cast<LightType>(lc.Type);
			m_Lights.push_back(l);
		}
	}

	auto& audio = Application::Get().GetAudioSystem();
	audio.LoadMusicAsync("Resources/Sounds/menu_music_2.mp3", /*autoPlay=*/true, 0.9f, /*loop=*/true);
}

void GameLayer::OnDetach()
{
	Application::Get().GetAudioSystem().StopMusic();
	m_GameMode->OnExit();
	m_ActiveScene.reset();
}

void GameLayer::OnUpdate(Timestep ts)
{
	Application::Get().GetAudioSystem().UpdateMusic();
	m_ActiveScene->OnUpdate(ts);
	m_GameMode->OnUpdate(ts);

	m_FrameTimeMs = ts.GetMilliseconds();
	m_FPS         = ts > 0.f ? 1.f / ts : 0.f;
}

void GameLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	frame.clearColor         = { 0.1f, 0.1f, 0.1f, 1.f };
	frame.light              = m_Light;
	frame.lights             = m_Lights;
	frame.debugLights        = m_DebugLights;
	frame.bloomEnabled       = m_BloomEnabled;
	frame.toneMappingEnabled = m_ToneMappingEnabled;
	frame.bloomStrength      = m_BloomStrength;
	frame.bloomThreshold     = m_BloomThreshold;

	if (m_ActiveScene)
		m_ActiveScene->BuildRenderFrame(frame);
}

void GameLayer::OnImGuiRender()
{
	// Perf panel — top left, toggled with P
	if (m_ShowPerfPanel)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		constexpr ImGuiWindowFlags kOverlayFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

		if (ImGui::Begin("##PerfPanel", nullptr, kOverlayFlags))
		{
			ImVec4 fpsColor = m_FPS >= 60.f ? ImVec4(0.2f, 1.f, 0.2f, 1.f)
			                : m_FPS >= 30.f ? ImVec4(1.f,  0.8f, 0.f,  1.f)
			                                : ImVec4(1.f,  0.3f, 0.3f, 1.f);
			ImGui::TextColored(fpsColor, "%.0f FPS  %.2f ms", m_FPS, m_FrameTimeMs);

			uint64_t tris = Renderer3D::GetTriangles();
			const char* triUnit = tris >= 1000000 ? "M" : tris >= 1000 ? "K" : "";
			float       triVal  = tris >= 1000000 ? tris / 1000000.f : tris >= 1000 ? tris / 1000.f : (float)tris;
			ImGui::Text("Draw calls  %u", Renderer3D::GetDrawCalls());
			ImGui::Text("Triangles   %.1f%s", triVal, triUnit);
			ImGui::Text("Groups      %u / %u culled", Renderer3D::GetGroupsTotal(), Renderer3D::GetGroupsCulled());

			if (m_ActiveScene && m_ActiveScene->GetPhysicsWorld())
				ImGui::Text("Physics     %.2f ms", m_ActiveScene->GetPhysicsWorld()->GetLastStepMs());
		}
		ImGui::End();
	}

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
	if (ImGui::CollapsingHeader("Lights"))
	{
		ImGui::Text("%d light(s)", (int)m_Lights.size());
		ImGui::SameLine();
		if (ImGui::SmallButton("Add Point"))
		{
			Light l;
			l.Position = { 0.f, 1.f, 0.f };
			l.Color    = { 1.f, 0.9f, 0.7f };
			l.Type     = LightType::Point;
			m_Lights.push_back(l);
			m_SelectedLight = (int)m_Lights.size() - 1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Add Spot"))
		{
			Light l;
			l.Position = { 0.f, 2.f, 0.f };
			l.Color    = { 1.f, 0.9f, 0.7f };
			l.Type     = LightType::Spot;
			m_Lights.push_back(l);
			m_SelectedLight = (int)m_Lights.size() - 1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Save"))
		{
			WorldConfig cfg;
			WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg);
			cfg.Lights.clear();
			for (const auto& l : m_Lights)
			{
				LightConfig lc;
				lc.Position   = l.Position;
				lc.Color      = l.Color;
				lc.Direction  = l.Direction;
				lc.Intensity  = l.Intensity;
				lc.Radius     = l.Radius;
				lc.InnerAngle = l.InnerAngle;
				lc.OuterAngle = l.OuterAngle;
				lc.Type       = static_cast<uint32_t>(l.Type);
				cfg.Lights.push_back(lc);
			}
			WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
		}

		for (int i = 0; i < (int)m_Lights.size(); i++)
		{
			ImGui::PushID(i);
			auto& l = m_Lights[i];
			char label[32];
			snprintf(label, sizeof(label), "%s %d",
			         l.Type == LightType::Spot ? "Spot" : "Point", i);
			bool selected = (m_SelectedLight == i);
			if (ImGui::Selectable(label, selected))
				m_SelectedLight = i;
			if (m_SelectedLight == i)
			{
				ImGui::DragFloat3("Position",  &l.Position.x, 0.05f);
				ImGui::ColorEdit3("Color",     &l.Color.x);
				ImGui::DragFloat("Intensity",  &l.Intensity,  0.1f, 0.f, 100.f);
				ImGui::DragFloat("Radius",     &l.Radius,     0.1f, 0.1f, 50.f);
				if (l.Type == LightType::Spot)
				{
					ImGui::DragFloat3("Direction",   &l.Direction.x,  0.01f, -1.f, 1.f);
					ImGui::DragFloat("Inner Angle",  &l.InnerAngle,   0.5f,  0.f, 89.f);
					ImGui::DragFloat("Outer Angle",  &l.OuterAngle,   0.5f,  0.f, 89.f);
				}
				if (ImGui::SmallButton("Remove"))
				{
					m_Lights.erase(m_Lights.begin() + i);
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
		m_DebugLights = !m_DebugLights;

	if (e.GetKeyCode() == HMN_KEY_H)
		Renderer3D::SetDebugHeatmap(!Renderer3D::GetDebugHeatmap());

	if (e.GetKeyCode() == HMN_KEY_P)
		m_ShowPerfPanel = !m_ShowPerfPanel;

	return false;
}
