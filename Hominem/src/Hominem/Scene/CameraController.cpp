#include "hmnpch.h"
#include "CameraController.h"
#include "Components.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

	void EntityCameraController::OnUpdate(Timestep ts)
	{
		if (!m_CameraEntity)
			return;

		auto& transform = m_CameraEntity.GetComponent<TransformComponent>();
		glm::vec3 position = transform.Translation;

		// 2D camera movement
		// WASD moves camera in XY plane only
		float velocity = m_MoveSpeed * ts * 0.75;

		if (Input::IsKeyPressed(HMN_KEY_W))
			position.y += velocity;  // Move up
		if (Input::IsKeyPressed(HMN_KEY_S))
			position.y -= velocity;  // Move down
		if (Input::IsKeyPressed(HMN_KEY_A))
			position.x -= velocity;  // Move left
		if (Input::IsKeyPressed(HMN_KEY_D))
			position.x += velocity;  // Move right

		// Update transform
		SetPosition(position);
	}

	void EntityCameraController::OnEvent(Event& e)
	{
		// No mouse control for 2D cinematic camera
		// EventDispatcher dispatcher(e);
		// dispatcher.Dispatch<MouseMovedEvent>(HMN_BIND_EVENT_FN(EntityCameraController::OnMouseMoved));
		// dispatcher.Dispatch<MouseScrolledEvent>(HMN_BIND_EVENT_FN(EntityCameraController::OnMouseScrolled));
	}

	bool EntityCameraController::OnMouseMoved(MouseMovedEvent& e)
	{
		// Disabled for 2D cinematic camera
		return false;
	}

	bool EntityCameraController::OnMouseScrolled(MouseScrolledEvent& e)
	{
		// Disabled for 2D cinematic camera
		return false;
	}

	void EntityCameraController::UpdateCameraVectors()
	{
		if (!m_CameraEntity)
			return;

		// For 2D orthographic camera (Planet of Lana style):
		// Camera is fixed looking at the XY plane (no rotation)
		// Just set position, no need to recalculate view matrix
		// The camera transform is just a translation matrix
	}

	glm::vec3 EntityCameraController::GetPosition()
	{
		if (!m_CameraEntity)
			return glm::vec3(0.0f);

		auto& transform = m_CameraEntity.GetComponent<TransformComponent>();
		return transform.Translation;
	}

	void EntityCameraController::SetPosition(const glm::vec3& position)
	{
		if (!m_CameraEntity)
			return;

		auto& transform = m_CameraEntity.GetComponent<TransformComponent>();
		transform.Translation = position;
	}

}
