#pragma once

#include "Level.h"
#include "Player.h"
#include "SideScrollerCamera.h"
#include "Game/Actors/InfiniteFloorActor.h"
#include "Game/Actors/SceneActor.h"
#include "Game/WorldConfig.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Events/ApplicationEvent.h"

#include <imgui.h>

class FactoryLevel : public Level
{
public:
	FactoryLevel()
	{
		if (!WorldConfig::LoadFromFile("Resources/Config/game_config.json", m_Config))
			HMN_CORE_WARN("FactoryLevel: using default world config");
	}

	void OnEnter(Hominem::Scene& scene) override
	{
		using namespace Hominem;

		scene.SetPhysicsWorld(CreateRef<PhysicsWorld>(m_Config.Physics.Gravity));

		auto& window  = Application::Get().GetWindow();
		float aspect  = (float)window.GetWidth() / (float)window.GetHeight();
		scene.OnViewportResize(window.GetWidth(), window.GetHeight());

		const auto& sc = m_Config.Scene;
		m_Scene3D = &scene.SpawnActor<SceneActor>(sc.MeshPath);
		m_Scene3D->Position = sc.Position;
		m_Scene3D->Rotation = glm::radians(sc.Rotation);
		m_Scene3D->Scale    = sc.Scale;

		if (m_Scene3D->GetMesh())
		{
			// ── Factory world AABB ───────────────────────────────────────────────
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
			HMN_CORE_INFO("FactoryLevel: AABB min({:.2f},{:.2f},{:.2f}) max({:.2f},{:.2f},{:.2f})",
				wMin.x, wMin.y, wMin.z, wMax.x, wMax.y, wMax.z);

			scene.SetWorldTransform(glm::translate(glm::mat4(1.f), wMin));

			// ── Player spawn & floor ─────────────────────────────────────────────
			// k_PlayerRestY: desired world-space Y of the player's body when standing.
			// The physics floor is derived from this so the player always rests here.
			constexpr float k_PlayerRestY = 0.06f;

			const auto& col  = m_Config.Player.Collider;
			float floorTop   = k_PlayerRestY - glm::abs(col.Offset.y) - col.Extents.y;
			m_Config.Physics.Floor.Position.y =
				floorTop - m_Config.Physics.Floor.Scale.y * 0.5f;

			float playerWorldY = k_PlayerRestY;
			float spawnX       = (wMin.x + wMax.x) * 0.5f + size.x * 0.15f;
			m_Config.Physics.Floor.Position.x = spawnX;

			PlayerConfig pc   = m_Config.Player;
			pc.Spawn.Position = { spawnX - wMin.x, k_PlayerRestY - wMin.y, size.z * 0.9f };
			m_Player = &scene.SpawnActor<Player>(pc);

			// ── Camera ───────────────────────────────────────────────────────────
			SideScrollerCamera::Config camCfg;
			camCfg.VisibleHeight = 2.0f;
			camCfg.PlayerScreenY = 0.25f;
			camCfg.YBias         = 0.280f;
			camCfg.FOVDeg        = 30.f;
			camCfg.XSpeed        = 0.12f;
			camCfg.YSpeed        = 0.05f;
			camCfg.LeadStrength  = 0.45f;
			camCfg.YDeadZone     = 0.15f;

			float cz = m_Camera.ComputeCameraZ(wMax.z);
			m_Camera.Init(scene.GetCamera(),
			              scene.GetCameraPosition(),
			              scene.GetCameraFront(),
			              aspect, cz, playerWorldY, camCfg);
			m_Camera.SetTarget(m_Player);
			scene.GetCameraPosition().x = spawnX;
		}
		else
		{
			// Fallback: no mesh loaded.
			SideScrollerCamera::Config camCfg;
			m_Camera.Init(scene.GetCamera(),
			              scene.GetCameraPosition(),
			              scene.GetCameraFront(),
			              aspect, 10.f, 0.f, camCfg);
			m_Player = &scene.SpawnActor<Player>(m_Config.Player);
			m_Camera.SetTarget(m_Player);
		}

		scene.SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);

		if (m_Player)
			m_Player->Scale = glm::vec3(0.0025f);
	}

	void OnExit() override
	{
		m_Player  = nullptr;
		m_Scene3D = nullptr;
	}

	void OnUpdate(Hominem::Timestep ts) override
	{
		using namespace Hominem;

		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			RenderThread::QueueUpload([] {
				Renderer2D::GetShaderLibrary()->ReloadAll();
				Renderer3D::GetShaderLibrary()->ReloadAll();
			});
		}

		m_Camera.OnUpdate(ts);
	}

	void OnImGuiRender() override
	{
		ImGui::DragFloat("Camera Y Bias", &m_Camera.GetConfig().YBias, 0.01f);

		if (m_Scene3D)
		{
			ImGui::Text("Factory Scene");
			glm::vec3 rotDeg = glm::degrees(m_Scene3D->Rotation);
			ImGui::DragFloat3("Position##scene", &m_Scene3D->Position.x, 0.1f);
			if (ImGui::DragFloat3("Rotation##scene", &rotDeg.x, 0.5f))
				m_Scene3D->Rotation = glm::radians(rotDeg);
			ImGui::DragFloat3("Scale##scene", &m_Scene3D->Scale.x, 0.01f, 0.001f, 100.f);

			if (ImGui::Button("Save Scene Transform"))
			{
				m_Config.Scene.Position = m_Scene3D->Position;
				m_Config.Scene.Rotation = glm::degrees(m_Scene3D->Rotation);
				m_Config.Scene.Scale    = m_Scene3D->Scale;
				WorldConfig::SaveToFile("Resources/Config/game_config.json", m_Config);
			}
		}

		if (m_Player) m_Player->OnImGuiRender();
	}

	void OnEvent(Hominem::Event& e) override
	{
		Hominem::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Hominem::WindowResizeEvent>([this](Hominem::WindowResizeEvent& e)
		{
			if (e.GetWidth() == 0 || e.GetHeight() == 0) return false;
			m_Camera.OnWindowResize((float)e.GetWidth() / (float)e.GetHeight());
			return false;
		});
	}

private:
	WorldConfig        m_Config;
	SideScrollerCamera m_Camera;
	Player*            m_Player  = nullptr;
	SceneActor*        m_Scene3D = nullptr;
};
