#include "hmnpch.h"
#include "GameLayer.h"
#include "MenuLayer.h"
#include "CutsceneLayer.h"

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
	: SceneLayer("Game")
{
}

SceneDesc GameLayer::Describe()
{
	if (!WorldConfig::LoadFromFile(WorldConfig::k_Path, m_Config))
		HMN_CORE_WARN("GameLayer: using default world config");

	return {
		.Physics     = { .Gravity = { 0.f, -9.81f, 0.f } },
		.Lights      = m_Config.Lights,
		.PostProcess = { .renderScale = 0.8f },
		.Spawners    = {
			[this](SceneContext& ctx) {
				m_Scene3D = &ctx.SpawnActor<SceneActor>(m_Config.Scene.MeshPath);
				m_Config.Scene.ApplyTo(*m_Scene3D);
				m_InitCameraX = m_Config.CameraX;
				m_InitCameraZ = m_Config.CameraZ;
				if (m_InitCameraX == 0.f || m_InitCameraZ == 0.f ||
				    m_Config.PlayerSpawnPos == glm::vec3(0.f, 1.f, 0.f))
					BootstrapFromAABB();
			},
			[this](SceneContext& ctx) {
				Ref<SkinnedMesh> mesh;
				if (auto r = ctx.Load<SkinnedMesh>(Player::k_MeshPath)) mesh = r->Get();
				PlayerConfig cfg;
				cfg.Spawn.Position = m_Config.PlayerSpawnPos;
				m_Player = &ctx.SpawnActor<Player>(cfg, mesh);
			},
			[this](SceneContext& ctx) { ctx.SpawnActor<InfiniteFloorActor>(m_Config.Floor); },
			[this](SceneContext& ctx) { ctx.BakeEnvironment({ 0.1f, 1.5f, 27.0f }); },
			[this](SceneContext& ctx) {
				if (auto r = ctx.Load<SoundBuffer>(k_MusicPath)) {
					m_Music       = *r;
					m_MusicHandle = ctx.audio.Play(m_Music, 0.9f, /*loop=*/true);
				}
			}
		}
	};
}

void GameLayer::OnSceneReady(SceneContext& ctx)
{
	m_Aspect = ctx.aspect;
	m_Camera.Init(ctx.scene.GetCamera(),
	              ctx.scene.GetCameraPosition(),
	              ctx.scene.GetCameraFront(),
	              ctx.aspect, m_InitCameraZ, PlayerConfig{}.RestY,
	              SideScrollerCamera::Config::From(CameraConfig{}));
	m_Camera.SetTarget(m_Player);
	ctx.scene.GetCameraPosition().x = m_InitCameraX;

	// --- intro cutscene ---
	m_IntroCtx.scene = &ctx.scene;

	if (s_SkipIntro)
	{
		m_Camera.Snap();

		s_EyeTarget   = ResolveEyeTarget(k_EyeTarget);
		s_EyeTarget.z = k_EyeTarget.z;

		const auto [position, orthoSize] = m_Scene->GetCameraSnapshot();
		m_Scene->ApplyCameraSnapshot({ s_EyeTarget, k_EyeZoom });

		m_IntroCutscene.Add<CameraCue>(s_EyeTarget, position, k_EyeZoom, orthoSize).For(k_ZoomDur);
		m_IntroCutscene.Play();
	}
	m_IntroTimer = 0.f;
	s_SkipIntro  = false;
}

void GameLayer::OnSceneDetach()
{
	if (m_MusicHandle != InvalidSound) AudioSystem::Get().Stop(m_MusicHandle);
	m_Music       = {};
	m_MusicHandle = InvalidSound;
	m_Player  = nullptr;
	m_Scene3D = nullptr;
}

void GameLayer::OnWindowResized(uint32_t w, uint32_t h)
{
	if (w > 0 && h > 0)
	{
		m_Aspect = (float)w / (float)h;
		m_Camera.OnWindowResize(m_Aspect);
	}
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
				HMN_CORE_INFO("Config reloaded");
				m_Config = newCfg;
				m_Camera.OnWindowResize(m_Aspect);
				m_Player->Reload(PlayerConfig{});
				newCfg.Scene.ApplyTo(*m_Scene3D);
				m_Scene->GetLights() = newCfg.Lights;
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
					cfg.Lights = lights;
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
					cfg.CameraX = m_InitCameraX;
					cfg.CameraZ = m_Camera.GetCameraZ();
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

void GameLayer::OnEvent(Event& e)
{
	SceneLayer::OnEvent(e);
	EventDispatcher dispatcher(e);
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
	if (m_Config.PlayerSpawnPos == glm::vec3(0.f, 1.f, 0.f))
	{
		m_Config.PlayerSpawnPos = {
			m_InitCameraX,
			PlayerConfig{}.RestY,
			wMin.z + size.z * 0.9f
		};
	}
}
