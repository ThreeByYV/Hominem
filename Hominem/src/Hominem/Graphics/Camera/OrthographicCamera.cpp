#include "hmnpch.h"
//
//#include "OrthographicCamera.h"
//#include <glm/gtc/matrix_transform.hpp>
//
//namespace Hominem {
//
//	OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
//		: m_ProjectionMatrix(glm::ortho(left, right, bottom, top, -1.0f, -1.0f)), m_viewMatrix(1.0f)
//	{
//		m_ViewProjectionMatrixCache = m_ProjectionMatrix * m_viewMatrix;
//	}
//
//	void OrthographicCamera::RecalculateViewMatrix()
//	{
//		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) * 
//			glm::rotate(glm::mat4(1.0f), m_Rotation, glm::vec3(0, 0, 1));
//
//		//need to invert transform here
//		m_viewMatrix = glm::inverse(transform);
//		m_ViewProjectionMatrixCache = m_ProjectionMatrix * m_viewMatrix;
//
//	}
//
//}
