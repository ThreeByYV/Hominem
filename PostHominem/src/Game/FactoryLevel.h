#pragma once

#include "Level.h"
#include "Player.h"
#include "Game/Actors/InfiniteFloorActor.h"
#include "Game/Actors/SceneActor.h"
#include "Game/WorldConfig.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Scene/SceneCamera.h"
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
		m_Aspect      = (float)window.GetWidth() / (float)window.GetHeight();
		m_Camera      = &scene.GetCamera();
		m_CameraPos   = &scene.GetCameraPosition();
		m_CameraFront = &scene.GetCameraFront();

		scene.OnViewportResize(window.GetWidth(), window.GetHeight());
		*m_CameraFront = { 0.f, 0.f, -1.f }; // always look at -Z

		const auto& sc = m_Config.Scene;
		m_Scene3D = &scene.SpawnActor<SceneActor>(sc.MeshPath);
		m_Scene3D->Position = sc.Position;
		m_Scene3D->Rotation = glm::radians(sc.Rotation);
		m_Scene3D->Scale    = sc.Scale;

		// Compute world AABB to find factory floor and size.
		if (m_Scene3D->GetMesh())
		{
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
			m_FactoryFloorY  = wMin.y;
			m_FactoryCenterY = (wMin.y + wMax.y) * 0.5f;

			// Perspective camera: FOV chosen so factory height fits with 10% margin.
			// dist = half_height / tan(half_fov)
			constexpr float k_FOVDeg  = 60.f;
			float halfFovRad          = glm::radians(k_FOVDeg * 0.5f);
			float dist                = (size.y * 0.5f * 1.1f) / glm::tan(halfFovRad);
			float cz                  = wMax.z + dist;
			m_Camera->SetPerspective(k_FOVDeg, m_Aspect, 0.1f, dist + size.z + 100.f);

			// Camera starts centered on the factory.
			float factoryCenterX = (wMin.x + wMax.x) * 0.5f;
			m_CameraZ            = cz;
			*m_CameraPos         = { factoryCenterX, m_FactoryCenterY, cz };

			// Scene origin = factory bottom-left: (0,0,0) in actor space = floor, left, back.
			scene.SetWorldTransform(glm::translate(glm::mat4(1.f), wMin));

			// Factory AABB min can include sub-floor geometry (plants, cables).
			// Raise the reference 0.5m to approximate the actual walkable surface.
			constexpr float k_FloorLift = 0.5f;
			m_FactoryFloorY += k_FloorLift;
			m_Config.Physics.Floor.Position.y =
				wMin.y - m_Config.Physics.Floor.Scale.y * 0.5f;

			// Player: left edge (X=0), feet at floor, front face of factory (Z=size.z).
			const auto& col = m_Config.Player.Collider;
			float floorY    = col.Extents.y + glm::abs(col.Offset.y);

			PlayerConfig pc   = m_Config.Player;
			pc.Spawn.Position = { 0.f, floorY, size.z - 0.01f };
			m_Player = &scene.SpawnActor<Player>(pc);
		}
		else
		{
			m_CameraZ = 10.f;
			m_Camera->SetPerspective(60.f, m_Aspect, 0.1f, 1000.f);
			m_Player = &scene.SpawnActor<Player>(m_Config.Player);
		}

		scene.SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);

		// Scale character to 5'9". Adjust k_NativeHeightCm to measured Blender height.
		if (m_Player)
		{
			constexpr float k_NativeHeightCm = 350.0f;
			m_Player->Scale = glm::vec3(Hominem::FeetInches(5, 9.f) / k_NativeHeightCm * 0.5f);
		}
	}

	void OnExit() override
	{
		m_Player      = nullptr;
		m_Scene3D     = nullptr;
		m_Camera      = nullptr;
		m_CameraPos   = nullptr;
		m_CameraFront = nullptr;
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

		// Smooth camera follow — track player X only; Y and Z locked.
		if (m_Player && m_CameraPos)
		{
			float smooth   = glm::clamp(m_Config.Camera.Smoothing * (float)ts * 60.f, 0.f, 1.f);
			m_CameraPos->x = glm::mix(m_CameraPos->x, m_Player->Position.x, smooth);
			m_CameraPos->y = m_FactoryCenterY;
			m_CameraPos->z = m_CameraZ;
		}
	}

	void OnImGuiRender() override
	{
		float fovDeg = m_Camera->GetPerspectiveFOV();
		if (ImGui::SliderFloat("FOV", &fovDeg, 20.f, 120.f) && m_Camera)
			m_Camera->SetPerspectiveFOV(fovDeg);

		if (m_Scene3D)
		{
			ImGui::Separator();
			ImGui::Text("Factory Scene");
			glm::vec3 rotDeg = glm::degrees(m_Scene3D->Rotation);
			ImGui::DragFloat3("Position##scene", &m_Scene3D->Position.x, 0.1f);
			if (ImGui::DragFloat3("Rotation##scene", &rotDeg.x, 0.5f))
				m_Scene3D->Rotation = glm::radians(rotDeg);
			ImGui::DragFloat3("Scale##scene",    &m_Scene3D->Scale.x,    0.01f, 0.001f, 100.f);

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
			m_Aspect = (float)e.GetWidth() / (float)e.GetHeight();
			if (m_Camera)
				m_Camera->SetPerspective(m_Camera->GetPerspectiveFOV(), m_Aspect, 0.1f, 1000.f);
			return false;
		});
	}

private:
	WorldConfig m_Config;

	Player*     m_Player  = nullptr;
	SceneActor* m_Scene3D = nullptr;

	Hominem::SceneCamera* m_Camera      = nullptr;
	glm::vec3*            m_CameraPos   = nullptr;
	glm::vec3*            m_CameraFront = nullptr;

	float m_CameraZ        = 10.f;
	float m_Aspect         = 1.f;
	float m_FactoryFloorY  = 0.f;
	float m_FactoryCenterY = 0.f;
};
