#include "hmnpch.h"
#include "SandboxLayer.h"
#include "MenuLayer.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderSettings.h"
#include "Game/Actors/InfiniteBackgroundActor.h"
#include "Game/Actors/InfiniteFloorActor.h"

#include "Hominem/ImGui/UI.h"

using namespace Hominem;

SandboxLayer::SandboxLayer()
	: SceneLayer("Sandbox")
{
}

SceneDesc SandboxLayer::Describe()
{
	return {
		.Camera  = { .OrthoSize = 10.f, .Near = -10.f, .Far = 10.f },
		.Physics = { .Gravity = { 0.f, -9.81f, 0.f } }
	};
}

void SandboxLayer::OnSceneReady(SceneContext& ctx)
{
	m_CinematicCamera = CreateRef<CinematicCameraController>(ctx.aspect);
	m_CinematicCamera->SetCameraTarget(&ctx.scene.GetCamera(), &ctx.scene.GetCameraPosition());

	if (!m_CinematicCamera->LoadFromFile("Resources/Config/camera_config.json",
	                                     "camera_sequences.level_01_vista"))
		HMN_CORE_WARN("SandboxLayer: camera_config.json not found, using defaults");

	m_CinematicCamera->RegisterOnComplete("vista_reveal",   [] { HMN_CORE_INFO("Vista 1 complete"); });
	m_CinematicCamera->RegisterOnComplete("vista_reveal_2", [] { HMN_CORE_INFO("Vista 2 complete"); });

	if (auto result = ctx.Load<Texture2D>("game://Textures/gamebg.png"))
		m_BgTexHandle = *result;
	ctx.SpawnActor<InfiniteBackgroundActor>(m_BgTexHandle.Get(), 20.f);
	ctx.SpawnActor<InfiniteFloorActor>(FloorConfig{});

	m_Player        = &ctx.SpawnActor<Player>(PlayerConfig{});
	m_NarrativeText = &ctx.SpawnActor<NarrativeTextActor>();
	m_NarrativeText->Show("Any victory against death will always be temporary");
}

void SandboxLayer::OnSceneDetach()
{
	m_Player        = nullptr;
	m_NarrativeText = nullptr;
}

void SandboxLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_1))
	{
		TransitionTo<MenuLayer>();
		return;
	}

	if (Input::IsKeyPressed(HMN_KEY_R))
	{
		RenderSettings::RequestShaderReload();
		m_Player->ReloadShader();
	}

	const glm::vec2 playerPos = { m_Player->Position.x, m_Player->Position.y };

	m_CinematicCamera->UpdateCameraForPlayer(playerPos);
	m_CinematicCamera->OnUpdate(ts);

	m_Scene->OnUpdate(ts);
}

void SandboxLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	Layer::OnBuildRenderFrame(frame);
}

void SandboxLayer::OnImGuiRender()
{
	UI::Window("Settings", [&] {
		m_Player->OnImGuiRender();

		ImGui::Separator();
		ImGui::TextUnformatted("Camera");

		if (ImGui::DragFloat("Ortho Size", &m_OrthoSize, 0.5f, 1.f, 50.f))
			m_CinematicCamera->SetZoomLevel(m_OrthoSize);

		ImGui::Text("In Cinematic: %s", m_CinematicCamera->IsInCinematic() ? "YES" : "NO");

		float smoothing = m_CinematicCamera->GetSmoothingFactor();
		if (ImGui::SliderFloat("Smoothing", &smoothing, 0.01f, 1.f))
			m_CinematicCamera->SetSmoothingFactor(smoothing);

		float zoom = m_CinematicCamera->GetZoomLevel();
		if (ImGui::SliderFloat("Zoom", &zoom, 1.f, 30.f))
			m_CinematicCamera->SetZoomLevel(zoom);

		if (ImGui::Button("Trigger Vista"))
			m_CinematicCamera->TriggerCinematic("vista_reveal");
		ImGui::SameLine();
		if (ImGui::Button("End Cinematic"))
			m_CinematicCamera->EndCinematic();
	});
}

void SandboxLayer::OnEvent(Event& e)
{
	SceneLayer::OnEvent(e);
	m_CinematicCamera->OnEvent(e);
}
