#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>

namespace Hominem {

    class Camera
    {
    public:
        enum class ProjectionType { Perspective = 0, Orthographic = 1 };

        Camera() { RecalculateProjection(); }

        const glm::mat4& GetProjectionMatrix()     const { return m_ProjectionMatrix; }
        const glm::mat4& GetViewMatrix()            const { return m_ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix()  const { return m_ViewProjectionMatrix; }

        void SetViewMatrix(const glm::mat4& view)
        {
            m_ViewMatrix = view;
            RecalculateViewProjectionMatrix();
        }

        void SetOrthographic(float size, float nearClip, float farClip)
        {
            m_ProjectionType    = ProjectionType::Orthographic;
            m_OrthographicSize  = size;
            m_OrthographicNear  = nearClip;
            m_OrthographicFar   = farClip;
            RecalculateProjection();
        }

        void SetPerspective(float fov, float aspectRatio, float nearClip, float farClip)
        {
            m_ProjectionType  = ProjectionType::Perspective;
            m_PerspectiveFOV  = fov;
            m_AspectRatio     = aspectRatio;
            m_PerspectiveNear = nearClip;
            m_PerspectiveFar  = farClip;
            RecalculateProjection();
        }

        // Remember to set this before rendering, whenever the viewport resizes.
        void SetViewportSize(uint32_t width, uint32_t height)
        {
            if (width == 0 || height == 0) return;
            m_AspectRatio = (float)width / (float)height;
            RecalculateProjection();
        }

        float GetOrthographicSize()  const { return m_OrthographicSize; }
        void  SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjection(); }

        float GetPerspectiveFOV()   const { return m_PerspectiveFOV; }
        void  SetPerspectiveFOV(float fov) { m_PerspectiveFOV = fov; RecalculateProjection(); }

        ProjectionType GetProjectionType() const { return m_ProjectionType; }
        void SetProjectionType(ProjectionType type) { m_ProjectionType = type; RecalculateProjection(); }

    private:
        void RecalculateViewProjectionMatrix()
        {
            m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
        }

        void RecalculateProjection()
        {
            if (m_ProjectionType == ProjectionType::Perspective)
            {
                if (m_AspectRatio <= 0.f) return;
                m_ProjectionMatrix = glm::perspective(glm::radians(m_PerspectiveFOV), m_AspectRatio,
                    m_PerspectiveNear, m_PerspectiveFar);
            }
            else
            {
                float orthoLeft   = -m_OrthographicSize * m_AspectRatio * 0.5f;
                float orthoRight  =  m_OrthographicSize * m_AspectRatio * 0.5f;
                float orthoBottom = -m_OrthographicSize * 0.5f;
                float orthoTop    =  m_OrthographicSize * 0.5f;
                m_ProjectionMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop,
                    m_OrthographicNear, m_OrthographicFar);
            }
            RecalculateViewProjectionMatrix();
        }

        glm::mat4 m_ProjectionMatrix     = glm::mat4(1.0f);
        glm::mat4 m_ViewMatrix           = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);

        ProjectionType m_ProjectionType = ProjectionType::Orthographic;

        float m_OrthographicSize = 10.0f;
        float m_OrthographicNear = -1.0f, m_OrthographicFar = 1.0f;

        float m_PerspectiveFOV  = 45.0f;
        float m_PerspectiveNear = 0.1f, m_PerspectiveFar = 1000.0f;

        float m_AspectRatio = 0.0f;
    };

}
