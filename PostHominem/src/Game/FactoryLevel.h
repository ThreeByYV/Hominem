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

		auto& window = Application::Get().GetWindow();
		m_Aspect     = (float)window.GetWidth() / (float)window.GetHeight();
		scene.OnViewportResize(window.GetWidth(), window.GetHeight());

		const auto& sc = m_Config.Scene;
		m_Scene3D = &scene.SpawnActor<SceneActor>(sc.MeshPath);
		m_Scene3D->Position = sc.Position;
		m_Scene3D->Rotation = glm::radians(sc.Rotation);
		m_Scene3D->Scale    = sc.Scale;

		m_InitCameraX = m_Config.Camera.CameraX;
		m_InitCameraZ = m_Config.Camera.CameraZ;

		// First-run only: derive camera and player spawn from scene AABB when not yet saved.
		if (m_InitCameraX == 0.f || m_InitCameraZ == 0.f ||
		    m_Config.Player.Spawn.Position == glm::vec3(0.f, 1.f, 0.f))
			BootstrapFromAABB();

		// Player — spawn position comes from config (or bootstrapped above).
		m_Player = &scene.SpawnActor<Player>(m_Config.Player);
		if (m_Player)
			m_Player->Scale = m_Config.Player.Scale;

		// Camera — all values from config; camera_x/camera_z saved by "Save Scene Transform".
		SideScrollerCamera::Config camCfg;
		camCfg.VisibleHeight = m_Config.Camera.VisibleHeight;
		camCfg.PlayerScreenY = m_Config.Camera.PlayerScreenY;
		camCfg.YBias         = m_Config.Camera.YBias;
		camCfg.FOVDeg        = m_Config.Camera.FOVDeg;
		camCfg.XSpeed        = m_Config.Camera.XSpeed;
		camCfg.YSpeed        = m_Config.Camera.YSpeed;
		camCfg.LeadStrength  = m_Config.Camera.LeadStrength;
		camCfg.YDeadZone     = m_Config.Camera.YDeadZone;

		m_Camera.Init(scene.GetCamera(),
		              scene.GetCameraPosition(),
		              scene.GetCameraFront(),
		              m_Aspect, m_InitCameraZ, m_Config.Player.RestY, camCfg);
		m_Camera.SetTarget(m_Player);
		scene.GetCameraPosition().x = m_InitCameraX;

		scene.SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);

		scene.BakeEnvironment(glm::vec3(0.08405184f, 1.5f, 27.0f));

		m_Scene = &scene;
	}

	void OnExit() override
	{
		m_Player  = nullptr;
		m_Scene3D = nullptr;
		m_Scene   = nullptr;
	}

	void OnUpdate(Hominem::Timestep ts) override
	{
		using namespace Hominem;

		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			// Reload config and re-apply hot-reloadable values.
#ifdef HMN_SOURCE_RESOURCES_PATH
			constexpr const char* k_ConfigPath = HMN_SOURCE_RESOURCES_PATH "/Config/game_config.json";
#else
			constexpr const char* k_ConfigPath = "Resources/Config/game_config.json";
#endif
			WorldConfig newCfg;
			if (WorldConfig::LoadFromFile(k_ConfigPath, newCfg))
			{
				HMN_CORE_INFO("Config reloaded — player scale({:.4f},{:.4f},{:.4f})", newCfg.Player.Scale.x, newCfg.Player.Scale.y, newCfg.Player.Scale.z);
				m_Config = newCfg;
				auto& cam        = m_Camera.GetConfig();
				cam.VisibleHeight = newCfg.Camera.VisibleHeight;
				cam.PlayerScreenY = newCfg.Camera.PlayerScreenY;
				cam.YBias         = newCfg.Camera.YBias;
				cam.FOVDeg        = newCfg.Camera.FOVDeg;
				cam.XSpeed        = newCfg.Camera.XSpeed;
				cam.YSpeed        = newCfg.Camera.YSpeed;
				cam.LeadStrength  = newCfg.Camera.LeadStrength;
				cam.YDeadZone     = newCfg.Camera.YDeadZone;
				m_Camera.OnWindowResize(m_Aspect); // reapply projection with new FOV
				if (m_Player) m_Player->Reload(newCfg.Player);
				if (m_Scene3D)
				{
					m_Scene3D->Position = newCfg.Scene.Position;
					m_Scene3D->Rotation = glm::radians(newCfg.Scene.Rotation);
					m_Scene3D->Scale    = newCfg.Scene.Scale;
				}
			}
			RenderThread::QueueUpload([] {
				Renderer2D::GetShaderLibrary()->ReloadAll();
				Renderer3D::GetShaderLibrary()->ReloadAll();
				Renderer3D::ReloadVariants();
			});
		}

		m_Camera.OnUpdate(ts);
	}

	void OnImGuiRender() override
	{
		if (m_Scene)
		{
			ImGui::Separator();
			ImGui::Text("Environment Map");
			if (ImGui::Checkbox("Enable##envmap", &m_EnvMapEnabled))
				m_Scene->SetEnvMapIntensity(m_EnvMapEnabled ? m_EnvMapIntensity : 0.f);
			if (m_EnvMapEnabled)
			{
				if (ImGui::SliderFloat("Intensity##envmap", &m_EnvMapIntensity, 0.f, 1.f))
					m_Scene->SetEnvMapIntensity(m_EnvMapIntensity);
			}
			ImGui::Separator();
		}

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
				// Load fresh from disk so we don't overwrite lights or other fields
				// that other systems (GameLayer) may have already saved.
				WorldConfig cfg;
				WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg);
				cfg.Scene.Position  = m_Scene3D->Position;
				cfg.Scene.Rotation  = glm::degrees(m_Scene3D->Rotation);
				cfg.Scene.Scale     = m_Scene3D->Scale;
				cfg.Camera.CameraX  = m_InitCameraX;
				cfg.Camera.CameraZ  = m_Camera.GetCameraZ();
				WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
				m_Config = cfg;
			}
		}

		if (m_Player) m_Player->OnImGuiRender();
	}

	PlayerDebugInfo GetPlayerDebugInfo() const override
	{
		if (!m_Player) return {};
		PlayerDebugInfo info;
		info.valid    = true;
		info.position = m_Player->Position;
		info.velocity = m_Player->GetVelocity();
		info.speed    = glm::length(info.velocity);
		info.isMoving = m_Player->IsMoving();
		return info;
	}

	void OnEvent(Hominem::Event& e) override
	{
		Hominem::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Hominem::WindowResizeEvent>([this](Hominem::WindowResizeEvent& e)
		{
			if (e.GetWidth() == 0 || e.GetHeight() == 0) return false;
			m_Aspect = (float)e.GetWidth() / (float)e.GetHeight();
			m_Camera.OnWindowResize(m_Aspect);
			return false;
		});
	}

private:
	void BootstrapFromAABB()
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
		// Place player near the front-centre of the scene
		if (m_Config.Player.Spawn.Position == glm::vec3(0.f, 1.f, 0.f))
		{
			m_Config.Player.Spawn.Position = {
				m_InitCameraX,
				m_Config.Player.RestY,
				wMin.z + size.z * 0.9f
			};
		}
	}

	WorldConfig        m_Config;
	float              m_Aspect       = 1.f;
	float              m_InitCameraX  = 0.f;
	float              m_InitCameraZ  = 0.f;
	SideScrollerCamera m_Camera;
	Player*            m_Player  = nullptr;
	SceneActor*        m_Scene3D = nullptr;
	Hominem::Scene*    m_Scene   = nullptr;

	bool               m_EnvMapEnabled   = true;
	float              m_EnvMapIntensity  = 1.f;
};
