#include "hmnpch.h"
#include "SandboxLayer.h"
#include "MenuLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderSettings.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsTypes.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/Actors/InfiniteBackgroundActor.h"
#include "Game/Actors/InfiniteFloorActor.h"

#include "Hominem/ImGui/UI.h"

using namespace Hominem;

SandboxLayer::SandboxLayer()
	: Layer("Sandbox")
{
	if (!WorldConfig::LoadFromFile("Resources/Config/game_config.json", m_Config))
		HMN_CORE_WARN("SandboxLayer: using default world config");
}

void SandboxLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();
	m_Scene->SetClearColor({ 0.1f, 0.1f, 0.1f, 1.f });

	InitPhysics();
	InitCamera();
	InitWorld();

	m_CinematicCamera->RegisterOnComplete("vista_reveal",   [] { HMN_CORE_INFO("Vista 1 complete"); });
	m_CinematicCamera->RegisterOnComplete("vista_reveal_2", [] { HMN_CORE_INFO("Vista 2 complete"); });

	m_Player = &m_Scene->SpawnActor<Player>(m_Config.Player);

	m_NarrativeText = &m_Scene->SpawnActor<NarrativeTextActor>();
	m_NarrativeText->Show("Any victory against death will always be temporary");
}

void SandboxLayer::OnDetach()
{
	m_Player        = nullptr;
	m_NarrativeText = nullptr;
	m_Scene.reset();
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
		if (m_Player) m_Player->ReloadShader();
	}

	glm::vec2 playerPos = m_Player
		? glm::vec2(m_Player->Position.x, m_Player->Position.y)
		: glm::vec2(0.f);

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
		if (m_Player) m_Player->OnImGuiRender();

		ImGui::Separator();
		ImGui::TextUnformatted("Camera");

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
	});
}

bool SandboxLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (m_Scene)
		m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
	return false;
}

void SandboxLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(SandboxLayer::OnWindowResize));
	if (m_CinematicCamera)
		m_CinematicCamera->OnEvent(e);
}

void SandboxLayer::InitPhysics()
{
	m_Scene->SetPhysicsWorld(CreateRef<PhysicsWorld>(m_Config.Physics.Gravity));
}

void SandboxLayer::InitCamera()
{
	auto& window = Application::Get().GetWindow();
	uint32_t w   = window.GetWidth();
	uint32_t h   = window.GetHeight();

	m_Scene->GetCamera().SetOrthographic(
		m_Config.Camera.OrthoSize,
		m_Config.Camera.OrthoNear,
		m_Config.Camera.OrthoFar);

	m_Scene->OnViewportResize(w, h);

	m_CinematicCamera = CreateRef<CinematicCameraController>((float)w / (float)h);
	m_CinematicCamera->SetCameraTarget(&m_Scene->GetCamera(), &m_Scene->GetCameraPosition());

	if (!m_CinematicCamera->LoadFromFile("Resources/Config/camera_config.json",
	                                     "camera_sequences.level_01_vista"))
		HMN_CORE_WARN("SandboxLayer: camera_config.json not found, using defaults");
}

void SandboxLayer::InitWorld()
{
	if (auto result = AssetManager::Load<Texture2D>("game://Textures/gamebg.png"))
		m_BgTexHandle = *result;
	m_Scene->SpawnActor<InfiniteBackgroundActor>(m_BgTexHandle.Get(), 20.f);

	m_Scene->SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);
}
