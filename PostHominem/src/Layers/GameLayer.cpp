#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"
#include "CutsceneLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderSettings.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Game/Actors/InfiniteFloorActor.h"
#include "Game/Actors/SceneActor.h"

#include <imgui.h>
#include "Hominem/ImGui/UIHelpers.h"

using namespace Hominem;

GameLayer::GameLayer()
	: Layer("Game")
{
}

void GameLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();

	if (!WorldConfig::LoadFromFile("Resources/Config/game_config.json", m_Config))
		HMN_CORE_WARN("GameLayer: using default world config");

	auto& window = Application::Get().GetWindow();
	m_Aspect     = (float)window.GetWidth() / (float)window.GetHeight();
	m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());

	// --- world setup ---
	m_Scene->SetPhysicsWorld(CreateRef<PhysicsWorld>(m_Config.Physics.Gravity));

	const auto& sc = m_Config.Scene;
	m_Scene3D = &m_Scene->SpawnActor<SceneActor>(sc.MeshPath);
	m_Scene3D->Position = sc.Position;
	m_Scene3D->Rotation = glm::radians(sc.Rotation);
	m_Scene3D->Scale    = sc.Scale;

	m_InitCameraX = m_Config.Camera.CameraX;
	m_InitCameraZ = m_Config.Camera.CameraZ;

	if (m_InitCameraX == 0.f || m_InitCameraZ == 0.f ||
	    m_Config.Player.Spawn.Position == glm::vec3(0.f, 1.f, 0.f))
		BootstrapFromAABB();

	m_Player = &m_Scene->SpawnActor<Player>(m_Config.Player, Player::PreloadedMesh());
	if (m_Player)
		m_Player->Scale = m_Config.Player.Scale;

	SideScrollerCamera::Config camCfg;
	camCfg.VisibleHeight = m_Config.Camera.VisibleHeight;
	camCfg.PlayerScreenY = m_Config.Camera.PlayerScreenY;
	camCfg.YBias         = m_Config.Camera.YBias;
	camCfg.FOVDeg        = m_Config.Camera.FOVDeg;
	camCfg.XSpeed        = m_Config.Camera.XSpeed;
	camCfg.YSpeed        = m_Config.Camera.YSpeed;
	camCfg.LeadStrength  = m_Config.Camera.LeadStrength;
	camCfg.YDeadZone     = m_Config.Camera.YDeadZone;

	m_Camera.Init(m_Scene->GetCamera(),
	              m_Scene->GetCameraPosition(),
	              m_Scene->GetCameraFront(),
	              m_Aspect, m_InitCameraZ, m_Config.Player.RestY, camCfg);
	m_Camera.SetTarget(m_Player);
	m_Scene->GetCameraPosition().x = m_InitCameraX;

	m_Scene->SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);
	m_Scene->BakeEnvironment(glm::vec3(0.1f, 1.5f, 27.0f), /*intensity=*/1.0f);
	m_Scene->SetClearColor({ 0.1f, 0.1f, 0.1f, 1.f });

	// --- intro animation ---
	if (s_SkipIntro)
	{
		m_Camera.Snap();

		s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
		s_EyeTarget.z = k_EyeTarget.z;

		m_IntroFromPos  = m_Scene->GetCameraPosition();
		m_IntroFromZoom = m_Scene->GetCamera().GetOrthographicSize();

		m_Scene->GetCameraPosition() = s_EyeTarget;
		m_Scene->GetCamera().SetOrthographicSize(k_EyeZoom);
		m_IntroPhase = IntroPhase::ZoomOut;
	}
	else
	{
		m_IntroFromPos  = m_Scene->GetCameraPosition();
		m_IntroFromZoom = m_Scene->GetCamera().GetOrthographicSize();
		m_IntroPhase    = IntroPhase::Wait;
	}
	m_IntroTimer = 0.f;
	s_SkipIntro  = false;

	PostProcessSettings pp;
	pp.renderScale = RenderSettings::RecommendedRenderScale;
	m_Scene->SetPostProcess(pp);

	WorldConfig::ApplyLights(m_Config, m_Scene->GetLights());

	auto& audio = Application::Get().GetAudioSystem();
	audio.LoadMusicAsync("Resources/Sounds/menu_music_2.mp3", /*autoPlay=*/true, 0.9f, /*loop=*/true);
}

void GameLayer::OnDetach()
{
	Application::Get().GetAudioSystem().StopMusic();
	m_Player  = nullptr;
	m_Scene3D = nullptr;
	m_Scene.reset();
}

void GameLayer::OnUpdate(Timestep ts)
{
	Application::Get().GetAudioSystem().UpdateMusic();

	if (m_IntroPhase == IntroPhase::Done || m_IntroPhase == IntroPhase::Wait)
	{
		m_Scene->OnUpdate(ts);

		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			if (WorldConfig newCfg; WorldConfig::LoadFromFile("Resources/Config/game_config.json", newCfg))
			{
				HMN_CORE_INFO("Config reloaded — player scale({:.4f},{:.4f},{:.4f})",
				              newCfg.Player.Scale.x, newCfg.Player.Scale.y, newCfg.Player.Scale.z);
				m_Config = newCfg;
				auto& cam         = m_Camera.GetConfig();
				cam.VisibleHeight  = newCfg.Camera.VisibleHeight;
				cam.PlayerScreenY  = newCfg.Camera.PlayerScreenY;
				cam.YBias          = newCfg.Camera.YBias;
				cam.FOVDeg         = newCfg.Camera.FOVDeg;
				cam.XSpeed         = newCfg.Camera.XSpeed;
				cam.YSpeed         = newCfg.Camera.YSpeed;
				cam.LeadStrength   = newCfg.Camera.LeadStrength;
				cam.YDeadZone      = newCfg.Camera.YDeadZone;
				m_Camera.OnWindowResize(m_Aspect);
				if (m_Player) m_Player->Reload(newCfg.Player);
				if (m_Scene3D)
				{
					m_Scene3D->Position = newCfg.Scene.Position;
					m_Scene3D->Rotation = glm::radians(newCfg.Scene.Rotation);
					m_Scene3D->Scale    = newCfg.Scene.Scale;
				}
			}
			RenderSettings::RequestShaderReload();
		}

		m_Camera.OnUpdate(ts);

		if (m_IntroPhase == IntroPhase::Wait)
		{
			m_IntroTimer += ts;
			if (m_IntroTimer >= k_WaitDur)
			{
				s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
				s_EyeTarget.z = k_EyeTarget.z;

				m_IntroFromPos  = m_Scene->GetCameraPosition();
				m_IntroFromZoom = m_Scene->GetCamera().GetOrthographicSize();
				m_IntroPhase    = IntroPhase::ZoomIn;
				m_IntroTimer    = 0.f;
			}
		}
	}
	else if (m_IntroPhase == IntroPhase::ZoomIn)
	{
		m_IntroTimer += ts;
		const float n    = glm::clamp(m_IntroTimer / k_ZoomDur, 0.f, 1.f);
		const float ease = n * n * (3.f - 2.f * n);

		m_Scene->GetCameraPosition() = glm::mix(m_IntroFromPos, s_EyeTarget, ease);
		m_Scene->GetCamera().SetOrthographicSize(glm::mix(m_IntroFromZoom, k_EyeZoom, ease));

		if (n >= 1.f)
		{
			m_IntroPhase = IntroPhase::Flash;
			m_IntroTimer = 0.f;
		}
	}
	else if (m_IntroPhase == IntroPhase::Flash)
	{
		m_IntroTimer += ts;
		if (m_IntroTimer >= k_FlashDur)
		{
			m_IntroPhase = IntroPhase::Done;
			TransitionTo<CutsceneLayer>();
		}
	}
	else if (m_IntroPhase == IntroPhase::ZoomOut)
	{
		m_IntroTimer += ts;
		const float n    = glm::clamp(m_IntroTimer / k_ZoomDur, 0.f, 1.f);
		const float ease = n * n * (3.f - 2.f * n);

		m_Scene->GetCameraPosition() = glm::mix(s_EyeTarget, m_IntroFromPos, ease);
		m_Scene->GetCamera().SetOrthographicSize(glm::mix(k_EyeZoom, m_IntroFromZoom, ease));

		if (n >= 1.f)
			m_IntroPhase = IntroPhase::Done;
	}

	m_FrameTimeMs = ts.GetMilliseconds();
	m_FPS         = ts > 0.f ? 1.f / ts : 0.f;
}

void GameLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	m_Scene->SetEnvMapIntensity(m_EnvMapEnabled ? m_EnvMapIntensity : 0.f);
	Layer::OnBuildRenderFrame(frame);

	if (m_IntroPhase == IntroPhase::Flash)
	{
		QuadDraw q;
		q.transform = glm::scale(glm::mat4(1.f), { 1000.f, 1000.f, 1.f });
		q.color     = { 1.f, 1.f, 1.f, 1.f };
		frame.quads.push_back(std::move(q));
	}
}

void GameLayer::OnImGuiRender()
{
	if (m_ShowPerfPanel)
		UI::PerfPanel(m_FPS, m_FrameTimeMs, m_Scene.get());

	if (!m_ShowDebugUI) return;
	UI::Window("Settings", [&] {
		UI::Collapsible("Lighting", [&] {
			UI::EditDirectionalLight(m_Scene->GetDirectionalLight());
			UI::Checkbox("IBL", m_EnvMapEnabled);
			if (m_EnvMapEnabled)
				UI::Slider("IBL Intensity", m_EnvMapIntensity, 0.f, 5.f);
			if (UI::Checkbox("Area Lights", RenderSettings::AreaLights)) {}
		});

		UI::Collapsible("Lights", [&] {
			auto& lights = m_Scene->GetLights();
			if (UI::SmallButton("Save"))
			{
				WorldConfig cfg;
				WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg);
				WorldConfig::CaptureLights(cfg, lights);
				WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
			}
			UI::EditLightList(lights, m_SelectedLight);
		});

		UI::Collapsible("Render", [&] {
			auto& pp = m_Scene->GetPostProcess();
			UI::Slider("Render Scale", pp.renderScale, 0.25f, 1.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("%.0f%%", pp.renderScale * 100.f);
		});

		UI::Collapsible("Post Processing", [&] { UI::EditPostProcess(m_Scene->GetPostProcess()); });

		UI::Collapsible("Camera", [&] {
			UI::Drag("Y Bias", m_Camera.GetConfig().YBias, 0.01f);
		});

		if (m_Scene3D)
		{
			UI::Collapsible("Scene Transform", [&] {
				UI::EditTransform(*m_Scene3D, 0.1f);
				if (UI::Button("Save Scene Transform"))
				{
					WorldConfig cfg;
					WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg);
					cfg.Scene.Position = m_Scene3D->Position;
					cfg.Scene.Rotation = glm::degrees(m_Scene3D->Rotation);
					cfg.Scene.Scale    = m_Scene3D->Scale;
					cfg.Camera.CameraX = m_InitCameraX;
					cfg.Camera.CameraZ = m_Camera.GetCameraZ();
					WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
					m_Config = cfg;
				}
			});
		}

		if (m_Player)
			UI::Collapsible("Player", [&] { m_Player->OnImGuiRender(); });
	});
}

bool GameLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (m_Scene) m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
	if (e.GetWidth() > 0 && e.GetHeight() > 0)
	{
		m_Aspect = (float)e.GetWidth() / (float)e.GetHeight();
		m_Camera.OnWindowResize(m_Aspect);
	}
	return false;
}

void GameLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(GameLayer::OnWindowResize));
	dispatcher.Dispatch<KeyPressedEvent>(HMN_BIND_EVENT_FN(GameLayer::OnKeyPressed));
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

	if (e.GetKeyCode() == HMN_KEY_1)
	{
		s_SkipIntro = false;
		TransitionTo<CutsceneLayer>();
		return true;
	}

	if (ImGui::GetIO().WantCaptureKeyboard)
		return false;

	if (e.GetKeyCode() == HMN_KEY_N)
		RenderSettings::DrawNormals = !RenderSettings::DrawNormals;

	if (e.GetKeyCode() == HMN_KEY_B)
		RenderSettings::DrawAABB = !RenderSettings::DrawAABB;

	if (e.GetKeyCode() == HMN_KEY_L)
		m_Scene->GetPostProcess().debugLights = !m_Scene->GetPostProcess().debugLights;

	if (e.GetKeyCode() == HMN_KEY_H)
		RenderSettings::DebugHeatmap = !RenderSettings::DebugHeatmap;

	if (e.GetKeyCode() == HMN_KEY_T)
		RenderSettings::ToonShading = !RenderSettings::ToonShading;

	if (e.GetKeyCode() == HMN_KEY_O)
		RenderSettings::DrawBoneWeights = !RenderSettings::DrawBoneWeights;

	if (e.GetKeyCode() == HMN_KEY_LEFT_BRACKET && RenderSettings::DrawBoneWeights)
		RenderSettings::DisplayBoneIndex = std::max(0, RenderSettings::DisplayBoneIndex - 1);

	if (e.GetKeyCode() == HMN_KEY_RIGHT_BRACKET && RenderSettings::DrawBoneWeights)
		RenderSettings::DisplayBoneIndex++;

	if (e.GetKeyCode() == HMN_KEY_P)
		m_ShowPerfPanel = !m_ShowPerfPanel;

	return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& e) { return false; }

glm::vec3 GameLayer::ResolveEyeTarget(const glm::vec3& fallback) const
{
	if (!m_Player) return fallback;
	auto* mesh = m_Player->GetMesh();
	if (!mesh)    return fallback;
	auto mat = mesh->GetBoneWorldTransform("mixamorig:Head");
	if (!mat)     return fallback;
	return glm::vec3(m_Player->GetTransform() * glm::vec4(glm::vec3((*mat)[3]), 1.0f));
}

void GameLayer::BootstrapFromAABB()
{
	if (!m_Scene3D->GetMesh()) return;
	glm::vec3 lMin = m_Scene3D->GetMesh()->GetAABBMin();
	glm::vec3 lMax = m_Scene3D->GetMesh()->GetAABBMax();
	glm::mat4 tx   = m_Scene3D->GetTransform();
	glm::vec3 wMin(FLT_MAX), wMax(-FLT_MAX);
	for (int i = 0; i < 8; i++)
	{
		glm::vec3 c = glm::vec3(tx * glm::vec4(
			(i & 1) ? lMax.x : lMin.x,
			(i & 2) ? lMax.y : lMin.y,
			(i & 4) ? lMax.z : lMin.z, 1.f));
		wMin = glm::min(wMin, c);
		wMax = glm::max(wMax, c);
	}
	glm::vec3 size = wMax - wMin;
	if (m_InitCameraX == 0.f) m_InitCameraX = (wMin.x + wMax.x) * 0.5f;
	if (m_InitCameraZ == 0.f) m_InitCameraZ = m_Camera.ComputeCameraZ(wMax.z);
	if (m_Config.Player.Spawn.Position == glm::vec3(0.f, 1.f, 0.f))
	{
		m_Config.Player.Spawn.Position = {
			m_InitCameraX,
			m_Config.Player.RestY,
			wMin.z + size.z * 0.9f
		};
	}
}
