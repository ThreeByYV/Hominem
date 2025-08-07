//#pragma once

//#include <glm/glm.hpp>
//
//namespace Hominem {
//
//	class OrthographicCamera
//	{
//	public:
//		OrthographicCamera(float left, float right, float bottom, float top);
//
//		const glm::vec3& GetPosition() const { return m_Position; }
//		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }
//		void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }
//	
//		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix;  }
//		const glm::mat4& GetViewMatrix() const { return m_ViewProjectionMatrixCache; }
//		const glm::mat4& GetViewProjectionMatrixCache() const { return m_ViewProjectionMatrixCache; }
//
//	private:
//		void RecalculateViewMatrix(); 
//	private:
//		glm::mat4 m_ProjectionMatrix;
//		glm::mat4 m_viewMatrix;
//		glm::mat4 m_ViewProjectionMatrixCache;
//
//		glm::vec3 m_Position;
//		float m_Rotation = 0.0f;
//	};
//}