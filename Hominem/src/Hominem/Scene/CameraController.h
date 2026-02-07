#pragma once

#include "Entity.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Events/MouseEvent.h"
#include <glm/glm.hpp>

namespace Hominem {

	// ECS-based camera controller that operates on Entity transforms
	class EntityCameraController
	{
	public:
		EntityCameraController() = default;

		void SetEntity(Entity entity) { m_CameraEntity = entity; }
		Entity GetEntity() const { return m_CameraEntity; }

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		float GetSpeed() const { return m_MoveSpeed; }
		void SetSpeed(float speed) { m_MoveSpeed = speed; }

		float GetMouseSensitivity() const { return m_MouseSensitivity; }
		void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

		// Get current camera position from entity
		glm::vec3 GetPosition();
		void SetPosition(const glm::vec3& position);

	private:
		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);

		void UpdateCameraVectors();

	private:
		Entity m_CameraEntity;

		// Movement settings
		float m_MoveSpeed = 5.0f;
		float m_MouseSensitivity = 0.1f;
		bool m_FirstMouse = true;

		// Camera angles
		float m_Yaw = 90.0f;   // Initialized to face +Z
		float m_Pitch = 0.0f;

		// Camera vectors (computed from angles)
		glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);

		glm::vec2 m_LastMousePos = glm::vec2(0.0f);
	};

}
