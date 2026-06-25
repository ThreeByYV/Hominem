#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"
#include "CutsceneLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/InputMap.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderSettings.h"
#include "Hominem/Renderer/DebugCommands.h"
#include "Hominem/Utils/MathUtils.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Cinematics/Cues/CameraCue.h"
#include "Hominem/Cinematics/Cues/FadeCue.h"
#include "Hominem/Cinematics/Cues/EventCue.h"
#include "Cinematics/IntroCutscene.h"
#include "Game/Actors/InfiniteFloorActor.h"
#include "Game/Actors/SceneActor.h"

#include <imgui.h>
#include "Hominem/ImGui/UI.h"

using namespace Hominem;

GameLayer::GameLayer()
	: Layer("Game")
{
}

void GameLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();

	if (!WorldConfig::LoadFromFile(WorldConfig::k_Path, m_Config))
		HMN_CORE_WARN("GameLayer: using default world config");

	auto& window = Application::Get().GetWindow();
	m_Aspect     = (float)window.GetWidth() / (float)window.GetHeight();
	m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());

	// --- world setup ---
	WorldConfig::ApplyToScene(m_Config, *m_Scene);

	const auto& sc = m_Config.Scene;
	m_Scene3D = &m_Scene->SpawnActor<SceneActor>(sc.MeshPath);
	sc.ApplyTo(*m_Scene3D);

	m_InitCameraX = m_Config.Camera.CameraX;
	m_InitCameraZ = m_Config.Camera.CameraZ;

	if (m_InitCameraX == 0.f || m_InitCameraZ == 0.f ||
	    m_Config.Player.Spawn.Position == glm::vec3(0.f, 1.f, 0.f))
		BootstrapFromAABB();

	Ref<SkinnedMesh> playerMesh{};
	if (const auto r =
		AssetManager::Load<SkinnedMesh>(Player::k_MeshPath))
		playerMesh = r->Get();

	m_Player = &m_Scene->SpawnActor<Player>(m_Config.Player, playerMesh);
	m_Player->Scale = m_Config.Player.Scale;

	const auto camCfg = SideScrollerCamera::Config::From(m_Config.Camera);

	m_Camera.Init(m_Scene->GetCamera(),
	              m_Scene->GetCameraPosition(),
	              m_Scene->GetCameraFront(),
	              m_Aspect, m_InitCameraZ, m_Config.Player.RestY, camCfg);
	m_Camera.SetTarget(m_Player);
	m_Scene->GetCameraPosition().x = m_InitCameraX;

	m_Scene->SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);
	m_Scene->BakeEnvironment(glm::vec3(0.1f, 1.5f, 27.0f), /*intensity=*/1.0f);
	m_Scene->SetClearColor({ 0.1f, 0.1f, 0.1f, 1.f });

	// --- intro cutscene ---
	m_IntroCtx.scene = m_Scene.get();

	if (s_SkipIntro)
	{
		m_Camera.Snap();

		s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
		s_EyeTarget.z = k_EyeTarget.z;

		const auto from = m_Scene->GetCameraSnapshot();
		m_Scene->ApplyCameraSnapshot({ s_EyeTarget, k_EyeZoom });

		// Zoom back out to the resting view -- no flash, no transition.
		m_IntroCutscene.Add<CameraCue>(s_EyeTarget, from.position, k_EyeZoom, from.orthoSize).For(k_ZoomDur);
		m_IntroCutscene.Play();
	}
	m_IntroTimer = 0.f;
	s_SkipIntro  = false;

	PostProcessSettings pp;
	pp.renderScale = RenderSettings::RecommendedRenderScale;
	m_Scene->SetPostProcess(pp);

	if (const auto r =
		AssetManager::Load<SoundBuffer>(k_MusicPath))
	{
		m_Music       = *r;
		m_MusicHandle = AudioSystem::Get().Play(m_Music, 0.9f, /*loop=*/true);
	}
}

void GameLayer::OnDetach()
{
	if (m_MusicHandle != InvalidSound) AudioSystem::Get().Stop(m_MusicHandle);
	m_Music       = {};
	m_MusicHandle = InvalidSound;
	m_Player  = nullptr;
	m_Scene3D = nullptr;
	m_Scene.reset();
}

void GameLayer::OnUpdate(Timestep ts)
{
	if (!m_IntroCutscene.IsPlaying())
	{
		m_Scene->OnUpdate(ts);

		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			if (WorldConfig newCfg; WorldConfig::LoadFromFile(WorldConfig::k_Path, newCfg))
			{
				HMN_CORE_INFO("Config reloaded — player scale({:.4f},{:.4f},{:.4f})",
				              newCfg.Player.Scale.x, newCfg.Player.Scale.y, newCfg.Player.Scale.z);
				m_Config = newCfg;
				m_Camera.GetConfig() = SideScrollerCamera::Config::From(newCfg.Camera);
				m_Camera.OnWindowResize(m_Aspect);
				m_Player->Reload(newCfg.Player);
				newCfg.Scene.ApplyTo(*m_Scene3D);
			}
			RenderSettings::RequestShaderReload();
		}

		m_Camera.OnUpdate(ts);

		// Still waiting to kick off the zoom-in (normal, non-skip path only --
		// the skip path's zoom-out cue is already playing by the time we get here).
		if (!m_IntroCutscene.IsFinished())
		{
			m_IntroTimer += ts;
			if (m_IntroTimer >= k_WaitDur)
				StartIntroZoomIn();
		}
	}
	else
	{
		m_IntroCutscene.OnUpdate(ts, m_IntroCtx);
	}

	m_FrameTimeMs = ts.GetMilliseconds();
	m_FPS         = ts > 0.f ? 1.f / ts : 0.f;
}

void GameLayer::StartIntroZoomIn()
{
	s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
	s_EyeTarget.z = k_EyeTarget.z;

	const auto from = m_Scene->GetCameraSnapshot();
	m_IntroCutscene.Add<CameraCue>(from.position, s_EyeTarget, from.orthoSize, k_EyeZoom).For(k_ZoomDur);
	// Hard cut to white (From == To == 1), not a fade.
	m_IntroCutscene.Add<FadeCue>(glm::vec3(1.f), 1.f, 1.f).At(k_ZoomDur).For(k_FlashDur);
	m_IntroCutscene.Add<EventCue>([this] { TransitionTo<CutsceneLayer>(IntroCutscene::Make()); })
		.At(k_ZoomDur + k_FlashDur);

	m_IntroCutscene.Play();
}

void GameLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	m_Scene->SetEnvMapIntensity(m_EnvMapEnabled ? m_EnvMapIntensity : 0.f);
	Layer::OnBuildRenderFrame(frame);
	m_IntroCutscene.BuildRenderFrame(frame);
}

void GameLayer::OnImGuiRender()
{
	if (m_ShowPerfPanel)
		UI::PerfPanel(m_FPS, m_FrameTimeMs, m_Scene.get());

	if (!m_ShowDebugUI) return;
	UI::Window("Settings", [&] {
		if (ImGui::CollapsingHeader("Lighting")) {
			UI::EditDirectionalLight(m_Scene->GetDirectionalLight());
			ImGui::Checkbox("IBL", &m_EnvMapEnabled);
			if (m_EnvMapEnabled)
				ImGui::SliderFloat("IBL Intensity", &m_EnvMapIntensity, 0.f, 5.f);
			ImGui::Checkbox("Area Lights", &RenderSettings::AreaLights);
		}

		if (ImGui::CollapsingHeader("Lights")) {
			auto& lights = m_Scene->GetLights();
			if (ImGui::SmallButton("Save"))
				WorldConfig::ModifyAndSave(WorldConfig::k_Path, [&lights](WorldConfig& cfg) {
					WorldConfig::CaptureLights(cfg, lights);
				});
			UI::EditLightList(lights, m_SelectedLight);
		}

		if (ImGui::CollapsingHeader("Render")) {
			auto& pp = m_Scene->GetPostProcess();
			ImGui::SliderFloat("Render Scale", &pp.renderScale, 0.25f, 1.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("%.0f%%", pp.renderScale * 100.f);
		}

		if (ImGui::CollapsingHeader("Post Processing")) UI::EditPostProcess(m_Scene->GetPostProcess());

		if (ImGui::CollapsingHeader("Camera"))
			ImGui::DragFloat("Y Bias", &m_Camera.GetConfig().YBias, 0.01f);

		if (ImGui::CollapsingHeader("Scene Transform")) {
			ImGui::PushID("SceneTx");
			UI::EditTransform(*m_Scene3D, 0.1f);
			if (ImGui::Button("Save Scene Transform"))
				WorldConfig::ModifyAndSave(WorldConfig::k_Path, [this](WorldConfig& cfg) {
					cfg.Scene.Position = m_Scene3D->Position;
					cfg.Scene.Rotation = m_Scene3D->GetRotationDeg();
					cfg.Scene.Scale    = m_Scene3D->Scale;
					cfg.Camera.CameraX = m_InitCameraX;
					cfg.Camera.CameraZ = m_Camera.GetCameraZ();
					m_Config = cfg;
				});
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Player")) {
			ImGui::PushID("Player");
			m_Player->OnImGuiRender();
			ImGui::PopID();
		}
	});
}

bool GameLayer::OnWindowResize(WindowResizeEvent& e)
{
	m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
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

	if (e.GetKeyCode() == InputMap::GetKeyCode("OpenMenu"))
	{
		TransitionTo<MenuLayer>();
		return true;
	}

	if (e.GetKeyCode() == InputMap::GetKeyCode("SkipIntro"))
	{
		s_SkipIntro = false;
		TransitionTo<CutsceneLayer>(IntroCutscene::Make());
		return true;
	}

	if (ImGui::GetIO().WantCaptureKeyboard)
		return false;

	HandleDebugKey(e.GetKeyCode(), m_Scene->GetPostProcess(), m_ShowPerfPanel);

	return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& e) { return false; }

glm::vec3 GameLayer::ResolveEyeTarget(const glm::vec3& fallback) const
{
	auto* mesh = m_Player->GetMesh();
	if (!mesh)    return fallback;
	auto mat = mesh->GetBoneWorldTransform("mixamorig:Head");
	if (!mat)     return fallback;
	return glm::vec3(m_Player->GetTransform() * glm::vec4(glm::vec3((*mat)[3]), 1.0f));
}

void GameLayer::BootstrapFromAABB()
{
	if (!m_Scene3D->GetMesh()) return;

	glm::vec3 wMin, wMax;
	TransformAABB(m_Scene3D->GetMesh()->GetAABBMin(), m_Scene3D->GetMesh()->GetAABBMax(),
	              m_Scene3D->GetTransform(), wMin, wMax);

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
