#pragma once

#include "Level.h"
#include "Player.h"
#include "Game/Actors/InfiniteFloorActor.h"
#include "Game/Actors/SceneActor.h"
#include "Game/WorldConfig.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/MouseButtonCodes.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Scene/SceneCamera.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/MouseEvent.h"

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
		m_Aspect = (float)window.GetWidth() / (float)window.GetHeight();

		m_Camera    = &scene.GetCamera();
		m_CameraPos = &scene.GetCameraPosition();
		m_CameraFront = &scene.GetCameraFront();

		m_Camera->SetPerspective(glm::radians(m_Fov), m_Aspect, 0.1f, 1000.f);
		scene.OnViewportResize(window.GetWidth(), window.GetHeight());

		*m_CameraPos   = { 0.f, 2.f, 15.f };
		*m_CameraFront = ComputeFront();

		const auto& sc = m_Config.Scene;
		m_Scene3D = &scene.SpawnActor<SceneActor>(sc.MeshPath);
		m_Scene3D->Position = sc.Position;
		m_Scene3D->Rotation = glm::radians(sc.Rotation);
		m_Scene3D->Scale    = sc.Scale;

		scene.SpawnActor<InfiniteFloorActor>(m_Config.Physics.Floor);
		m_Player = &scene.SpawnActor<Player>(m_Config.Player);
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
			Renderer2D::GetShaderLibrary()->ReloadAll();
			Renderer3D::GetShaderLibrary()->ReloadAll();
			if (m_Player) m_Player->ReloadShader();
		}

		if (!m_CameraPos || !m_CameraFront) return;

		float dt = (float)ts;

		// --- LMB drag to look ---
		float mouseX = Input::GetMouseX();
		float mouseY = Input::GetMouseY();

		if (Input::IsMouseButtonPressed(HMN_MOUSE_BUTTON_LEFT))
		{
			if (!m_FirstMouse)
			{
				float dx = (mouseX - m_LastMouseX) * m_Sensitivity;
				float dy = (m_LastMouseY - mouseY) * m_Sensitivity;
				m_Yaw   += dx;
				m_Pitch  = glm::clamp(m_Pitch + dy, -89.f, 89.f);
				*m_CameraFront = ComputeFront();
			}
			m_FirstMouse = false;
		}
		else
		{
			m_FirstMouse = true;
		}

		m_LastMouseX = mouseX;
		m_LastMouseY = mouseY;

		// --- WASD fly movement with exponential acceleration ---
		glm::vec3 front = *m_CameraFront;
		glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.f, 1.f, 0.f)));
		glm::vec3 up    = glm::vec3(0.f, 1.f, 0.f);

		bool w = Input::IsKeyPressed(HMN_KEY_W);
		bool s = Input::IsKeyPressed(HMN_KEY_S);
		bool a = Input::IsKeyPressed(HMN_KEY_A);
		bool d = Input::IsKeyPressed(HMN_KEY_D);
		bool q = Input::IsKeyPressed(HMN_KEY_Q);
		bool e = Input::IsKeyPressed(HMN_KEY_E);

		if (w || s || a || d || q || e)
		{
			// Exponential ramp-up while keys are held, capped at 20x base speed
			m_CurrentSpeed = glm::min(m_CurrentSpeed * (1.f + 2.5f * dt), m_MoveSpeed * 20.f);
		}
		else
		{
			m_CurrentSpeed = m_MoveSpeed; // instant reset when keys released
		}

		float speed = m_CurrentSpeed * dt;
		if (w) *m_CameraPos += front * speed;
		if (s) *m_CameraPos -= front * speed;
		if (a) *m_CameraPos -= right * speed;
		if (d) *m_CameraPos += right * speed;
		if (e) *m_CameraPos += up    * speed;
		if (q) *m_CameraPos -= up    * speed;
	}

	void OnImGuiRender() override
	{
		ImGui::Text("Fly Camera  |  RMB + WASD/QE to move  |  Scroll = FOV");

		if (ImGui::SliderFloat("FOV",        &m_Fov,       10.f, 120.f) && m_Camera)
			m_Camera->SetPerspective(glm::radians(m_Fov), m_Aspect, 0.1f, 1000.f);
		ImGui::SliderFloat("Move Speed",     &m_MoveSpeed, 0.5f,  50.f);
		ImGui::SliderFloat("Look Sensitivity",&m_Sensitivity, 0.01f, 1.f);

		if (m_CameraPos)
			ImGui::DragFloat3("Position", &m_CameraPos->x, 0.1f);

		if (m_Scene3D)
		{
			ImGui::Separator();
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

		dispatcher.Dispatch<Hominem::MouseScrolledEvent>([this](Hominem::MouseScrolledEvent& e)
		{
			m_Fov -= e.GetYOffset() * 2.f;
			m_Fov  = glm::clamp(m_Fov, 10.f, 120.f);
			if (m_Camera)
				m_Camera->SetPerspective(glm::radians(m_Fov), m_Aspect, 0.1f, 1000.f);
			return false;
		});

		dispatcher.Dispatch<Hominem::WindowResizeEvent>([this](Hominem::WindowResizeEvent& e)
		{
			m_Aspect = (float)e.GetWidth() / (float)e.GetHeight();
			if (m_Camera)
				m_Camera->SetPerspective(glm::radians(m_Fov), m_Aspect, 0.1f, 1000.f);
			return false;
		});
	}

private:
	glm::vec3 ComputeFront() const
	{
		return glm::normalize(glm::vec3{
			cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
			sin(glm::radians(m_Pitch)),
			sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))
		});
	}

	WorldConfig m_Config;

	Player*     m_Player  = nullptr;
	SceneActor* m_Scene3D = nullptr;

	Hominem::SceneCamera* m_Camera      = nullptr;
	glm::vec3*            m_CameraPos   = nullptr;
	glm::vec3*            m_CameraFront = nullptr;

	// Perspective
	float m_Fov    = 60.f;
	float m_Aspect = 1.f;

	// Fly camera state
	float m_Yaw          = -90.f;
	float m_Pitch        =   0.f;
	float m_MoveSpeed    =   5.f;
	float m_CurrentSpeed =   5.f; // ramps up exponentially while key held
	float m_Sensitivity  =   0.03f;
	float m_LastMouseX   =   0.f;
	float m_LastMouseY   =   0.f;
	bool  m_FirstMouse   = true;
};
