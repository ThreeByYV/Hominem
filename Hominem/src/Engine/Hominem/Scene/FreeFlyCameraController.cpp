#include "hmnpch.h"
#include "FreeFlyCameraController.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/MouseButtonCodes.h"

#include <imgui.h>

namespace Hominem {

	void FreeFlyCameraController::OnScroll(float yOffset, Scene& scene)
	{
		if (!Enabled) return;
		constexpr float kZoomStep = 0.5f;
		scene.GetCameraPosition() += scene.GetCameraFront() * (yOffset * kZoomStep);
	}

	void FreeFlyCameraController::OnUpdate(Timestep ts, Scene& scene)
	{
		if (!Enabled)
		{
			m_Looking = false;
			return;
		}

		const ImGuiIO& io = ImGui::GetIO();

		const auto [mx, my] = Input::GetMousePosition();
		const glm::vec2 mouse = { mx, my };
		const bool lmbHeld = Input::IsMouseButtonPressed(HMN_MOUSE_BUTTON_LEFT) && !io.WantCaptureMouse;

		glm::vec3& pos   = scene.GetCameraPosition();
		glm::vec3& front = scene.GetCameraFront();

		if (lmbHeld && !m_Looking)
		{
			// Just grabbed the view: derive yaw/pitch from the current look direction
			// so the camera doesn't snap to a different orientation on first move.
			m_Yaw       = std::atan2(front.z, front.x);
			m_Pitch     = std::asin(glm::clamp(front.y, -1.f, 1.f));
			m_LastMouse = mouse;
			m_Looking   = true;
		}
		else if (!lmbHeld)
		{
			m_Looking = false;
		}

		if (m_Looking)
		{
			const glm::vec2 delta = mouse - m_LastMouse;
			constexpr float kLookSpeed = 0.0025f;
			m_Yaw   += delta.x * kLookSpeed;
			m_Pitch -= delta.y * kLookSpeed;
			m_Pitch  = glm::clamp(m_Pitch, glm::radians(-89.f), glm::radians(89.f));

			front = glm::normalize(glm::vec3(
				std::cos(m_Yaw) * std::cos(m_Pitch),
				std::sin(m_Pitch),
				std::sin(m_Yaw) * std::cos(m_Pitch)));
		}
		m_LastMouse = mouse;

		const glm::vec3 right = glm::normalize(glm::cross(front, { 0.f, 1.f, 0.f }));
		const glm::vec3 up    = { 0.f, 1.f, 0.f };
		const float speed = m_Speed * (Input::IsKeyPressed(HMN_KEY_LEFT_SHIFT) ? 4.f : 1.f) * (float)ts;

		// Suppress WASD/Q/E flight while a panel/text field has keyboard focus.
		if (!io.WantCaptureKeyboard)
		{
			if (Input::IsKeyPressed(HMN_KEY_W)) pos += front * speed;
			if (Input::IsKeyPressed(HMN_KEY_S)) pos -= front * speed;
			if (Input::IsKeyPressed(HMN_KEY_A)) pos -= right * speed;
			if (Input::IsKeyPressed(HMN_KEY_D)) pos += right * speed;
			if (Input::IsKeyPressed(HMN_KEY_E)) pos += up    * speed;
			if (Input::IsKeyPressed(HMN_KEY_Q)) pos -= up    * speed;
		}
	}

}
