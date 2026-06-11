#include "hmnpch.h"
#include "PlayLevel.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsTypes.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/Actors/InfiniteBackgroundActor.h"
#include "Game/Actors/InfiniteFloorActor.h"

#include <imgui.h>

using namespace Hominem;

PlayLevel::PlayLevel()
{
	if (!WorldConfig::LoadFromFile("Resources/Config/game_config.json", m_Config))
		HMN_CORE_WARN("PlayLevel: using default world config");
}

void PlayLevel::OnEnter(Scene& scene)
{
	InitPhysics(scene);
	InitCamera(scene);
	InitWorld(scene);

	m_CinematicCamera->RegisterOnComplete("vista_reveal",   [] { HMN_CORE_INFO("Vista 1 complete"); });
	m_CinematicCamera->RegisterOnComplete("vista_reveal_2", [] { HMN_CORE_INFO("Vista 2 complete"); });

	m_Player = &scene.SpawnActor<Player>(m_Config.Player);

	m_NarrativeText = &scene.SpawnActor<NarrativeTextActor>();
	m_NarrativeText->Show("Any victory against death will always be temporary");
}

void PlayLevel::OnExit()
{
	m_Player        = nullptr;
	m_NarrativeText = nullptr;
}

void PlayLevel::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_R))
	{
		Renderer2D::GetShaderLibrary()->ReloadAll();
		Renderer3D::GetShaderLibrary()->ReloadAll();
		if (m_Player) m_Player->ReloadShader();
	}

	glm::vec2 playerPos = m_Player
		? glm::vec2(m_Player->Position.x, m_Player->Position.y)
		: glm::vec2(0.f);

	m_CinematicCamera->UpdateCameraForPlayer(playerPos);
	m_CinematicCamera->OnUpdate(ts);
}

void PlayLevel::OnImGuiRender()
{
	if (m_Player) m_Player->OnImGuiRender();

	ImGui::Separator();
	ImGui::Text("Camera");

	if (ImGui::DragFloat("Ortho Size", &m_OrthoSize, 0.5f, 1.f, 50.f))
		m_CinematicCamera->SetZoomLevel(m_OrthoSize);

	if (m_CinematicCamera)
	{
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
	}
}

void PlayLevel::OnEvent(Event& e)
{
	if (m_CinematicCamera)
		m_CinematicCamera->OnEvent(e);
}

void PlayLevel::InitPhysics(Scene& scene)
{
	scene.SetPhysicsWorld(CreateRef<PhysicsWorld>(m_Config.Physics.Gravity));
}

void PlayLevel::InitCamera(Scene& scene)
{
	auto& window = Application::Get().GetWindow();
	uint32_t w = window.GetWidth();
	uint32_t h = window.GetHeight();

	scene.GetCamera().SetOrthographic(
		m_Config.Camera.OrthoSize,
		m_Config.Camera.OrthoNear,
		m_Config.Camera.OrthoFar);

	scene.OnViewportResize(w, h);

	m_CinematicCamera = CreateRef<CinematicCameraController>((float)w / (float)h);
	m_CinematicCamera->SetCameraTarget(&scene.GetCamera(), &scene.GetCameraPosition());

	if (!m_CinematicCamera->LoadFromFile("Resources/Config/camera_config.json",
	                                     "camera_sequences.level_01_vista"))
		HMN_CORE_WARN("PlayLevel: camera_config.json not found, using defaults");
}

void PlayLevel::InitWorld(Scene& scene)
{
	auto bgTex = Texture2D::Create("Resources/Textures/gamebg.png");
	scene.SpawnActor<InfiniteBackgroundActor>(bgTex, 20.f /*tileWidth*/);

	const auto& fc = m_Config.Physics.Floor;
	scene.SpawnActor<InfiniteFloorActor>(fc);
}
