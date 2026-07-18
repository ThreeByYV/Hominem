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

	const float scale = (m_Config.RenderScale > 0.f)
	    ? m_Config.RenderScale
	    : RenderSettings::RecommendedRenderScale;

	return {
		.Physics     = { .Gravity = { 0.f, -9.81f, 0.f } },
		.Lights      = m_Config.Lights,
		.PostProcess = { .renderScale = scale },
		.Spawners    = {
			[this](SceneContext& ctx) {
				m_Scene3D = &ctx.SpawnActor<SceneActor>(m_Config.Scene.MeshPath);
				m_Config.Scene.ApplyTo(*m_Scene3D);
				m_InitCameraX = m_Config.CameraX;
				m_InitCameraZ = m_Config.CameraZ;
				if (!m_InitCameraX || !m_InitCameraZ ||
				    m_Config.PlayerSpawnPos == glm::vec3(0.f, 1.f, 0.f))
					BootstrapFromAABB();
			},
			[this](SceneContext& ctx) {
				Ref<SkinnedMesh> mesh;
				if (const auto r = ctx.Load<SkinnedMesh>(Player::k_MeshPath)) mesh = r->Get();
				PlayerConfig cfg;
				cfg.Spawn.Position = m_Config.PlayerSpawnPos;
				m_Player = &ctx.SpawnActor<Player>(cfg, mesh);
			},
			[this](SceneContext& ctx) { ctx.SpawnActor<InfiniteFloorActor>(m_Config.Floor); },
			[this](SceneContext& ctx) { ctx.BakeEnvironment({ 0.1f, 1.5f, 27.0f }); },
			[this](SceneContext& ctx) {
				m_NarrativeText = &ctx.SpawnActor<NarrativeTextActor>();
				m_NarrativeText->ScreenSpace  = true;
				m_NarrativeText->ScreenOffset = { 0.f, 0.75f, 0.f };
				m_NarrativeText->PanelSize    = { 1.7f, 0.3f };
				m_NarrativeText->TextScale    = 0.06f;
				m_NarrativeText->PanelAlpha   = 0.65f;
				m_NarrativeText->FadeDuration = 0.25f;
			},
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
	m_Camera.Init(ctx.scene.GetCamera(), ctx.scene.GetCameraPosition(), ctx.scene.GetCameraFront(),
	              ctx.aspect, m_InitCameraZ.value_or(0.f), PlayerConfig{}.RestY,
	              SideScrollerCamera::Config::From(CameraConfig{}));
	m_Camera.SetTarget(m_Player);
	ctx.scene.GetCameraPosition().x = m_InitCameraX.value_or(0.f);

	m_Camera.AddVistaTrigger(1.f, { 1.f, 3.f, m_Camera.GetCameraZ() }, 1.5f);

	m_Intro = std::make_unique<IntroSequence>(*m_Player, m_Camera);
	m_Intro->SetOnFinished([this] { TransitionTo<CutsceneLayer>(IntroCutscene::Make()); });
	m_Intro->Start(ctx.scene, s_SkipIntro);
	s_SkipIntro = false;
}

void GameLayer::OnSceneDetach()
{
	if (m_MusicHandle != InvalidSound) AudioSystem::Get().Stop(m_MusicHandle);
	m_Music       = {};
	m_MusicHandle = InvalidSound;
	m_Player  = nullptr;
	m_Scene3D = nullptr;
	m_NarrativeText = nullptr;
	m_Intro.reset();
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
	if (!m_Intro->IsPlaying())
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

		const bool wasInVista = m_Camera.IsInVista();
		m_Camera.OnUpdate(ts);
		if (wasInVista && !m_Camera.IsInVista())
			m_NarrativeText->Show("Any victory against death will always be temporary");
	}

	m_Intro->OnUpdate(ts);

	m_FrameTimeMs = ts.GetMilliseconds();
	m_FPS         = ts > 0.f ? 1.f / ts : 0.f;
}

void GameLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	m_Scene->SetEnvMapIntensity(m_EnvMapEnabled ? m_EnvMapIntensity : 0.f);
	Layer::OnBuildRenderFrame(frame);
	m_Intro->OnBuildRenderFrame(frame);
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
			if (ImGui::SmallButton("Save##rs"))
				WorldConfig::ModifyAndSave(WorldConfig::k_Path, [scale = pp.renderScale, this](WorldConfig& cfg) {
					cfg.RenderScale    = scale;
					m_Config.RenderScale = scale;
				});
			ImGui::SliderFloat("Render Scale", &pp.renderScale, 0.25f, 1.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("%.0f%%", pp.renderScale * 100.f);
		}

		if (ImGui::CollapsingHeader("Post Processing")) UI::EditPostProcess(m_Scene->GetPostProcess());

		if (ImGui::CollapsingHeader("Camera"))
			ImGui::DragFloat("Y Bias", &m_Camera.GetConfig().YBias, 0.01f);

		if (ImGui::CollapsingHeader("Narrative Text"))
		{
			if (ImGui::Button("Show Test Line"))
				m_NarrativeText->Show("Any victory against death will always be temporary");
			ImGui::SameLine();
			if (ImGui::Button("Hide")) m_NarrativeText->Hide();

			ImGui::DragFloat3("Screen Offset", &m_NarrativeText->ScreenOffset.x, 0.01f);
			ImGui::DragFloat2("Panel Size",    &m_NarrativeText->PanelSize.x,    0.01f, 0.f, 4.f);
			ImGui::DragFloat ("Text Scale",    &m_NarrativeText->TextScale,      0.005f, 0.f, 1.f);
			ImGui::DragFloat ("Panel Alpha",   &m_NarrativeText->PanelAlpha,     0.01f, 0.f, 1.f);
		}

		if (ImGui::CollapsingHeader("Scene Transform")) {
			ImGui::PushID("SceneTx");
			UI::EditTransform(*m_Scene3D, 0.1f);
			if (ImGui::Button("Save Scene Transform"))
				WorldConfig::ModifyAndSave(WorldConfig::k_Path, [this](WorldConfig& cfg) {
					cfg.Scene.Position = m_Scene3D->Position;
					cfg.Scene.Rotation = m_Scene3D->GetRotationDeg();
					cfg.Scene.Scale    = m_Scene3D->Scale;
					cfg.CameraX = m_InitCameraX;
					cfg.CameraZ = m_InitCameraZ;
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

	// if (e.GetKeyCode() == InputMap::GetKeyCode("SkipIntro"))
	// {
	// 	s_SkipIntro = false;
	// 	TransitionTo<CutsceneLayer>(IntroCutscene::Make());
	// 	return true;
	// }

	if (ImGui::GetIO().WantCaptureKeyboard)
		return false;

	HandleDebugKey(e.GetKeyCode(), m_Scene->GetPostProcess(), m_ShowPerfPanel);

	return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& e) { return false; }

void GameLayer::BootstrapFromAABB()
{
	if (!m_Scene3D->GetMesh()) return;

	glm::vec3 wMin, wMax;
	TransformAABB(m_Scene3D->GetMesh()->GetAABBMin(), m_Scene3D->GetMesh()->GetAABBMax(),
	              m_Scene3D->GetTransform(), wMin, wMax);

	glm::vec3 size = wMax - wMin;
	if (!m_InitCameraX) m_InitCameraX = (wMin.x + wMax.x) * 0.5f;
	if (!m_InitCameraZ) m_InitCameraZ = m_Camera.ComputeCameraZ(wMax.z);
	if (m_Config.PlayerSpawnPos == glm::vec3(0.f, 1.f, 0.f))
	{
		m_Config.PlayerSpawnPos = {
			*m_InitCameraX,
			PlayerConfig{}.RestY,
			wMin.z + size.z * 0.9f
		};
	}
}
