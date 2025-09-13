#include "hmnpch.h"

#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

    void Camera::SetProjection(const glm::mat4& projection)
    {
        m_ProjectionMatrix = projection;
        RecalculateViewProjectionMatrix();
    }

    void Camera::SetViewMatrix(const glm::mat4& view)
    {
        m_ViewMatrix = view;
        RecalculateViewProjectionMatrix();
    }

    void Camera::RecalculateViewProjectionMatrix()
    {
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    // OrthographicCamera Implementation
    OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
    {
        SetProjection(left, right, bottom, top);
    }

    void OrthographicCamera::SetProjection(float left, float right, float bottom, float top)
    {
        glm::mat4 projection = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
        Camera::SetProjection(projection);
    }

    void OrthographicCamera::RecalculateViewMatrix()
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
            glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

        glm::mat4 viewMatrix = glm::inverse(transform);
        Camera::SetViewMatrix(viewMatrix);
    }

    // PerspectiveCamera Implementation
    PerspectiveCamera::PerspectiveCamera(float fov, float aspectRatio, float nearPlane, float farPlane)
        : m_FOV(fov), m_AspectRatio(aspectRatio), m_NearPlane(nearPlane), m_FarPlane(farPlane)
    {
        SetProjection(fov, aspectRatio, nearPlane, farPlane);
    }

    void PerspectiveCamera::SetProjection(float fov, float aspectRatio, float nearPlane, float farPlane)
    {
        m_FOV = fov;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        Camera::SetProjection(projection);
    }

    void PerspectiveCamera::SetViewMatrix(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up)
    {
        glm::mat4 viewMatrix = glm::lookAt(position, position + target, up);
        Camera::SetViewMatrix(viewMatrix);
    }
}