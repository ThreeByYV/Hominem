#pragma once

#include "Hominem/Renderer/Camera.h"
#include "Hominem/Core/Timestep.h"

#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/MouseEvent.h"

namespace Hominem {

	class OrthographicCameraController
	{
	public:
		OrthographicCameraController(float aspectRatio, bool hasRotation = false); // by default aspectratio * 2 unit for zooming functionality
	
		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);
		
		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const { return m_Camera; }
	private:
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);
	private:
		//order matters here, for initialization
		float m_AspectRatio;
		float m_ZoomLevel = 2.0f;
		OrthographicCamera m_Camera;

		bool m_Rotation;

		glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
		float m_CameraRotation = 0.0f;
		float m_CameraTranslationSpeed = 5.0f, m_CameraRotationSpeed = 180.0f;
	};
}
