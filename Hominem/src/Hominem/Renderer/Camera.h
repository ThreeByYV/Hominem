#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

<<<<<<< HEAD
	class Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			: m_Projection(projection) 
		{
		}
=======
    class Camera 
    {
    public:
        Camera() = default;
        virtual ~Camera() = default;

        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

        virtual void SetProjection(const glm::mat4& projection);
        virtual void SetViewMatrix(const glm::mat4& view);

    protected:
        void RecalculateViewProjectionMatrix();

        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    };

    class OrthographicCamera : public Camera
    {
    public:
        OrthographicCamera() = default;
        OrthographicCamera(float left, float right, float bottom, float top);

        void SetProjection(float left, float right, float bottom, float top);
>>>>>>> 3dbe5f9 (feat: add basic/weird perspective camera)

		virtual ~Camera() = default;

<<<<<<< HEAD
		const glm::mat4& GetProjection() const { return m_Projection; }
	protected:
		glm::mat4 m_Projection = glm::mat4(1.0f);
	};
=======
        float GetRotation() const { return m_Rotation; }
        void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }

    private:
        void RecalculateViewMatrix();

        glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
        float m_Rotation = 0.0f;
    };

    class PerspectiveCamera : public Camera 
    {
    public:
        PerspectiveCamera() = default;
        PerspectiveCamera(float fov, float aspectRatio, float nearPlane, float farPlane);

        void SetProjection(float fov, float aspectRatio, float nearPlane, float farPlane);
        void SetViewMatrix(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);

        float GetFOV() const { return m_FOV; }
        float GetAspectRatio() const { return m_AspectRatio; }
        float GetNearPlane() const { return m_NearPlane; }
        float GetFarPlane() const { return m_FarPlane; }

    private:
        float m_FOV;
        float m_AspectRatio;
        float m_NearPlane;
        float m_FarPlane;
    };
>>>>>>> 3dbe5f9 (feat: add basic/weird perspective camera)
}